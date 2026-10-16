"""Execute the exact U575 software reference through the maintained ARM graph.

Vector bytes, resource/retention reports and bounded raw disassembly are software
evidence. They do not observe option bytes, execute the MCU or measure cycles.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
try:
    from . import arch_compile, provider_assemblies
except ImportError:
    import arch_compile
    import provider_assemblies
from tools.evidence.common import (EvidenceError, atomic_json, fields,
                                   file_identity, load_json,
                                   verify_file_identity)
from tools.evidence.identity import git_source
from tools.measurement.elf import Elf32
from tools.measurement.soc_costs import decode_function


def matrix_artifact(identity):
    """Accept only the actual matrix producer's byte-count identity schema."""
    fields(identity, {"path", "bytes", "sha256"})
    return verify_file_identity({"path": identity["path"],
        "size": identity["bytes"], "sha256": identity["sha256"]})


def exact_profile(actual, expected):
    """Preserve JSON types; boolean/integer/float equality is insufficient."""
    if type(actual) is not type(expected):
        return False
    if isinstance(expected, dict):
        return (actual.keys() == expected.keys() and
                all(exact_profile(actual[key], value)
                    for key, value in expected.items()))
    if isinstance(expected, list):
        return (len(actual) == len(expected) and
                all(exact_profile(left, right)
                    for left, right in zip(actual, expected)))
    return actual == expected


def validate_profile(resolved):
    """Do not infer a complete SoC from the same CPU architecture string."""
    if (resolved.get("part") != "STM32U575ZIT6Q" or
            resolved.get("soc_family") != "stm32u575" or
            resolved.get("board") != "stm32u575-nucleo-zi-q" or
            resolved.get("backend") not in {"baremetal", "freertos"}):
        raise EvidenceError("exact U575 part/Board/backend required")
    expected_cpu = {"arch": "cortex-m33", "fpu": "fpv5-sp-d16",
        "float_abi": "hard", "dsp": True, "dwt_cyccnt": False,
        "mpu_version": 8, "mpu_regions": 8, "icache_line_bytes": 0,
        "dcache_line_bytes": 0, "security": "single", "sau": False,
        "mve": "none"}
    expected_clock = {"source": "msis", "input_hz": 4000000,
        "core_hz": 4000000, "apb1_hz": 4000000, "apb2_hz": 4000000}
    if (not exact_profile(resolved.get("cpu"), expected_cpu) or
            not exact_profile(resolved.get("clock"), expected_clock)):
        raise EvidenceError("U575 single-state MSIS4MHz profile required")
    expected_memory = [{"id": "sram1", "origin": 0x20000000,
        "size": 196608, "linker": True,
        "dma_engines": {"GPDMA1": True, "LPDMA1": False}}]
    if not exact_profile(resolved.get("memory"), expected_memory):
        raise EvidenceError("U575 default physical SRAM1 profile differs")
    allowed = {("gpio", "GPIO" + bank, mode)
               for bank in "ABCDEFGH" for mode in ("input", "output")}
    allowed.update({("uart", "USART1", "irq-bytes"),
                    ("uart", "USART2", "irq-bytes"),
                    ("watchdog", "IWDG", "independent")})
    for item in resolved.get("controllers", []):
        if (item.get("kind"), item.get("controller"), item.get("mode")) not in allowed:
            raise EvidenceError("unmaintained U575 controller mode")


def validate_image(elf, resolved):
    """Bind actual vector entries and stack to the selected real ELF symbols."""
    validate_profile(resolved)
    vectors = [s for s in elf.sections if s["name"] == ".isr_vector"]
    if len(vectors) != 1:
        raise EvidenceError("one U575 vector section required")
    vector = vectors[0]
    if (vector["address"] != 0x08000000 or vector["size"] != 142 * 4 or
            vector["kind"] == 8 or vector["flags"] & 3 != 2):
        raise EvidenceError("official U575 vector range/count differs")
    raw = elf.data[vector["offset"]:vector["offset"] + vector["size"]]
    entries = struct.unpack("<142I", raw)
    if (entries[0] != elf.symbol("__nx_msp_end") or entries[0] % 8 or
            not 0x20000000 < entries[0] <= 0x20030000 or
            entries[1] != elf.symbol("Reset_Handler") or
            entries[61] != elf.symbol("TIM2_IRQHandler")):
        raise EvidenceError("U575 reset/time/stack vector differs from ELF")
    selected_uarts = sorted(item["controller"]
        for item in resolved["controllers"] if item["kind"] == "uart")
    for controller in selected_uarts:
        slot = 77 if controller == "USART1" else 78
        if entries[slot] != elf.symbol(controller + "_IRQHandler"):
            raise EvidenceError(
                "selected USART vector differs from actual handler")
    symbols = {item["name"] for item in elf.defined_symbols}
    if symbols.intersection({"malloc", "calloc", "realloc", "free", "_malloc_r",
                             "SystemCoreClock", "SystemCoreClockUpdate"}):
        raise EvidenceError("unexpected heap or stock clock global retained")
    for name in ("SystemInit", "TIM2_IRQHandler"):
        definitions = [s for s in elf.defined_symbols if s["name"] == name]
        if (len(definitions) != 1 or definitions[0]["binding"] != 1 or
                definitions[0]["type"] != 2):
            raise EvidenceError("one strong owned U575 function required")
    return {"vector_count": len(entries), "stack_end": entries[0],
            "selected_uart": bool(selected_uarts),
            "selected_uarts": selected_uarts,
            "physical_status": "not_executed"}


