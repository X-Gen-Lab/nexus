#!/usr/bin/env python3
"""Build minimum empty/GPIO/UART/SPI images under O2/Os/O3, with/without LTO."""

from __future__ import annotations

import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, file_identity,
    load_json, regular_file)
from tools.evidence.identity import git_source
from tools.evidence.reproduce import run_logged
from tools.measurement.resources import measure
from tools.measurement.elf import Elf32

WORKLOADS = ("empty", "gpio", "uart", "spi")
OPTIMIZATIONS = ("O2", "Os", "O3", "O2-lto", "Os-lto", "O3-lto")


def tool_identity(executable: str, version_args: list[str]) -> dict:
    actual = shutil.which(executable)
    if actual is None:
        raise EvidenceError("required measurement tool unavailable: " + executable)
    result = subprocess.run([actual, *version_args], capture_output=True, check=False,
                            timeout=30)
    if result.returncode or not result.stdout.strip():
        raise EvidenceError("measurement tool version failed: " + executable)
    return {"file": file_identity(actual), "version": result.stdout.decode().splitlines()[0]}


def compare(source: Path, assembly_path: Path, build_root: Path,
            *, compiler: str = "arm-none-eabi-gcc", cmake: str = "cmake",
            ninja: str = "ninja", workloads: tuple = WORKLOADS,
            optimizations: tuple = OPTIMIZATIONS) -> dict:
    if build_root.exists():
        raise EvidenceError("measurement matrix requires new independent build roots")
    if (not workloads or not optimizations or len(set(workloads)) != len(workloads)
            or len(set(optimizations)) != len(optimizations) or
            not set(workloads).issubset(WORKLOADS) or
            not set(optimizations).issubset(OPTIMIZATIONS)):
        raise EvidenceError("unsupported or empty measurement profile selection")
    source = source.resolve()
    assembly_path = regular_file(assembly_path)
    profile_inputs = {}
    for workload in workloads:
        selected_path = assembly_path.with_name(assembly_path.stem + "-" + workload + ".json")
        assembly = load_json(selected_path)
        board = (selected_path.parent / assembly["board_package"]).resolve()
        if assembly.get("layout") is not None:
            layout = (selected_path.parent / assembly["layout"]).resolve()
            assembly["layout"] = os.path.relpath(layout, build_root.resolve())
        assembly["board_package"] = os.path.relpath(board, build_root.resolve())
        profile_inputs[workload] = assembly
    tools = {"compiler": tool_identity(compiler, ["--version"]),
             "cmake": tool_identity(cmake, ["--version"]),
             "ninja": tool_identity(ninja, ["--version"])}
    build_root.mkdir(parents=True)
    report = {"schema_version": 1, "kind": "minimal_resource_matrix",
              "status": "failed", "source": git_source(source), "tools": tools,
              "assembly_input": file_identity(assembly_path), "images": [],
              "physical_cycles_status": "not_measured",
              "instruction_counts": "not_measured",
              "hermetic_status": "not_asserted_by_host_measurement"}
    for optimization in optimizations:
        for workload in workloads:
            profile = build_root / (optimization + "-" + workload)
            selected = {**profile_inputs[workload], "optimization": optimization}
            selected_path = build_root / (optimization + "-" + workload + ".json")
            atomic_json(selected_path, selected)
            commands = [[tools["cmake"]["file"]["path"], "-S", str(source),
                "-B", str(profile), "-G", "Ninja",
                "-DCMAKE_MAKE_PROGRAM=" + tools["ninja"]["file"]["path"],
                "-DCMAKE_TOOLCHAIN_FILE=" + str(source / "cmake/toolchains/arm-gcc.cmake"),
                "-DCMAKE_C_COMPILER=" + tools["compiler"]["file"]["path"],
                "-DNEXUS_ASSEMBLY_FILE=" + str(selected_path),
                "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_WORKLOAD=" + workload,
                "-DCMAKE_C_FLAGS=-ffile-prefix-map=" + str(source) + "=. " +
                "-fdebug-prefix-map=" + str(build_root.resolve()) + "=build"],
                [tools["cmake"]["file"]["path"], "--build", str(profile), "--parallel", "2"]]
            image = {"workload": workload, "optimization": optimization,
                     "commands": [], "status": "failed"}
            report["images"].append(image)
            for index, argv in enumerate(commands):
                execution = run_logged(argv, build_root / (profile.name + f"-{index}.log"))
                image["commands"].append(execution)
                if execution["exit_code"]:
                    report["failure"] = "measurement matrix build failed: " + profile.name
                    atomic_json(build_root / "matrix.json", report)
                    return report
            try:
                resources = measure(profile / "bin/nexus_firmware.elf",
                    profile / "generated/resolved.json", profile / "generated/resource-budget.json")
            except (EvidenceError, OSError, KeyError, TypeError) as error:
                image["failure"] = str(error)
                report["failure"] = "actual linked resource measurement rejected: " + profile.name
                atomic_json(build_root / "matrix.json", report)
                return report
            atomic_json(profile / "resources.json", resources)
            image["resources"] = file_identity(profile / "resources.json")
            image["resolved"] = resources["resolved"]
            image["elf"] = resources["elf"]
            image["configuration_sha256"] = load_json(profile / "generated/resolved.json")["configuration_sha256"]
            image["metrics"] = resources["metrics"]
            image["compile_commands"] = file_identity(profile / "compile_commands.json")
            image["map"] = file_identity(profile / "bin/nexus_firmware.map")
            image["status"] = resources["status"]
            if workload == "empty":
                linked = Elf32(Path(resources["elf"]["path"]).read_bytes())
                active = sorted({item["name"] for item in linked.defined_symbols
                    if item["binding"] != 2 and re.match(
                        r"nx_(?:binding_|(?:stm32|gd32)_(?:gpio|uart|spi|i2c|flash|adc|pwm|exti|watchdog))",
                        item["name"])})
                image["unreferenced_device_gc"] = {"status": "failed" if active else "passed",
                    "active_device_provider_symbols": active,
                    "method": "actual defined ELF symbols; weak startup aliases excluded"}
                if active:
                    image["status"] = "failed"
                    report["failure"] = "empty image retains an active device provider"
                    atomic_json(build_root / "matrix.json", report)
                    return report
            if resources["status"] != "passed":
                report["failure"] = "measurement matrix resource budget failed"
                atomic_json(build_root / "matrix.json", report)
                return report
    for optimization in optimizations:
        baseline = next((item for item in report["images"] if
            item["optimization"] == optimization and item["workload"] == "empty"), None)
        if baseline:
            for item in report["images"]:
                if item["optimization"] != optimization:
                    continue
                item["delta_from_empty"] = {
                    "flash_loaded_bytes": item["metrics"]["flash"]["loaded_bytes"] -
                        baseline["metrics"]["flash"]["loaded_bytes"],
                    "ram_reserved_bytes": {name: domain["reserved_bytes"] -
                        baseline["metrics"]["ram"][name]["reserved_bytes"]
                        for name, domain in item["metrics"]["ram"].items()}}
    report["status"] = "passed"
    atomic_json(build_root / "matrix.json", report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--assembly", type=Path, required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--ninja", default="ninja")
    parser.add_argument("--workloads", nargs="+", choices=WORKLOADS, default=WORKLOADS)
    parser.add_argument("--optimizations", nargs="+", choices=OPTIMIZATIONS, default=OPTIMIZATIONS)
    args = parser.parse_args()
    try:
        report = compare(args.source, args.assembly, args.build_root,
            compiler=args.compiler, cmake=args.cmake, ninja=args.ninja,
            workloads=tuple(args.workloads), optimizations=tuple(args.optimizations))
        print("Minimal resource comparison: " + report["status"])
        return 0 if report["status"] == "passed" else 1
    except (EvidenceError, OSError, ValueError, KeyError, TypeError,
            subprocess.SubprocessError) as error:
        print("Minimal measurement blocked: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
