"""Compile exact F4 SDK selectors and startup vectors, never qualify a Board.

The catalog records device-header facts rather than ordering-part memory sizes.
Each probe links the original startup into an artificial small memory layout;
SystemInit and libc initialization are explicit link stubs and are never run.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path, PurePosixPath
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "scripts/ci"))
from arch_compile import Recorder, digest, tools  # noqa: E402
from tools.configure.metadata import parse_json, regular  # noqa: E402
from tools.configure.providers.common import vendor_device_catalog  # noqa: E402
from tools.maintenance import cmsis_core_probe  # noqa: E402
from tools.maintenance.cmsis_device_lock import git  # noqa: E402

CATALOG = Path("soc/stm32/stm32f4/devices.json")
DEVICE_DIRECTORY = Path("vendors/st/cmsis_device_f4")
TIMEBASE_NAMES = ("TIM2", "TIM5", "LPTIM1")
FLAGS = ("-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard")
LINKER = """ENTRY(Reset_Handler)
MEMORY {
    PROBE_FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 16K
    PROBE_RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 4K
}
_estack = ORIGIN(PROBE_RAM) + LENGTH(PROBE_RAM);
SECTIONS {
    .isr_vector : { KEEP(*(.isr_vector)) } > PROBE_FLASH
    .text : { *(.text*) *(.rodata*) } > PROBE_FLASH
    .ARM.exidx : { *(.ARM.exidx*) } > PROBE_FLASH
    .data : {
        _sdata = .; *(.data*) _edata = .;
    } > PROBE_RAM AT > PROBE_FLASH
    _sidata = LOADADDR(.data);
    .bss : { _sbss = .; *(.bss*) *(COMMON) _ebss = .; } > PROBE_RAM
}
"""


def relative_input(name):
    """Reject alternate roots, traversal and platform-dependent separators."""
    if not isinstance(name, str):
        raise ValueError("canonical relative SDK input required")
    path = PurePosixPath(name)
    if (not name or "\\" in name or
            path.is_absolute() or path.as_posix() != name or
            any(part in {"", ".", ".."} for part in path.parts)):
        raise ValueError("canonical relative SDK input required")
    return path


def header_facts(text):
    """Read literal official enum values; the C probe independently asserts them."""
    irqs = {name: int(number) for name, number in re.findall(
        r"\b(\w+_IRQn)\s*=\s*(-?\d+)", text) if int(number) >= 0}
    bits = re.search(r"^#define\s+__NVIC_PRIO_BITS\s+(\d+)", text, re.MULTILINE)
    if not irqs or bits is None:
        raise ValueError("official IRQ or priority facts missing")
    last = max(irqs, key=irqs.get)
    presence = {name: bool(re.search(
        r"^#define\s+" + name + r"\s+\(", text, re.MULTILINE))
        for name in TIMEBASE_NAMES}
    return {"external_irq_count": irqs[last] + 1,
            "last_external_irq": {"name": last, "number": irqs[last]},
            "nvic_priority_bits": int(bits[1]), "timebase_presence": presence}


def load_catalog(root, *, catalog_path=None):
    """Admit the reviewed catalog only against matching original SDK bytes."""
    root = Path(root).resolve()
    path = Path(catalog_path) if catalog_path is not None else root / CATALOG
    catalog = parse_json(regular(path).read_bytes())
    if (set(catalog) != {"schema_version", "scope", "sdk", "devices", "inputs"} or
            type(catalog["schema_version"]) is not int or
            catalog["schema_version"] != 1 or catalog["scope"] != "sdk_device_compile"):
        raise ValueError("SDK device compile catalog schema required")
    vendor_device_catalog(catalog)
    sdk = catalog["sdk"]
    if (set(sdk) != {"directory", "dependency", "commit", "core_directory",
                     "core_dependency", "core_commit"} or
            sdk["directory"] != DEVICE_DIRECTORY.as_posix() or
            sdk["dependency"] != "cmsis_device_f4" or
            sdk["core_directory"] != "vendors/arm/CMSIS_6" or
            sdk["core_dependency"] != "cmsis_core" or
            sdk["core_commit"] != cmsis_core_probe.CORE_COMMIT):
        raise ValueError("reviewed F4 SDK source identity required")
    directory = root / DEVICE_DIRECTORY
    dispatcher = regular(directory / "Include/stm32f4xx.h").read_text()
    selectors = dict(re.findall(
        r'#(?:if|elif) defined\((STM32F\w+)\)\s*\n#include "([^"]+)"', dispatcher))
    if len(selectors) != 23 or not isinstance(catalog["devices"], list):
        raise ValueError("complete official F4 selector set required")
    defines = [item["define"] for item in catalog["devices"]]
    if len(set(defines)) != len(defines):
        raise ValueError("duplicate SDK device selector")
    if set(defines) != set(selectors):
        raise ValueError("unknown or missing SDK device selector")
    expected_inputs = {"Include/stm32f4xx.h", "Include/system_stm32f4xx.h", "LICENSE.md"}
    for profile in catalog["devices"]:
        fields = {"id", "define", "header", "startup", "external_irq_count",
                  "last_external_irq", "nvic_priority_bits", "timebase_presence"}
        if not fields.issubset(profile) or set(profile) - fields - {"irq_bindings"}:
            raise ValueError("exact SDK device profile fields required")
        if (not isinstance(profile["timebase_presence"], dict) or
                set(profile["timebase_presence"]) != set(TIMEBASE_NAMES) or
                any(type(value) is not bool for value in profile["timebase_presence"].values())):
            raise ValueError("exact boolean timebase presence facts required")
        header = "Include/" + selectors[profile["define"]]
        startup = "Source/Templates/gcc/startup_" + Path(header).stem + ".s"
        if profile["header"] != header or profile["startup"] != startup:
            raise ValueError("device header/startup identity mismatch")
        expected_inputs.update((header, startup))
        header_text = regular(directory / header).read_text()
        facts = header_facts(header_text)
        if any(profile[key] != value for key, value in facts.items()):
            raise ValueError("device IRQ/timebase facts differ from official header")
        table = regular(directory / startup).read_text().split("g_pfnVectors:", 1)[1]
        table = table.split(".size", 1)[0]
        words = re.findall(r"^\s*\.word\s+(\w+)", table, re.MULTILINE)
        if len(words) != 16 + profile["external_irq_count"]:
            raise ValueError("startup vector span differs from official IRQ enum")
        if "irq_bindings" in profile:
            irqs = {name: int(number) for name, number in re.findall(
                r"\b(\w+_IRQn)\s*=\s*(-?\d+)", header_text)
                    if int(number) >= 0}
            actual = {name: {"number": number, "handler": words[16 + number]}
                      for name, number in irqs.items()}
            if profile["irq_bindings"] != actual:
                raise ValueError("IRQ enum/handler differs from locked startup vector")
    if not isinstance(catalog["inputs"], dict) or set(catalog["inputs"]) != expected_inputs:
        raise ValueError("complete SDK input identity set required")
    for name, expected in catalog["inputs"].items():
        relative_input(name)
        actual = digest(regular(directory / name))
        if expected != {"bytes": actual["bytes"], "sha256": actual["sha256"]}:
            raise ValueError("SDK input identity mismatch: " + name)
    lock = parse_json(regular(root / "dependencies/stm32f4-hal.lock.json").read_bytes())
    if sdk["commit"] != lock["pairing"]["cmsis_commit"]:
        raise ValueError("SDK device commit differs from reviewed HAL pairing")
    return catalog


def select_profiles(catalog, choices):
    """Use exact declared selectors without shell fragments or fallback."""
    indexed = {item["define"]: item for item in catalog["devices"]}
    if len(set(choices)) != len(choices):
        raise ValueError("duplicate requested device selector")
    if any(choice not in indexed for choice in choices):
        raise ValueError("unknown requested device selector")
    return tuple(indexed[name] for name in choices) if choices else tuple(catalog["devices"])


def check_compiler_version(text, expected):
    """Require the repository's actual GNU ARM release, not a host compiler."""
    if "arm-none-eabi-gcc" not in text or expected.lower() not in text.lower():
        raise ValueError("locked ARM compiler toolchain release required")


