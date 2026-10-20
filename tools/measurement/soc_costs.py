#!/usr/bin/env python3
"""Observe actual ARM costs and compare equivalent recorded build inputs.

This observer never qualifies a release or authenticates historical build
commands. Compiler metadata is an identified input, not signed provenance.
Static decoded instructions are neither measured cycles nor a WCET bound.
"""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import re
import shlex
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.evidence.common import (EvidenceError, atomic_json, command, fields,
    file_identity, parse_json, regular_file, run_adapter, verify_file_identity)
from tools.evidence.identity import resolved_identity, resolved_input_files
from tools.measurement.elf import Elf32
from tools.measurement.resources import measure, resolved_float_abi
from tools.measurement.static_storage import inspect_storage

MAX_INPUT = 16 * 1024 * 1024
PROVENANCE = {"configuration_sha256", "inputs", "source_dependencies",
              "platform_metadata",
              "limitations", "physical_qualified", "physical_pcb_revision"}


def cost_values(metrics):
    """Project the maintained resource observer's actual metrics only."""
    return {"flash.loaded_bytes": metrics["flash"]["loaded_bytes"],
            "flash.footprint_bytes": metrics["flash"]["footprint_bytes"],
            **{"ram." + key: value["reserved_bytes"]
               for key, value in metrics["ram"].items()},
            **{key: metrics["reservations"][key]["bytes"]
               for key in ("msp", "heap")},
            **{"static." + key + "_bytes": value for key, value in
               metrics["static_storage"]["bytes"].items()}}


def compiler_commands(path, cpu):
    """Read actual compile metadata without executing command strings."""
    path = regular_file(path)
    if path.stat().st_size > MAX_INPUT:
        raise EvidenceError("compile database exceeds bounded input size")
    rows = parse_json(b'{"commands":' + path.read_bytes() + b'}')["commands"]
    if not isinstance(rows, list) or not rows or len(rows) > 4096:
        raise EvidenceError("nonempty bounded compile database required")
    profiles, tools, sources = [], {}, {}
    for row in rows:
        if (not isinstance(row, dict) or
                set(row) - {"directory", "file", "arguments", "command",
                            "output"}
                or not {"directory", "file"}.issubset(row) or
                ("arguments" in row) == ("command" in row)):
            raise EvidenceError("invalid compile database fields")
        directory = Path(row["directory"])
        if not directory.is_absolute() or not directory.is_dir():
            raise EvidenceError("compile directory missing or relative")
        source = Path(row["file"])
        source = source if source.is_absolute() else directory / source
        sources[str(source)] = file_identity(source)
        argv = row.get("arguments")
        if argv is None:
            if not isinstance(row["command"], str):
                raise EvidenceError("compile command must be text")
            argv = shlex.split(row["command"])
        command(argv)
        if any(token.startswith("@") or
               token in {";", "&&", "||", "|", ">", "<"}
               for token in argv):
            raise EvidenceError("opaque response file or shell command refused")
        tool = shutil.which(argv[0])
        if tool is None:
            raise EvidenceError("actual compile tool unavailable")
        tool = str(Path(tool).resolve())
        tools[tool] = file_identity(tool)
        values = {}
        for prefix in ("-mcpu=", "-march=", "-mfpu=", "-mfloat-abi="):
            selected = [arg[len(prefix):] for arg in argv
                        if arg.startswith(prefix)]
            if selected:
                values[prefix] = selected[-1]
        fpu = cpu.get("fpu")
        expected_fpu = None if fpu == "none" else fpu
        if (not isinstance(fpu, str) or
                values.get("-mcpu=") != cpu.get("arch") or
                "-mthumb" not in argv or
                values.get("-mfpu=") != expected_fpu or
                values.get("-mfloat-abi=") != cpu.get("float_abi")):
            raise EvidenceError("compile CPU/Thumb/FPU ABI differs from IR")
        optimization = [arg for arg in argv
                        if re.fullmatch(r"-O(?:0|1|2|3|s|g|fast)", arg)]
        if not optimization:
            raise EvidenceError("actual optimization flag missing")
        # Retain order-sensitive effective optimization plus all codegen flags.
        # Source paths, warnings, preprocessing and output/dependency names
        # are separate identities; they are not a second hardware configuration.
        codegen = []
        for arg in argv:
            if not arg.startswith(("-f", "-m", "-std=")):
                continue
            if arg.startswith(("-ffile-prefix-map=", "-fdebug-prefix-map=",
                               "-fmacro-prefix-map=")):
                parts = arg.split("=", 2)
                if len(parts) != 3:
                    raise EvidenceError("malformed compiler prefix map")
                arg = parts[0] + "=<path>=" + parts[2]
            codegen.append(arg)
        profiles.append({"compiler_sha256": tools[tool]["sha256"],
                         "optimization": optimization[-1], "codegen": codegen})
    unique = {json.dumps(row, sort_keys=True): row for row in profiles}
    ordered = sorted(unique.values(),
                     key=lambda row: json.dumps(row, sort_keys=True))
    return ordered, list(tools.values()), list(sources.values())


