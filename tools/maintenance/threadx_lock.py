#!/usr/bin/env python3
"""Verify the exact reviewed ThreadX source graph before kernel composition.

The lock is the source-list authority. Verification never downloads, rewrites
source, repairs a lock or treats a checkout/SDK identity as runtime qualification.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure.metadata import fields, parse_json, snapshot  # noqa: E402

ORIGIN = "https://github.com/eclipse-threadx/threadx.git"
PORTS = frozenset({"linux", "cortex_m4"})
# These official M4 helpers are not composed by the reviewed GNU port: Nexus
# owns bootstrap/exception entry and uses the selected interrupt-control path.
M4_UNUSED = frozenset({"tx_misra.S", "tx_thread_interrupt_disable.S",
                       "tx_thread_interrupt_restore.S"})
HEX40 = re.compile(r"[0-9a-f]{40}")
HEX64 = re.compile(r"[0-9a-f]{64}")


def relative(name):
    if not isinstance(name, str):
        raise ValueError("ThreadX source path must be a string")
    path = PurePosixPath(name)
    if (not name or "\\" in name or path.is_absolute() or ".." in path.parts or
            path.as_posix() != name or name == "."):
        raise ValueError("Unsafe ThreadX source path")
    return path


def directory(source, name):
    path = source / relative(name)
    if any(p.is_symlink() for p in (path, *path.parents)) or not path.is_dir():
        raise ValueError("ThreadX source directory must be regular")
    return path


def file_bytes(source, name):
    relative(name)
    return snapshot(source / name)[0]


def git(source, *arguments):
    environment = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    result = subprocess.run(["git", "-C", str(source), *arguments],
                            capture_output=True, text=True, env=environment,
                            timeout=20)
    if result.returncode:
        raise ValueError("Cannot verify ThreadX Git identity")
    return result.stdout.strip()


def path_list(value, prefix, suffix, count):
    if not isinstance(value, list) or len(value) != count:
        raise ValueError("Reviewed ThreadX source/header count changed")
    for name in value:
        relative(name)
        if (not name.startswith(prefix + "/") or
                PurePosixPath(name).parent.as_posix() != prefix or
                not name.endswith(suffix)):
            raise ValueError("ThreadX path belongs to an unsupported source family")
    if len(set(value)) != len(value):
        raise ValueError("Duplicate ThreadX source path")
    return set(value)


def contract(lock):
    fields(lock, {"schema_version", "kind", "repository", "commit", "tag",
                  "common", "ports", "headers", "files", "license",
                  "definitions", "bootstrap_replacement"}, "ThreadX lock")
    if (type(lock["schema_version"]) is not int or lock["schema_version"] != 1 or
            lock["kind"] != "nexus-threadx-source-lock"):
        raise ValueError("Unsupported ThreadX lock schema")
    if (lock["repository"] != ORIGIN or not isinstance(lock["commit"], str) or
            not HEX40.fullmatch(lock["commit"]) or
            not isinstance(lock["tag"], str) or
            not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]{6}_rel", lock["tag"])):
        raise ValueError("Exact official ThreadX origin, commit and release required")
    common = path_list(lock["common"], "common/src", ".c", 185)
    fields(lock["ports"], PORTS, "ThreadX ports")
    linux = path_list(lock["ports"]["linux"], "ports/linux/gnu/src", ".c", 8)
    m4 = path_list(lock["ports"]["cortex_m4"], "ports/cortex_m4/gnu/src", ".S", 7)
    if not isinstance(lock["headers"], list) or len(lock["headers"]) != 14:
        raise ValueError("Reviewed ThreadX header closure changed")
    headers = set(lock["headers"])
    if len(headers) != 14:
        raise ValueError("Duplicate ThreadX header")
    for name in headers:
        relative(name)
        if (PurePosixPath(name).parent.as_posix() not in
                {"common/inc", "ports/linux/gnu/inc", "ports/cortex_m4/gnu/inc"} or
                not name.endswith(".h")):
            raise ValueError("Unsupported ThreadX header family")
    selected = common | linux | m4 | headers
    if not isinstance(lock["files"], dict) or set(lock["files"]) != selected:
        raise ValueError("ThreadX identities must close the entire source graph")
    for record in lock["files"].values():
        fields(record, {"bytes", "sha256"}, "ThreadX file identity")
        if (type(record["bytes"]) is not int or record["bytes"] <= 0 or
                not isinstance(record["sha256"], str) or
                not HEX64.fullmatch(record["sha256"])):
            raise ValueError("Nonempty exact ThreadX file identity required")
    notice = lock["license"]
    fields(notice, {"path", "expression", "bytes", "sha256"}, "ThreadX license")
    if (notice["path"] != "LICENSE.txt" or notice["expression"] != "MIT" or
            type(notice["bytes"]) is not int or notice["bytes"] <= 0 or
            not isinstance(notice["sha256"], str) or
            not HEX64.fullmatch(notice["sha256"])):
        raise ValueError("Reviewed original ThreadX MIT notice identity required")
    fields(lock["definitions"], {"all", "linux", "cortex_m4_basepri"},
           "ThreadX definitions")
    for values in lock["definitions"].values():
        if (not isinstance(values, list) or not values or
                any(not isinstance(v, str) or not v or len(v) > 256 for v in values) or
                len(set(values)) != len(values)):
            raise ValueError("Explicit ThreadX compile definitions required")
    fields(lock["bootstrap_replacement"], PORTS, "ThreadX bootstrap ownership")
    if any(not isinstance(v, str) or not v or len(v) > 512
           for v in lock["bootstrap_replacement"].values()):
        raise ValueError("Reviewed ThreadX bootstrap owner required")
    return selected


def verify(source, lock_path, port, *, packaged_commit=None):
    """Verify all supported profiles, not just the port selected for this build.

    A development checkout must match the actual clean HEAD, origin and release
    tag. An installed SDK caller supplies its independently checked dependency
    manifest commit; lack of Git is never accepted by itself. Every official
    source/header and original license is hashed in both delivery forms.
    """
    if not isinstance(port, str) or port not in PORTS:
        raise ValueError("Unsupported ThreadX port")
    source = Path(source).absolute()
    if source.is_symlink() or source.resolve() != source or not source.is_dir():
        raise ValueError("ThreadX source requires a canonical regular directory")
    raw, lock_identity = snapshot(lock_path)
    lock = parse_json(raw)
    selected = contract(lock)
    for prefix, expected, unused in (
            ("common/src", set(lock["common"]), frozenset()),
            ("common/inc", {x for x in lock["headers"] if x.startswith("common/")}, frozenset()),
            ("ports/linux/gnu/src", set(lock["ports"]["linux"]), frozenset()),
            ("ports/linux/gnu/inc", {"ports/linux/gnu/inc/tx_port.h"}, frozenset()),
            ("ports/cortex_m4/gnu/src", set(lock["ports"]["cortex_m4"]), M4_UNUSED),
            ("ports/cortex_m4/gnu/inc", {"ports/cortex_m4/gnu/inc/tx_port.h"}, frozenset())):
        actual = {prefix + "/" + p.name for p in directory(source, prefix).iterdir()
                  if p.name not in unused}
        if actual != expected:
            raise ValueError("ThreadX source/header directory closure changed: " + prefix)
    if packaged_commit is None:
        if (not (source / ".git").exists() or
                Path(git(source, "rev-parse", "--show-toplevel")) != source or
                git(source, "rev-parse", "HEAD") != lock["commit"] or
                git(source, "remote", "get-url", "origin") != ORIGIN or
                git(source, "rev-parse", lock["tag"] + "^{commit}") != lock["commit"]):
            raise ValueError("ThreadX checkout differs from exact official locked revision")
    elif packaged_commit != lock["commit"]:
        raise ValueError("Packaged ThreadX dependency commit differs from source lock")
    for name, record in [*lock["files"].items(),
                         (lock["license"]["path"], lock["license"])]:
        data = file_bytes(source, name)
        if len(data) != record["bytes"] or hashlib.sha256(data).hexdigest() != record["sha256"]:
            raise ValueError("ThreadX source/license identity changed: " + name)
    if snapshot(lock_path)[1] != lock_identity:
        raise ValueError("ThreadX lock changed during verification")
    if packaged_commit is None and (
            git(source, "rev-parse", "HEAD") != lock["commit"] or
            git(source, "status", "--porcelain", "--untracked-files=normal")):
        raise ValueError("ThreadX checkout changed or is not clean")
    return {"kind": "nexus-threadx-source-verification", "commit": lock["commit"],
            "port": port, "verified_files": len(selected) + 1,
            "license": lock["license"]["expression"],
            "source_kind": "installed_source_sdk" if packaged_commit is not None else "git_checkout",
            "software_qualified": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--lock", type=Path, required=True)
    parser.add_argument("--port", required=True)
    args = parser.parse_args(argv)
    try:
        result = verify(args.source, args.lock, args.port)
    except (OSError, ValueError, TypeError, KeyError, subprocess.SubprocessError) as error:
        print("ThreadX source rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
