#!/usr/bin/env python3
"""Verify fixed reviewed official CMSIS-device source and original notices.

Only the selected part's five original upstream files belong to the component
source closure. A clean full Git checkout establishes its official provenance;
an installed source SDK requires independent full-manifest verification instead.
Neither source proof qualifies a driver, a firmware image or a physical Board.
"""
from __future__ import annotations
import argparse
import hashlib
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
    fields, parse_json, regular, snapshot,
)

REVIEWED_DEVICES = {
    "STM32H743ZIT6": {
        "origin": "https://github.com/STMicroelectronics/cmsis-device-h7.git",
        "commit": "8f922cdc7cc6de2344e75ddd657889f4ff761790",
        "directory": "vendors/st/cmsis_device_h7",
        "lock_name": "dependencies/stm32h7.lock.json",
        "files": frozenset({
            "Include/stm32h743xx.h", "Include/stm32h7xx.h",
            "Include/system_stm32h7xx.h",
            "Source/Templates/gcc/startup_stm32h743xx.s", "LICENSE.md",
        }),
    },
    "STM32U575ZIT6Q": {
        "origin": "https://github.com/STMicroelectronics/cmsis-device-u5.git",
        "commit": "624374fa1e21ca195d6f2102ac0caaa50d0ea4c8",
        "directory": "vendors/st/cmsis_device_u5",
        "lock_name": "dependencies/stm32u5.lock.json",
        "files": frozenset({
            "Include/stm32u575xx.h", "Include/stm32u5xx.h",
            "Include/system_stm32u5xx.h",
            "Source/Templates/gcc/startup_stm32u575xx.s", "LICENSE.md",
        }),
    },
}


def contract(lock):
    """Return the exact original source closure after validating all fields."""
    fields(lock, {"schema_version", "kind", "repository", "commit",
                  "selected_part", "files", "license"},
           "CMSIS-device source lock")
    if (type(lock["schema_version"]) is not int or
            lock["schema_version"] != 1 or
            lock["kind"] != "nexus-reviewed-device-source"):
        raise ValueError("Unsupported CMSIS-device source lock schema")
    part = lock["selected_part"]
    if not isinstance(part, str) or part not in REVIEWED_DEVICES:
        raise ValueError("Exact reviewed CMSIS-device part required")
    review = REVIEWED_DEVICES[part]
    if (lock["repository"] != review["origin"] or
            lock["commit"] != review["commit"]):
        raise ValueError("Reviewed official origin, part and commit required")
    if (not isinstance(lock["files"], dict) or
            set(lock["files"]) != review["files"]):
        raise ValueError("Identities must close the reviewed device graph")
    for record in lock["files"].values():
        fields(record, {"bytes", "sha256"}, "CMSIS-device file identity")
        if (type(record["bytes"]) is not int or record["bytes"] <= 0 or
                not isinstance(record["sha256"], str) or
                not re.fullmatch(r"[0-9a-f]{64}", record["sha256"])):
            raise ValueError("Nonempty exact device file identity required")
    fields(lock["license"], {"path", "expression"}, "CMSIS-device license")
    if (lock["license"]["path"] != "LICENSE.md" or
            lock["license"]["expression"] != "Apache-2.0"):
        raise ValueError("Original reviewed Apache-2.0 notice required")
    return set(lock["files"])


def git(source, *arguments):
    """Read Git identity without inheriting caller-selected Git directories."""
    environment = {k: v for k, v in os.environ.items()
                   if not k.startswith("GIT_")}
    result = subprocess.run(["git", "-C", str(source), *arguments],
                            capture_output=True, text=True, env=environment,
                            timeout=20)
    if result.returncode:
        raise ValueError("Cannot verify CMSIS-device Git identity")
    return result.stdout.strip()


def stable_snapshot(path):
    """Bind bytes to an unchanged regular file, including same-hash rewrites.

    Content identity alone does not detect replacement with identical bytes.
    Retain filesystem identity and modification/change times around each read
    and compare them again before returning the completed verification result.
    """
    path = regular(path)
    if path.resolve() != path:
        raise ValueError("CMSIS-device inputs require canonical file paths")

    def identity():
        value = regular(path).stat()
        return (value.st_dev, value.st_ino, value.st_size,
                value.st_mtime_ns, value.st_ctime_ns)

    before = identity()
    raw, recorded = snapshot(path)
    after = identity()
    if before != after or len(raw) != after[2]:
        raise ValueError("CMSIS-device input changed during read: " + str(path))
    return raw, {**recorded, "filesystem": after}


