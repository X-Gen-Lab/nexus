#!/usr/bin/env python3
"""Verify STM32F4 LL and ETH definitions with their official CMSIS pairing.

STM32CubeF4 v1.28.3 pins both reviewed components. Only selected inline LL
headers, transitive ETH definitions and their original notice are admitted.
No vendor C, HAL initialization, tick, Handle instance or full LL implementation
is selected implicitly.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure.metadata import fields, parse_json  # noqa: E402
from tools.maintenance import cmsis_device_lock  # noqa: E402

ORIGIN = "https://github.com/STMicroelectronics/stm32f4xx-hal-driver.git"
COMMIT = "b6f0ed3829f3829eb358a2e7417d80bba1a42db7"
CMSIS_ORIGIN = "https://github.com/STMicroelectronics/cmsis_device_f4.git"
CMSIS_COMMIT = "3c77349ce04c8af401454cc51f85ea9a50e34fc1"
DIRECTORY = "vendors/st/stm32f4_hal"
CMSIS_DIRECTORY = "vendors/st/cmsis_device_f4"
LOCK_NAME = "dependencies/stm32f4-hal.lock.json"
REVIEWED_FILES = frozenset({
    "Inc/stm32f4xx_ll_gpio.h", "Inc/stm32f4xx_ll_usart.h",
    "Inc/stm32f4xx_ll_spi.h",
    "Inc/stm32f4xx_ll_adc.h", "Inc/stm32f4xx_ll_tim.h",
    "Inc/stm32f4xx_ll_iwdg.h", "LICENSE.md",
    "Inc/stm32f4xx_ll_i2c.h",
    "Inc/stm32f4xx_hal_eth.h", "Inc/stm32f4xx_hal_def.h",
    "Inc/Legacy/stm32_hal_legacy.h",
})
PAIRING = {
    "repository": "https://github.com/STMicroelectronics/STM32CubeF4.git",
    "release": "v1.28.3", "commit": "94cae6e83f00e276a11957e7833c01ac3d0bd7af",
    "cmsis_repository": CMSIS_ORIGIN, "cmsis_commit": CMSIS_COMMIT,
}


def contract(lock):
    """Validate the fixed official release, pairing, source graph and notice."""
    fields(lock, {"schema_version", "kind", "repository", "commit", "release",
                  "selected_api", "pairing", "files", "license"},
           "STM32F4 official LL source lock")
    if (type(lock["schema_version"]) is not int or
            lock["schema_version"] != 1 or
            lock["kind"] != "nexus-reviewed-official-driver"):
        raise ValueError("Unsupported STM32F4 official driver lock schema")
    if (lock["repository"] != ORIGIN or lock["commit"] != COMMIT or
            lock["release"] != "v1.8.5" or
            lock["selected_api"] != "stm32f407-ll-inline-eth-definitions" or
            lock["pairing"] != PAIRING):
        raise ValueError("Exact official STM32F4 release and CMSIS pairing required")
    if not isinstance(lock["files"], dict) or set(lock["files"]) != REVIEWED_FILES:
        raise ValueError("STM32F4 LL identities must close the reviewed graph")
    for record in lock["files"].values():
        fields(record, {"bytes", "sha256"}, "STM32F4 LL source identity")
        value = record["sha256"]
        if (type(record["bytes"]) is not int or record["bytes"] <= 0 or
                not isinstance(value, str) or len(value) != 64 or
                any(character not in "0123456789abcdef" for character in value)):
            raise ValueError("Nonempty exact STM32F4 LL file identity required")
    fields(lock["license"], {"path", "expression"}, "STM32F4 LL license")
    if lock["license"] != {"path": "LICENSE.md", "expression": "BSD-3-Clause"}:
        raise ValueError("Original reviewed STM32F4 BSD-3-Clause notice required")
    return set(lock["files"])


def canonical_directory(path):
    path = Path(path).absolute()
    if path.resolve() != path or path.is_symlink() or not path.is_dir():
        raise ValueError("STM32F4 sources require canonical regular directories")
    return path


def verify_pair(source, cmsis):
    """Read both clean upstream identities before and after source snapshots."""
    cmsis_device_lock.verify_git(source, {"commit": COMMIT}, {"origin": ORIGIN})
    cmsis_device_lock.verify_git(cmsis, {"commit": CMSIS_COMMIT},
                                 {"origin": CMSIS_ORIGIN})


def verify(source, lock_path, cmsis_source, *, packaged_commit=None,
           packaged_cmsis_commit=None):
    """Verify Git sources or exact commits from a fully verified Source SDK.

    Installed callers must first verify the complete manifest. Missing Git is
    never permission for byte-only admission. Source proof does not qualify
    driver behavior, an interrupt budget or physical hardware.
    """
    source = canonical_directory(source)
    cmsis = canonical_directory(cmsis_source)
    raw, lock_identity = cmsis_device_lock.stable_snapshot(lock_path)
    lock = parse_json(raw)
    selected = contract(lock)
    packaged = packaged_commit is not None or packaged_cmsis_commit is not None
    if packaged:
        if packaged_commit != COMMIT or packaged_cmsis_commit != CMSIS_COMMIT:
            raise ValueError("Packaged STM32F4 needs both exact paired SDK commits")
        if any((path / ".git").exists() or (path / ".git").is_symlink()
               for path in (source, cmsis)):
            raise ValueError("Packaged STM32F4 sources cannot contain Git markers")
    else:
        verify_pair(source, cmsis)
    identities = {}
    for name in sorted(selected):
        data, identity = cmsis_device_lock.stable_snapshot(source / name)
        record = lock["files"][name]
        if (len(data) != record["bytes"] or
                hashlib.sha256(data).hexdigest() != record["sha256"]):
            raise ValueError("STM32F4 official source/notice changed: " + name)
        identities[name] = identity
    if not packaged:
        verify_pair(source, cmsis)
    if cmsis_device_lock.stable_snapshot(lock_path)[1] != lock_identity or any(
            cmsis_device_lock.stable_snapshot(source / name)[1] != identity
            for name, identity in identities.items()):
        raise ValueError("STM32F4 official source/lock changed during verification")
    return {"kind": "nexus-stm32f4-official-source-verification",
            "commit": COMMIT, "release": lock["release"],
            "cmsis_commit": CMSIS_COMMIT, "verified_files": len(selected),
            "license": lock["license"]["expression"],
            "source_kind": "installed_source_sdk" if packaged else "git_checkout",
            "software_qualified": False}


def verify_sdk(root):
    """Delegate installed source proof to the complete SDK verifier."""
    sys.path.insert(0, str(ROOT / "cmake/package"))
    import package_source_sdk
    return package_source_sdk.verify(root)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--lock", required=True, type=Path)
    parser.add_argument("--cmsis-source", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        source, lock_path = args.source.absolute(), args.lock.absolute()
        cmsis = args.cmsis_source.absolute()
        _, lock_identity = cmsis_device_lock.stable_snapshot(lock_path)
        sdk = source.parents[2]
        manifest_path = sdk / ".nexus-source-sdk.json"
        manifest_identity = None
        packaged = {}
        if manifest_path.exists() or manifest_path.is_symlink():
            if (source != sdk / DIRECTORY or lock_path != sdk / LOCK_NAME or
                    cmsis != sdk / CMSIS_DIRECTORY):
                raise ValueError("STM32F4 requires exact SDK source and lock paths")
            _, manifest_identity = cmsis_device_lock.stable_snapshot(manifest_path)
            manifest = verify_sdk(sdk)
            records = {path: [record for record in manifest["dependencies"]
                              if record["path"] == path]
                       for path in (DIRECTORY, CMSIS_DIRECTORY)}
            if any(len(records[path]) != 1 for path in records):
                raise ValueError("SDK lacks exact paired STM32F4 dependencies")
            packaged = {"packaged_commit": records[DIRECTORY][0]["commit"],
                        "packaged_cmsis_commit": records[CMSIS_DIRECTORY][0]["commit"]}
        result = verify(source, lock_path, cmsis, **packaged)
        if cmsis_device_lock.stable_snapshot(lock_path)[1] != lock_identity or (
                manifest_identity is not None and
                cmsis_device_lock.stable_snapshot(manifest_path)[1] != manifest_identity):
            raise ValueError("STM32F4 lock or SDK manifest changed during proof")
    except (OSError, ValueError, TypeError, KeyError, IndexError, ImportError,
            subprocess.SubprocessError) as error:
        print("STM32F4 official source rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
