#!/usr/bin/env python3
"""Verify the exact optional coreMQTT source without claiming release."""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

if __name__ == "__main__":
    sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.maintenance.lwip_lock import git  # noqa: E402
from tools.configure.metadata import (  # noqa: E402
    fields, parse_json, snapshot,
)

COMMIT = "f4445aa70cb211504bca2c9e4177cd74609f5fba"
TREE = "8e3605e081b031d617fa0501697cc5e4f7b0b828"
UPSTREAM = "https://github.com/FreeRTOS/coreMQTT.git"
FETCH = "https://github.com/X-Gen-Lab/coreMQTT.git"
LOCK = ROOT / "dependencies/coremqtt.lock.json"
REVIEWED_FILE_MAP_SHA256 = (
    "e7d89940619ff9e43647615f4c5f608d401221dec4ef1b8f51a568a88a3aa229"
)


def contract(lock):
    """Validate the reviewed version and one immutable source-byte closure."""
    fields(lock, {"schema", "name", "version", "commit", "tree", "upstream_commit",
                  "upstream_url", "fetch_url", "path", "license",
                  "admission", "files"}, "coreMQTT source lock")
    if (type(lock["schema"]) is not int or lock["schema"] != 1 or
            lock["name"] != "coremqtt" or lock["version"] != "2.3.1" or
            lock["commit"] != COMMIT or lock["tree"] != TREE or
            lock["upstream_commit"] != "2beef04725328923e05e576b884212d53ec97af7" or
            lock["upstream_url"] != UPSTREAM or lock["fetch_url"] != FETCH or
            lock["path"] != "ext/coremqtt" or lock["license"] != "MIT" or
            lock["admission"] != "organization_gitlink"):
        raise ValueError("Exact reviewed coreMQTT source lock required")
    files = lock["files"]
    if not isinstance(files, dict) or not {"LICENSE", "NEXUS-SOURCE-CURATION.md"}.issubset(files):
        raise ValueError("coreMQTT source files and license identity required")
    for name, digest in files.items():
        if (not isinstance(name, str) or not name or
                PurePosixPath(name).is_absolute() or "\\" in name or
                any(piece in ("", ".", "..") for piece in name.split("/")) or
                not isinstance(digest, str) or
                re.fullmatch(r"[0-9a-f]{64}", digest) is None):
            raise ValueError("Invalid coreMQTT source file identity")
    mapping = json.dumps(files, sort_keys=True, separators=(",", ":")).encode()
    if hashlib.sha256(mapping).hexdigest() != REVIEWED_FILE_MAP_SHA256:
        raise ValueError("coreMQTT reviewed source closure differs")
    return lock


def source_identity(path):
    """Read one regular dependency file through the common snapshot boundary."""
    return snapshot(path)[1]


