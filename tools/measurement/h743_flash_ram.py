#!/usr/bin/env python3
"""Audit selected H743 RAM residency/frames in the normal firmware link.

This binds actual inputs and tool execution. C data ownership, MPU/device
protocol, physical behavior and software-candidate qualification remain separate.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shlex
import shutil
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.evidence.common import (EvidenceError, atomic_json, file_identity,
    load_json, parse_json, regular_file, run_adapter, separate_outputs)
from tools.evidence.identity import resolved_identity, resolved_input_files
from tools.measurement.elf import Elf32
from tools.measurement.flash_execution import (verify_h743_cpu_bridge,
    verify_h743_ram_layout)
from tools.measurement.flash_ram_static import verify_h743_ram_static
from tools.measurement.resources import (input_protection_paths,
    resolved_float_abi, validate_elf_configuration)
from tools.measurement.soc_costs import compiler_commands

RAM_SOURCES = ("flash_ram.c", "flash_ram_owner.c", "flash_ram_transaction.c",
               "flash_mpu.c")
FLAGS = ("-mgeneral-regs-only", "-fno-lto", "-fno-builtin",
         "-fno-stack-protector", "-fno-jump-tables",
         "-fno-tree-loop-distribute-patterns", "-fno-unwind-tables",
         "-fno-asynchronous-unwind-tables", "-fstack-usage")
DEPENDENCIES = (
    "tools/measurement/h743_flash_ram.py", "tools/measurement/flash_execution.py",
    "tools/measurement/flash_ram_static.py", "tools/measurement/soc_costs.py",
    "tools/measurement/static_storage.py", "tools/measurement/resources.py",
    "tools/measurement/elf.py", "tools/evidence/common.py",
    "tools/evidence/identity.py", "tools/configure/memory.py")


class _OutputError(EvidenceError):
    pass


def _require(ok, message):
    if not ok:
        raise EvidenceError("H743 link audit: " + message)


def _argv(row):
    _require(isinstance(row, dict) and ("command" in row) != ("arguments" in row),
             "compile entry needs exactly one argument representation")
    argv = row.get("arguments")
    if argv is None:
        _require(isinstance(row["command"], str), "compile command is not text")
        argv = shlex.split(row["command"])
    _require(isinstance(argv, list) and argv and
             all(isinstance(arg, str) and arg for arg in argv),
             "invalid compile argv")
    _require(not any(arg.startswith("@") or arg in
                     {";", "&&", "||", "|", ">", "<"} for arg in argv),
             "opaque compile response/shell argument")
    return argv


def _located(value, directory):
    _require(isinstance(value, str) and value, "missing compile path")
    path = Path(value)
    return path if path.is_absolute() else directory / path


def _stack_inputs(path):
    _require(path.stat().st_size <= 4 * 1024 * 1024,
             "compile database exceeds its bounded input size")
    data = parse_json(b'{"commands":' + path.read_bytes() + b'}')["commands"]
    _require(isinstance(data, list) and 0 < len(data) <= 4096,
             "nonempty bounded compile database required")
    selected = []
    for name in RAM_SOURCES:
        source = regular_file(ROOT / "soc/stm32h743/drivers" / name)
        matches = []
        for row in data:
            _require(isinstance(row, dict), "compile entry is not an object")
            directory = Path(row.get("directory", ""))
            _require(directory.is_absolute() and directory.is_dir(),
                     "compile directory missing or relative")
            file = _located(row.get("file"), directory)
            if file.resolve() == source.resolve():
                matches.append((row, directory))
        _require(len(matches) == 1, "missing/duplicate actual RAM source: " + name)
        row, directory = matches[0]
        argv = _argv(row)
        _require(argv.count("-c") == 1 and argv.index("-c") + 1 < len(argv),
                 "one explicit CMake compile input is required")
        _require(_located(argv[argv.index("-c") + 1], directory).resolve() ==
                 source.resolve(), "compile argv source differs from row.file")
        _require(argv.count("-o") == 1 and argv.index("-o") + 1 < len(argv),
                 "one explicit compiler output is required")
        obj = _located(argv[argv.index("-o") + 1], directory)
        _require(obj.resolve().is_relative_to(directory.resolve()),
                 "object leaves its declared build directory")
        if "output" in row:
            _require(_located(row["output"], directory).resolve() == obj.resolve(),
                     "compile output field differs from -o")
        compiler = shutil.which(argv[0])
        _require(compiler is not None, "compile tool is unavailable")
        selected.append({"source": source, "object": regular_file(obj),
                         "su": regular_file(obj.with_suffix(".su")),
                         "directory": directory, "arguments": argv,
                         "compiler": regular_file(Path(compiler).resolve())})
    _require(len({row["compiler"] for row in selected}) == 1,
             "RAM translation units use different compilers")
    return selected


def _flags(argv):
    for flag in FLAGS:
        if flag.startswith("-fno-"):
            name = flag[5:]
            choices = [arg for arg in argv if arg in
                       {"-f" + name, "-fno-" + name} or
                       arg.startswith("-f" + name + "=") or
                       (name == "stack-protector" and
                        arg.startswith("-fstack-protector"))]
            _require(choices and choices[-1] == flag,
                     "private code-generation flag missing/overridden: " + flag)
        else:
            _require(flag in argv, "private code-generation flag missing: " + flag)
    _require("-fno-stack-usage" not in argv and
             "-mno-general-regs-only" not in argv,
             "private code-generation flag disabled")


def _outputs(report):
    base = str(report.with_suffix(""))
    return {"report": report, "objdump": Path(base + ".objdump.raw"),
            "objdump_receipt": Path(base + ".objdump.command.json"),
            "compiler": Path(base + ".compiler.raw"),
            "compiler_receipt": Path(base + ".compiler.command.json")}


def _protect(outputs, inputs):
    paths = list(outputs.values())
    try:
        separate_outputs(paths, inputs)
    except EvidenceError as error:
        raise _OutputError(str(error)) from error
    for index, output in enumerate(paths):
        if output.exists() and output.is_file():
            if any(output.samefile(path) for path in [*inputs, *paths[:index]]
                   if path.exists() and path.is_file()):
                raise _OutputError("output is a hardlink alias of an input/output")


def _execute(argv, raw_path, receipt_path, directory, limit):
    started = time.time_ns()
    try:
        raw = run_adapter(argv, 30, cwd=directory, output_limit=limit)
    except EvidenceError as error:
        atomic_json(receipt_path, {"argv": argv, "started_ns": started,
                    "finished_ns": time.time_ns(), "exit_code": None,
                    "outcome": "failed", "failure": str(error)})
        raise
    raw_path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(dir=raw_path.parent,
                                            prefix=".nexus-raw-")
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(raw)
        os.replace(temporary, raw_path)
    finally:
        Path(temporary).unlink(missing_ok=True)
    receipt = {"argv": argv, "cwd": str(directory), "started_ns": started,
               "finished_ns": time.time_ns(), "exit_code": 0,
               "outcome": "completed", "stdout": file_identity(raw_path)}
    atomic_json(receipt_path, receipt)
    return raw


def audit(elf_path, resolved_path, commands_path, objdump, report_path):
    elf_path, resolved_path, commands_path = map(
        regular_file, (elf_path, resolved_path, commands_path))
    report_path = Path(report_path).absolute()
    outputs = _outputs(report_path)
    preliminary = [elf_path, resolved_path, commands_path,
                   *input_protection_paths(resolved_path),
                   *(ROOT / "soc/stm32h743/drivers" / name for name in RAM_SOURCES),
                   *(ROOT / name for name in DEPENDENCIES)]
    tool = shutil.which(str(objdump))
    _require(tool is not None, "actual CMake objdump is unavailable")
    tool = regular_file(Path(tool).resolve())
    preliminary.append(tool)
    _protect(outputs, preliminary)
    # Dynamic object/SU/compiler discovery must finish before even a failed
    # report is written. An incomplete database cannot authorize any output.
    destinations_ready = False
    try:
        resolved, resolved_file = resolved_identity(resolved_path)
        declared = resolved_input_files(resolved_path, resolved)
        _require(resolved.get("soc_family") == "stm32h743", "exact H743 required")
        flash = [row for row in resolved.get("controllers", [])
                 if row.get("kind") == "flash"]
        _require(len(flash) == 1 and flash[0].get("mode") == "poll-word",
                 "one selected physical Flash controller is required")
        _require(resolved.get("clock_profile") in {
            "hsi32-fixed-cache-off", "hsi64-fixed-cache-off",
            "hsi64-pll384-cache-off"}, "unsupported Flash/cache profile")
        selected = _stack_inputs(commands_path)
        protected = preliminary[:]
        for row in selected:
            protected += [row[key] for key in ("source", "object", "su", "compiler")]
        _protect(outputs, protected)
        destinations_ready = True
        before = [file_identity(path) for path in protected]
        for row in selected:
            _flags(row["arguments"])
        compiler_commands(commands_path, resolved["cpu"])
        elf = Elf32(elf_path.read_bytes(),
                    float_abi=resolved_float_abi(resolved, require_cpu=True))
        validate_elf_configuration(elf, resolved)
        report_path.unlink(missing_ok=True)
        compiler = selected[0]["compiler"]
        probe = _execute([str(compiler), "-dumpmachine"], outputs["compiler"],
                         outputs["compiler_receipt"], commands_path.parent, 4096)
        _require(probe.strip() == b"arm-none-eabi",
                 "actual RAM compiler is not arm-none-eabi GCC")
        raw = _execute([str(tool), "-drwC", str(elf_path)], outputs["objdump"],
                       outputs["objdump_receipt"], commands_path.parent,
                       16 * 1024 * 1024)
        _require(0 < len(raw) <= 16 * 1024 * 1024, "objdump size is invalid")
        observations = {"elf_sha256": file_identity(elf_path)["sha256"],
                        "objdump": raw}
        layout = verify_h743_ram_layout(elf, observations)
        cpu = verify_h743_cpu_bridge(elf, observations)
        static = verify_h743_ram_static(elf, observations, [
            {"source": str(row["source"]), "raw": row["su"].read_bytes(),
             "compile": {"directory": str(row["directory"]),
                         "arguments": row["arguments"]}}
            for row in selected],
            return_gate="nx_stm32h743_flash_ram_return")
        _require(before == [file_identity(path) for path in protected],
                 "an audit input changed during execution")
        report = {
            "schema_version": 1, "kind": "h743_selected_RAM_link_audit",
            "status": "passed", "qualification": False,
            "clock_profile": resolved["clock_profile"],
            "resolved": resolved_file, "declared_inputs": declared,
            "elf": file_identity(elf_path),
            "compile_commands": file_identity(commands_path),
            "tools": [file_identity(tool), file_identity(compiler)],
            "checker_sources": [file_identity(ROOT / name) for name in DEPENDENCIES],
            "stack_usage": [{**{key: file_identity(row[key])
                                 for key in ("source", "object", "su")},
                             "directory": str(row["directory"]),
                             "arguments": row["arguments"]} for row in selected],
            "objdump_execution": load_json(outputs["objdump_receipt"]),
            "compiler_execution": load_json(outputs["compiler_receipt"]),
            "ram_layout": layout, "cpu_bridge": cpu, "static_closure": static,
            "physical_execution": "not_executed",
            "limits": ["conditional CPU/AAPCS and static RAM residency/frame checks",
                       "C data ownership, Flash-read and MPU/device protocol not proved",
                       "identified compile metadata is not signed build history",
                       "no hardware or software-candidate qualification"],
        }
    except _OutputError:
        raise
    except (EvidenceError, OSError, ValueError, KeyError, TypeError) as error:
        if not destinations_ready:
            raise
        report = {"schema_version": 1, "kind": "h743_selected_RAM_link_audit",
                  "status": "failed", "failure": str(error),
                  "qualification": False, "physical_execution": "not_executed"}
    atomic_json(report_path, report)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("elf", "resolved", "compile-commands", "objdump", "report"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        report = audit(args.elf, args.resolved, args.compile_commands,
                       args.objdump, args.report)
    except (EvidenceError, OSError, ValueError, TypeError) as error:
        print("H743 RAM audit rejected: " + str(error), file=sys.stderr)
        return 1
    print("H743 RAM link audit: " + report["status"])
    if report["status"] != "passed":
        print(report["failure"], file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
