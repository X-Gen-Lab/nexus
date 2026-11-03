#!/usr/bin/env python3
"""Verify the exact optional lwIP source without claiming release."""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

if __name__ == "__main__":
    sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure.metadata import (  # noqa: E402
    fields, parse_json, snapshot,
)

COMMIT = "1f53f34b4aa7c6583540c0f4c35142fa66616c78"
TREE = "7e65d6e65cefeffa2be44be0bb0ee51136626f1d"
UPSTREAM = "https://github.com/lwip-tcpip/lwip.git"
FETCH = "https://github.com/X-Gen-Lab/lwip.git"
LOCK = ROOT / "dependencies/lwip.lock.json"
REVIEWED_FILE_MAP_SHA256 = (
    "2491e60fddb9b34a04d211e6bb6f75b80c188c0b6ca6c3f6090fb5f0f0756e2b"
)


def contract(lock):
    """Validate the reviewed version and one immutable source-byte closure."""
    fields(lock, {"schema", "name", "version", "commit", "tree",
                  "upstream_url", "fetch_url", "path", "license",
                  "admission", "files"}, "lwIP source lock")
    if (type(lock["schema"]) is not int or lock["schema"] != 1 or
            lock["name"] != "lwip" or lock["version"] != "2.2.1" or
            lock["commit"] != COMMIT or lock["tree"] != TREE or
            lock["upstream_url"] != UPSTREAM or lock["fetch_url"] != FETCH or
            lock["path"] != "ext/lwip" or lock["license"] != "BSD-3-Clause" or
            lock["admission"] != "organization_gitlink"):
        raise ValueError("Exact reviewed lwIP source lock required")
    files = lock["files"]
    if not isinstance(files, dict) or "COPYING" not in files:
        raise ValueError("lwIP source files and license identity required")
    for name, digest in files.items():
        if (not isinstance(name, str) or not name or
                PurePosixPath(name).is_absolute() or "\\" in name or
                any(piece in ("", ".", "..") for piece in name.split("/")) or
                not isinstance(digest, str) or
                re.fullmatch(r"[0-9a-f]{64}", digest) is None):
            raise ValueError("Invalid lwIP source file identity")
    mapping = json.dumps(files, sort_keys=True, separators=(",", ":")).encode()
    if hashlib.sha256(mapping).hexdigest() != REVIEWED_FILE_MAP_SHA256:
        raise ValueError("lwIP reviewed source closure differs")
    return lock


def git(source, *arguments, parent_index=False):
    """Read this checkout's own Git state, independent of parent hook state."""
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("GIT_")}
    if parent_index and "GIT_INDEX_FILE" in os.environ:
        environment["GIT_INDEX_FILE"] = os.environ["GIT_INDEX_FILE"]
    result = subprocess.run(
        ["git", "-c", "core.fsmonitor=false", "-C", str(source), *arguments],
        capture_output=True, text=True, env=environment, timeout=20,
        check=False)
    if result.returncode:
        raise ValueError("Cannot verify lwIP Git identity: " +
                         result.stderr.strip())
    return result.stdout.strip()


def source_identity(path):
    """Read one regular dependency file through the common snapshot boundary."""
    return snapshot(path)[1]


