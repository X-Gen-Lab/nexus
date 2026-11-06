"""Apply reviewed integer-only M7 patches without changing pinned sources."""

import argparse
import hashlib
import importlib.util
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    "nexus_reviewed_patch", Path(__file__).resolve().parents[1] / "reviewed_patch.py")
PATCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCH)


SOURCE_IDENTITIES = {
    "port.c": "037f78c536c8cf8dff0cc46b5a0f11e997fdbed1828ad6072a18d4848d7a9f3b",
    "portmacro.h": "8cd1066faa43ef966ec0e971dabdd82b5f2f00bd38ea86de84969dd7833072f7",
}


def prepare(kernel_root: Path, output: Path) -> None:
    """Validate every input, then publish the explicit errata 837070 overlay."""
    sources = {}
    for name, expected in SOURCE_IDENTITIES.items():
        raw = (kernel_root / "portable/GCC/ARM_CM3" / name).read_bytes()
        if hashlib.sha256(raw).hexdigest() != expected:
            raise ValueError(f"Unreviewed integer kernel source identity: {name}")
        patch = Path(__file__).with_name("patches") / ("m7-integer-" + name + ".patch")
        sources[name] = PATCH.apply_reviewed_patch(raw.decode("utf-8"), patch)
    output.mkdir(parents=True, exist_ok=True)
    for name, content in sources.items():
        temporary = output / (name + ".tmp")
        temporary.write_text(content, encoding="utf-8", newline="\n")
        temporary.replace(output / name)


def main() -> None:
    """CMake supplies the immutable kernel root and private build output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        prepare(arguments.kernel_root, arguments.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
