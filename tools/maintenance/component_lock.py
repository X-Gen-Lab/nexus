"""One source admission path for optional, catalogue-owned components.

Gitlinks choose revisions; data locks close selected source and notice bytes.
Source-SDK callers must first verify the owning package manifest. This helper
does not establish release, behavioral or hardware qualification.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path, PurePosixPath
import re
import sys

if __name__ == "__main__":
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.configure.metadata import fields, parse_json, snapshot
from tools.dev.dependencies import verify_selected_checkout
from tools.maintenance.source_catalogue import ALL_SOURCE_DEPENDENCIES, dependency

IDENTIFIERS = frozenset(item.identifier for item in ALL_SOURCE_DEPENDENCIES
                        if item.component_source)


def contract(lock, identifier):
    """Require canonical catalogue placement, exact identity and source closure."""
    if identifier not in IDENTIFIERS:
        raise ValueError("Unknown optional component source")
    item = dependency(identifier)
    fields(lock, {"schema_version", "kind", "identifier", "path", "upstream_url",
                  "fetch_url", "commit", "tree", "license", "files"},
           "Component source lock")
    if (type(lock["schema_version"]) is not int or lock["schema_version"] != 1 or
            lock["kind"] != "nexus-component-source" or
            lock["identifier"] != identifier or lock["path"] != item.path or
            lock["upstream_url"] != item.upstream_url or
            lock["fetch_url"] != item.fetch_url):
        raise ValueError("Component source catalogue differs")
    if any(not isinstance(lock[key], str) or not re.fullmatch(r"[0-9a-f]{40}", lock[key])
           for key in ("commit", "tree")):
        raise ValueError("Component source requires exact commit/tree")
    fields(lock["license"], {"path", "expression"}, "Component source notice")
    if (lock["license"]["path"] != item.notice or
            not isinstance(lock["license"]["expression"], str) or
            not lock["license"]["expression"]):
        raise ValueError("Component source requires its original notice")
    files = lock["files"]
    if not isinstance(files, dict) or not files or item.notice not in files:
        raise ValueError("Component source closure requires files and notice")
    for name, digest in files.items():
        if (not isinstance(name, str) or not name or "\\" in name or
                PurePosixPath(name).is_absolute() or
                any(part in ("", ".", "..") for part in name.split("/")) or
                any(ord(char) < 32 for char in name) or
                not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest)):
            raise ValueError("Invalid component source path or digest")
    return lock


def verify(root, identifier, *, source_sdk=False):
    """Verify stable source bytes and, for checkouts, their owning Gitlink."""
    root = Path(root).absolute()
    item = dependency(identifier)
    raw, lock_identity = snapshot(root / item.source_lock)
    lock = contract(parse_json(raw), identifier)
    source = root / item.path
    if source.resolve() != source or not source.is_dir():
        raise ValueError("Component source requires canonical directories")
    if source_sdk:
        marker = source / ".git"
        if marker.exists() or marker.is_symlink():
            raise ValueError("Packaged component source cannot contain Git state")
    else:
        verify_selected_checkout(root, source, identifier, lock["commit"], lock["tree"],
                                 parent_index=True)
    observed = {}
    for name, digest in lock["files"].items():
        path = source / name
        _, identity = snapshot(path)
        if identity["sha256"] != digest:
            raise ValueError("Component source bytes differ: " + name)
        observed[path] = identity
    for path, identity in observed.items():
        if snapshot(path)[1] != identity:
            raise ValueError("Component source changed during observation")
    if snapshot(root / item.source_lock)[1] != lock_identity:
        raise ValueError("Component source lock changed during observation")
    if not source_sdk:
        verify_selected_checkout(root, source, identifier, lock["commit"], lock["tree"],
                                 parent_index=True)
    return {"path": str(source), "commit": lock["commit"], "tree": lock["tree"],
            "configure_inputs": [str(root / item.source_lock),
                                 str(root / "tools/maintenance/source_catalogue.py"),
                                 str(Path(__file__).absolute()),
                                 *[str(path) for path in observed]]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--dependency", required=True, choices=sorted(IDENTIFIERS))
    parser.add_argument("--source-sdk", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = verify(args.root, args.dependency, source_sdk=args.source_sdk)
    except (ValueError, OSError) as error:
        print("Component source rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