def probe_source(profile):
    """Generate compile assertions and link stubs, never hardware behavior."""
    lines = ['#include "stm32f4xx.h"',
             '_Static_assert(__CORTEX_M == 4, "Cortex-M4 required");',
             '_Static_assert(__FPU_PRESENT == 1, "single-precision FPU required");',
             f'_Static_assert(__NVIC_PRIO_BITS == {profile["nvic_priority_bits"]}, "IRQ priorities");',
             f'_Static_assert({profile["last_external_irq"]["name"]} == '
             f'{profile["external_irq_count"] - 1}, "IRQ span");']
    for name, present in profile["timebase_presence"].items():
        lines.extend([f"#if {'!' if present else ''}defined({name})",
                      f'#error "{name} device presence mismatch"', "#endif"])
    lines.extend(["void SystemInit(void) {}", "void __libc_init_array(void) {}",
                  "int main(void) { for (;;) { __NOP(); } }"])
    return "\n".join(lines) + "\n"


def check_linked_image(profile, symbols, elf_header):
    """Check the actual ARM vector object size and Thumb reset entry."""
    observed = {}
    for address, size, kind, name in re.findall(
            r"^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+(\w)\s+(\w+)\s*$", symbols, re.MULTILINE):
        observed[name] = (int(address, 16), int(size, 16), kind)
    vector = observed.get("g_pfnVectors")
    reset = observed.get("Reset_Handler")
    if vector is None or vector[:2] != (0x08000000, 4 * (16 + profile["external_irq_count"])):
        raise ValueError("linked startup vector address/span mismatch")
    entry = re.search(r"Entry point address:\s*(0x[0-9a-fA-F]+)", elf_header)
    if not re.search(r"Machine:\s+ARM\s*$", elf_header, re.MULTILINE):
        raise ValueError("ARM ELF required")
    if reset is None or entry is None or int(entry[1], 16) != (reset[0] | 1):
        raise ValueError("linked Thumb Reset_Handler entry mismatch")