def verify_git(source, lock, review):
    """Check the full checkout before and after reading the selected closure."""
    marker = source / ".git"
    if (marker.is_symlink() or not marker.exists() or
            Path(git(source, "rev-parse", "--show-toplevel")) != source or
            git(source, "rev-parse", "HEAD") != lock["commit"] or
            git(source, "remote", "get-url", "origin") != review["origin"] or
            git(source, "status", "--porcelain", "--untracked-files=all")):
        raise ValueError("Checkout differs from clean official locked revision")


def verify(source, lock_path, *, packaged_commit=None):
    """Require an exact official checkout or independently verified full SDK.

    Packaged callers must already verify the complete installed SDK manifest and
    supply its exact dependency commit. Missing Git alone never authorizes a
    byte-only fallback. This result does not imply software or HIL approval.
    """
    source = Path(source).absolute()
    if source.resolve() != source or source.is_symlink() or not source.is_dir():
        raise ValueError("CMSIS-device source requires a canonical directory")
    raw, lock_identity = stable_snapshot(lock_path)
    lock = parse_json(raw)
    selected = contract(lock)
    review = REVIEWED_DEVICES[lock["selected_part"]]
    if packaged_commit is None:
        verify_git(source, lock, review)
    elif (packaged_commit != lock["commit"] or (source / ".git").exists() or
          (source / ".git").is_symlink()):
        raise ValueError("Packaged source needs exact SDK commit without Git")
    identities = {}
    for name in sorted(selected):
        data, actual = stable_snapshot(source / name)
        record = lock["files"][name]
        if (len(data) != record["bytes"] or
                hashlib.sha256(data).hexdigest() != record["sha256"]):
            raise ValueError("Device source/license identity changed: " + name)
        identities[name] = actual
    if packaged_commit is None:
        verify_git(source, lock, review)
    if stable_snapshot(lock_path)[1] != lock_identity or any(
            stable_snapshot(source / name)[1] != identity
            for name, identity in identities.items()):
        raise ValueError("CMSIS-device source/lock changed during verification")
    return {"kind": "nexus-cmsis-device-source-verification",
            "commit": lock["commit"], "selected_part": lock["selected_part"],
            "verified_files": len(selected),
            "license": lock["license"]["expression"],
            "source_kind": "installed_source_sdk" if packaged_commit is not None
            else "git_checkout", "software_qualified": False}


def verify_sdk(root):
    """Delegate installed payload proof to the complete SDK verifier."""
    sys.path.insert(0, str(ROOT / "cmake/package"))
    import package_source_sdk
    return package_source_sdk.verify(root)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--lock", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        source = args.source.absolute()
        lock_path = args.lock.absolute()
        raw, lock_identity = stable_snapshot(lock_path)
        lock = parse_json(raw)
        contract(lock)
        review = REVIEWED_DEVICES[lock["selected_part"]]
        sdk = source.parents[2]
        packaged_commit = None
        identity_path = sdk / ".nexus-source-sdk.json"
        sdk_identity = None
        if identity_path.exists() or identity_path.is_symlink():
            if (source != sdk / review["directory"] or
                    lock_path != sdk / review["lock_name"]):
                raise ValueError("Device needs exact SDK source/lock paths")
            _, sdk_identity = stable_snapshot(identity_path)
            manifest = verify_sdk(sdk)
            records = [r for r in manifest["dependencies"]
                       if r["path"] == review["directory"]]
            if len(records) != 1:
                raise ValueError("SDK lacks the exact device dependency")
            packaged_commit = records[0]["commit"]
        result = verify(source, lock_path, packaged_commit=packaged_commit)
        if stable_snapshot(lock_path)[1] != lock_identity or (
                sdk_identity is not None and
                stable_snapshot(identity_path)[1] != sdk_identity):
            raise ValueError("Device lock or SDK manifest changed during proof")
    except (OSError, ValueError, TypeError, KeyError, IndexError, ImportError,
            subprocess.SubprocessError) as error:
        print("CMSIS-device source rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
