"""Execute exact F407 default/CCM placement, reset closure and ARM ELF gates.

The raw build and disassembly are software evidence. They neither execute the
MCU nor establish physical CCM retention, throughput or DMA behavior.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
try:
    from . import arch_compile, gd32_memory, memory_budget
except ImportError:
    import arch_compile
    import gd32_memory
    import memory_budget
from tools.measurement.elf import Elf32
from tools.measurement.resources import measure_data

FACTS = {"sram": (0x20000000, 128 * 1024, True),
         "ccm": (0x10000000, 64 * 1024, False)}
VARIANTS = {"ve-default": ("STM32F407VET6", False),
            "ve-ccm": ("STM32F407VET6", True),
            "zg-default": ("STM32F407ZGT6", False),
            "zg-ccm": ("STM32F407ZGT6", True)}


def validate_layout(elf, resolved, budget, part, selected):
    """Compare actual ELF boot bounds against independent silicon facts."""
    if resolved["part"] != part or part not in {
            "STM32F407VET6", "STM32F407ZGT6"}:
        raise ValueError("Memory matrix requires exact F407 VE/ZG density")
    domains = {item["id"]: item for item in resolved["memory"]}
    if set(domains) != set(FACTS):
        raise ValueError("Missing or expanded physical RAM domains")
    for name, (origin, size, dma) in FACTS.items():
        domain = domains[name]
        if (domain["origin"] != origin or domain["size"] != size or
                domain["dma"] is not dma or
                domain["linker"] is not (name == "sram" or selected)):
            raise ValueError("Physical/selected memory differs: " + name)
    metrics, violations = measure_data(elf, resolved, budget)
    if violations:
        raise ValueError("Actual ELF exceeded memory budgets: " + str(violations))
    if not (FACTS["sram"][0] <= elf.symbol("__nx_msp_start") <
            elf.symbol("__nx_msp_end") <= sum(FACTS["sram"][:2])):
        raise ValueError("Main MSP left primary SRAM")
    if (elf.symbol("__nx_flash_start") != resolved["layout"]["origin"] or
            elf.symbol("__nx_flash_end") != resolved["layout"]["origin"] +
            resolved["layout"]["size"]):
        raise ValueError("Reset Flash bounds differ from resolved layout")
    symbols = {item["name"] for item in elf.defined_symbols}
    if any("_dead_" in name for name in symbols):
        raise ValueError("Unused placement storage survived actual section GC")
    for helper in ("nx_stm32_memory_initialize", "nx_stm32_memory_boot"):
        if (helper in symbols) != selected:
            raise ValueError("Reset helper retention differs from selection")
    sections = {item["name"]: item for item in elf.sections}
    for kind, size, section_kind in (("data", 4, 1), ("bss", 16, 8),
                                    ("noinit", 16, 8)):
        name = ".nx_ccm_" + kind
        if not selected:
            if name in sections and sections[name]["size"]:
                raise ValueError("Unselected CCM section survived")
            continue
        section = sections.get(name)
        if (section is None or section["size"] != size or
                section["kind"] != section_kind or section["flags"] != 3):
            raise ValueError("Unexpected CPU-only CCM section: " + name)
        start = section["address"]
        origin, capacity, _ = FACTS["ccm"]
        if start % 4 or not origin <= start < start + size <= origin + capacity:
            raise ValueError("CCM section is unaligned or outside silicon")
        if (elf.symbol(f"__nx_ccm_{kind}_start") != start or
                elf.symbol(f"__nx_ccm_{kind}_end") != start + size or
                elf.symbol("g_ccm_" + kind) != start):
            raise ValueError("Reset symbols differ from actual CCM storage")
    if selected and (elf.symbol("__nx_ccm_region_start") != FACTS["ccm"][0] or
            elf.symbol("__nx_ccm_region_end") != sum(FACTS["ccm"][:2])):
        raise ValueError("Reset CCM bounds differ from silicon")
    if metrics["ram"]["ccm"]["reserved_bytes"] != (36 if selected else 0):
        raise ValueError("CCM reservation differs from live 36-byte fixture")
    if budget["ram_reserved_max"]["ccm"] != (128 if selected else 0):
        raise ValueError("Independent authored CCM budget was lost")
    return metrics


def source_inputs(root):
    inputs = [root / "CMakeLists.txt", Path(__file__).resolve(),
              root / "scripts/ci/gd32_memory.py",
              root / "scripts/ci/memory_budget.py",
              root / "scripts/ci/arch_compile.py"]
    for directory in ("arch", "core", "io", "os", "soc/stm32f407",
                      "boards/stm32f407_qiming_v31",
                      "boards/stm32f407ve_sky_qingchun", "cmake",
                      "tools/configure", "tools/measurement", "tools/evidence",
                      "tests/contracts/stm32_memory"):
        inputs.extend(path for path in (root / directory).rglob("*")
            if path.is_file() and path.suffix in {
                ".c", ".h", ".S", ".cmake", ".py", ".json", ".toml"})
    return [arch_compile.digest(path) for path in sorted(set(inputs))]


def run(root, compiler, output, parallel=2):
    root, output = root.resolve(), output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError("Memory matrix output must be fresh")
    recorder = arch_compile.Recorder(output)
    report = {"schema_version": 1, "kind": "stm32_optional_memory_matrix",
        "status": "failed", "scope": "development_actual_platform_arm_elf",
        "physical_status": "not_executed", "cycles_status": "not_measured",
        "variants": [], "commands": recorder.commands}
    try:
        tools = arch_compile.tools(compiler)
        for name in ("cmake", "ninja"):
            tools[name] = shutil.which(name)
            if not tools[name]:
                raise ValueError("Missing actual build tool: " + name)
        tools["python"] = sys.executable
        report["tools"] = {name: arch_compile.digest(Path(path))
                           for name, path in tools.items()}
        for name, tool in tools.items():
            recorder.execute([tool, "--version"], name + "-version", nonempty=True)
        report["source_inputs"] = source_inputs(root)
        fixture = root / "tests/contracts/stm32_memory"
        for variant, (part, selected) in VARIANTS.items():
            build = output / variant
            recorder.execute([tools["cmake"], "-S", str(fixture), "-B", str(build),
                "-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=" + tools["ninja"],
                "-DCMAKE_TOOLCHAIN_FILE=" + str(root / "cmake/toolchains/arm-gcc.cmake"),
                "-DPython3_EXECUTABLE=" + tools["python"],
                "-DNEXUS_ASSEMBLY_FILE=" + str(fixture / (variant + ".toml"))],
                variant + "-configure")
            recorder.execute([tools["cmake"], "--build", str(build), "--target",
                "nexus_stm32_memory", "--parallel", str(parallel)], variant + "-build")
            elf_path = build / "bin/nexus_stm32_memory.elf"
            generated = build / "nexus/generated"
            resolved = json.loads((generated / "resolved.json").read_text())
            budget = json.loads((generated / "resource-budget.json").read_text())
            metrics = validate_layout(Elf32(elf_path.read_bytes()), resolved,
                                      budget, part, selected)
            entries = json.loads((build / "compile_commands.json").read_text())
            helpers = [item for item in entries if
                       Path(item["file"]).name == "memory_init.c"]
            if len(helpers) != int(selected):
                raise ValueError("Default or selected helper graph differs")
            if not selected:
                system = [item for item in entries if
                          Path(item["file"]).name == "system.c"]
                if len(system) != 1:
                    raise ValueError("Default SystemInit identity ambiguous")
                expanded = recorder.execute(
                    gd32_memory.preprocessing_command(system[0]),
                    variant + "-SystemInit-preprocessor", nonempty=True)
                if "nx_stm32_memory_boot" in expanded:
                    raise ValueError("Default startup retained a CCM path")
            else:
                recorder.execute([tools["objdump"], "-d", str(elf_path)],
                    variant + "-actual-disassembly", nonempty=True)
            report["variants"].append({"name": variant, "part": part,
                "status": "passed", "selected": selected, "metrics": metrics,
                **{name: arch_compile.digest(path) for name, path in {
                    "elf": elf_path, "bin": elf_path.with_suffix(".bin"),
                    "map": elf_path.with_suffix(".map"),
                    "resources": elf_path.with_suffix(".resources.json"),
                    "resolved": generated / "resolved.json",
                    "budget": generated / "resource-budget.json",
                    "memory_ld": generated / "memory.ld",
                    "compile_commands": build / "compile_commands.json"}.items()}})
            gd32_memory.verify_inputs(report["source_inputs"])
        selected = output / "zg-ccm"
        budget = json.loads((selected / "nexus/generated/resource-budget.json").read_text())
        budget["ram_reserved_max"]["ccm"] = 35
        bad_budget = output / "ccm-too-small-budget.json"
        bad_budget.write_text(json.dumps(budget, indent=2) + "\n")
        bad_report = output / "ccm-budget-rejected.json"
        memory_budget.rejected(recorder, [tools["python"], "-B",
            str(root / "tools/measurement/resources.py"), "--elf",
            str(selected / "bin/nexus_stm32_memory.elf"), "--resolved",
            str(selected / "nexus/generated/resolved.json"), "--budget",
            str(bad_budget), "--report", str(bad_report)],
            "ccm-budget-refusal", "ELF budget: failed")
        bad = json.loads(bad_report.read_text())
        memory_budget.check_budget_tamper(bad)
        overflow = memory_budget.execute_budget_overflow(root, recorder, tools,
            fixture, fixture / "zg-ccm.toml", output, "ccm",
            "nexus_stm32_memory", parallel)
        build = output / "ccm-unselected"
        recorder.execute([tools["cmake"], "-S", str(fixture), "-B", str(build),
            "-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=" + tools["ninja"],
            "-DCMAKE_TOOLCHAIN_FILE=" + str(root / "cmake/toolchains/arm-gcc.cmake"),
            "-DPython3_EXECUTABLE=" + tools["python"],
            "-DNEXUS_ASSEMBLY_FILE=" + str(fixture / "zg-default.toml"),
            "-DSTM32_MEMORY_INJECT=ON"], "ccm-unselected-configure")
        rejection = memory_budget.rejected(recorder, [tools["cmake"], "--build",
            str(build), "--target", "nexus_stm32_memory", "--parallel", str(parallel)],
            "ccm-unselected-link", "memory region is not selected")
        report["negative_executions"] = [
            {"kind": "budget_tamper", "report": arch_compile.digest(bad_report),
             "budget": arch_compile.digest(bad_budget)},
            overflow,
            {"kind": "unselected_linker_section", "command": rejection}]
        gd32_memory.verify_inputs(report["source_inputs"])
        for name, identity in report["tools"].items():
            if arch_compile.digest(Path(tools[name])) != identity:
                raise ValueError("Build tool changed during actual execution")
        report.update(status="passed", source_inputs_stable=True,
                      tools_stable=True, default_helper_objects=0,
                      default_ccm_reserved_bytes=0)
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
