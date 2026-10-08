#!/usr/bin/env python3
"""Measure linked ARM sections or execute native contracts under Valgrind."""

from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import re
import shutil
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.evidence.common import (EvidenceError, atomic_json, file_identity, fields,
                                     load_json, regular_file, run_adapter)


def parse_size(output: bytes) -> list[dict]:
    sections = []
    for line in output.decode("utf-8", errors="strict").splitlines():
        match = re.fullmatch(r"\s*(\S+)\s+(\d+)\s+(0x[0-9a-fA-F]+|\d+)\s*", line)
        if match:
            name, size, address = match.groups()
            if name == "Total":
                continue
            sections.append({"name": name, "bytes": int(size), "address": int(address, 16 if address.startswith("0x") else 10)})
    if not sections or not any(section["bytes"] > 0 for section in sections):
        raise EvidenceError("size tool returned no nonempty sections")
    if len({section["name"] for section in sections}) != len(sections):
        raise EvidenceError("duplicate section records")
    return sections


def sizes(build: Path, size_tool: str = "arm-none-eabi-size") -> dict:
    executable = shutil.which(size_tool)
    if not executable:
        raise EvidenceError("ARM size tool unavailable")
    images = sorted((build / "bin").glob("*.elf"))
    if not images:
        raise EvidenceError("no linked ARM ELF images")
    result = {"schema_version": 1, "kind": "linked_section_measurements", "status": "measured",
              "product_budget_status": "pending_reviewed_product_limits",
              "hardware_timing_status": "not_measured", "images": []}
    for image in images:
        regular_file(image)
        with image.open("rb") as stream:
            if stream.read(4) != b"\x7fELF":
                raise EvidenceError("size input is not ELF")
        link_map = image.with_suffix(".map")
        result["images"].append({"elf": file_identity(image), "map": file_identity(link_map),
                                 "sections": parse_size(run_adapter([executable, "-A", str(image)], 10))})
    result["configuration"] = {name: file_identity(build / "generated" / name)
                               for name in ("effective.config", "nexus_config.h", "config.cmake")}
    result["size_tool_version"] = run_adapter([executable, "--version"], 10).decode("utf-8").splitlines()[0]
    return result


def valgrind_errors(path: Path) -> dict:
    try:
        root = ET.parse(regular_file(path)).getroot()
    except ET.ParseError as error:
        raise EvidenceError("Valgrind report malformed") from error
    if root.tag != "valgrindoutput" or root.findtext("protocoltool") != "memcheck":
        raise EvidenceError("Valgrind memcheck XML required")
    states = root.findall("status/state")
    if not states or states[-1].text != "FINISHED":
        raise EvidenceError("Valgrind did not finish")
    kinds = [entry.findtext("kind", "unknown") for entry in root.findall("error")]
    failures = [kind for kind in kinds if kind != "Leak_StillReachable"]
    if failures:
        raise EvidenceError("memory checker reported invalid accesses or lost allocations")
    return {"lost_or_invalid_errors": 0, "reachable_retained_records": kinds.count("Leak_StillReachable")}


def memory(build: Path, valgrind: str = "valgrind", ctest: str = "ctest") -> dict:
    checker = shutil.which(valgrind)
    runner = shutil.which(ctest)
    if not checker or not runner:
        raise EvidenceError("CTest or Valgrind unavailable")
    build = build.absolute()
    try:
        listing = json.loads(run_adapter([runner, "--test-dir", str(build), "--show-only=json-v1"], 60, output_limit=8 * 1024 * 1024))
    except (ValueError, UnicodeError) as error:
        raise EvidenceError("CTest enumeration is malformed") from error
    if not isinstance(listing, dict) or not isinstance(listing.get("tests"), list) or not listing["tests"]:
        raise EvidenceError("zero CTest contracts rejected")
    report = {"schema_version": 1, "kind": "native_memory_contracts", "status": "pass",
              "tests": [], "excluded_tool_tests": [],
              "leak_policy": "lost allocations and invalid accesses fail; reachable retained objects are counted",
              "valgrind_version": run_adapter([checker, "--version"], 10).decode("utf-8").strip()}
    logs = build / "memory-logs"
    logs.mkdir(parents=True, exist_ok=True)
    ids = set()
    for index, test in enumerate(listing["tests"]):
        name = test.get("name")
        argv = test.get("command")
        if not isinstance(name, str) or not name or name in ids or not isinstance(argv, list) or not argv:
            raise EvidenceError("CTest test identity/command missing or duplicated")
        ids.add(name)
        executable = Path(argv[0]).absolute()
        if executable.parent != (build / "bin"):
            report["excluded_tool_tests"].append({"name": name, "reason": "not a linked native build/bin executable"})
            continue
        regular_file(executable)
        with executable.open("rb") as stream:
            if stream.read(4) != b"\x7fELF":
                raise EvidenceError("native memory contract executable is not ELF")
        properties = {item["name"]: item["value"] for item in test.get("properties", [])}
        if properties.get("DISABLED") is True:
            raise EvidenceError("disabled memory contract rejected")
        timeout = properties.get("TIMEOUT", 120)
        if type(timeout) not in (int, float) or not math.isfinite(timeout) or timeout <= 0:
            raise EvidenceError("invalid memory test deadline")
        environment = dict(os.environ)
        for assignment in properties.get("ENVIRONMENT", []):
            if not isinstance(assignment, str) or "=" not in assignment:
                raise EvidenceError("invalid CTest environment assignment")
            key, value = assignment.split("=", 1)
            if not key or "\0" in assignment:
                raise EvidenceError("invalid CTest environment assignment")
            environment[key] = value
        xml = logs / f"{index:05d}.xml"
        output = run_adapter([checker, "--tool=memcheck", "--xml=yes", f"--xml-file={xml}",
                              "--error-exitcode=97", "--leak-check=full", "--show-leak-kinds=all",
                              "--errors-for-leak-kinds=definite,possible", "--track-origins=yes", *argv],
                             min(timeout * 5, 300), cwd=properties.get("WORKING_DIRECTORY", str(build)),
                             env=environment)
        # Console output is not copied to evidence: diagnostics may contain
        # application values. The checker XML is the authoritative error record.
        report["tests"].append({"name": name, "status": "pass", "executable": file_identity(executable),
                                "memcheck": file_identity(xml), **valgrind_errors(xml)})
    if not report["tests"]:
        raise EvidenceError("zero linked native memory contracts rejected")
    report["executed_tests"] = len(report["tests"])
    report["configuration"] = {name: file_identity(build / "generated" / name)
                               for name in ("effective.config", "nexus_config.h", "config.cmake")}
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("memory", "size"))
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--size-tool", default="arm-none-eabi-size")
    parser.add_argument("--valgrind", default="valgrind")
    parser.add_argument("--ctest", default="ctest")
    args = parser.parse_args()
    try:
        report = (sizes(args.build, args.size_tool) if args.operation == "size"
                  else memory(args.build, args.valgrind, args.ctest))
    except (EvidenceError, OSError, KeyError, TypeError, UnicodeError) as error:
        report = {"schema_version": 1, "kind": args.operation + "_execution", "status": "fail", "failure": str(error)}
    atomic_json(args.report, report)
    print(f"{args.operation}: {report['status']}; report={args.report}")
    return 1 if report["status"] == "fail" else 0


if __name__ == "__main__":
    raise SystemExit(main())
