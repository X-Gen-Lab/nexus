"""Build the real optional-memory platform graph and inspect actual ARM ELF.

This is software placement, reset closure and GC evidence. It neither executes
the MCU nor measures cycles, memory throughput or physical DMA access.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
try:
    from . import arch_compile, memory_budget
except ImportError:
    import arch_compile
    import memory_budget
from tools.measurement.elf import Elf32
from tools.measurement.resources import measure_data

FACTS = {"sram": (0x20000000, 192 * 1024, True),
         "addsram": (0x20030000, 256 * 1024, True),
         "tcm": (0x10000000, 64 * 1024, False)}
VARIANTS = {"default": (), "addsram": ("addsram",), "tcm": ("tcm",),
            "both": ("addsram", "tcm")}


def validate_layout(elf, resolved, budget, selected):
    """Require exact silicon domains, fresh configuration and live section ABI."""
    if resolved["part"] != "GD32F470ZGT6":
        raise ValueError("Memory matrix requires the exact ZG density")
    domains = {item["id"]: item for item in resolved["memory"]}
    if set(domains) != set(FACTS):
        raise ValueError("Missing or expanded physical RAM domains")
    for name, (origin, size, dma) in FACTS.items():
        domain = domains[name]
        expected = name == "sram" or name in selected
        if (domain["origin"] != origin or domain["size"] != size or
                domain["dma"] is not dma or domain["linker"] is not expected):
            raise ValueError("Physical/selected memory differs: " + name)
    metrics, violations = measure_data(elf, resolved, budget)
    if violations:
        raise ValueError("Actual ELF exceeded memory budgets: " + str(violations))
    if not (FACTS["sram"][0] <= elf.symbol("__nx_msp_start") <
            elf.symbol("__nx_msp_end") <= FACTS["sram"][0] + FACTS["sram"][1]):
        raise ValueError("Main MSP left primary SRAM")
    if elf.symbol("__nx_flash_start") != resolved["layout"]["origin"] or (
            elf.symbol("__nx_flash_end") != resolved["layout"]["origin"] +
            resolved["layout"]["size"]):
        raise ValueError("Startup Flash bounds differ from the resolved layout")
    symbols = {item["name"] for item in elf.defined_symbols}
    if any("_dead_" in name for name in symbols):
        raise ValueError("Unused placement storage survived actual section GC")
    sections = {item["name"]: item for item in elf.sections}
    for region in ("addsram", "tcm"):
        origin, capacity, _ = FACTS[region]
        if region in selected and (
                elf.symbol(f"__nx_{region}_region_start") != origin or
                elf.symbol(f"__nx_{region}_region_end") != origin + capacity):
            raise ValueError("Reset region bounds differ from silicon: " + region)
        for kind, size, section_kind in (("data", 4, 1), ("bss", 16, 8),
                                        ("noinit", 16, 8)):
            name = f".nx_{region}_{kind}"
            if region not in selected:
                if name in sections and sections[name]["size"]:
                    raise ValueError("Unselected memory section survived")
                continue
            section = sections.get(name)
            if (section is None or section["size"] != size or
                    section["kind"] != section_kind or section["flags"] != 3):
                raise ValueError("Unexpected live data-only section: " + name)
            start = section["address"]
            origin, capacity, _ = FACTS[region]
            if start % 4 or not origin <= start < start + size <= origin + capacity:
                raise ValueError("Extra section is unaligned or outside silicon")
            if (elf.symbol(f"__nx_{region}_{kind}_start") != start or
                    elf.symbol(f"__nx_{region}_{kind}_end") != start + size):
                raise ValueError("Reset linker symbols differ from actual section")
            if elf.symbol(f"g_{region}_{kind}") != start:
                raise ValueError("Live variable differs from its physical section")
        actual = metrics["ram"][region]["reserved_bytes"]
        if actual != (36 if region in selected else 0):
            raise ValueError("Actual extra RAM reservation is not the live 36 bytes")
        if budget["ram_reserved_max"][region] != (128 if region in selected else 0):
            raise ValueError("Independent authored RAM budget was lost")
    helper = "nx_gd32_memory_initialize" in symbols
    if helper != bool(selected):
        raise ValueError("Optional startup helper retention differs from selection")
    return metrics


def preprocessing_command(entry):
    """Inspect the actual default compile scope without rewriting depfiles."""
    command = entry.get("arguments") or shlex.split(entry["command"])
    result = []
    index = 0
    while index < len(command):
        argument = command[index]
        if argument in {"-o", "-MF", "-MT", "-MQ"}:
            index += 2
        elif any(argument.startswith(prefix) and len(argument) > len(prefix)
                 for prefix in ("-o", "-MF", "-MT", "-MQ")):
            index += 1
        elif argument in {"-c", "-MD", "-MMD", "-MP", "-MG"}:
            index += 1
        else:
            result.append(argument)
            index += 1
    return result + ["-E", "-P"]


def source_inputs(root):
    inputs = [root / "CMakeLists.txt", Path(__file__).resolve(),
              root / "scripts/ci/memory_budget.py",
              root / "scripts/ci/arch_compile.py"]
    for directory in ("arch", "core", "io", "os", "soc/gd32/gd32f470",
                      "boards/gd32f470_liangshan", "cmake", "tools/configure",
                      "tools/measurement", "tools/evidence",
                      "tests/contracts/gd32_memory"):
        inputs.extend(path for path in (root / directory).rglob("*")
            if path.is_file() and path.suffix in {
                ".c", ".h", ".S", ".cmake", ".py", ".json", ".toml"})
    return [arch_compile.digest(path) for path in sorted(set(inputs))]


def verify_inputs(inputs):
    for identity in inputs:
        if arch_compile.digest(Path(identity["path"])) != identity:
            raise ValueError("Memory matrix source changed: " + identity["path"])


def run(root, compiler, output, parallel=2):
    root, output = root.resolve(), output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError("Memory matrix output must be fresh")
    recorder = arch_compile.Recorder(output)
    report = {"schema_version": 1, "kind": "gd32_optional_memory_matrix",
        "status": "failed", "scope": "development_actual_platform_arm_elf",
        "physical_status": "not_executed", "cycles_status": "not_measured",
        "variants": [], "commands": recorder.commands}
    try:
        tools = arch_compile.tools(compiler)
        for name in ("cmake", "ninja"):
            tool = shutil.which(name)
            if not tool:
                raise ValueError("Missing actual build tool: " + name)
            tools[name] = tool
        tools["python"] = sys.executable
        report["tools"] = {name: arch_compile.digest(Path(path))
                           for name, path in tools.items()}
        for name, tool in tools.items():
            recorder.execute([tool, "--version"], name + "-version", nonempty=True)
        report["source_inputs"] = source_inputs(root)
        fixture = root / "tests/contracts/gd32_memory"
        for variant, selected in VARIANTS.items():
            build = output / variant
            recorder.execute([tools["cmake"], "-S", str(fixture), "-B", str(build),
                "-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=" + tools["ninja"],
                "-DCMAKE_TOOLCHAIN_FILE=" + str(root / "cmake/toolchains/arm-gcc.cmake"),
                "-DPython3_EXECUTABLE=" + tools["python"],
                "-DNEXUS_ASSEMBLY_FILE=" + str(fixture / (variant + ".toml"))],
                variant + "-configure")
            recorder.execute([tools["cmake"], "--build", str(build), "--target",
                "nexus_gd32_memory", "--parallel", str(parallel)], variant + "-build")
            elf_path = build / "bin/nexus_gd32_memory.elf"
            generated = build / "nexus/generated"
            resolved_path = generated / "resolved.json"
            budget_path = generated / "resource-budget.json"
            resolved = json.loads(resolved_path.read_text())
            budget = json.loads(budget_path.read_text())
            elf = Elf32(elf_path.read_bytes())
            metrics = validate_layout(elf, resolved, budget, selected)
            entries = json.loads((build / "compile_commands.json").read_text())
            helpers = [entry for entry in entries if
                       Path(entry["file"]).name == "memory_init.c"]
            if len(helpers) != int(bool(selected)):
                raise ValueError("Default or selected helper source graph differs")
            if not selected:
                system = [entry for entry in entries if
                          Path(entry["file"]).name == "system.c"]
                if len(system) != 1:
                    raise ValueError("Default SystemInit compile identity ambiguous")
                expanded = recorder.execute(preprocessing_command(system[0]),
                    "default-SystemInit-preprocessor", nonempty=True)
                if "nx_gd32_memory_initialize" in expanded:
                    raise ValueError("Default startup retained an extra-memory path")
            row = {"name": variant, "status": "passed", "selected": list(selected),
                "metrics": metrics, "elf": arch_compile.digest(elf_path),
                "bin": arch_compile.digest(elf_path.with_suffix(".bin")),
                "map": arch_compile.digest(elf_path.with_suffix(".map")),
                "resolved": arch_compile.digest(resolved_path),
                "budget": arch_compile.digest(budget_path),
                "resources": arch_compile.digest(elf_path.with_suffix(".resources.json")),
                "compile_commands": arch_compile.digest(build / "compile_commands.json"),
                "memory_ld": arch_compile.digest(generated / "memory.ld")}
            report["variants"].append(row)
            verify_inputs(report["source_inputs"])
            (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        negatives = []
        both = output / "both"
        for region in ("addsram", "tcm"):
            budget = json.loads((both / "nexus/generated/resource-budget.json").read_text())
            budget["ram_reserved_max"][region] = 35
            bad_budget = output / (region + "-too-small-budget.json")
            bad_budget.write_text(json.dumps(budget, indent=2) + "\n")
            result_path = output / (region + "-budget-rejected.json")
            memory_budget.rejected(recorder, [tools["python"], "-B",
                str(root / "tools/measurement/resources.py"), "--elf",
                str(both / "bin/nexus_gd32_memory.elf"), "--resolved",
                str(both / "nexus/generated/resolved.json"), "--budget",
                str(bad_budget), "--report", str(result_path)],
                region + "-budget-refusal", "ELF budget: failed")
            bad = json.loads(result_path.read_text())
            memory_budget.check_budget_tamper(bad)
            negatives.append({"kind": "budget_tamper", "region": region,
                              "report": arch_compile.digest(result_path),
                              "budget": arch_compile.digest(bad_budget)})
            negatives.append(memory_budget.execute_budget_overflow(root, recorder, tools,
                fixture, fixture / "both.toml", output, region,
                "nexus_gd32_memory", parallel))
            build = output / (region + "-unselected")
            recorder.execute([tools["cmake"], "-S", str(fixture), "-B", str(build),
                "-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=" + tools["ninja"],
                "-DCMAKE_TOOLCHAIN_FILE=" + str(root / "cmake/toolchains/arm-gcc.cmake"),
                "-DPython3_EXECUTABLE=" + tools["python"],
                "-DNEXUS_ASSEMBLY_FILE=" + str(fixture / "default.toml"),
                "-DGD32_MEMORY_INJECT=" + region.upper()], region + "-unselected-configure")
            rejection = memory_budget.rejected(recorder, [tools["cmake"], "--build", str(build),
                "--target", "nexus_gd32_memory", "--parallel", str(parallel)],
                region + "-unselected-link", "memory region is not selected")
            negatives.append({"kind": "unselected_linker_section", "region": region,
                              "command": rejection})
            verify_inputs(report["source_inputs"])
        report["negative_executions"] = negatives
        report["default_cost"] = {"helper_objects": 0, "retained_helper_symbols": 0,
            "extra_ram_bytes": 0, "extra_sections_bytes": 0,
            "startup_path": "removed_by_preprocessor"}
        for name, identity in report["tools"].items():
            if arch_compile.digest(Path(tools[name])) != identity:
                raise ValueError("Build tool changed during actual execution")
        report["status"] = "passed"
        report["source_inputs_stable"] = True
        report["tools_stable"] = True
    except (OSError, ValueError, KeyError, UnicodeError) as error:
        report["error"] = str(error)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["status"] == "passed" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--parallel", type=int, choices=(1, 2), default=2)
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if compiler:
        os.environ["PATH"] = str(Path(compiler).parent) + os.pathsep + os.environ.get("PATH", "")
    os.chdir(args.root)
    return run(args.root, args.compiler, args.output, args.parallel)


if __name__ == "__main__":
    raise SystemExit(main())
