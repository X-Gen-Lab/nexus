#!/usr/bin/env python3
"""Compile exact maintained Device headers against the single CMSIS Core 6.4.

Full GCC dependency files include SYSTEM headers. Successful compilation alone
does not admit a foreign, legacy or mixed Core, or establish physical support.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.dev.process import run
from tools.maintenance.cmsis_device_lock import git

CORE_COMMIT = "af1b110e46a1720349f6f125e386e126767699bd"
CORE_INCLUDE = Path("vendors/arm/CMSIS_6/CMSIS/Core/Include")


@dataclass(frozen=True)
class Profile:
    family: str
    directory: str
    header: str
    device_header: str
    define: str
    cortex: int
    fpu: str = "fpv4-sp-d16"
    security: str = "single"

    @property
    def name(self):
        return self.family + "-" + self.security


PROFILES = (
    Profile("stm32f103", "vendors/st/cmsis_device_f1/Include", "stm32f1xx.h",
            "stm32f103xb.h", "STM32F103xB", 3, "none"),
    Profile("stm32f407", "vendors/st/cmsis_device_f4/Include", "stm32f4xx.h",
            "stm32f407xx.h", "STM32F407xx", 4),
    Profile("stm32g474", "vendors/st/cmsis_device_g4/Include", "stm32g4xx.h",
            "stm32g474xx.h", "STM32G474xx", 4),
    Profile("stm32h743", "vendors/st/cmsis_device_h7/Include", "stm32h7xx.h",
            "stm32h743xx.h", "STM32H743xx", 7, "fpv5-d16"),
    *(Profile("stm32u575", "vendors/st/cmsis_device_u5/Include", "stm32u5xx.h",
              "stm32u575xx.h", "STM32U575xx", 33, "fpv5-sp-d16", state)
      for state in ("secure", "nonsecure")),
    Profile("gd32f303",
            "vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Include",
            "gd32f30x.h", "gd32f30x.h", "GD32F30X_HD", 4),
    Profile("gd32f470",
            "vendors/gigadevice/gd32f4xx/Firmware/CMSIS/GD/GD32F4xx/Include",
            "gd32f4xx.h", "gd32f4xx.h", "GD32F470", 4),
)


def source(profile):
    """Probe the version, used register layouts and explicit world state."""
    assertions = [
        "__CORTEX_M == " + str(profile.cortex),
        "__NVIC_PRIO_BITS == 4",
        "offsetof(SCB_Type, VTOR) == 0x08",
        "offsetof(SCB_Type, SHCSR) == 0x24",
        "offsetof(NVIC_Type, ICER) == 0x80",
        "offsetof(NVIC_Type, IPR) == 0x300",
        "offsetof(SysTick_Type, LOAD) == 0x04",
    ]
    if profile.cortex != 3:
        assertions.extend(["__FPU_PRESENT == 1",
                           "offsetof(SCB_Type, CPACR) == 0x88"])
    world = ""
    if profile.cortex == 33:
        assertions.extend(["offsetof(NVIC_Type, ITNS) == 0x280",
                           "offsetof(SCB_Type, NSACR) == 0x8c"])
        if profile.security == "secure":
            world = """
#if !defined(__ARM_FEATURE_CMSE) || __ARM_FEATURE_CMSE != 3
#error Secure compiler context required
#endif
NX_ASSERT(offsetof(SAU_Type, RLAR) == 0x10, "SAU region layout");
NX_ASSERT(GPIOA_BASE == GPIOA_BASE_S, "Secure device alias required");
uint32_t nexus_ns_vector(void) { return SCB_NS->VTOR; }
"""
        else:
            world = """
#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE & 2)
#error Nonsecure compiler context required
#endif
#ifdef SCB_NS
#error Nonsecure compilation must not expose Secure world aliases
#endif
NX_ASSERT(GPIOA_BASE == GPIOA_BASE_NS, "Nonsecure device alias required");
"""
    text = '\n'.join('NX_ASSERT(' + item + ', "Device/Core layout");'
                     for item in assertions)
    return f"""#include <stddef.h>
#include "{profile.header}"
#if !defined(__CM_CMSIS_VERSION_MAIN) || \\
    __CM_CMSIS_VERSION_MAIN != 6 || __CM_CMSIS_VERSION_SUB != 4
#error CMSIS Core 6.4 required
#endif
#ifdef __cplusplus
#define NX_ASSERT static_assert
#else
#define NX_ASSERT _Static_assert
#endif
{text}
{world}
uint32_t nexus_cmsis_probe(void) {{
    __DMB();
    return __get_PRIMASK() ^ SCB->CPUID ^ SysTick->CTRL;
}}
"""


def dependencies(text):
    """Read GCC's fixed target and escaped absolute SYSTEM paths."""
    text = text.replace("\\\n", "")
    if not text.startswith("cmsis_probe:") or len(text.splitlines()) != 1:
        raise ValueError("one complete CMSIS probe dependency rule required")
    words = re.findall(r"(?:\\.|[^\s\\])+", text[len("cmsis_probe:"):])
    paths = [Path(re.sub(r"\\(.)", r"\1", word).replace("$$", "$"))
             for word in words]
    if not paths or any(not path.is_absolute() for path in paths):
        raise ValueError("nonempty absolute compiler dependencies required")
    return paths