def comparison_profile(resolved, profiles):
    """Compare the sole IR, retaining all routes, bindings and resource claims.

    Unused route changes are conservatively incomparable, rather than guessing
    which Board bindings a particular provider may consume through endpoints.
    """
    required = {"part", "soc_family", "board", "backend", "cpu", "clock",
                "controllers", "flash", "memory"}
    if not required.issubset(resolved):
        raise EvidenceError("hardware/backend/clock comparison facts missing")
    result = {key: copy.deepcopy(value) for key, value in resolved.items()
              if key not in PROVENANCE}
    if "memory_budgets" in result:
        budgets = result["memory_budgets"]
        result["memory_budgets"] = {key: value for key, value in budgets.items()
            if key in {"main_stack_bytes", "libc_heap_bytes"}}
    result["actual_compile_profiles"] = profiles
    return result


def decode_function(elf, name, raw):
    """Check objdump raw Thumb encodings against the selected ELF bytes."""
    symbols = [(index, item) for index, item in enumerate(elf.defined_symbols)
               if item["name"] == name and item["type"] == 2]
    if (len(symbols) != 1 or not symbols[0][1]["size"] or
            not symbols[0][1]["value"] & 1):
        raise EvidenceError("unique nonempty Thumb function required: " + name)
    return decode_symbol(elf, symbols[0][0], raw)


