#!/usr/bin/env python3
"""Measure actual ARM ELF LOAD/RAM intervals and reject static budget overruns."""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, fields,
    file_identity, identifier, load_json, regular_file)
from tools.evidence.identity import resolved_identity, resolved_input_files
from tools.measurement.elf import (Elf32, image_window, interval_bytes,
                                   intervals_union)
from tools.configure.memory import resource_budget_projection


def integer(value, name: str, *, minimum: int = 0) -> int:
    if type(value) is not int or value < minimum or value >= 2**32:
        raise EvidenceError("invalid " + name)
    return value


def resolved_float_abi(resolved: dict, *, float_abi: str | None = None,
                       require_cpu: bool = False) -> str:
    """Bind an explicit ELF interpretation to the sole resolved CPU contract.

    The canonical CPU retains its code-generation ABI: softfp may use FPU/MVE
    instructions but shares the ELF base calling convention with soft. Only
    hard selects the VFP argument convention. An explicit override asserts that
    ELF convention (hard/soft); it never changes CPU facts or compiler flags.
    CPU-absent historical documents retain their hard-only scope. Malformed
    present metadata never falls back. ELF flags are validated, never guessed.
    """
    if "cpu" not in resolved and not require_cpu:
        selected = "hard"
    else:
        cpu = resolved.get("cpu")
        if (not isinstance(cpu, dict) or type(cpu.get("float_abi")) is not str
                or cpu["float_abi"] not in {"hard", "soft", "softfp"}):
            raise EvidenceError(
                "resolved CPU requires an explicit hard, soft or softfp float ABI")
        selected = "hard" if cpu["float_abi"] == "hard" else "soft"
    if float_abi is not None and (type(float_abi) is not str or
            float_abi not in {"hard", "soft"} or float_abi != selected):
        raise EvidenceError("explicit resource ABI differs from resolved CPU")
    return selected


def validate_elf_configuration(elf: Elf32, resolved: dict) -> None:
    """Require the actual linked configuration symbols, not matching filenames."""
    actual = "".join(f"{elf.symbol(f'__nx_resolved_sha256_{index}'):08x}"
                     for index in range(8))
    if actual != resolved["configuration_sha256"]:
        raise EvidenceError("ELF does not bind this resolved configuration")


def input_protection_paths(resolved_path: Path) -> set[Path]:
    """Protect navigation and every located source before any output mutation.

    Navigation can only deny an overwrite here. It never authorizes an ABI,
    selection or input identity; the actual measurement revalidates the sole
    resolved digest and exact declared-input closure through the shared reader.
    """
    navigation = resolved_path.parent / "input_paths.json"
    protected = {resolved_path.resolve(), navigation.resolve()}
    if navigation.exists():
        paths = load_json(navigation).values()
        if any(not isinstance(path, str) or not path for path in paths):
            raise EvidenceError("invalid configuration input navigation path")
        protected.update(Path(path).resolve() for path in paths)
    return protected


def validate_budget(budget: dict, memory: list[dict]) -> None:
    fields(budget, {"schema_version", "flash_loaded_max", "flash_footprint_max",
                    "ram_reserved_max", "msp_max", "heap_max"})
    if type(budget["schema_version"]) is not int or budget["schema_version"] != 1:
        raise EvidenceError("unsupported resource budget schema")
    for key in ("flash_loaded_max", "flash_footprint_max", "msp_max", "heap_max"):
        integer(budget[key], key)
    if not isinstance(budget["ram_reserved_max"], dict) or set(
            budget["ram_reserved_max"]) != {domain["id"] for domain in memory}:
        raise EvidenceError("budget must enumerate every physical RAM domain")
    for key, value in budget["ram_reserved_max"].items():
        identifier(key, "RAM domain")
        integer(value, "RAM budget")


def memory_contract(resolved: dict) -> tuple[dict, list[dict]]:
    """Consume memory facts from the resolver's selected exact variant."""
    flash = resolved["flash"]
    memory = resolved["memory"]
    if not isinstance(flash, dict) or not isinstance(memory, list) or not memory:
        raise EvidenceError("resolved physical Flash/RAM contract missing")
    integer(flash["origin"], "Flash origin")
    integer(flash["size"], "Flash size", minimum=1)
    ranges = [(flash["origin"], flash["origin"] + flash["size"])]
    ids = set()
    for domain in memory:
        identifier(domain["id"], "memory domain")
        if domain["id"] in ids:
            raise EvidenceError("duplicate RAM domain")
        ids.add(domain["id"])
        start = integer(domain["origin"], "RAM origin")
        end = start + integer(domain["size"], "RAM size", minimum=1)
        if end > 2**32 or any(start < other_end and other_start < end
                             for other_start, other_end in ranges):
            raise EvidenceError("overlapping or overflowing memory domains")
        ranges.append((start, end))
        if type(domain["linker"]) is not bool:
            raise EvidenceError("resolved linker memory support must be explicit")
    return flash, memory