def check_dependencies(root, profile, paths):
    """Reject old or mixed Core headers even if their macros compile."""
    central = root / CORE_INCLUDE
    core_names = {path.name for path in central.rglob("*.h")}
    required = {central / name for name in (
        f"core_cm{profile.cortex}.h", "cmsis_version.h",
        "cmsis_compiler.h", "cmsis_gcc.h")}
    required.update(root / profile.directory / name
                    for name in (profile.header, profile.device_header))
    for path in paths:
        if (not path.is_file() or path.is_symlink() or path.resolve() != path):
            raise ValueError("canonical existing include required: " +
                             str(path))
        core_header = (path.name in core_names or
                       re.fullmatch(r"(?:core_.*|cmsis_.*)\.h", path.name))
        if core_header and not path.is_relative_to(central):
            raise ValueError("foreign CMSIS Core include: " + str(path))
    if not required.issubset(paths):
        raise ValueError("incomplete Device/Core SYSTEM dependency closure")


def compile_command(root, profile, language, compiler, source_path, output):
    """Use GNU ARM language/ABI choices and dependency-inclusive SYSTEMs."""
    if language not in ("c11", "c++17"):
        raise ValueError("explicit C11 or C++17 probe required")
    argv = [str(compiler), "-std=" + language, "-mthumb",
            "-mcpu=cortex-m" + str(profile.cortex), "-ffreestanding",
            "-Wall", "-Wextra", "-Werror", "-O1",
            "-DCMSIS_DISABLE_DEPRECATED=1"]
    argv += (["-mfloat-abi=soft"] if profile.fpu == "none" else
             ["-mfloat-abi=hard", "-mfpu=" + profile.fpu])
    if profile.security == "secure":
        argv.append("-mcmse")
    argv += ["-D" + profile.define, "-isystem", str(root / CORE_INCLUDE),
             "-isystem", str(root / profile.directory)]
    if profile.family.startswith("gd32"):
        vendor = "gd32f30x" if profile.family == "gd32f303" else "gd32f4xx"
        peripheral = "GD32F30x" if vendor == "gd32f30x" else "GD32F4xx"
        argv += ["-I", str(root / "soc/gd32" / profile.family / "private"),
                 "-isystem", str(root / "vendors/gigadevice" / vendor /
                                  "Firmware" /
                                  (peripheral +
                                   "_standard_peripheral/Include"))]
    return [*argv, "-MD", "-MF", str(output / "probe.d"), "-MT", "cmsis_probe",
            "-c", str(source_path), "-o", str(output / "probe.o")]


def identity(path):
    raw = path.read_bytes()
    return {"path": str(path), "bytes": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest()}


def verify_core(root):
    core = (root / CORE_INCLUDE).parents[2]
    if (git(core, "rev-parse", "HEAD") != CORE_COMMIT or
            git(core, "status", "--porcelain", "--untracked-files=all")):
        raise ValueError("clean pinned CMSIS Core 6.4 checkout required")


def execute(root, output, compiler, cxx):
    """Retain fresh objects, compiler receipts and full actual dependencies."""
    root, output = Path(root).absolute(), Path(output).absolute()
    if (root.resolve() != root or output.exists() or
            any(part.is_symlink() for part in (output, *output.parents))):
        raise ValueError("canonical source and fresh regular output required")
    output.mkdir(parents=True)
    report = {"kind": "cmsis_core_device_compile", "status": "running",
              "core_commit": CORE_COMMIT, "probes": [], "compilers": {},
              "software_qualified": False, "physical_status": "not_executed"}
    try:
        verify_core(root)
        tools = {}
        for language, selected in (("c11", compiler), ("c++17", cxx)):
            found = shutil.which(str(selected))
            if found is None:
                raise ValueError("required ARM compiler missing: " +
                                 str(selected))
            path = Path(found).resolve()
            tools[language] = path
            result = run([str(path), "--version"], cwd=root,
                         env=os.environ.copy(),
                         log=output / (language + ".raw"),
                         timeout=20, stage="cmsis-core-compiler")
            if result.returncode or "arm-none-eabi" not in result.stdout:
                raise ValueError("GNU ARM compiler identity unavailable")
            report["compilers"][language] = identity(path)
        for profile in PROFILES:
            for language in tools:
                directory = output / (profile.name + "-" + language)
                directory.mkdir()
                source_path = directory / (
                    "probe.c" if language == "c11" else "probe.cpp")
                source_path.write_text(source(profile), encoding="utf-8")
                result = run(compile_command(root, profile, language,
                             tools[language], source_path, directory), cwd=root,
                             env=os.environ.copy(),
                             log=directory / "compile.raw",
                             timeout=30, stage="cmsis-core-device")
                if result.returncode:
                    raise ValueError("Device/Core compile failed: " +
                                     profile.name + " " + language + "\n" +
                                     result.stderr)
                paths = dependencies((directory / "probe.d").read_text())
                check_dependencies(root, profile, paths)
                report["probes"].append({
                    "profile": profile.name, "family": profile.family,
                    "security": profile.security, "language": language,
                    "object": identity(directory / "probe.o"),
                    "dependency_file": identity(directory / "probe.d"),
                    "dependencies": [identity(path) for path in paths],
                    "command_receipt": str(result.receipt_path)})
        verify_core(root)
        report["status"] = "passed"
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        report.update(status="failed", failure=str(error))
        raise
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    parser.add_argument("--cxx", default="arm-none-eabi-g++")
    args = parser.parse_args(argv)
    try:
        report = execute(args.root, args.output, args.compiler, args.cxx)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print("CMSIS Core probe rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps({"status": report["status"],
                      "probes": len(report["probes"])}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