def decode_symbol(elf, index, raw):
    """Validate one actual defined-symbol index, retaining duplicate locals.

    A local name is scoped by its ELF symbol identity, not a process-wide
    registry. The index selects the existing parsed entry; callers cannot
    provide replacement address, size, section or source-file claims.
    """
    if (type(index) is not int or not 0 <= index < len(elf.defined_symbols)):
        raise EvidenceError("actual ELF defined-symbol index required")
    symbol = elf.defined_symbols[index]
    name = symbol["name"]
    if symbol["type"] != 2 or not symbol["size"] or not symbol["value"] & 1:
        raise EvidenceError("nonempty Thumb function symbol required: " + name)
    if symbol["section"] >= len(elf.sections):
        raise EvidenceError("function section is not file-backed")
    section = elf.sections[symbol["section"]]
    start, end = symbol["value"] & ~1, (symbol["value"] & ~1) + symbol["size"]
    if (section["kind"] == 8 or section["flags"] & 6 != 6 or
            not section["address"] <= start < end <=
            section["address"] + section["size"]):
        raise EvidenceError("function leaves executable section")
    instructions, addresses, spans = [], set(), []
    mappings = sorted((item["value"], item["name"].startswith("$d"))
        for item in elf.defined_symbols
        if item["section"] == symbol["section"] and
        re.fullmatch(r"\$(?:t|d)(?:\.\d+)?", item["name"]))
    if not mappings:
        raise EvidenceError("Thumb/data ELF mapping symbols missing")
    pattern = re.compile(r"^\s*([0-9a-f]+):\s+"
        r"([0-9a-f]{4}(?:\s+[0-9a-f]{4})?|[0-9a-f]{8})\s+([.\w]+)",
        re.MULTILINE)
    for match in pattern.finditer(raw.decode("ascii")):
        address = int(match[1], 16)
        words = match[2].split()
        encoded = b"".join(int(word, 16).to_bytes(len(word) // 2, "little")
                           for word in words)
        if (address in addresses or
                not start <= address < address + len(encoded) <= end):
            raise EvidenceError("disassembly leaves/repeats selected function")
        addresses.add(address)
        spans.append((address, address + len(encoded)))
        offset = section["offset"] + address - section["address"]
        if encoded != elf.data[offset:offset + len(encoded)]:
            raise EvidenceError("disassembly bytes differ from actual ELF")
        classification = [is_data for value, is_data in mappings
                          if value <= address]
        if (not classification or
                classification[-1] != match[3].startswith(".")):
            raise EvidenceError("disassembly disagrees with Thumb/data map")
        if not classification[-1]:
            first = int.from_bytes(encoded[:2], "little")
            width = 4 if first >> 11 in {29, 30, 31} else 2
            if address & 1 or len(encoded) != width:
                raise EvidenceError("encoding width differs from Thumb prefix")
            instructions.append({"address": address, "encoding": encoded.hex(),
                                 "mnemonic": match[3]})
    if not instructions:
        raise EvidenceError("function disassembly contains zero instructions")
    cursor = start
    for first, last in sorted(spans):
        if first != cursor:
            raise EvidenceError("function disassembly has a gap or overlap")
        cursor = last
    if cursor != end:
        raise EvidenceError("function disassembly is incomplete")
    return {"symbol": name, "address": start, "size": symbol["size"],
            "static_instructions": len(instructions),
            "instructions": instructions}


def observe(elf_path, resolved_path, budget_path, compile_commands_path, *,
            output, objdump="arm-none-eabi-objdump", symbols=(), timeout_s=10):
    """Read actual inputs using bounded subprocesses without a shell."""
    if (isinstance(timeout_s, bool) or
            not isinstance(timeout_s, (int, float)) or
            not 0 < timeout_s <= 30):
        raise EvidenceError("measurement timeout must be in (0, 30] seconds")
    if (not isinstance(symbols, (list, tuple)) or len(symbols) > 32 or
            any(not isinstance(name, str) or
                not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{0,127}", name)
                for name in symbols) or len(set(symbols)) != len(symbols)):
        raise EvidenceError("invalid or duplicate function selection")
    inputs = {name: file_identity(path) for name, path in {
        "elf": elf_path, "resolved": resolved_path, "budget": budget_path,
        "compile_commands": compile_commands_path,
        "input_paths": Path(resolved_path).parent / "input_paths.json"}.items()}
    if inputs["elf"]["size"] > MAX_INPUT:
        raise EvidenceError("ELF exceeds bounded observer size")
    output = Path(output).absolute()
    if output.exists():
        raise EvidenceError("observation output requires a fresh directory")
    if any(output == Path(item["path"]) or output in Path(item["path"]).parents
           for item in inputs.values()):
        raise EvidenceError("output cannot overwrite an observed input")
    resolved, _ = resolved_identity(Path(resolved_path))
    configured = resolved_input_files(Path(resolved_path), resolved)
    profiles, compilers, sources = compiler_commands(
        Path(compile_commands_path), resolved.get("cpu", {}))
    profile = comparison_profile(resolved, profiles)
    objdump = shutil.which(str(objdump))
    if objdump is None:
        raise EvidenceError("actual objdump unavailable")
    objdump = str(Path(objdump).resolve())
    tools = {"observer": file_identity(__file__),
             "objdump": file_identity(objdump),
             "compilers": compilers}
    abi = resolved_float_abi(resolved, require_cpu=True)
    resources = measure(Path(elf_path), Path(resolved_path), Path(budget_path),
                        float_abi=abi)
    if resources["status"] != "passed":
        raise EvidenceError("actual resource budget exceeded")
    elf = Elf32(Path(elf_path).read_bytes(), float_abi=abi)
    resources["metrics"]["static_storage"] = inspect_storage(
        elf, resources["metrics"])
    output.mkdir(parents=True)
    executions = []
    def execute(argv, label):
        raw = run_adapter(argv, timeout_s, output_limit=1024 * 1024)
        if not raw.strip():
            raise EvidenceError("measurement tool returned empty output")
        path = output / (label + ".stdout")
        path.write_bytes(raw)
        executions.append({"argv": argv, "exit_code": 0, "timeout_s": timeout_s,
                           "stdout": file_identity(path)})
        return raw
    execute([objdump, "--version"], "objdump-version")
    for index, compiler in enumerate(compilers):
        execute([compiler["path"], "--version"], f"compiler-{index}-version")
        machine = execute([compiler["path"], "-dumpmachine"],
                          f"compiler-{index}-target")
        if machine.strip() != b"arm-none-eabi":
            raise EvidenceError("compile metadata names a non-ARM toolchain")
    functions = []
    for index, name in enumerate(symbols):
        raw = execute([objdump, "-d", "-z", "--disassemble=" + name,
                       inputs["elf"]["path"]],
                      f"function-{index}")
        row = decode_function(elf, name, raw)
        row["raw"] = executions[-1]["stdout"]
        functions.append(row)
    for identity in [*inputs.values(), *sources,
                     *(item["file"] for item in configured),
                     tools["observer"], tools["objdump"], *compilers]:
        verify_file_identity(identity)
    report = {"schema_version": 1, "kind": "soc_cost_observation",
              "status": "observed", "inputs": inputs,
              "configured_inputs": configured, "compiled_sources": sources,
              "comparison_profile": profile, "tools": tools,
              "executions": executions,
              "metrics": resources["metrics"], "functions": functions,
              "input_stability": "verified", "physical_status": "not_executed",
              "cycles_status": "not_measured", "wcet_status": "not_measured",
              "historical_build_provenance": "not_attested"}
    atomic_json(output / "report.json", report)
    return report


def validate_executions(report):
    """Bind the complete adapter receipts to tools and selected functions.

    These identified receipts establish consistency, not signed execution
    history. Replaying or editing every unsigned input cannot be attested here.
    """
    tools, functions = report["tools"], report["functions"]
    if (not isinstance(functions, list) or len(functions) > 32 or
            len({row["symbol"] for row in functions}) != len(functions)):
        raise EvidenceError("invalid selected function receipt set")
    objdump = tools["objdump"]["path"]
    expected = [([objdump, "--version"], None)]
    for compiler in tools["compilers"]:
        expected.extend([([compiler["path"], "--version"], None),
                         ([compiler["path"], "-dumpmachine"], "target")])
    expected.extend(([objdump, "-d", "-z", "--disassemble=" + row["symbol"],
                      report["inputs"]["elf"]["path"]], row["raw"])
                    for row in functions)
    executions = report["executions"]
    if not isinstance(executions, list) or len(executions) != len(expected):
        raise EvidenceError("complete tool/function execution receipts required")
    outputs = set()
    for execution, (argv, output) in zip(executions, expected):
        fields(execution, {"argv", "exit_code", "timeout_s", "stdout"})
        timeout = execution["timeout_s"]
        if (execution["argv"] != argv or
                type(execution["exit_code"]) is not int or
                execution["exit_code"] != 0 or isinstance(timeout, bool) or
                not isinstance(timeout, (int, float)) or not 0 < timeout <= 30):
            raise EvidenceError("invalid observation execution receipt")
        path = verify_file_identity(execution["stdout"])
        if path in outputs or path.stat().st_size > 1024 * 1024:
            raise EvidenceError("duplicate or unbounded adapter output")
        outputs.add(path)
        raw = path.read_bytes()
        if not raw.strip() or (output == "target" and
                               raw.strip() != b"arm-none-eabi"):
            raise EvidenceError("missing version/output or non-ARM target")
        if isinstance(output, dict) and execution["stdout"] != output:
            raise EvidenceError("function receipt differs from decoded output")


def validate_observation(report):
    """Recompute resource/profile/raw-encoding facts; do not trust summaries."""
    fields(report, {"schema_version", "kind", "status", "inputs",
        "configured_inputs", "compiled_sources", "comparison_profile", "tools",
        "executions", "metrics", "functions", "input_stability",
        "physical_status", "cycles_status", "wcet_status",
        "historical_build_provenance"})
    if (type(report["schema_version"]) is not int or
            report["schema_version"] != 1 or
            report["kind"] != "soc_cost_observation" or
            report["status"] != "observed" or
            report["input_stability"] != "verified" or
            report["cycles_status"] != "not_measured" or
            report["wcet_status"] != "not_measured" or
            report["physical_status"] != "not_executed" or
            report["historical_build_provenance"] != "not_attested"):
        raise EvidenceError("invalid cost observation")
    if (file_identity(__file__)["sha256"] !=
            report["tools"]["observer"]["sha256"]):
        raise EvidenceError("cost observer implementation changed")
    validate_executions(report)
    for identity in [*report["inputs"].values(), *report["compiled_sources"],
            *(item["file"] for item in report["configured_inputs"]),
            report["tools"]["observer"], report["tools"]["objdump"],
            *report["tools"]["compilers"]]:
        verify_file_identity(identity)
    paths = {name: Path(item["path"])
             for name, item in report["inputs"].items()}
    resolved, _ = resolved_identity(paths["resolved"])
    abi = resolved_float_abi(resolved, require_cpu=True)
    resources = measure(paths["elf"], paths["resolved"], paths["budget"],
                        float_abi=abi)
    measured_elf = Elf32(paths["elf"].read_bytes(), float_abi=abi)
    resources["metrics"]["static_storage"] = inspect_storage(
        measured_elf, resources["metrics"])
    profiles, compilers, sources = compiler_commands(
        paths["compile_commands"], resolved["cpu"])
    if (resources["status"] != "passed" or
            resources["metrics"] != report["metrics"] or
            comparison_profile(resolved, profiles) !=
            report["comparison_profile"] or
            compilers != report["tools"]["compilers"] or
            sources != report["compiled_sources"] or
            resolved_input_files(paths["resolved"], resolved) !=
            report["configured_inputs"]):
        raise EvidenceError("observation summary differs from actual inputs")
    elf = Elf32(paths["elf"].read_bytes(), float_abi=abi)
    for row in report["functions"]:
        path = verify_file_identity(row["raw"])
        actual = {**decode_function(elf, row["symbol"], path.read_bytes()),
                  "raw": row["raw"]}
        if row != actual:
            raise EvidenceError("instruction summary differs from ELF/output")


def compare(baseline, candidate, growth):
    """Require equivalent actual profiles and explicit growth ceilings."""
    validate_observation(baseline)
    validate_observation(candidate)
    if (baseline["comparison_profile"] != candidate["comparison_profile"] or
            baseline["tools"]["objdump"]["sha256"] !=
            candidate["tools"]["objdump"]["sha256"]):
        raise EvidenceError("cost observations are not strictly comparable")
    old = cost_values(baseline["metrics"])
    new = cost_values(candidate["metrics"])
    if (not isinstance(growth, dict) or set(growth) != set(old) or
            set(old) != set(new) or any(type(limit) is not int or limit < 0
                                      for limit in growth.values())):
        raise EvidenceError("explicit nonnegative growth limits required")
    delta = {key: new[key] - old[key] for key in old}
    violations = [{"metric": key, "growth": value, "maximum": growth[key]}
                  for key, value in delta.items() if value > growth[key]]
    return {"schema_version": 1, "kind": "soc_cost_comparison",
            "status": ("growth_exceeded" if violations
                       else "within_growth_budget"),
            "delta": delta, "growth_limits": growth, "violations": violations,
            "baseline_inputs": baseline["inputs"],
            "candidate_inputs": candidate["inputs"],
            "physical_status": "not_executed", "cycles_status": "not_measured",
            "wcet_status": "not_measured",
            "historical_build_provenance": "not_attested"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    observer = commands.add_parser("observe")
    for name in ("elf", "resolved", "budget", "compile-commands", "output"):
        observer.add_argument("--" + name, type=Path, required=True)
    observer.add_argument("--objdump", default="arm-none-eabi-objdump")
    observer.add_argument("--symbols", nargs="*", default=[])
    observer.add_argument("--timeout-s", type=float, default=10)
    comparison = commands.add_parser("compare")
    for name in ("baseline", "candidate", "growth", "report"):
        comparison.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.action == "observe":
            result = observe(args.elf, args.resolved, args.budget,
                args.compile_commands,
                output=args.output, objdump=args.objdump, symbols=args.symbols,
                timeout_s=args.timeout_s)
        else:
            from tools.evidence.common import load_json
            baseline = load_json(args.baseline)
            candidate = load_json(args.candidate)
            protected = {args.baseline.absolute(), args.candidate.absolute(),
                         args.growth.absolute()}
            for report in (baseline, candidate):
                protected.update(Path(item["path"]).absolute()
                    for item in [*report["inputs"].values(),
                        *report["compiled_sources"],
                        *(item["file"] for item in report["configured_inputs"]),
                        report["tools"]["observer"], report["tools"]["objdump"],
                        *report["tools"]["compilers"],
                        *(item["stdout"] for item in report["executions"])])
            if args.report.absolute() in protected:
                raise EvidenceError("comparison cannot overwrite an input")
            result = compare(baseline, candidate, load_json(args.growth))
            atomic_json(args.report, result)
        print("SoC costs: " + result["status"])
        return 1 if result["status"] == "growth_exceeded" else 0
    except (EvidenceError, OSError, KeyError, TypeError, UnicodeError) as error:
        print("SoC cost observation rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