def allocation(elf: Elf32, flash: dict, memory: list[dict],
               reservation_symbols: dict | None = None) -> dict:
    flash_start, flash_end = flash["origin"], flash["origin"] + flash["size"]
    flash_spans = []
    ram_spans = {domain["id"]: [] for domain in memory}
    def domain_for(start, end):
        selected = [domain for domain in memory if domain["origin"] <= start and
                    end <= domain["origin"] + domain["size"]]
        if len(selected) != 1 or not selected[0]["linker"]:
            raise EvidenceError("ELF allocation uses unsupported RAM domain")
        return selected[0]["id"]
    for load in elf.loads:
        if load["files"]:
            start = load["physical"]
            end = start + load["files"]
            if not flash_start <= start < end <= flash_end:
                raise EvidenceError("ELF file-backed LOAD bytes leave selected Flash image")
            flash_spans.append((start, end))
        if not load["memory"]:
            continue
        start = load["virtual"]
        end = start + load["memory"]
        if flash_start <= start < end <= flash_end:
            if load["flags"] & 2:
                raise EvidenceError("writable Flash LOAD")
        else:
            domain_for(start, end)
    if not flash_spans:
        raise EvidenceError("ELF has no Flash load bytes")
    reservations = {}
    symbols = reservation_symbols or {
        "msp": ["__nx_msp_start", "__nx_msp_end"],
        "heap": ["__nx_heap_start", "__nx_heap_end"]}
    for name in ("msp", "heap"):
        start, end = (elf.symbol(symbol) for symbol in symbols[name])
        if end < start:
            raise EvidenceError("reversed linker reservation")
        domain = domain_for(start, end)
        reservations[name] = {"start": start, "end": end,
                              "bytes": end - start, "domain": domain}
        ram_spans[domain].append((start, end))
    msp, heap = reservations["msp"], reservations["heap"]
    if msp["bytes"] <= 0 or (msp["start"] < heap["end"] and
                             heap["start"] < msp["end"]):
        raise EvidenceError("empty MSP or overlapping MSP/heap reservations")
    # Allocated sections must actually reside in LOAD memory, including NOBITS.
    for section in elf.sections:
        if not section["flags"] & 2 or not section["size"]:
            continue
        start, end = section["address"], section["address"] + section["size"]
        if not any(load["virtual"] <= start and end <= load["virtual"] +
                   load["memory"] for load in elf.loads):
            raise EvidenceError("allocated ELF section has no LOAD allocation")
        if not flash_start <= start < end <= flash_end:
            ram_spans[domain_for(start, end)].append((start, end))
    flash_union = intervals_union(flash_spans)
    return {"flash": {"loaded_bytes": interval_bytes(flash_spans),
                      "footprint_bytes": flash_union[-1][1] - flash_start,
                      "ranges": [list(span) for span in flash_union],
                      "capacity_bytes": flash["size"]},
            "ram": {domain["id"]: {
                "reserved_bytes": interval_bytes(ram_spans[domain["id"]]),
                "ranges": [list(span) for span in
                           intervals_union(ram_spans[domain["id"]])],
                "capacity_bytes": domain["size"],
                "linker_supported": domain["linker"]} for domain in memory},
            "reservations": reservations,
            "accounting": "LOAD physical file bytes include RAM initialization; "
                "RAM is allocated-section/reservation interval union, excluding "
                "unallocated segment address gaps and never adding MSP/heap twice",
            "dynamic_high_water": "not_measured",
            "cycles_latency": "not_measured"}