def capture(path):
    """Bind bytes and filesystem identity around the executed compiler calls."""
    path = regular(path)
    before = path.stat()
    record = digest(path)
    after = path.stat()
    identity = lambda value: (value.st_dev, value.st_ino, value.st_size,
                              value.st_mtime_ns, value.st_ctime_ns)
    if identity(before) != identity(after):
        raise ValueError("SDK probe input changed while reading: " + str(path))
    return record, identity(after)


def execute_profile(root, selected_tools, profile, output):
    recorder = Recorder(output / profile["define"])
    report = {"define": profile["define"], "status": "failed", "commands": recorder.commands}
    try:
        source = recorder.output / "probe.c"
        source.write_text(probe_source(profile))
        linker = recorder.output / "probe.ld"
        linker.write_text(LINKER)
        flags = [*FLAGS, "-std=c11", "-Os", "-ffreestanding", "-fno-builtin",
                 "-Wall", "-Wextra", "-Werror", "-D" + profile["define"],
                 "-DCMSIS_DISABLE_DEPRECATED=1", "-isystem", str(root / DEVICE_DIRECTORY / "Include"),
                 "-isystem", str(root / cmsis_core_probe.CORE_INCLUDE)]
        deps = recorder.output / "probe.d"
        obj = recorder.output / "probe.o"
        startup = root / DEVICE_DIRECTORY / profile["startup"]
        startup_obj = recorder.output / "startup.o"
        elf = recorder.output / "probe.elf"
        recorder.execute([selected_tools["gcc"], *flags, "-M", "-MF", str(deps),
                          "-MT", "cmsis_probe", str(source)], "dependencies")
        paths = cmsis_core_probe.dependencies(deps.read_text())
        device = cmsis_core_probe.Profile("stm32f4", str(DEVICE_DIRECTORY / "Include"),
                                         "stm32f4xx.h", Path(profile["header"]).name,
                                         profile["define"], 4)
        cmsis_core_probe.check_dependencies(root, device, paths)
        snapshots = {path: capture(path) for path in sorted(set(paths) | {startup, linker})}
        recorder.execute([selected_tools["gcc"], *flags, "-c", str(source), "-o", str(obj)], "compile")
        recorder.execute([selected_tools["gcc"], *FLAGS, "-c", str(startup),
                          "-o", str(startup_obj)], "startup")
        recorder.execute([selected_tools["gcc"], *FLAGS, "-nostdlib", "-Wl,--no-undefined",
                          "-Wl,--fatal-warnings", "-Wl,--gc-sections", "-T", str(linker),
                          "-Wl,-Map=" + str(recorder.output / "probe.map"), str(startup_obj),
                          str(obj), "-o", str(elf)], "link")
        symbols = recorder.execute([selected_tools["nm"], "-S", "--defined-only", str(elf)],
                                   "symbols", nonempty=True)
        header = recorder.execute([selected_tools["readelf"], "-h", str(elf)], "elf", nonempty=True)
        check_linked_image(profile, symbols, header)
        for path, before in snapshots.items():
            if capture(path) != before:
                raise ValueError("SDK probe input changed during execution: " + str(path))
        report.update(status="compile_link_passed", elf=digest(elf),
                      inputs=[record for record, _ in snapshots.values()],
                      external_irq_count=profile["external_irq_count"],
                      timebase_presence=profile["timebase_presence"])
    except (ValueError, OSError) as error:
        report["error"] = str(error)
    return report