def tool_inputs(root):
    """Bind maintained automation inputs through the complete inspection."""
    paths = set()
    for directory in ("tools", "scripts"):
        paths.update(path for path in (root / directory).rglob("*")
            if path.is_file() and "evidence" not in path.relative_to(root).parts
            and path.suffix in {".py", ".json", ".toml", ".cmake", ".yml", ".yaml"})
    return [file_identity(path) for path in sorted(paths)]


def run(root, compiler, output, *, parallel=2):
    root, output = root.resolve(), Path(output).absolute()
    output = provider_assemblies.fresh_output(output, root)
    tool_input = file_identity(__file__)
    report = {"schema_version": 1, "kind": "stm32u575_reference_matrix",
        "status": "failed", "scope": "actual_platform_cmake_arm_elf",
        "physical_status": "not_executed", "cycles_status": "not_measured",
        "tool": tool_input, "images": []}
    try:
        source_before = git_source(root)
        report["tool_inputs"] = tool_inputs(root)
        fixture = root / "tests/contracts/u575_assembly"
        assemblies = [fixture / name for name in (
            "empty.toml", "io.toml", "empty-freertos.toml", "io-freertos.toml",
            "mainstream.toml", "mainstream-freertos.toml")]
        fixtures = provider_assemblies.validate_fixtures(
            root, assemblies, family="stm32u575")
        if provider_assemblies.run(root, assemblies, compiler, output / "matrix",
                                   family="stm32u575", parallel=parallel):
            raise EvidenceError("actual U575 assembly matrix rejected")
        matrix_input = file_identity(output / "matrix/report.json")
        matrix = load_json(verify_file_identity(matrix_input),
                           max_bytes=64*1024*1024)
        recorder = provider_assemblies.Recorder(output / "inspection", root,
                                                timeout=30)
        report["commands"] = recorder.commands
        tools = arch_compile.tools(compiler)
        for index, row in enumerate(matrix["fixtures"]):
            elf_path = matrix_artifact(row["artifacts"]["elf"])
            resolved = load_json(matrix_artifact(row["artifacts"]["resolved"]))
            elf = Elf32(elf_path.read_bytes())
            checks = validate_image(elf, resolved)
            raw = recorder.execute([tools["objdump"], "-d", "-z",
                "--disassemble=SystemInit", str(elf_path)], f"image-{index}-init")
            decoded = decode_function(elf, "SystemInit", raw.encode("ascii"))
            report["images"].append({"elf": row["artifacts"]["elf"],
                "resolved": row["artifacts"]["resolved"], "checks": checks,
                "pre_ram_static_instructions": decoded["static_instructions"],
                "pre_ram_raw": recorder.commands[-1]["stdout"],
                "pre_ram_memory_safety": "source_and_call_closure_review_required"})
        for row in matrix["fixtures"]:
            provider_assemblies.verify_inputs(row["compiler_dependencies"])
            provider_assemblies.verify_inputs(list(row["artifacts"].values()))
        provider_assemblies.verify_snapshot(root, fixtures, matrix["source_inputs"])
        provider_assemblies.verify_inputs(list(matrix["tools"].values()))
        if tool_inputs(root) != report["tool_inputs"]:
            raise EvidenceError("automation inputs changed during U575 inspection")
        if git_source(root) != source_before:
            raise EvidenceError("Git source identity changed during U575 inspection")
        verify_file_identity(matrix_input)
        verify_file_identity(tool_input)
        report["matrix"] = matrix_input
        report["source"] = source_before
        report["status"] = "passed"
    except (OSError, ValueError, KeyError, TypeError) as error:
        report["error"] = str(error)
    atomic_json(output / "report.json", report)
    return 0 if report["status"] == "passed" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--parallel", type=int, default=2)
    args = parser.parse_args()
    try:
        return run(args.root, args.compiler, args.output, parallel=args.parallel)
    except (OSError, ValueError) as error:
        print("U575 reference gate rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
