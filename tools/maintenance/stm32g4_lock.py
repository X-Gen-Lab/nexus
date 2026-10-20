#!/usr/bin/env python3
"""Verify the reviewed G474 CMSIS/startup source and original Apache notice.

The lock owns byte identities and the exact official commit. Only the reviewed
part's fixed minimal closure may be selected. Other files in the official
repository are not implicitly admitted to the platform or its source SDK.
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
from tools.configure.metadata import fields, parse_json, snapshot  # noqa: E402

ORIGIN = "https://github.com/STMicroelectronics/cmsis-device-g4.git"
PART = "STM32G474RET6"
REVIEWED_FILES = frozenset({
    "Include/stm32g474xx.h", "Include/stm32g4xx.h",
    "Include/system_stm32g4xx.h", "Source/Templates/gcc/startup_stm32g474xx.s",
    "LICENSE.md",
})


def contract(lock):
    fields(lock, {"schema_version", "kind", "repository", "commit",
                  "selected_part", "files", "license"}, "G4 source lock")
    if (type(lock["schema_version"]) is not int or lock["schema_version"] != 1 or
            lock["kind"] != "nexus-reviewed-device-source"):
        raise ValueError("Unsupported G4 source lock schema")
    if (lock["repository"] != ORIGIN or lock["selected_part"] != PART or
            not isinstance(lock["commit"], str) or
            not re.fullmatch(r"[0-9a-f]{40}", lock["commit"])):
        raise ValueError("Exact reviewed G4 official origin, part and commit required")
    if not isinstance(lock["files"], dict) or set(lock["files"]) != REVIEWED_FILES:
        raise ValueError("G4 identities must close the entire reviewed part graph")
    for record in lock["files"].values():
        fields(record, {"bytes", "sha256"}, "G4 file identity")
        if (type(record["bytes"]) is not int or record["bytes"] <= 0 or
                not isinstance(record["sha256"], str) or
                not re.fullmatch(r"[0-9a-f]{64}", record["sha256"])):
            raise ValueError("Nonempty exact G4 file identity required")
    fields(lock["license"], {"path", "expression"}, "G4 license")
    if (lock["license"]["path"] != "LICENSE.md" or
            lock["license"]["expression"] != "Apache-2.0"):
        raise ValueError("Reviewed original G4 Apache-2.0 notice required")
    return set(lock["files"])


def git(source, *arguments):
    environment = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    result = subprocess.run(["git", "-C", str(source), *arguments],
                            capture_output=True, text=True, env=environment,
                            timeout=20)
    if result.returncode:
        raise ValueError("Cannot verify G4 Git identity")
    return result.stdout.strip()


def verify(source, lock_path, *, packaged_commit=None):
    """Require a clean exact official checkout or independently verified SDK.

    Packaged callers must already verify the complete SDK manifest and supply
    its dependency commit. Missing Git never authorizes a byte-only fallback.
    Source verification is not runtime qualification or a physical Board test.
    """
    source = Path(source).absolute()
    if source.resolve() != source or source.is_symlink() or not source.is_dir():
        raise ValueError("G4 source requires a canonical regular directory")
    raw, lock_identity = snapshot(lock_path)
    lock = parse_json(raw)
    selected = contract(lock)
    if packaged_commit is None:
        if (not (source / ".git").exists() or
                Path(git(source, "rev-parse", "--show-toplevel")) != source or
                git(source, "rev-parse", "HEAD") != lock["commit"] or
                git(source, "remote", "get-url", "origin") != ORIGIN):
            raise ValueError("G4 checkout differs from exact official locked revision")
    elif packaged_commit != lock["commit"]:
        raise ValueError("Packaged G4 dependency commit differs from source lock")
    identities = {}
    for name in sorted(selected):
        data, actual = snapshot(source / name)
        record = lock["files"][name]
        if (len(data) != record["bytes"] or
                hashlib.sha256(data).hexdigest() != record["sha256"]):
            raise ValueError("G4 source/license identity changed: " + name)
        identities[name] = actual
    if packaged_commit is None and (
            git(source, "rev-parse", "HEAD") != lock["commit"] or
            git(source, "remote", "get-url", "origin") != ORIGIN or
            git(source, "status", "--porcelain", "--untracked-files=all")):
        raise ValueError("G4 checkout changed or is not clean")
    if snapshot(lock_path)[1] != lock_identity or any(
            snapshot(source / name)[1] != identity
            for name, identity in identities.items()):
        raise ValueError("G4 source/lock changed during verification")
    return {"kind": "nexus-g4-source-verification", "commit": lock["commit"],
            "selected_part": PART, "verified_files": len(selected),
            "license": lock["license"]["expression"],
            "source_kind": "installed_source_sdk" if packaged_commit is not None else "git_checkout",
            "software_qualified": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--lock", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        source = args.source.absolute()
        sdk = source.parents[2]
        packaged_commit = None
        identity_path = sdk / ".nexus-source-sdk.json"
        if identity_path.exists() or identity_path.is_symlink():
            if identity_path.is_symlink():
                raise ValueError("SDK manifest cannot follow symlinks")
            if (source != sdk / "vendors/st/cmsis_device_g4" or
                    args.lock.absolute() != sdk / "dependencies/stm32g4.lock.json"):
                raise ValueError("Installed G4 requires exact SDK source and lock paths")
            sys.path.insert(0, str(sdk / "cmake/package"))
            import package_source_sdk
            manifest = package_source_sdk.verify(sdk)
            records = [r for r in manifest["dependencies"]
                       if r["path"] == "vendors/st/cmsis_device_g4"]
            if len(records) != 1:
                raise ValueError("Installed SDK lacks exact G4 dependency")
            packaged_commit = records[0]["commit"]
        result = verify(source, args.lock, packaged_commit=packaged_commit)
    except (OSError, ValueError, TypeError, KeyError, IndexError,
            subprocess.SubprocessError) as error:
        print("G4 source rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