def measure_data(elf: Elf32, resolved: dict, budget: dict) -> tuple[dict, list]:
    """Recompute the same physical allocation from live or sealed inputs."""
    _, memory = memory_contract(resolved)
    image = image_window(resolved)
    validate_budget(budget, memory)
    if "memory_budgets" in resolved:
        try:
            expected = resource_budget_projection(resolved)
        except (KeyError, TypeError, StopIteration, ValueError) as error:
            raise EvidenceError("invalid resolved memory budget contract") from error
        if budget != expected:
            raise EvidenceError("resource budget differs from the resolved memory contract")
    validate_elf_configuration(elf, resolved)
    metrics = allocation(elf, image, memory)
    vectors = [section for section in elf.sections if section["name"] == ".isr_vector"]
    if len(vectors) != 1 or vectors[0]["size"] < 8 or vectors[0]["address"] != image["origin"]:
        raise EvidenceError("selected image reset vector missing or relocated")
    vector = vectors[0]
    if "vector_alignment" in image:
        if (vector["size"] > image["vector_alignment"] or
                elf.symbol(image["vector_symbol"]) != vector["address"]):
            raise EvidenceError("startup vector table differs from reviewed image")
    stack, reset = struct.unpack_from("<II", elf.data, vector["offset"])
    if stack != elf.symbol("__nx_msp_end") or reset != elf.entry or reset != elf.symbol("Reset_Handler"):
        raise EvidenceError("physical reset vector differs from linked startup/MSP")
    violations = []
    def limit(name, measured, maximum):
        if measured > maximum:
            violations.append({"metric": name, "actual": measured,
                               "maximum": maximum})
    limit("flash.loaded_bytes", metrics["flash"]["loaded_bytes"],
          budget["flash_loaded_max"])
    limit("flash.footprint_bytes", metrics["flash"]["footprint_bytes"],
          budget["flash_footprint_max"])
    for name, domain in metrics["ram"].items():
        limit("ram." + name, domain["reserved_bytes"],
              budget["ram_reserved_max"][name])
    for name in ("msp", "heap"):
        limit(name, metrics["reservations"][name]["bytes"], budget[name + "_max"])
    return metrics, violations


def measure(elf_path: Path, resolved_path: Path, budget_path: Path, *,
            float_abi: str | None = None, require_cpu: bool = False) -> dict:
    resolved, config = resolved_identity(resolved_path)
    abi = resolved_float_abi(resolved, float_abi=float_abi,
                             require_cpu=require_cpu)
    input_files = resolved_input_files(resolved_path, resolved)
    metrics, violations = measure_data(Elf32(regular_file(elf_path).read_bytes(),
                                           float_abi=abi),
                                       resolved, load_json(budget_path))
    return {"schema_version": 1, "kind": "elf_resource_budget",
            "status": "failed" if violations else "passed",
            "physical_status": "not_executed", "elf": file_identity(elf_path),
            "resolved": config, "budget": file_identity(budget_path),
            "configuration_inputs": input_files,
            "board": resolved["board"], "soc_family": resolved["soc_family"],
            "part": resolved["part"],
            "metrics": metrics, "violations": violations}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("elf", "resolved", "budget", "report"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--qualification", type=Path)
    parser.add_argument("--require-cpu", action="store_true",
                        help="require current resolved CPU metadata; no legacy hard-only scope")
    parser.add_argument("--float-abi", choices=("hard", "soft"),
                        help="assert ELF calling convention against CPU; softfp uses soft")
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    protected = {args.elf.resolve(), args.resolved.resolve(), args.budget.resolve()}
    try:
        protected.update(input_protection_paths(args.resolved))
    except (EvidenceError, OSError, TypeError) as error:
        parser.exit(1, f"input protection rejected: {error}\n")
    if args.report.resolve() in protected or (args.qualification is not None and
            args.qualification.resolve() in protected | {args.report.resolve()}):
        parser.exit(1, "report cannot overwrite a measured input\n")
    try:
        report = measure(args.elf, args.resolved, args.budget,
                         float_abi=args.float_abi, require_cpu=args.require_cpu)
    except (EvidenceError, OSError, KeyError, TypeError) as error:
        report = {"schema_version": 1, "kind": "elf_resource_budget",
                  "status": "failed", "failure": str(error)}
    atomic_json(args.report, report)
    if args.qualification:
        args.qualification.unlink(missing_ok=True)
        if report["status"] == "passed":
            from tools.evidence.identity import git_source
            from tools.evidence.qualification import create_qualification
            qualified = create_qualification("resource_budget", git_source(args.source),
                args.resolved, args.elf, [{"name": "elf_resource_budget", "exit_code": 0,
                "raw": file_identity(args.report)}], evidence=[file_identity(args.report)])
            atomic_json(args.qualification, qualified)
    print("ELF budget: " + report["status"])
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
