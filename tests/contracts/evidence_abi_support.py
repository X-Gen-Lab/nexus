"""Actual linked M3 soft and M4F hard/softfp ELF fixtures for consumer gates."""

import os
import re
import shutil
import subprocess

from tools.evidence.common import atomic_json, digest, load_json
from tools.evidence.identity import canonical_digest
from tools.measurement.elf import Elf32, binary_from_elf
from tests.contracts.test_evidence_support import link_fixture

DEFAULT_CPU = object()


def link_abi_fixture(root, declared="hard", *, linked=None, cpu=DEFAULT_CPU,
                     extra_source=""):
    """Build actual ARM bytes, including intentional ABI metadata mismatches."""
    root.mkdir(parents=True, exist_ok=True)
    elf, resolved_path, budget = link_fixture(root)
    resolved = load_json(resolved_path)
    producer = linked or declared
    if producer not in {"hard", "soft", "softfp"}:
        raise ValueError("Unsupported linked fixture float ABI")
    if extra_source or producer in {"hard", "softfp"}:
        source = root / "image.c"
        source.write_text(source.read_text() + extra_source +
                          ("\nfloat nx_abi_add(float a, float b) { return a + b; }\n"
                           if producer in {"hard", "softfp"} else ""))
        resolved["inputs"][0]["sha256"] = digest(source)
    if cpu is DEFAULT_CPU:
        resolved["cpu"] = {"core": "cortex-m3" if declared == "soft" else "cortex-m4",
                           "float_abi": declared}
    elif cpu != "absent":
        resolved["cpu"] = cpu
    resolved.pop("configuration_sha256")
    resolved["configuration_sha256"] = canonical_digest(resolved)
    atomic_json(resolved_path, resolved)
    script = root / "memory.ld"
    text = script.read_text()
    for index in range(8):
        word = resolved["configuration_sha256"][index * 8:index * 8 + 8]
        text = re.sub(rf"__nx_resolved_sha256_{index} = 0x[0-9a-f]+;",
                      f"__nx_resolved_sha256_{index} = 0x{word};", text)
    script.write_text(text)
    flags = (["-mcpu=cortex-m3", "-mfloat-abi=soft"] if producer == "soft"
             else ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16",
                   "-mfloat-abi=" + producer])
    compiler = os.environ.get("NEXUS_ARM_GCC") or shutil.which("arm-none-eabi-gcc")
    subprocess.run([compiler, *flags, "-mthumb", "-nostdlib", "-Os",
        "-Wl,-Map=" + str(root / "image.map"), "-T", str(script),
        str(root / "image.c"), "-o", str(elf)], capture_output=True, check=True)
    (root / "image.bin").write_bytes(binary_from_elf(
        Elf32(elf.read_bytes(), float_abi="hard" if producer == "hard" else "soft"),
        0x08000000, 0x08100000))
    return elf, resolved_path, budget


def build_layout(root, declared="hard", **kwargs):
    """Put a real linked fixture in the production build artifact layout."""
    elf, resolved, budget = link_abi_fixture(root, declared, **kwargs)
    generated, binary = root / "generated", root / "bin"
    generated.mkdir()
    binary.mkdir()
    for path in (resolved, root / "input_paths.json"):
        path.rename(generated / path.name)
    budget.rename(generated / "resource-budget.json")
    for extension in ("elf", "bin", "map"):
        (root / ("image." + extension)).rename(binary / ("nexus_firmware." + extension))
    atomic_json(root / "compile_commands.json", [{"fixture": "actual ARM link"}])
    return binary / "nexus_firmware.elf", generated / "resolved.json", generated / "resource-budget.json"
