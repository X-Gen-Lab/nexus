#!/usr/bin/env python3
"""Verify an imported vendor source subtree against its reviewed source lock."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import sys


class VendorSourceError(ValueError):
    """The SDK source no longer matches the locked import."""


def verify(root):
    root = Path(root).resolve()
    lock_path = root / "source.lock.json"
    if lock_path.is_symlink():
        raise VendorSourceError("The source lock must be a regular file")
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    if not isinstance(lock, dict):
        raise VendorSourceError("The source lock must be an object")
    entries = lock.get("files")
    if not isinstance(entries, list) or not entries:
        raise VendorSourceError("The source lock must contain imported files")
    expected = set()
    total = 0
    for entry in entries:
        if not isinstance(entry, dict):
            raise VendorSourceError("Imported source entries must be objects")
        name = entry.get("path", "")
        if not isinstance(name, str):
            raise VendorSourceError("Imported source paths must be strings")
        rel = PurePosixPath(name)
        if ("\\" in name or rel.is_absolute()
                or ".." in rel.parts or len(rel.parts) < 2
                or rel.parts[0] != "Firmware" or str(rel) != name):
            raise VendorSourceError("Unsafe imported source path")
        if name in expected:
            raise VendorSourceError("Duplicate imported source path")
        expected.add(name)
        source = root / rel
        if any((root / Path(*rel.parts[:i])).is_symlink()
               for i in range(1, len(rel.parts) + 1)):
            raise VendorSourceError("Imported source cannot follow symlinks")
        if not source.is_file():
            raise VendorSourceError(f"Imported source is missing: {name}")
        digest = entry.get("sha256", "")
        if not isinstance(digest, str) or len(digest) != 64:
            raise VendorSourceError("Invalid imported source digest")
        data = source.read_bytes()
        if len(data) != entry.get("bytes") or hashlib.sha256(data).hexdigest() != digest:
            raise VendorSourceError(f"Imported source changed: {name}")
        if not isinstance(entry.get("license"), str) or not entry["license"]:
            raise VendorSourceError("Imported source requires a license identity")
        total += len(data)
    actual = {p.relative_to(root).as_posix()
              for p in (root / "Firmware").rglob("*") if p.is_file() or p.is_symlink()}
    if actual != expected:
        raise VendorSourceError("Firmware contains files outside the reviewed import")
    return {"vendor": lock.get("vendor"), "package": lock.get("package"),
            "version": lock.get("version"), "files": len(expected), "bytes": total}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        result = verify(args.root)
    except (VendorSourceError, OSError, ValueError, TypeError) as exc:
        print(f"Vendor source verification failed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
