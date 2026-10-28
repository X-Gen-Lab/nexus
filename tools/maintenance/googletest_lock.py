#!/usr/bin/env python3
"""Admit one pinned, clean GoogleTest submodule for offline host tests only."""
from __future__ import annotations
import argparse
import configparser
import json
import os
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure.metadata import (  # noqa: E402
    VERSION, fields, parse_json, regular, snapshot,
)
from tools.maintenance import source_catalogue  # noqa: E402

ORIGIN = "https://github.com/google/googletest.git"
DIRECTORY = "ext/googletest"
LOCK_NAME = "dependencies/googletest.lock.json"
SCOPE = "host GoogleTest/GoogleMock targets only; no configure-time network"
PREPARE = "python tools/dev/dev.py prepare --scope native --allow-network"


def contract(lock):
    """Validate reviewed source placement and a complete, exact Git identity."""
    fields(lock, {"schema", "name", "version", "commit", "path", "url",
                  "license", "scope"}, "GoogleTest source lock")
    if (type(lock["schema"]) is not int or lock["schema"] != 1 or
            lock["name"] != "googletest" or lock["path"] != DIRECTORY or
            lock["url"] != ORIGIN or lock["license"] != "BSD-3-Clause" or
            lock["scope"] != SCOPE):
        raise ValueError("Exact host-only GoogleTest source lock required")
    if (not isinstance(lock["commit"], str) or
            not re.fullmatch(r"[0-9a-f]{40}", lock["commit"]) or
            not isinstance(lock["version"], str) or
            not VERSION.fullmatch(lock["version"])):
        raise ValueError("Exact GoogleTest commit and version required")
    return lock


def git(source, *arguments, parent_index=False):
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("GIT_")}
    if parent_index and "GIT_INDEX_FILE" in os.environ:
        environment["GIT_INDEX_FILE"] = os.environ["GIT_INDEX_FILE"]
    result = subprocess.run(["git", "-C", str(source), *arguments],
        capture_output=True, text=True, env=environment, timeout=20)
    if result.returncode:
        raise ValueError("Cannot verify GoogleTest Git identity: " +
                         result.stderr.strip())
    return result.stdout.strip()


def verify(source, lock_path):
    """Check the active index, checkout, origin and source version offline.

    The index admits a reviewed staged dependency update during ordinary commit
    hooks. Published provenance independently binds the committed parent tree.
    Git must start at the dependency's own marker: an empty submodule must never
    inherit the parent repository's HEAD or clean status.
    """
    source, lock_path = Path(source).absolute(), Path(lock_path).absolute()
    root = lock_path.parents[1]
    if (source != root / DIRECTORY or lock_path != root / LOCK_NAME or
            any(path.is_symlink() for path in (source, *source.parents))):
        raise ValueError("GoogleTest sources require exact regular directories")
    raw, lock_identity = snapshot(lock_path)
    lock = contract(parse_json(raw))
    approved_fetch = source_catalogue.dependency("googletest").fetch_url
    marker = source / ".git"
    if not source.is_dir() or marker.is_symlink() or not marker.exists():
        raise ValueError("GoogleTest submodule is not initialized; run " + PREPARE)
    if Path(git(source, "rev-parse", "--show-toplevel")) != source:
        raise ValueError("GoogleTest requires its own initialized Git checkout")
    if Path(git(root, "rev-parse", "--show-toplevel")) != root:
        raise ValueError("GoogleTest requires the owning Nexus Git checkout")
    modules_raw, modules_identity = snapshot(root / ".gitmodules")
    parser = configparser.ConfigParser(interpolation=None)
    try:
        parser.read_string(modules_raw.decode("utf-8"))
        section = parser['submodule "' + DIRECTORY + '"']
        if (parser.defaults() or dict(section) !=
                {"path": DIRECTORY, "url": approved_fetch}):
            raise ValueError("GoogleTest submodule origin declaration differs")
    except (configparser.Error, UnicodeError, KeyError) as error:
        raise ValueError("Invalid GoogleTest submodule origin declaration") from error
    record = git(root, "ls-files", "--stage", "--", DIRECTORY, parent_index=True)
    expected = "160000 " + lock["commit"] + " 0\t" + DIRECTORY
    if record != expected:
        raise ValueError("GoogleTest parent index gitlink differs from source lock")
    if git(source, "rev-parse", "HEAD") != lock["commit"]:
        raise ValueError("GoogleTest checkout commit differs from source lock")
    if git(source, "config", "--get-all", "remote.origin.url") != approved_fetch:
        raise ValueError("GoogleTest checkout origin differs from approved fetch URL")
    if git(source, "status", "--porcelain", "--untracked-files=all"):
        raise ValueError("GoogleTest checkout is dirty")
    cmake = regular(source / "CMakeLists.txt").read_text(encoding="utf-8")
    versions = re.findall(r"^set\(GOOGLETEST_VERSION ([0-9.]+)\)$", cmake,
                          flags=re.MULTILINE)
    if versions != [lock["version"]]:
        raise ValueError("GoogleTest source version differs from source lock")
    regular(source / "LICENSE")
    if (snapshot(lock_path)[1] != lock_identity or
            snapshot(root / ".gitmodules")[1] != modules_identity or
            git(root, "ls-files", "--stage", "--", DIRECTORY,
                parent_index=True) != expected or
            git(source, "rev-parse", "HEAD") != lock["commit"] or
            git(source, "config", "--get-all", "remote.origin.url") != approved_fetch or
            git(source, "status", "--porcelain", "--untracked-files=all")):
        raise ValueError("GoogleTest source identity changed during verification")
    git_directory = Path(git(source, "rev-parse", "--absolute-git-dir"))
    inputs = [git_directory / "HEAD", git_directory / "index"]
    reference = git(source, "rev-parse", "--symbolic-full-name", "HEAD")
    if reference != "HEAD":
        reference_path = Path(git(source, "rev-parse", "--git-path", reference))
        if not reference_path.is_absolute():
            reference_path = source / reference_path
        if reference_path.is_file():
            inputs.append(reference_path)
        if (git_directory / "packed-refs").is_file():
            inputs.append(git_directory / "packed-refs")
    return {"commit": lock["commit"], "version": lock["version"],
            "source_kind": "git_submodule", "license": lock["license"],
            "configure_inputs": [str(path) for path in inputs]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--lock", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        result = verify(args.source, args.lock)
    except (OSError, ValueError, TypeError, IndexError,
            subprocess.SubprocessError) as error:
        print("GoogleTest source rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
