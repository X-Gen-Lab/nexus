"""Build and inspect actual U575 Secure/NonSecure reference images.

This gate proves software bytes and a paired CMSE linkage boundary. It never
programs option bytes, runs a station or seals a single-ELF software candidate.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.ci import arch_compile, provider_assemblies
from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.identity import git_source
from tools.measurement.elf import Elf32, binary_from_elf
from tools.measurement.soc_costs import decode_function


def section(elf, name):
    matches = [item for item in elf.sections if item["name"] == name]
    if len(matches) != 1:
        raise EvidenceError("one required section missing: " + name)
    return matches[0]


def bytes_at(elf, address, size):
    for item in elf.sections:
        if (item["kind"] != 8 and item["address"] <= address
                and address + size <= item["address"] + item["size"]):
            offset = item["offset"] + address - item["address"]
            return elf.data[offset:offset + size]
    raise EvidenceError("required bytes leave the file-backed section")


def contained(base, size, regions):
    return any(first <= base < base + size <= end for first, end in regions)


def validate_pair(secure_bytes, nonsecure_bytes, imported):
    """Validate real vectors, LOAD permissions, gateway and partition bytes."""
    secure = Elf32(secure_bytes, float_abi="soft")
    nonsecure = Elf32(nonsecure_bytes, float_abi="soft")
    stacks = []
    for elf, flash, ram, top, reset in (
            (secure, (0x0c000000, 0x0c100000),
             (0x30000000, 0x30030000), 0x30030000,
             "Secure_Reset_Handler"),
            (nonsecure, (0x08100000, 0x08110000),
             (0x20040000, 0x20044000), 0x20044000,
             "NonSecure_Reset_Handler")):
        vectors = section(elf, ".isr_vector")
        if (vectors["address"] != flash[0] or vectors["size"] != 568
                or vectors["kind"] == 8 or vectors["flags"] != 2):
            raise EvidenceError("exact U575 vector placement required")
        values = struct.unpack("<142I", bytes_at(elf, flash[0], 568))
        if (values[0] != top or values[1] != elf.entry
                or values[1] != elf.symbol(reset)
                or not flash[0] <= values[1] & ~1 < flash[1]):
            raise EvidenceError("actual vector stack/reset differs from partition")
        if elf is nonsecure and any(values[2:]):
            raise EvidenceError("NS privileged exception handlers are forbidden")
        if elf is nonsecure and (values[1] & ~1) < flash[0] + 1024:
            raise EvidenceError("NS Reset enters the reserved vector page")
        stack = section(elf, ".stack")
        expected = 4096 if elf is secure else 2048
        if (stack["address"] != top - expected or stack["size"] != expected
                or stack["kind"] != 8 or stack["flags"] != 3
                or elf.symbols.get("__stack_bottom") != stack["address"]
                or elf.symbols.get("__stack_top") != top):
            raise EvidenceError("actual stack reservation differs from partition")
        stacks.append(stack["size"])
        for load in elf.loads:
            if (load["memory"] and contained(
                    load["virtual"], load["memory"], (flash,))
                    and load["physical"] != load["virtual"]):
                raise EvidenceError("Flash load address differs from its VMA")
            if load["memory"] and not contained(
                    load["virtual"], load["memory"], (flash, ram)):
                raise EvidenceError("LOAD leaves its security-domain partition")
            if load["files"] and not contained(
                    load["physical"], load["files"], (flash,)):
                raise EvidenceError("initialization LOAD leaves owned Flash")
            if load["flags"] & 1 and not contained(
                    load["virtual"], load["files"], (flash,)):
                raise EvidenceError("executable RAM is not in the reference")
            if load["flags"] & 2 and not contained(
                    load["virtual"], load["memory"], (ram,)):
                raise EvidenceError("Flash is not writable reference storage")
        if elf.symbol("__data_load") & 3:
            raise EvidenceError("startup copy requires aligned DATA LMA")
        # Resolve each allocated file-backed section through exactly one LOAD.
        # A copied SG section outside LOAD bytes cannot become flash evidence.
        binary_from_elf(elf, flash[0], flash[1])
    nsc = section(secure, ".gnu.sgstubs")
    gateway = secure.symbol("nx_reference_gpio") & ~1
    if (nsc["address"] != 0x0c0fe000 or nsc["size"] == 0
            or nsc["size"] > 0x2000 or nsc["address"] & 31
            or not nsc["address"] <= gateway < nsc["address"] + nsc["size"]
            or bytes_at(secure, gateway, 4) != b"\x7f\xe9\x7f\xe9"):
        raise EvidenceError("actual SG veneer missing from exact NSC partition")
    if (type(imported) is not int or imported != gateway
            or nonsecure.symbol("nx_reference_gpio") & ~1 != gateway):
        raise EvidenceError("Secure import and NS gateway addresses differ")
    for name in ("nx_stm32u575_tz_prepare", "nx_stm32u575_tz_launch",
                 "nx_arch_nonsecure_check", "nx_user_io_broker_gpio"):
        if not 0x0c000000 <= secure.symbol(name) & ~1 < 0x0c0fe000:
            raise EvidenceError("trusted implementation missing from Secure Flash")
    return {"gateway_address": gateway, "nsc_bytes": nsc["size"],
            "ns_vector_count": 142, "ns_exception_handlers": 0,
            "secure_stack_bytes": stacks[0], "nonsecure_stack_bytes": stacks[1],
            "float_abi": "soft", "principal": "one fixed NS world"}


def imported_gateway(raw):
    matches = re.findall(r"^nx_reference_gpio\s+[TtAa]\s+([0-9a-fA-F]+)(?:\s|$)",
                         raw, re.MULTILINE)
    if len(matches) != 1:
        raise EvidenceError("one actual gateway import symbol required")
    return int(matches[0], 16) & ~1


def source_inputs(root):
    """Bind the complete maintained graph and fixture, including new files."""
    paths = {Path(item["path"]) for item in
             provider_assemblies.source_inputs(root, [])}
    paths.add(Path(__file__).resolve())
    for directory in (root / "tests/contracts/u575_trustzone",
                      root / "vendors/st/cmsis_device_u5/Include",
                      root / "vendors/arm/CMSIS_5/CMSIS/Core/Include"):
        paths.update(path for path in directory.rglob("*") if path.is_file()
                     and "__pycache__" not in path.parts)
    return [arch_compile.digest(path) for path in sorted(paths)]


def compile_profile(build, stage, compiler):
    """Check effective CPU/security flags in each real compiler invocation."""
    path = build / "compile_commands.json"
    commands = json.loads(path.read_text())
    if not isinstance(commands, list) or not commands:
        raise EvidenceError("actual paired compile database required")
    for row in commands:
        argv = row.get("arguments") or shlex.split(row["command"])
        cpu = [arg for arg in argv if arg.startswith("-mcpu=")]
        abi = [arg for arg in argv if arg.startswith("-mfloat-abi=")]
        if (Path(argv[0]).resolve() != Path(compiler).resolve()
                or cpu != ["-mcpu=cortex-m33"] or abi != ["-mfloat-abi=soft"]
                or "-mthumb" not in argv
                or any(arg.startswith("-mfpu=") for arg in argv)
                or ("-mcmse" in argv) != (stage == "secure")):
            raise EvidenceError("actual compiler CPU/security ABI differs")
    return arch_compile.digest(path)


def run(root, output, compiler="arm-none-eabi-gcc", parallel=2):
    root = root.resolve()
    output = provider_assemblies.fresh_output(output, root)
    report = {"schema_version": 1, "kind": "u575_trustzone_software_pair",
              "status": "failed", "physical_status": "not_executed",
              "candidate_sealing": "not_established_for_multi_image_pair"}
    recorder = provider_assemblies.Recorder(output / "commands", root)
    report["commands"] = recorder.commands
    try:
        if root != ROOT.resolve():
            raise EvidenceError("selected root differs from actual gate source")
        if any(name.startswith("GIT_") for name in os.environ):
            raise EvidenceError("Git environment overrides cannot identify source")
        if type(parallel) is not int or not 1 <= parallel <= 4:
            raise EvidenceError("bounded build parallelism required")
        source = git_source(root)
        fixture = root / "tests/contracts/u575_trustzone"
        inputs = source_inputs(root)
        report["source"], report["source_inputs"] = source, inputs
        tools = arch_compile.tools(compiler)
        for name in ("cmake", "ninja"):
            tools[name] = shutil.which(name)
            if not tools[name]:
                raise EvidenceError("missing actual build tool: " + name)
        tools["python"] = sys.executable
        report["tools"] = {name: arch_compile.digest(Path(path).resolve())
                           for name, path in tools.items()}
        recorder.env["PATH"] = str(Path(tools["gcc"]).parent) + os.pathsep + recorder.env["PATH"]
        for name, tool in tools.items():
            recorder.execute([tool, "--version"], name + "-version")
        report["build_inputs"] = []
        for stage in ("secure", "nonsecure"):
            build = output / stage
            argv = [tools["cmake"], "-S", str(fixture), "-B", str(build), "-G", "Ninja",
                    "-DCMAKE_TOOLCHAIN_FILE=" + str(root / "cmake/toolchains/arm-gcc.cmake"),
                    "-DCMAKE_C_COMPILER=" + tools["gcc"],
                    "-DCMAKE_MAKE_PROGRAM=" + tools["ninja"],
                    "-DPython3_EXECUTABLE=" + tools["python"],
                    "-DNEXUS_TRUSTZONE_STAGE=" + stage]
            if stage == "nonsecure":
                argv.append("-DNEXUS_SECURE_IMPORT_LIBRARY=" + str(
                    output / "secure/secure-import.o"))
            recorder.execute(argv, stage + "-configure")
            recorder.execute([tools["cmake"], "--build", str(build), "--parallel",
                              str(parallel)], stage + "-build")
            dependencies = recorder.execute([tools["ninja"], "-C", str(build),
                "-t", "deps"], stage + "-dependencies")
            report["build_inputs"].append({"stage": stage,
                "compile_commands": compile_profile(build, stage, tools["gcc"]),
                "compiler_dependencies": [arch_compile.digest(path) for path in
                    provider_assemblies.dependency_paths(dependencies, build)]})
        secure_path = output / "secure/u575_secure.elf"
        nonsecure_path = output / "nonsecure/u575_nonsecure.elf"
        import_path = output / "secure/secure-import.o"
        raw = recorder.execute([tools["nm"], "--defined-only", "-P",
                                str(import_path)], "import-library")
        report["checks"] = validate_pair(
            secure_path.read_bytes(), nonsecure_path.read_bytes(),
            imported_gateway(raw))
        secure = Elf32(secure_path.read_bytes(), float_abi="soft")
        nonsecure = Elf32(nonsecure_path.read_bytes(), float_abi="soft")
        cross_call = recorder.execute([tools["objdump"], "-d", "-z",
            "--disassemble=__gnu_cmse_nonsecure_call", str(secure_path)],
            "actual-cmse-call")
        decoded = decode_function(secure, "__gnu_cmse_nonsecure_call",
                                  cross_call.encode("ascii"))
        if not any(item["mnemonic"] == "blxns"
                   for item in decoded["instructions"]):
            raise EvidenceError("real CMSE NonSecure call instruction missing")
        report["cross_call"] = decoded
        forbidden = {"msr", "cpsid", "cpsie", "svc", "mcr", "vmsr"}
        inspections = []
        for symbol in nonsecure.defined_symbols:
            if symbol["type"] != 2 or symbol["size"] == 0:
                continue
            # GNU CMSE import symbols are absolute gateways, not local NS code.
            # validate_pair already binds this sole import to actual Secure SG.
            if (symbol["name"] == "nx_reference_gpio"
                    and symbol["section"] == 0xfff1):
                continue
            name = symbol["name"]
            raw = recorder.execute([tools["objdump"], "-d", "-z",
                                    "--disassemble=" + name, str(nonsecure_path)],
                                   "ns-function-" + str(len(inspections)))
            decoded = decode_function(nonsecure, name, raw.encode("ascii"))
            if any(item["mnemonic"].split(".")[0] in forbidden
                   for item in decoded["instructions"]):
                raise EvidenceError("NS image contains privileged exception control")
            inspections.append(decoded)
        if not inspections:
            raise EvidenceError("NonSecure code inspection cannot be empty")
        report["nonsecure_functions"] = inspections
        report["artifacts"] = {"secure": file_identity(secure_path),
            "nonsecure": file_identity(nonsecure_path),
            "secure_import": file_identity(import_path),
            "secure_map": file_identity(output / "secure/u575_secure.map"),
            "nonsecure_map": file_identity(output / "nonsecure/u575_nonsecure.map")}
        if source_inputs(root) != inputs:
            raise EvidenceError("maintained graph input snapshot changed")
        for stage in report["build_inputs"]:
            provider_assemblies.verify_inputs(stage["compiler_dependencies"])
            provider_assemblies.verify_inputs([stage["compile_commands"]])
        provider_assemblies.verify_inputs(list(report["tools"].values()))
        if git_source(root) != source:
            raise EvidenceError("source identity changed during paired build")
        report["status"] = "passed"
    except (OSError, ValueError, KeyError, TypeError) as error:
        report["failure"] = str(error)
    atomic_json(output / "report.json", report)
    return 0 if report["status"] == "passed" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    parser.add_argument("--parallel", type=int, default=2)
    args = parser.parse_args()
    try:
        return run(args.root, args.output, args.compiler, args.parallel)
    except (OSError, ValueError) as error:
        print("U575 pair rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
