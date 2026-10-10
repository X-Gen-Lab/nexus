#!/usr/bin/env python3
"""Strict CPU software profiles shared by SoC and external runtime assemblies.

A CPU profile validates instruction, optional-feature and kernel contracts. It
does not create a SoC, board, startup, pin route or hardware qualification.
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
import sys
import tempfile
from types import MappingProxyType

try:
    from .ir import CpuProfileIR
    from .providers.common import fail, integer
except ImportError:
    from ir import CpuProfileIR
    from providers.common import fail, integer


@dataclass(frozen=True)
class Architecture:
    macro: str
    basepri: bool
    atomic: str
    mpu: int
    fpus: tuple[str, ...]
    cache: bool
    security: bool
    mve: bool
    irq_count: int
    priority_bits: tuple[int, int]
    dsp: tuple[bool, ...]


# Processor limits count regular external IRQs, excluding system exceptions.
# CMSIS register-array capacity is not the processor's implemented IRQ limit.
PROFILES = MappingProxyType({
    "native": Architecture("", False, "native", 0, (), False, False, False, 0,
                            (1, 8), (False,)),
    "cortex-m0": Architecture("__ARM_ARCH_6M__", False, "irq", 0, (),
                              False, False, False, 32, (2, 2), (False,)),
    "cortex-m0plus": Architecture("__ARM_ARCH_6M__", False, "irq", 7, (),
                                  False, False, False, 32, (2, 2), (False,)),
    "cortex-m3": Architecture("__ARM_ARCH_7M__", True, "exclusive", 7, (),
                              False, False, False, 240, (3, 8), (False,)),
    "cortex-m4": Architecture("__ARM_ARCH_7EM__", True, "exclusive", 7,
                              ("fpv4-sp-d16",), False, False, False, 240,
                              (3, 8), (True,)),
    "cortex-m7": Architecture("__ARM_ARCH_7EM__", True, "exclusive", 7,
                              ("fpv5-sp-d16", "fpv5-d16"), True, False,
                              False, 240, (3, 8), (True,)),
    "cortex-m23": Architecture("__ARM_ARCH_8M_BASE__", False, "exclusive", 8,
                               (), False, True, False, 240, (2, 2), (False,)),
    "cortex-m33": Architecture("__ARM_ARCH_8M_MAIN__", True, "exclusive", 8,
                               ("fpv5-sp-d16",), False, True, False, 480,
                               (3, 8), (False, True)),
    "cortex-m55": Architecture("__ARM_ARCH_8M_MAIN__", True, "exclusive", 8,
                               ("auto", "fpv5-sp-d16", "fpv5-d16"), True,
                               True, True, 480, (3, 8), (True,)),
    "cortex-m85": Architecture("__ARM_ARCH_8M_MAIN__", True, "exclusive", 8,
                               ("auto", "fpv5-sp-d16", "fpv5-d16"), True,
                               True, True, 480, (3, 8), (True,)),
})
CPU_FIELDS = frozenset({"arch", "fpu", "float_abi", "dwt_cyccnt",
                        "mpu_version", "icache_line_bytes", "dcache_line_bytes",
                        "security", "sau", "mve", "dsp"})
_BUNDLE = ".nexus-cpu-bundle"


def fields(value, required, context):
    if not isinstance(value, dict):
        fail(f"{context}: expected object")
    missing, unknown = required - value.keys(), value.keys() - required
    if missing or unknown:
        fail(f"{context}: missing {sorted(missing)}, unknown {sorted(unknown)}")


def boolean(value, context):
    if type(value) is not bool:
        fail(f"{context}: boolean required")
    return value


def resolve(cpu, irq, backend, *, enum_abi=None):
    """Resolve explicit capabilities without inferring optional hardware."""
    fields(cpu, CPU_FIELDS, "CPU facts")
    arch = cpu["arch"]
    if not isinstance(arch, str) or arch not in PROFILES:
        fail(f"Unsupported CPU architecture: {arch}")
    profile = PROFILES[arch]
    native = arch == "native"
    if not isinstance(backend, str) or backend not in (
            {"native", "baremetal"} if native else {"baremetal", "freertos"}):
        fail("CPU and OS backend contradict each other")
    if enum_abi is None:
        enum_abi = "native-int" if native else "short-enums"
    if enum_abi != ("native-int" if native else "short-enums"):
        fail("Unsupported CPU enum ABI")
    dsp = boolean(cpu["dsp"], "DSP capability")
    if dsp not in profile.dsp:
        fail("DSP differs from the processor instruction profile")
    fpu, abi, mve = cpu["fpu"], cpu["float_abi"], cpu["mve"]
    if not isinstance(fpu, str) or fpu not in ("none", *profile.fpus):
        fail("FPU differs from the CPU instruction profile")
    if not isinstance(mve, str) or mve not in {"none", "integer", "float"} or (
            mve != "none" and not profile.mve):
        fail("MVE differs from the CPU instruction profile")
    if not isinstance(abi, str):
        fail("CPU float ABI must be explicit text")
    if native:
        if abi != "native" or fpu != "none":
            fail("Unsupported Native CPU ABI")
    elif fpu == "none":
        expected_abi = "softfp" if mve == "integer" else "soft"
        if abi != expected_abi or mve == "float":
            fail("No-FPU ABI differs from the declared instruction features")
    elif abi not in {"hard", "softfp"}:
        fail("Enabled FPU requires hard or softfp ABI")
    dwt = boolean(cpu["dwt_cyccnt"], "DWT cycle counter")
    if dwt and (native or not profile.basepri):
        fail("DWT cycle counter is unavailable in the maintained CPU profile")
    mpu = integer(cpu["mpu_version"], 0, 8, "MPU version")
    if mpu not in {0, profile.mpu}:
        fail("MPU format differs from the CPU profile")
    for field in ("icache_line_bytes", "dcache_line_bytes"):
        line = integer(cpu[field], 0, 32, "CPU cache line")
        if line not in {0, 32} or (line and not profile.cache):
            fail("Cache line differs from the maintained CPU cache profile")
    security = cpu["security"]
    if not isinstance(security, str) or security not in {
            "single", "secure", "nonsecure"} or (
            security != "single" and not profile.security):
        fail("Security state differs from the CPU profile")
    sau = boolean(cpu["sau"], "SAU capability")
    if sau and (not profile.security or security != "secure"):
        fail("SAU access needs an explicit Secure execution state")
    fields(irq, {"priority_bits", "external_count"}, "IRQ facts")
    low, high = profile.priority_bits
    bits = integer(irq["priority_bits"], low, high, "IRQ priority bits")
    count = integer(irq["external_count"], 0 if native else 1,
                    profile.irq_count, "External IRQ count")
    maximum = (1 << bits) - 1
    ceiling = None
    port = None
    if backend == "freertos":
        ceiling = (10 if bits == 8 else 5) if profile.basepri else 0
        integer(ceiling, 1 if profile.basepri else 0, maximum,
                "FreeRTOS syscall priority")
        if arch in {"cortex-m0", "cortex-m0plus"}:
            port = "GCC/ARM_CM0"
        elif arch in {"cortex-m3", "cortex-m4"} and fpu == "none":
            port = "GCC/ARM_CM3"
        elif arch == "cortex-m4":
            port = "GCC/ARM_CM4F"
        elif arch == "cortex-m7":
            port = ("nexus/ARM_CM7_integer" if fpu == "none" else
                    "GCC/ARM_CM7/r0p1")
        else:
            port = f"GCC/ARM_CM{arch[8:]}_NTZ/non_secure"
    irq_result = {"priority_bits": bits, "maximum_priority": maximum,
                  "syscall_priority": ceiling, "kernel_port": port,
                  "external_count": count,
                  "mask_kind": "basepri" if profile.basepri else "primask"}
    flags = []
    if not native:
        target = arch
        if arch == "cortex-m33" and not dsp:
            target += "+nodsp"
        if profile.mve:
            if mve == "none":
                target += "+nomve"
            elif mve == "integer" and fpu != "none":
                target += "+nomve.fp"
            if fpu == "none":
                target += "+nofp"
        flags = [f"-mcpu={target}", "-mthumb"]
        if security == "secure":
            flags.append("-mcmse")
        if fpu != "none":
            flags.append(f"-mfpu={fpu}")
        flags.extend((f"-mfloat-abi={abi}", "-fshort-enums"))
    definitions = {
        "NEXUS_ARCH_HAS_DWT_CYCCNT": int(dwt),
        "NEXUS_ARCH_MPU_VERSION": mpu,
        "NEXUS_ARCH_ICACHE_LINE_BYTES": cpu["icache_line_bytes"],
        "NEXUS_ARCH_DCACHE_LINE_BYTES": cpu["dcache_line_bytes"],
        "NEXUS_ARCH_SECURITY_STATE": {"single": 0, "secure": 1,
                                      "nonsecure": 2}[security],
        "NEXUS_ARCH_HAS_SAU": int(sau),
        "NEXUS_CPU_HAS_BASEPRI": int(profile.basepri),
        "NEXUS_CPU_HAS_FPU": int(fpu != "none"),
        "NEXUS_CPU_HAS_MVE": int(mve != "none"),
        "NEXUS_CPU_HAS_DSP": int(dsp),
        "NEXUS_CPU_SECURE_ONLY": int(security == "secure"),
        "NEXUS_CPU_EXTERNAL_IRQ_COUNT": count,
        "NEXUS_CPU_ATOMIC_BACKEND_IRQ": int(profile.atomic == "irq"),
    }
    record = {"arch": arch, "fpu": fpu, "float_abi": abi,
              "enum_abi": enum_abi, "backend": backend,
              "arch_macro": profile.macro, "has_basepri": profile.basepri,
              "atomic_backend": profile.atomic, "cpu": dict(cpu),
              "irq": irq_result, "compile_options": flags,
              "definitions": definitions}
    return CpuProfileIR.from_validated(record)


def selection_lines(profile):
    """Emit build facts, not a source graph or authored override mechanism."""
    values = {"NEXUS_CPU_ARCH": profile.arch,
              "NEXUS_CPU_FPU": profile.fpu,
              "NEXUS_FLOAT_ABI": profile.float_abi,
              "NEXUS_ENUM_ABI": profile.enum_abi,
              "NEXUS_BACKEND": profile.backend,
              "NEXUS_FREERTOS_PORT": profile.irq.kernel_port or "",
              "NEXUS_IRQ_PRIORITY_BITS": profile.irq.priority_bits,
              "NEXUS_CPU_COMPILE_OPTIONS": ";".join(profile.compile_options),
              **profile.definitions}
    return [f'set({key} "{value}")' for key, value in values.items()]


def header_lines(profile):
    lines = [f"#define {key} {value}u" for key, value in
             profile.definitions.items()]
    lines.append("#define NEXUS_IRQ_PRIORITY_BITS "
                 f"{profile.irq.priority_bits}u")
    if profile.irq.syscall_priority is not None:
        lines.append("#define NEXUS_IRQ_SYSCALL_PRIORITY "
                     f"{profile.irq.syscall_priority}u")
    return lines


def emit_profile(profile, output, *, clock_hz, optimization="Os", inputhash):
    """Write a complete bundle into the caller's fresh temporary directory."""
    integer(clock_hz, 1, context="CPU clock Hz")
    if (not isinstance(optimization, str) or
            optimization not in {"O2", "Os", "O3"}):
        fail("Unsupported CPU runtime optimization")
    if (not isinstance(inputhash, str) or
            not re.fullmatch(r"[0-9a-f]{64}", inputhash)):
        fail("CPU bundle requires an exact input SHA-256")
    output = Path(output)
    if not output.is_dir() or any(output.iterdir()):
        fail("CPU emission requires an empty temporary directory")
    resolved = {"schema_version": 1, "cpu_profile": profile.to_dict(),
                "clock_hz": clock_hz, "optimization": optimization,
                "input_sha256": inputhash}
    content = json.dumps(resolved, sort_keys=True, separators=(",", ":"))
    digest = hashlib.sha256(content.encode()).hexdigest()
    resolved["configuration_sha256"] = digest
    (output / "resolved-cpu.json").write_text(
        json.dumps(resolved, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    cmake = selection_lines(profile) + [
        f'set(NEXUS_OPTIMIZATION "{optimization}")',
        f'set(NEXUS_CONFIG_SHA256 "{digest}")']
    (output / "selection.cmake").write_text("\n".join(cmake) + "\n",
                                           encoding="utf-8")
    header = ["/* Generated from one validated CPU runtime assembly. */",
              "#ifndef NEXUS_CONFIG_H", "#define NEXUS_CONFIG_H",
              f'#define NEXUS_CONFIG_SHA256 "{digest}"',
              f"#define NEXUS_CORE_HZ {clock_hz}u",
              f"#define NEXUS_BACKEND_{profile.backend.upper()} 1",
              *header_lines(profile), "#endif", ""]
    (output / "nexus_config.h").write_text("\n".join(header), encoding="utf-8")
    (output / _BUNDLE).write_text("Nexus CPU runtime bundle\n",
                                 encoding="utf-8")
    return resolved


def checked_path(path):
    result = Path(os.path.abspath(path))
    if any(part.is_symlink() for part in (result, *result.parents)):
        fail("CPU input and output paths must not contain a symlink")
    return result


def invalidate(output):
    output = checked_path(output)
    if output.exists():
        marker = output / _BUNDLE
        if not output.is_dir() or not marker.is_file() or marker.is_symlink():
            fail("Existing CPU output is not an owned bundle")
        shutil.rmtree(output)


def unique_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail(f"Duplicate CPU fact key: {key}")
        result[key] = value
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--facts", type=Path, required=True)
    parser.add_argument("--backend", choices=("baremetal", "freertos"),
                        required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--optimization", choices=("O2", "Os", "O3"),
                        default="Os")
    args = parser.parse_args(argv)
    temporary = None
    try:
        source, output = checked_path(args.facts), checked_path(args.output)
        if output == source or output in source.parents:
            fail("CPU output must not replace facts or their source parent")
        invalidate(output)
        content = source.read_bytes()
        facts = json.loads(content, object_pairs_hook=unique_keys)
        fields(facts, {"schema_version", "cpu", "irq", "clock_hz"},
               "CPU package")
        integer(facts["schema_version"], 1, 1, "CPU facts schema")
        profile = resolve(facts["cpu"], facts["irq"], args.backend)
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = Path(tempfile.mkdtemp(prefix=".nexus-cpu-",
                                          dir=output.parent))
        emit_profile(profile, temporary, clock_hz=facts["clock_hz"],
                     optimization=args.optimization,
                     inputhash=hashlib.sha256(content).hexdigest())
        if checked_path(source).read_bytes() != content:
            fail("CPU facts changed during generation")
        os.replace(temporary, output)
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"CPU configuration rejected: {error}", file=sys.stderr)
        return 1
    finally:
        if temporary is not None and temporary.exists():
            shutil.rmtree(temporary)


if __name__ == "__main__":
    raise SystemExit(main())
