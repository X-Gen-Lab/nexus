"""Derive the reviewed Linux port without modifying upstream files.

The selecting CMake target must compile/link a sem_clockwait capability probe.
This source derivation never claims runtime privilege, latency or clock-step HIL.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import os
from pathlib import Path
import tempfile

SOURCE_SHA256 = "760b2e8e341b7fcdd2b5984eb9724100c562ccd27b79a18673f9fe45dd93b1da"
SCHEDULER_SHA256 = "5846be75427239c2bbe51c26684bb739877e2d61b4c3eb7754e2fe3d75ac3400"
# Exact reviewed monotonic-only predecessor from this same pinned source.
# A sidecar's self-declared identity never authorizes an output replacement.
REVIEWED_PREDECESSORS = frozenset({
    "b4226dbaa4e6638c6d3715182e9633f5435bdf702dd63aead527d236775de22e",
    "9ce0f296784a02f9b351dc720f9c6f964317a5c064c81df98d810e7bf4e2a88f",
    "9356ce08abc258d8fac7285938594cd7c22cf74eb0cf5018720c44287e4a4710",
})
# The host tool applies reviewed diffs; all authored C lives in those patches.
PATCH_DIRECTORY = Path(__file__).resolve().parent / "patches"
DERIVED_IDENTITIES = {
    "lowlevel": "5875c5332424ea1eb3bc701671651a87258c1289538c4240704eb758b36e0388",
    "scheduler": "337f9b324632784bef1703decaa751b490bb1a8f6bb51d9e524c9fe540f14bb0",
}
_patch_spec = importlib.util.spec_from_file_location(
    "nexus_os_reviewed_patch", Path(__file__).resolve().parents[1] /
    "reviewed_patch.py")
_patch_module = importlib.util.module_from_spec(_patch_spec)
_patch_spec.loader.exec_module(_patch_module)
apply_reviewed_patch = _patch_module.apply_reviewed_patch


def regular(path, *, output=False):
    path = Path(path).absolute()
    if ".." in path.parts or any(part.is_symlink() for part in (path, *path.parents)):
        raise ValueError("Canonical regular paths without symlinks required")
    if output and not path.exists():
        if not path.parent.is_dir():
            raise ValueError("Derived output parent must already exist")
    elif not path.is_file():
        raise ValueError("Regular input/output file required")
    return path


def derivation(kind):
    if kind == "lowlevel":
        return SOURCE_SHA256, REVIEWED_PREDECESSORS
    if kind == "scheduler":
        return SCHEDULER_SHA256, frozenset()
    raise ValueError("Unreviewed Linux derivation kind")


def derive(raw, *, kind="lowlevel"):
    source_sha, _ = derivation(kind)
    if hashlib.sha256(raw).hexdigest() != source_sha:
        raise ValueError("Unreviewed Linux ThreadX source identity")
    result = apply_reviewed_patch(
        raw.decode("utf-8"), PATCH_DIRECTORY / f"linux-{kind}.patch")
    derived = result.encode("utf-8")
    if hashlib.sha256(derived).hexdigest() != DERIVED_IDENTITIES[kind]:
        raise ValueError("Linux port derivation is incomplete or unreviewed")
    return derived


def prepare(source, output, *, kind="lowlevel"):
    _, predecessors = derivation(kind)
    source, output = regular(source), regular(output, output=True)
    before = source.stat()
    raw = source.read_bytes()
    derived = derive(raw, kind=kind)
    after = regular(source).stat()
    if (before.st_dev, before.st_ino) != (after.st_dev, after.st_ino) or source.read_bytes() != raw:
        raise ValueError("Reviewed source changed during derivation")
    if output.exists():
        existing = regular(output).read_bytes()
        if existing == derived:
            return
        if hashlib.sha256(existing).hexdigest() not in predecessors:
            raise ValueError("Reserved derived output has different contents")
        identity = output.stat()
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
                temporary = Path(stream.name)
                stream.write(derived)
                stream.flush()
                os.fsync(stream.fileno())
            current = regular(output).stat()
            if ((identity.st_dev, identity.st_ino, identity.st_mtime_ns) !=
                    (current.st_dev, current.st_ino, current.st_mtime_ns)
                    or output.read_bytes() != existing):
                raise ValueError("Reserved derived output changed during upgrade")
            os.replace(temporary, output)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
        return
    with output.open("xb") as stream:
        stream.write(derived)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--kind", choices=("lowlevel", "scheduler"), default="lowlevel")
    args = parser.parse_args(argv)
    try:
        prepare(args.source, args.output, kind=args.kind)
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(1, str(error) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
