"""Verify the exact optional STM32F103RB boot and offset app from Nexus sources."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

VECTOR = 0x08004200
VECTOR_BYTES = (16 + 43 + 8) * 4
BOOT_BASE, BOOT_END = 0x08000000, 0x08004000
IMAGE_END = 0x08013C00
STACK_MIN, STACK_END = 0x20000800, 0x20005000
FIXTURE = "tests/integration/assemblies/stm32f1_handoff"
TAIL = "nx_stm32f103rb_handoff_tail"
sys.dont_write_bytecode = True


def section(elf, name):
    return next(record for record in elf.sections if record["name"] == name)


def linked(path, address, end, elf_type):
    payload = path.read_bytes()
    elf = elf_type(payload, float_abi="soft")
    vectors = section(elf, ".isr_vector")
    if (vectors["address"], vectors["size"]) != (address, VECTOR_BYTES):
        raise ValueError("Linked vector differs from exact F103RB geometry")
    stack, pc = elf.unpack("<II", vectors["offset"])
    if (not STACK_MIN <= stack <= STACK_END or stack % 8 or
            pc != elf.entry or pc != elf.symbol("Reset_Handler") or
            elf.symbol("g_pfnVectors") != address):
        raise ValueError("Linked MSP or Thumb reset symbol is inconsistent")
    for load in elf.loads:
        if load["files"] and not address <= load["physical"] < load["physical"] + load["files"] <= end:
            raise ValueError("ELF load bytes extend outside the selected image")
    text = section(elf, ".text")
    begin, finish = text["address"], (text["address"] + text["size"]) & ~1
    if ((text["flags"] & 7) != 6 or begin % 2 or
            not address + VECTOR_BYTES <= begin <= (pc & ~1) < finish <= end):
        raise ValueError("Linked reset halfword lacks a reviewed executable section")
    digest = hashlib.sha256(payload).hexdigest()
    resources = json.loads(path.with_suffix(".resources.json").read_text())
    if (resources["status"], resources["part"], resources["elf"]["sha256"],
            resources["physical_status"]) != ("passed", "STM32F103RBT6", digest, "not_executed"):
        raise ValueError("ELF resource evidence differs from the exact linked image")
    return elf, {"image": str(path), "sha256": digest,
                 "begin": address, "end": end, "initial_msp": stack, "thumb_pc": pc,
                 "executable_begin": begin, "executable_end": finish,
                 "resources": resources["metrics"]}


def disassemble(image, symbol):
    return subprocess.check_output(["arm-none-eabi-objdump", "-d",
                                    "--disassemble=" + symbol, str(image)], text=True)


def audit_tail(boot, boot_path, app_path):
    tail = disassemble(boot_path, TAIL)
    instructions = []
    pattern = r"^\s*[0-9a-f]+:\s+(?:[0-9a-f]{4}\s+){1,2}\s*(\S+)\s*(.*?)\s*(?:@.*)?$"
    for line in tail.splitlines():
        match = re.match(pattern, line)
        if match and match[1] != ".word":
            instructions.append(" ".join((match[1] + " " + match[2]).lower().split()))
    literal = re.search(r"@ \(([0-9a-f]+) ", tail)
    text = section(boot, ".text")
    if literal is None or boot.unpack("<I", text["offset"] + int(literal[1], 16) - text["address"]) != (0xE000ED08,):
        raise ValueError("The linked tail does not select Cortex-M3 VTOR")
    expected = ["str r0, [r3, #0]", "dsb sy", "isb sy", "movs r0, #0",
                "msr psp, r0", "isb sy", "mvns r3, r0", "mov lr, r3",
                "msr msp, r1", "bx r2"]
    if (len(instructions) != len(expected) + 1 or not instructions[0].startswith("ldr r3, [pc,") or
            instructions[1:] != expected):
        raise ValueError("The linked handoff tail differs from the masked stack transition")
    enter = disassemble(boot_path, "nx_stm32f103rb_handoff_enter")
    startup = disassemble(app_path, "Reset_Handler")
    app_main = disassemble(app_path, "main")
    if ("<" + TAIL + ">" not in enter or
            not startup.index("<SystemInit>") < startup.index("<__libc_init_array>") < startup.index("<main>") or
            not app_main.index("<nx_arch_irq_restore>") < app_main.index("<nx_platform_start>")):
        raise ValueError("Linked reset and unmask order differs from the reference")
    return {"instructions": instructions, "vtor_register": 0xE000ED08,
            "app_initializes_before_unmask": True}


def sdk_identity(root):
    script = root / "cmake/package/package_source_sdk.py"
    value = json.loads(subprocess.check_output([sys.executable, "-B", str(script),
                                                "--verify", str(root)], text=True))
    if not value["publishable"]:
        raise ValueError("Use a verified publishable Source SDK")
    return value


def build(role, root, package, output, *, revision=None):
    folder = output / role
    command = ["cmake", "-S", str(root / FIXTURE), "-B", str(folder), "-G", "Ninja",
               "-DNexus_DIR=" + str(package), "-DNEXUS_HANDOFF_ROLE=" + role,
               "-DNEXUS_HANDOFF_CONTRACT_DIR=" + str(output),
               "-DCMAKE_TOOLCHAIN_FILE=" + str(root / "cmake/toolchains/arm-gcc.cmake")]
    if revision is not None:
        command.append("-DNEXUS_EXPECTED_SOURCE_REVISION=" + revision)
    subprocess.run(command, check=True)
    target = "nexus_f103rb_handoff_" + role
    subprocess.run(["cmake", "--build", str(folder), "--parallel", "1", "--target", target], check=True)
    return folder / "bin" / (target + ".elf")


def verify(root, package, output, *, identity=None):
    sys.path.insert(0, str(root))
    from tools.measurement.elf import Elf32
    revision = identity["source_revision"] if identity else None
    app_path = build("app", root, package, output, revision=revision)
    app, app_record = linked(app_path, VECTOR, IMAGE_END, Elf32)
    built_objects = (output / "app").rglob("*.obj")
    if (TAIL in app.symbols or any(path.name in {"handoff.c.obj", "handoff_tail.S.obj"}
                                    for path in built_objects)):
        raise ValueError("The unselected handoff entered the offset app build")
    contract = ("/* Derived from the linked app executable bounds. */\n"
                f'#define NX_TARGET_EXEC_BEGIN 0x{app_record["executable_begin"]:08x}U\n'
                f'#define NX_TARGET_EXEC_END 0x{app_record["executable_end"]:08x}U\n')
    (output / "target_contract.h").write_text(contract)
    boot_path = build("boot", root, package, output, revision=revision)
    boot, boot_record = linked(boot_path, BOOT_BASE, BOOT_END, Elf32)
    audited = audit_tail(boot, boot_path, app_path)
    if identity is not None and sdk_identity(root) != identity:
        raise ValueError("The installed source identity changed during verification")
    result = {"schema_version": 1, "kind": "stm32f103rb_handoff_pair", "status": "passed",
              "physical_status": "not_executed", "source_revision": revision,
              "boot": boot_record, "app": app_record, "tail": audited,
              "app_handoff_compiled": False}
    (output / "pair.json").write_text(json.dumps(result, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--source-root", type=Path)
    source.add_argument("--prefix", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if args.prefix:
        prefix = args.prefix.resolve()
        root, package = prefix / "share/nexus/src", prefix / "lib/cmake/Nexus"
        identity = sdk_identity(root)
        if output.is_relative_to(prefix):
            parser.error("The verification output must be outside the installed SDK")
    else:
        root = args.source_root.resolve()
        package, identity = root / "cmake", None
    if output.exists() or output.is_relative_to(root):
        parser.error("Use a fresh output directory outside Nexus sources")
    output.mkdir(parents=True)
    verify(root, package, output, identity=identity)
    print(output / "pair.json")


if __name__ == "__main__":
    main()