def verify(source, lock_path=LOCK, *, development=False, source_sdk=False):
    """Check source bytes, and checkout/index identity where Git exists.

    Source-SDK callers first execute the owning SDK manifest verifier. Its
    explicit scope checks the exact payload closure without inventing Git state.
    Development input can select the official upstream checkout but never gains
    the production organization-gitlink admission result.
    """
    source = Path(source).absolute()
    lock_path = Path(lock_path).absolute()
    raw, identity = snapshot(lock_path)
    lock = contract(parse_json(raw))
    if development and source_sdk:
        raise ValueError("coreMQTT development and source SDK scopes are exclusive")
    if (not source.is_dir() or
            any(path.is_symlink() for path in (source, *source.parents))):
        raise ValueError("coreMQTT source requires regular explicit directories")
    root = lock_path.parents[1]
    if not development and (source != root / "ext/coremqtt" or
                            lock_path != root / "dependencies/coremqtt.lock.json"):
        raise ValueError("coreMQTT production source requires canonical placement")
    modules_identity = None
    if not development and not source_sdk:
        modules_raw, modules_identity = snapshot(root / ".gitmodules")
        parser = configparser.ConfigParser(interpolation=None)
        try:
            parser.read_string(modules_raw.decode("utf-8"))
            if (parser.defaults() or
                    dict(parser['submodule "ext/coremqtt"']) !=
                    {"path": "ext/coremqtt", "url": FETCH}):
                raise ValueError("coreMQTT organization fork declaration differs")
        except (configparser.Error, UnicodeError, KeyError) as error:
            raise ValueError(
                "Invalid coreMQTT organization fork declaration") from error
    if not source_sdk:
        marker = source / ".git"
        if not marker.exists() or marker.is_symlink():
            raise ValueError("coreMQTT input requires its own Git checkout")

    def observe():
        if source_sdk:
            return
        if Path(git(source, "rev-parse", "--show-toplevel")) != source:
            raise ValueError("coreMQTT input inherited another checkout identity")
        if (git(source, "rev-parse", "HEAD") != COMMIT or
                git(source, "rev-parse", "HEAD^{tree}") != TREE):
            raise ValueError("coreMQTT checkout commit/tree differs")
        origins = (UPSTREAM, FETCH) if development else (FETCH,)
        origin = git(source, "config", "--get-all", "remote.origin.url")
        if origin not in origins:
            raise ValueError("coreMQTT organization fork origin differs")
        if git(source, "status", "--porcelain", "--untracked-files=all"):
            raise ValueError("coreMQTT checkout is dirty")
        if not development:
            if Path(git(root, "rev-parse", "--show-toplevel")) != root:
                raise ValueError("coreMQTT requires its owning Nexus checkout")
            expected = "160000 " + COMMIT + " 0\text/coremqtt"
            if git(root, "ls-files", "--stage", "--", "ext/coremqtt",
                   parent_index=True) != expected:
                raise ValueError("coreMQTT parent index gitlink differs")

    observe()
    observed_files = {}
    for name, expected in lock["files"].items():
        target = source / name
        for parent in (target, *target.relative_to(source).parents):
            checked = parent if parent.is_absolute() else source / parent
            if checked.is_symlink():
                raise ValueError("coreMQTT source requires regular files")
        file_identity = source_identity(target)
        if file_identity["sha256"] != expected:
            raise ValueError("coreMQTT source bytes differ: " + name)
        observed_files[target] = file_identity
    if source_sdk:
        actual = {path.relative_to(source).as_posix()
                  for path in source.rglob("*") if path.is_file()}
        if actual != set(lock["files"]):
            raise ValueError("coreMQTT SDK payload differs from locked closure")
    if snapshot(lock_path)[1] != identity:
        raise ValueError("coreMQTT lock changed during observation")
    for target, before in observed_files.items():
        if source_identity(target) != before:
            raise ValueError("coreMQTT source changed during observation: " +
                             str(target))
    if modules_identity is not None and (
            snapshot(root / ".gitmodules")[1] != modules_identity):
        raise ValueError("coreMQTT organization declaration changed during "
                         "observation")
    observe()
    return {"name": "coremqtt", "version": "2.3.1", "commit": COMMIT,
            "tree": TREE, "license": "MIT",
            "verified_files": len(lock["files"]),
            "scope": ("explicit_development_only" if development else
                      "source_sdk_payload" if source_sdk else
                      "organization_gitlink"),
            "production_admitted": not development,
            "software_qualified": False,
            "configure_inputs": [str(lock_path), *map(str, observed_files)],
            "hardware_status": "not_executed"}


def verify_sdk(root):
    """Delegate complete installed-payload proof to its existing verifier."""
    sys.path.insert(0, str(ROOT / "cmake/package"))
    import package_source_sdk
    return package_source_sdk.verify(root)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--lock", type=Path, default=LOCK)
    parser.add_argument("--development", action="store_true")
    parser.add_argument("--source-sdk", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.development and args.source_sdk:
            raise ValueError("coreMQTT development and source SDK scopes are "
                             "exclusive")
        _, lock_identity = snapshot(args.lock)
        sdk_root = args.lock.absolute().parents[1]
        manifest_path = sdk_root / ".nexus-source-sdk.json"
        manifest_identity = None
        if args.source_sdk:
            _, manifest_identity = snapshot(manifest_path)
            manifest = verify_sdk(manifest_path.parent)
            records = manifest.get("dependencies")
            if not isinstance(records, list):
                raise ValueError("SDK lacks coreMQTT dependency identities")
            selected = [row for row in records if isinstance(row, dict) and
                        row.get("path") == "ext/coremqtt"]
            if (len(selected) != 1 or selected[0].get("commit") != COMMIT or
                    selected[0].get("tree") != TREE):
                raise ValueError("SDK lacks exact coreMQTT commit/tree identity")
        result = verify(args.source, args.lock, development=args.development,
                        source_sdk=args.source_sdk)
        if snapshot(args.lock)[1] != lock_identity or (
                manifest_identity is not None and
                snapshot(manifest_path)[1] != manifest_identity):
            raise ValueError("coreMQTT lock or SDK manifest changed during proof")
    except (OSError, ValueError, TypeError, KeyError, ImportError,
            subprocess.SubprocessError) as error:
        print("coreMQTT source rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
