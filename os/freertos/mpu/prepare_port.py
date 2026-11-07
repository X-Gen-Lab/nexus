#!/usr/bin/env python3
"""Apply reviewed MPU port patches after checking exact upstream identities.

All firmware behavior is authored in explicit C/ASM diffs. Host Python selects
reviewed inputs, validates identities and publishes the derived build sources.
"""
import argparse
import hashlib
import importlib.util
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    "nexus_reviewed_patch", Path(__file__).resolve().parents[2] / "reviewed_patch.py")
PATCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCH)
PATCH_ROOT = Path(__file__).with_name("patches")

HASHES = {
    "ARM_CM3_MPU": "db5c79a7e3a28be646fab96729ac6620d5154392c6dd2e9d0debabe8690911e7",
    "ARM_CM4_MPU": "6df67233a5a97a6539c205e73b59507b697142afffed426d2efd0111a7fc9b20",
}
ORIGINAL = "portMPU_REGION_ATTRIBUTE_REG = ( portMPU_REGION_READ_WRITE | portMPU_REGION_EXECUTE_NEVER ) |"

PORT_V8_HASH = "0896f3232a773b553d8eb57a6fed8838a0c555386fdbb28bef791161339b6c65"
HASHES.update({name: PORT_V8_HASH for name in (
    "ARM_CM23_NTZ/non_secure", "ARM_CM33_NTZ/non_secure",
    "ARM_CM55_NTZ/non_secure", "ARM_CM85_NTZ/non_secure")})
ASSEMBLY_HASHES = {
    "ARM_CM23_NTZ/non_secure": "54a6bcb679672ae11faa06cf96a5a0b018da1ce0fe8446665ab95b1d11823b15",
    **{name: "524ffdb5144274bd0636431b787a933057e748373e5bcdce217d5a166e2ebbe9" for name in (
        "ARM_CM33_NTZ/non_secure", "ARM_CM55_NTZ/non_secure", "ARM_CM85_NTZ/non_secure")},
}


VENEER_HASHES = {
    "ARM_CM3_MPU": "58830afa90dc33943e5cd0a5b49aec398a733a7ffaf1c910e623d155466ba449",
    "ARM_CM4_MPU": "58830afa90dc33943e5cd0a5b49aec398a733a7ffaf1c910e623d155466ba449",
    "ARM_CM23_NTZ/non_secure": "2ce9bcd94fb291f35d9ccd5e037ae405a7f68b73fe1054a511c54d4153e5f3b1",
    "ARM_CM33_NTZ/non_secure": "620a82d247820b4f0ab8b158eb1411b29e21b308155d42176b9ffaaf4f95fd93",
    "ARM_CM55_NTZ/non_secure": "620a82d247820b4f0ab8b158eb1411b29e21b308155d42176b9ffaaf4f95fd93",
    "ARM_CM85_NTZ/non_secure": "620a82d247820b4f0ab8b158eb1411b29e21b308155d42176b9ffaaf4f95fd93",
}


NUMBERS_HASH = "6701d2eac55e478e90d62ff2947b0d2c7f5157f3b344bcfec47ac80186c57816"


def port_name(source: Path) -> str:
    """Identify only the exact maintained upstream port location."""
    name = source.parent.name
    return source.parent.parent.name + "/non_secure" if name == "non_secure" else name


def checked_text(source: Path, hashes: dict) -> str:
    """Reject unknown upstream bytes before touching any output."""
    content = source.read_bytes()
    if hashlib.sha256(content).hexdigest() != hashes.get(port_name(source)):
        raise ValueError("unreviewed or changed pinned MPU source")
    return content.decode("utf-8")


def unique_replace(text: str, original: str, replacement: str) -> str:
    """Retain the model harness's exact ISA-boundary substitution helper."""
    if text.count(original) != 1:
        raise ValueError("reviewed MPU transformation site changed or duplicated")
    return text.replace(original, replacement)


def publish(output: Path, text: str) -> None:
    """Only complete, validated transformations reach the build output."""
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(output)


def prepare(source: Path, output: Path) -> None:
    """Keep exact grants and the real port's single-use bootstrap lease."""
    text = checked_text(source, HASHES)
    name = port_name(source)
    patch = (name.lower() + "-port.patch" if name in
             {"ARM_CM3_MPU", "ARM_CM4_MPU"} else "v8-port.patch")
    publish(output, PATCH.apply_reviewed_patch(text, PATCH_ROOT / patch))


def prepare_assembly(source: Path, output: Path) -> None:
    """Preserve context assembly and attach special-SVC LR/origin explicitly."""
    text = checked_text(source, ASSEMBLY_HASHES)
    patch = ("v8-baseline-assembly.patch" if port_name(source) ==
             "ARM_CM23_NTZ/non_secure" else "v8-mainline-assembly.patch")
    publish(output, PATCH.apply_reviewed_patch(text, PATCH_ROOT / patch))


def prepare_numbers(source: Path, output: Path) -> None:
    """Keep exactly the seventy pinned syscall numbers."""
    content = source.read_bytes()
    if hashlib.sha256(content).hexdigest() != NUMBERS_HASH:
        raise ValueError("unreviewed or changed pinned syscall numbers")
    text = content.decode("utf-8")
    publish(output, text)


def prepare_veneers(source: Path, output: Path) -> None:
    """Retain the two pinned SVC bodies with no unreviewed service exports."""
    text = checked_text(source, VENEER_HASHES)
    name = port_name(source)
    profile = ("v7" if name in {"ARM_CM3_MPU", "ARM_CM4_MPU"} else
               "v8-baseline" if name == "ARM_CM23_NTZ/non_secure" else
               "v8-mainline")
    text = PATCH.apply_reviewed_patch(text, PATCH_ROOT / (profile + "-veneers.patch"))
    publish(output, text)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--veneers", action="store_true")
    modes.add_argument("--assembly", action="store_true")
    modes.add_argument("--numbers", action="store_true")
    arguments = parser.parse_args()
    try:
        (prepare_numbers if arguments.numbers else
         prepare_veneers if arguments.veneers else
         prepare_assembly if arguments.assembly else prepare)(
            arguments.source, arguments.output)
    except ValueError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
