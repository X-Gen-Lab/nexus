#!/usr/bin/env python3
"""Verify the exact STM32CubeF1 CMSIS Device and HAL/LL pairing."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
if __name__ == "__main__":
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(ROOT))

from tools.configure.metadata import fields, parse_json  # noqa: E402
from tools.maintenance import cmsis_device_lock  # noqa: E402

ORIGIN = "https://github.com/STMicroelectronics/stm32f1xx-hal-driver.git"
COMMIT = "fee494a92b5ad331f92ad21f76c66a5cb83773ee"
CMSIS_ORIGIN = "https://github.com/STMicroelectronics/cmsis-device-f1.git"
CMSIS_COMMIT = "c8e9a4a4f16b6d2cb2a2083cbe5161025280fb22"
DIRECTORY = "vendors/st/stm32f1_hal"
CMSIS_DIRECTORY = "vendors/st/cmsis_device_f1"
LOCK_NAME = "dependencies/stm32f1-hal.lock.json"
CMSIS_LOCK_NAME = "dependencies/stm32f1.lock.json"
REVIEWED_FILES = frozenset({
    "Inc/stm32f1xx_hal_hcd.h",
    "Inc/stm32f1xx_ll_usb.h",
    "Src/stm32f1xx_hal_hcd.c",
    "Src/stm32f1xx_ll_usb.c",

    "Inc/Legacy/stm32_hal_legacy.h",
    "Inc/stm32f1xx_hal.h",
    "Inc/stm32f1xx_hal_def.h",
    "Inc/stm32f1xx_hal_flash_ex.h",
    "Inc/stm32f1xx_ll_fsmc.h",
    "Src/stm32f1xx_ll_fsmc.c",
    "Inc/stm32f1xx_ll_adc.h",
    "Inc/stm32f1xx_ll_dac.h",
    "Inc/stm32f1xx_ll_exti.h",
    "Inc/stm32f1xx_ll_tim.h",
    "Inc/stm32f1xx_ll_crc.h",
    "Inc/stm32f1xx_ll_spi.h",
    "Inc/stm32f1xx_ll_iwdg.h",
    "Inc/stm32f1xx_ll_utils.h",
    "Inc/stm32f1xx_ll_wwdg.h",
    "Inc/stm32f1xx_ll_gpio.h", "Inc/stm32f1xx_ll_i2c.h",
    "Inc/stm32f1xx_ll_usart.h", "LICENSE.md",
})
PAIRING = {
    "repository": "https://github.com/STMicroelectronics/STM32CubeF1.git",
    "release": "v1.8.7",
    "commit": "d12e75247d5bcedc734f829b394517ab4c2726e3",
    "cmsis_repository": CMSIS_ORIGIN,
    "cmsis_commit": CMSIS_COMMIT,
}


def contract(lock):
    """Admit only the reviewed release, headers and original notice."""
    fields(lock, {"schema_version", "kind", "commit", "files", "license",
                  "pairing", "release", "repository", "selected_api"},
           "STM32F1 driver source lock")
    if (type(lock["schema_version"]) is not int or lock["schema_version"] != 1
            or lock["kind"] != "nexus-reviewed-official-driver"
            or lock["commit"] != COMMIT or lock["repository"] != ORIGIN
            or lock["pairing"] != PAIRING
            or lock["release"] != "STM32CubeF1-v1.8.7-pinned-driver"
            or lock["selected_api"] != "stm32f1-reviewed-ll-and-hcd"):
        raise ValueError("Exact official STM32CubeF1 pairing required")
    if lock["license"] != {"path": "LICENSE.md", "expression": "BSD-3-Clause"}:
        raise ValueError("Original reviewed STM32F1 notice required")
    if not isinstance(lock["files"], dict) or set(lock["files"]) != REVIEWED_FILES:
        raise ValueError("STM32F1 source lock must close the reviewed LL/HCD graph")
    for record in lock["files"].values():
        fields(record, {"bytes", "sha256"}, "STM32F1 driver file identity")
        if (type(record["bytes"]) is not int or record["bytes"] <= 0
                or not isinstance(record["sha256"], str)
                or not re.fullmatch(r"[0-9a-f]{64}", record["sha256"])):
            raise ValueError("Invalid STM32F1 driver file identity")
    return REVIEWED_FILES


def verify(source, lock_path, cmsis_source, *, packaged_commit=None,
           packaged_cmsis_commit=None, selected_device_files=()):
    """Verify a clean checkout or use the commits of an admitted Source SDK."""
    source = Path(source).absolute()
    cmsis = Path(cmsis_source).absolute()
    sdk = source.parents[2]
    lock_path = Path(lock_path).absolute()
    if (source != sdk / DIRECTORY or cmsis != sdk / CMSIS_DIRECTORY
            or lock_path != sdk / LOCK_NAME or source.resolve() != source
            or cmsis.resolve() != cmsis):
        raise ValueError("STM32F1 requires owned supplier and lock paths")

    raw, identity = cmsis_device_lock.stable_snapshot(lock_path)
    lock = parse_json(raw)
    names = contract(lock)
    packaged = packaged_commit is not None or packaged_cmsis_commit is not None
    if packaged:
        if (packaged_commit, packaged_cmsis_commit) != (COMMIT, CMSIS_COMMIT):
            raise ValueError("Installed STM32F1 requires both paired commits")
        marker = source / ".git"
        if marker.exists() or marker.is_symlink():
            raise ValueError("Installed STM32F1 HAL cannot contain Git metadata")
    else:
        cmsis_device_lock.verify_git(source, lock, {"origin": ORIGIN})

    cmsis_lock = sdk / CMSIS_LOCK_NAME
    raw, device_identity = cmsis_device_lock.stable_snapshot(cmsis_lock)
    device_record = parse_json(raw)
    device_names = cmsis_device_lock.contract(device_record)
    if not set(selected_device_files).issubset(device_names):
        raise ValueError("Selected STM32F1 header and startup must be locked")
    if device_record["commit"] != CMSIS_COMMIT:
        raise ValueError("Device source lock is outside the reviewed F1 pairing")
    observed = {lock_path: identity, cmsis_lock: device_identity}
    for name in sorted(device_names):
        member = cmsis / name
        observed[member] = cmsis_device_lock.stable_snapshot(member)[1]
    device = cmsis_device_lock.verify(cmsis, cmsis_lock,
                                      packaged_commit=packaged_cmsis_commit)
    for name in sorted(names):
        member = source / name
        data, observed[member] = cmsis_device_lock.stable_snapshot(member)
        record = lock["files"][name]
        if (len(data) != record["bytes"] or
                hashlib.sha256(data).hexdigest() != record["sha256"]):
            raise ValueError("STM32F1 source identity differs: " + name)
    if not packaged:
        cmsis_device_lock.verify_git(source, lock, {"origin": ORIGIN})
        cmsis_device_lock.verify_git(cmsis, device_record, {"origin": CMSIS_ORIGIN})
    if any(cmsis_device_lock.stable_snapshot(path)[1] != expected
           for path, expected in observed.items()):
        raise ValueError("STM32F1 driver source changed during admission")
    return {
        "kind": "nexus-stm32f1-official-source-verification",
        "commit": COMMIT, "cmsis_commit": CMSIS_COMMIT,
        "verified_files": len(names) + device["verified_files"],
        "configure_inputs": [str(path) for path in
            (*observed, Path(__file__),
             Path(cmsis_device_lock.__file__))],
    }


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--lock", required=True, type=Path)
    parser.add_argument("--cmsis-source", required=True, type=Path)
    parser.add_argument("--device", nargs=2, default=(),
                        metavar=("HEADER", "STARTUP"))
    options = parser.parse_args(arguments)
    try:
        source = options.source.absolute()
        sdk = source.parents[2]
        manifest_path = sdk / ".nexus-source-sdk.json"
        packaged = {}
        if manifest_path.exists() or manifest_path.is_symlink():
            manifest = cmsis_device_lock.verify_sdk(sdk)
            dependencies = {item["path"]: item["commit"]
                            for item in manifest["dependencies"]}
            packaged = {
                "packaged_commit": dependencies[DIRECTORY],
                "packaged_cmsis_commit": dependencies[CMSIS_DIRECTORY],
            }
        result = verify(source, options.lock, options.cmsis_source,
                        selected_device_files=options.device, **packaged)
        if packaged:
            result["configure_inputs"].append(str(manifest_path))
    except (OSError, ValueError, TypeError, KeyError, IndexError, ImportError) as error:
        print("STM32F1 official source rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
