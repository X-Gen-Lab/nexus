#!/usr/bin/env python3
"""Verified portable toolchain/OCI tree export; recipe claims stay explicit."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import EvidenceError, atomic_json, fields, load_json, separate_outputs
from tools.evidence import store
from tools.evidence.reproduce import tree_identity


def _identity(root: Path, kind: str) -> dict:
    if kind == "toolchain":
        return tree_identity(root)
    if kind == "oci":
        from tools.evidence.container import verify
        return verify(root / "environment.json", runtime=False)
    raise EvidenceError("portable environment kind must be toolchain or oci")


def export(source: Path, storage: Path, *, kind: str, expected: dict | None = None) -> dict:
    identity = _identity(source, kind)
    if expected is not None and identity != expected:
        raise EvidenceError("environment differs from expected qualified identity")
    package = store.export_tree(source, storage)
    if _identity(source, kind) != identity:
        raise EvidenceError("environment changed during export")
    return {"schema_version": 1, "kind": "portable_build_environment",
            "environment_kind": kind, "identity": identity, "package": package,
            "recipe_reproducible": False,
            "scope": "observed exported bytes; no rebuilt-image identity claim"}


def restore(envelope: dict, destination: Path, *,
            archive_override: Path | None = None) -> dict:
    fields(envelope, {"schema_version", "kind", "environment_kind", "identity", "package",
                      "recipe_reproducible", "scope"})
    if envelope["schema_version"] != 1 or envelope["kind"] != "portable_build_environment":
        raise EvidenceError("unsupported environment envelope")
    destination = destination.absolute()
    if destination.exists() or destination.is_symlink():
        raise EvidenceError("environment destination must be new")
    destination.parent.mkdir(parents=True, exist_ok=True)
    scratch = Path(tempfile.mkdtemp(prefix=".environment-", dir=destination.parent))
    try:
        restored = scratch / "tree"
        store.restore_tree(envelope["package"], restored, archive_override=archive_override)
        identity = _identity(restored, envelope["environment_kind"])
        if identity != envelope["identity"]:
            raise EvidenceError("restored environment differs from expected identity")
        restored.replace(destination)
        return {"status": "verified", "identity": identity,
                "environment_kind": envelope["environment_kind"],
                "destination": str(destination), "recipe_reproducible": False}
    finally:
        shutil.rmtree(scratch)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("export", "restore"))
    parser.add_argument("--kind", choices=("toolchain", "oci"))
    parser.add_argument("--source", type=Path)
    parser.add_argument("--store", type=Path)
    parser.add_argument("--envelope", type=Path)
    parser.add_argument("--destination", type=Path)
    parser.add_argument("--archive", type=Path,
                        help="Explicit transported archive; retain original expected digest")
    parser.add_argument("--expected", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        inputs = [path for path in (args.envelope, args.expected) if path]
        trees = [path for path in (args.source, args.destination) if path]
        envelope = None
        if args.operation == "restore" and args.envelope is not None:
            envelope = load_json(args.envelope)
            package = envelope["package"]
            identity = package["archive"]
            fields(identity, {"path", "sha256", "size"})
            effective_archive = args.archive or Path(identity["path"])
            inputs.append(effective_archive)
        elif args.operation == "export" and args.store is not None:
            # Content-addressed archive names are not yet known. Every report
            # stays outside storage before any export mutation is attempted.
            trees.append(args.store)
        separate_outputs([args.report], inputs, trees)
        if args.archive and args.operation != "restore":
            parser.error("--archive is only for restore")
        if args.operation == "export":
            if None in (args.kind, args.source, args.store):
                parser.error("export requires --kind --source --store")
            result = export(args.source, args.store, kind=args.kind,
                            expected=load_json(args.expected) if args.expected else None)
        else:
            if args.envelope is None or args.destination is None:
                parser.error("restore requires --envelope --destination")
            result = restore(envelope, args.destination,
                             archive_override=args.archive)
        atomic_json(args.report, result)
        return 0
    except (EvidenceError, OSError, ValueError, KeyError, TypeError) as error:
        print("Environment transport rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