def run(root, compiler, output, *, devices=(), jobs=1):
    root, output = Path(root).resolve(), Path(output).resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError("fresh empty SDK probe output required")
    recorder = Recorder(output)
    report = {"schema": 1, "status": "failed", "scope": "sdk_device_compile",
              "full_platform_status": "not_qualified", "physical_status": "not_executed",
              "link_layout": "artificial_probe_not_a_part_memory_map",
              "runtime_initialization": "link_stubs_not_executed", "profiles": [],
              "commands": recorder.commands}
    try:
        control_paths = (CATALOG, Path("scripts/ci/stm32f4_devices.py"),
                         Path("scripts/ci/arch_compile.py"),
                         Path("tools/configure/metadata.py"),
                         Path("tools/configure/providers/common.py"),
                         Path("tools/maintenance/cmsis_core_probe.py"),
                         Path("tools/maintenance/cmsis_device_lock.py"),
                         Path("dependencies/toolchains.lock.json"),
                         Path("dependencies/stm32f4-hal.lock.json"))
        controls = {root / name: capture(root / name) for name in control_paths}
        report["control_inputs"] = [record for record, _ in controls.values()]
        catalog = load_catalog(root)
        selected = select_profiles(catalog, devices)
        if jobs not in {1, 2}:
            raise ValueError("SDK probe parallelism must be one or two")
        report["catalog"] = digest(root / CATALOG)
        report["sdk"] = catalog["sdk"]
        reviewed_inputs = {root / DEVICE_DIRECTORY / name: capture(
            root / DEVICE_DIRECTORY / name) for name in catalog["inputs"]}
        for path, (record, _) in reviewed_inputs.items():
            expected = catalog["inputs"][path.relative_to(root / DEVICE_DIRECTORY).as_posix()]
            if {"bytes": record["bytes"], "sha256": record["sha256"]} != expected:
                raise ValueError("SDK source identity changed before compilation")
        report["reviewed_inputs"] = [record for record, _ in reviewed_inputs.values()]
        selected_tools = tools(compiler)
        report["tools"] = {name: digest(Path(path)) for name, path in selected_tools.items()}
        version = recorder.execute([selected_tools["gcc"], "--version"], "compiler-version", nonempty=True)
        lock_path = regular(root / "dependencies/toolchains.lock.json")
        lock = parse_json(lock_path.read_bytes())
        check_compiler_version(version, lock["arm_gnu"]["version"])
        report["toolchain_lock"] = digest(lock_path)
        directory = root / DEVICE_DIRECTORY
        if (git(directory, "rev-parse", "HEAD") != catalog["sdk"]["commit"] or
                git(directory, "status", "--porcelain", "--untracked-files=all")):
            raise ValueError("clean pinned F4 CMSIS Device checkout required")
        cmsis_core_probe.verify_core(root)
        with ThreadPoolExecutor(max_workers=jobs) as executor:
            report["profiles"] = list(executor.map(
                lambda item: execute_profile(root, selected_tools, item, output), selected))
        if any(item["status"] != "compile_link_passed" for item in report["profiles"]):
            raise ValueError("one or more SDK device probes failed")
        for path, before in {**controls, **reviewed_inputs}.items():
            if capture(path) != before:
                raise ValueError("SDK probe input changed during execution: " + str(path))
        report["status"] = "passed"
    except (ValueError, OSError, KeyError, IndexError) as error:
        report["error"] = str(error)
    (output / "results.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    return 0 if report["status"] == "passed" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--device", action="append", default=[])
    parser.add_argument("--jobs", type=int, choices=(1, 2), default=1)
    args = parser.parse_args()
    try:
        return run(args.root, args.compiler, args.output, devices=tuple(args.device), jobs=args.jobs)
    except ValueError as error:
        print("STM32F4 SDK device probe rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