def verify(source, lock_path=LOCK, *, development=False, source_sdk=False):
    """Check source bytes, and checkout/index identity where Git exists.

    Source-SDK callers first execute the owning SDK manifest verifier. Its
    explicit scope checks the exact payload closure without inventing Git state.
    Development input selects a clean reviewed checkout but never gains
    the production organization-gitlink admission result.
    """
    source = Path(source).absolute()
    lock_path = Path(lock_path).absolute()
    raw, identity = snapshot(lock_path)
    lock = contract(parse_json(raw))
    if development and source_sdk:
        raise ValueError("lwIP development and source SDK scopes are exclusive")
    if (not source.is_dir() or
            any(path.is_symlink() for path in (source, *source.parents))):
        raise ValueError("lwIP source requires regular explicit directories")
    root = lock_path.parents[1]
    if not development and (source != root / "ext/lwip" or
                            lock_path != root / "dependencies/lwip.lock.json"):
        raise ValueError("lwIP production source requires canonical placement")
    modules_identity = None
    if not development and not source_sdk:
        modules_raw, modules_identity = snapshot(root / ".gitmodules")
        parser = configparser.ConfigParser(interpolation=None)
        try:
            parser.read_string(modules_raw.decode("utf-8"))
            if (parser.defaults() or
                    dict(parser['submodule "ext/lwip"']) !=
                    {"path": "ext/lwip", "url": FETCH}):
                raise ValueError("lwIP organization fork declaration differs")
        except (configparser.Error, UnicodeError, KeyError) as error:
            raise ValueError(
                "Invalid lwIP organization fork declaration") from error
    if not source_sdk:
        marker = source / ".git"
        if not marker.exists() or marker.is_symlink():
            raise ValueError("lwIP input requires its own Git checkout")

    def observe():
        if source_sdk:
            return
        if Path(git(source, "rev-parse", "--show-toplevel")) != source:
            raise ValueError("lwIP input inherited another checkout identity")
        if (git(source, "rev-parse", "HEAD") != COMMIT or
                git(source, "rev-parse", "HEAD^{tree}") != TREE):
            raise ValueError("lwIP checkout commit/tree differs")
        origins = (UPSTREAM, FETCH) if development else (FETCH,)
        origin = git(source, "config", "--get-all", "remote.origin.url")
        if origin not in origins:
            raise ValueError("lwIP organization fork origin differs")
        if git(source, "status", "--porcelain", "--untracked-files=all"):
            raise ValueError("lwIP checkout is dirty")
        if not development:
            if Path(git(root, "rev-parse", "--show-toplevel")) != root:
                raise ValueError("lwIP requires its owning Nexus checkout")
            expected = "160000 " + COMMIT + " 0\text/lwip"
            if git(root, "ls-files", "--stage", "--", "ext/lwip",
                   parent_index=True) != expected:
                raise ValueError("lwIP parent index gitlink differs")

    observe()
    observed_files = {}
    for name, expected in lock["files"].items():
        target = source / name
        for parent in (target, *target.relative_to(source).parents):
            checked = parent if parent.is_absolute() else source / parent
            if checked.is_symlink():
                raise ValueError("lwIP source requires regular files")
        file_identity = source_identity(target)
        if file_identity["sha256"] != expected:
            raise ValueError("lwIP source bytes differ: " + name)
        observed_files[target] = file_identity
    if snapshot(lock_path)[1] != identity:
        raise ValueError("lwIP lock changed during observation")
    for target, before in observed_files.items():
        if source_identity(target) != before:
            raise ValueError("lwIP source changed during observation: " +
                             str(target))
    if modules_identity is not None and (
            snapshot(root / ".gitmodules")[1] != modules_identity):
        raise ValueError("lwIP organization declaration changed during "
                         "observation")
    observe()
    return {"name": "lwip", "version": "2.2.1", "commit": COMMIT,
            "tree": TREE, "license": "BSD-3-Clause",
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
            raise ValueError("lwIP development and source SDK scopes are "
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
                raise ValueError("SDK lacks lwIP dependency identities")
            selected = [row for row in records if isinstance(row, dict) and
                        row.get("path") == "ext/lwip"]
            if (len(selected) != 1 or selected[0].get("commit") != COMMIT or
                    selected[0].get("tree") != TREE):
                raise ValueError("SDK lacks exact lwIP commit/tree identity")
        result = verify(args.source, args.lock, development=args.development,
                        source_sdk=args.source_sdk)
        if snapshot(args.lock)[1] != lock_identity or (
                manifest_identity is not None and
                snapshot(manifest_path)[1] != manifest_identity):
            raise ValueError("lwIP lock or SDK manifest changed during proof")
    except (OSError, ValueError, TypeError, KeyError, ImportError,
            subprocess.SubprocessError) as error:
        print("lwIP source rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
