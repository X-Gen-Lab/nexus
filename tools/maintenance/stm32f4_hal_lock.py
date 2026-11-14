#!/usr/bin/env python3
"""Verify the selected STM32F4 HAL/LL source graph and paired CMSIS pin.

The reviewed source inventory closes the maintained F407 primitives and exact
F415/F417 crypto, F446/F479 SAI, F479 FMC/graphics/QSPI compile contexts. CMake
still selects only each mode's actual sources; inventory does not enable them.
The transitive DSI/PWR headers do not select DSI or PWR C transports. Whole HAL
initialization, SysTick and unreviewed peripheral C remain outside this graph.
Vendor Handles stay private. Source identity neither qualifies a transport nor
expands any exact part's declared runtime modes or physical support.
"""
from __future__ import annotations
import argparse
import glob
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

if __name__ == "__main__":
    sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure.metadata import fields, parse_json  # noqa: E402
from tools.maintenance import cmsis_device_lock  # noqa: E402
from tools.configure import dependencies  # noqa: E402

ORIGIN = "https://github.com/STMicroelectronics/stm32f4xx-hal-driver.git"
COMMIT = "b6f0ed3829f3829eb358a2e7417d80bba1a42db7"
CMSIS_ORIGIN = "https://github.com/STMicroelectronics/cmsis_device_f4.git"
CMSIS_COMMIT = "3c77349ce04c8af401454cc51f85ea9a50e34fc1"
DIRECTORY = "vendors/st/stm32f4_hal"
CMSIS_DIRECTORY = "vendors/st/cmsis_device_f4"
LOCK_NAME = "dependencies/stm32f4-hal.lock.json"
REVIEWED_FILES = frozenset({
    "Inc/stm32f4xx_ll_gpio.h", "Inc/stm32f4xx_ll_usart.h",
    "Inc/stm32f4xx_ll_spi.h", "Inc/stm32f4xx_ll_bus.h",
    "Inc/stm32f4xx_ll_rcc.h", "Inc/stm32f4xx_ll_rng.h",
    "Inc/stm32f4xx_ll_adc.h", "Inc/stm32f4xx_ll_tim.h",
    "Inc/stm32f4xx_ll_dac.h", "Inc/stm32f4xx_ll_dma.h",
    "Inc/stm32f4xx_ll_crc.h", "Inc/stm32f4xx_ll_utils.h",
    "Inc/stm32f4xx_ll_system.h",
    "Inc/stm32f4xx_ll_iwdg.h", "Inc/stm32f4xx_ll_wwdg.h", "LICENSE.md",
    "Inc/stm32f4xx_ll_i2c.h",
    "Inc/stm32f4xx_hal_eth.h", "Inc/stm32f4xx_hal_def.h",
    "Inc/Legacy/stm32_hal_legacy.h",
    "Inc/stm32f4xx_hal.h", "Inc/stm32f4xx_hal_dma.h",
    "Inc/stm32f4xx_hal_dma_ex.h", "Inc/stm32f4xx_hal_uart.h",
    "Inc/stm32f4xx_hal_rcc.h", "Inc/stm32f4xx_hal_rcc_ex.h",
    "Inc/stm32f4xx_hal_gpio.h", "Inc/stm32f4xx_hal_gpio_ex.h",
    "Inc/stm32f4xx_hal_flash.h", "Inc/stm32f4xx_hal_flash_ex.h",
    "Inc/stm32f4xx_hal_flash_ramfunc.h", "Src/stm32f4xx_hal_uart.c",
    "Src/stm32f4xx_hal_dma.c", "Src/stm32f4xx_hal_rcc.c",
    "Inc/stm32f4xx_ll_fsmc.h", "Src/stm32f4xx_ll_fsmc.c",
    "Inc/stm32f4xx_ll_sdmmc.h", "Src/stm32f4xx_ll_sdmmc.c",
    "Inc/stm32f4xx_ll_rtc.h", "Inc/stm32f4xx_ll_pwr.h",
    "Inc/stm32f4xx_hal_dcmi.h", "Inc/stm32f4xx_hal_dcmi_ex.h",
    "Src/stm32f4xx_hal_dcmi.c", "Inc/stm32f4xx_hal_hcd.h",
    "Src/stm32f4xx_hal_hcd.c", "Inc/stm32f4xx_ll_usb.h",
    "Src/stm32f4xx_ll_usb.c", "Inc/stm32f4xx_hal_pcd.h",
    "Inc/stm32f4xx_hal_cryp.h",
    "Inc/stm32f4xx_hal_dma2d.h",
    "Inc/stm32f4xx_hal_dsi.h",
    "Inc/stm32f4xx_hal_hash.h",
    "Inc/stm32f4xx_hal_hash_ex.h",
    "Inc/stm32f4xx_hal_ltdc.h",
    "Inc/stm32f4xx_hal_ltdc_ex.h",
    "Inc/stm32f4xx_hal_pwr.h",
    "Inc/stm32f4xx_hal_pwr_ex.h",
    "Inc/stm32f4xx_hal_qspi.h",
    "Inc/stm32f4xx_hal_sai.h",
    "Inc/stm32f4xx_hal_sai_ex.h",
    "Inc/stm32f4xx_ll_fmc.h",
    "Src/stm32f4xx_hal_cryp.c",
    "Src/stm32f4xx_hal_dma2d.c",
    "Src/stm32f4xx_hal_gpio.c",
    "Src/stm32f4xx_hal_hash.c",
    "Src/stm32f4xx_hal_ltdc.c",
    "Src/stm32f4xx_hal_qspi.c",
    "Src/stm32f4xx_hal_rcc_ex.c",
    "Src/stm32f4xx_hal_sai.c",
    "Src/stm32f4xx_hal_sai_ex.c",
    "Src/stm32f4xx_ll_fmc.c",
    "Inc/stm32f4xx_hal_pcd_ex.h",
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
           "STM32F4 official source lock")
    if (type(lock["schema_version"]) is not int or
            lock["schema_version"] != 1 or
            lock["kind"] != "nexus-reviewed-official-driver"):
        raise ValueError("Unsupported STM32F4 official driver lock schema")
    if (lock["repository"] != ORIGIN or lock["commit"] != COMMIT or
            lock["release"] != "v1.8.5" or
            lock["selected_api"] != (
                "stm32f4-reviewed-hal-ll-controllers") or
            lock["pairing"] != PAIRING):
        raise ValueError("Exact official STM32F4 release and CMSIS pairing required")
    if not isinstance(lock["files"], dict) or set(lock["files"]) != REVIEWED_FILES:
        raise ValueError("STM32F4 identities must close the reviewed graph")
    for record in lock["files"].values():
        fields(record, {"bytes", "sha256"}, "STM32F4 source identity")
        value = record["sha256"]
        if (type(record["bytes"]) is not int or record["bytes"] <= 0 or
                not isinstance(value, str) or len(value) != 64 or
                any(character not in "0123456789abcdef" for character in value)):
            raise ValueError("Nonempty exact STM32F4 file identity required")
    fields(lock["license"], {"path", "expression"}, "STM32F4 license")
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


def _current_verification(source, lock_path, cmsis):
    """Keep standalone and configure-boundary admission equally complete."""
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
    return result


def _input_snapshot(source, lock_path, cmsis):
    """Observe exact input bytes and filesystem identity, never a success flag.

    The selected Git checkouts include all their files, so restored edits in
    paired headers or unselected vendor files cannot hide behind clean status.
    SDK mode covers its complete source tree and declared package exports.
    Git administrative directories are not tree inputs: only actual identity
    files are observed, avoiding transient index.lock directory timestamps.
    """
    source, cmsis = canonical_directory(source), canonical_directory(cmsis)
    sdk = source.parents[2]
    manifest = sdk / ".nexus-source-sdk.json"
    required = {lock_path, ROOT / "soc/stm32/stm32f4/cmake/STM32Official.cmake"}
    required.update(ROOT / name for name in
                    dependencies.source_names("platform", "stm32f4", sdk=True))
    paths = {*required, manifest}
    trees = (sdk,) if manifest.exists() or manifest.is_symlink() else (source, cmsis)
    directories = {}

    def directory_identity(path):
        if path.resolve() != path or path.is_symlink() or not path.is_dir():
            raise ValueError("Configure input directory changed: " + str(path))
        state = path.stat()
        return [state.st_dev, state.st_ino, state.st_mode, state.st_size,
                state.st_mtime_ns, state.st_ctime_ns]

    for tree in trees:
        for directory, names, files in os.walk(tree, followlinks=False):
            path = Path(directory)
            directories[str(path)] = directory_identity(path)
            for name in names:
                if (path / name).is_symlink():
                    raise ValueError("Configure input directory is a symlink")
            names[:] = [name for name in names if name != ".git"]
            paths.update(path / name for name in files)
    if trees == (sdk,):
        # Paths are constrained before any file is opened; complete SDK
        # validation below remains the authority for schema and provenance.
        from pathlib import PurePosixPath
        value = parse_json(cmsis_device_lock.stable_snapshot(manifest)[0])
        for name in value.get("package_files_sha256", {}):
            relative = PurePosixPath(name)
            if (not name or relative.is_absolute() or ".." in relative.parts or
                    "\\" in name or relative.as_posix() != name):
                raise ValueError("Unsafe SDK export configure input")
            prefix = sdk.parents[2]
            exported = prefix / relative
            paths.add(exported)
            for directory in exported.parents:
                if not directory.is_relative_to(prefix):
                    break
                directories[str(directory)] = directory_identity(directory)
    else:
        for checkout in (source, cmsis):
            paths.update(cmsis_device_lock.git_identity_inputs(checkout))
    files = {str(path): (cmsis_device_lock.stable_snapshot(path)[1]
                         if path.exists() or path.is_symlink() else None)
             for path in sorted(paths)}
    if any(files[str(path)] is None for path in required):
        raise ValueError("Configure authority input is missing")
    # Hashing a later file must not hide a rewrite of an earlier one. Close
    # the whole sweep with filesystem identities, including absent optional
    # Git metadata and directory membership; this never reuses past success.
    for name, identity in files.items():
        path = Path(name)
        if identity is None:
            changed = path.exists() or path.is_symlink()
        else:
            state = cmsis_device_lock.regular(path).stat()
            changed = (state.st_dev, state.st_ino, state.st_size,
                       state.st_mtime_ns, state.st_ctime_ns) != identity["filesystem"]
        if changed:
            raise ValueError("Configure input changed during snapshot: " + name)
    if any(directory_identity(Path(name)) != identity
           for name, identity in directories.items()):
        raise ValueError("Configure input directories changed during snapshot")
    return {"source": str(source), "lock": str(lock_path), "cmsis": str(cmsis),
            "files": files, "directories": directories}


def _snapshot_bytes(value):
    return (json.dumps(value, sort_keys=True, indent=2) + "\n").encode()


def begin_configure(source, lock_path, cmsis, pending):
    """Verify fresh inputs; retain a one-use, digest-bound pending snapshot."""
    source, lock_path, cmsis, pending = (Path(path).absolute() for path in
                                       (source, lock_path, cmsis, pending))
    if (pending.resolve() != pending or pending.is_symlink() or
            pending.is_relative_to(source) or pending.is_relative_to(cmsis) or
            ((source.parents[2] / ".nexus-source-sdk.json").exists() and
             pending.is_relative_to(source.parents[2]))):
        raise ValueError("Configure snapshot must be outside verified inputs")
    if pending.exists():
        raise ValueError("Configure snapshot already exists")
    before = _snapshot_bytes(_input_snapshot(source, lock_path, cmsis))
    result = _current_verification(source, lock_path, cmsis)
    if before != _snapshot_bytes(_input_snapshot(source, lock_path, cmsis)):
        raise ValueError("Configure inputs changed during initial verification")
    data = _snapshot_bytes({"schema": 1,
        "kind": "nexus-pending-stm32f4-configure-inputs",
        "software_qualified": False, "inputs": json.loads(before)})
    created = False
    try:
        with pending.open("xb") as stream:
            created = True
            stream.write(data)
    except OSError:
        if created:
            pending.unlink(missing_ok=True)
        raise
    observed = json.loads(before)["files"]
    return {**result, "configure_snapshot_sha256": hashlib.sha256(data).hexdigest(),
            "configure_inputs": [name for name, identity in observed.items()
                                 if identity is not None],
            "configure_file_probes": [glob.escape(Path(name).as_posix())
                                      for name, identity in observed.items()
                                      if identity is None]}


def end_configure(source, lock_path, cmsis, pending, expected_digest):
    """Fully reverify unchanged inputs, then consume the pending snapshot."""
    source, lock_path, cmsis, pending = (Path(path).absolute() for path in
                                       (source, lock_path, cmsis, pending))
    raw, _ = cmsis_device_lock.stable_snapshot(pending)
    record = parse_json(raw)
    # Only a recognized pending input outside the verified source can be
    # consumed here. Unrelated/unreadable files remain untouched; CMake also
    # removes its own generated path after every final subprocess outcome.
    owned = (isinstance(record, dict) and
        record.get("kind") == "nexus-pending-stm32f4-configure-inputs" and
        record.get("software_qualified") is False and
        isinstance(record.get("inputs"), dict) and
        all(record["inputs"].get(name) == str(path) for name, path in
            (("source", source), ("lock", lock_path), ("cmsis", cmsis))) and
        not pending.is_relative_to(source) and not pending.is_relative_to(cmsis) and
        not ((source.parents[2] / ".nexus-source-sdk.json").exists() and
             pending.is_relative_to(source.parents[2])))
    try:
        if hashlib.sha256(raw).hexdigest() != expected_digest:
            raise ValueError("Configure snapshot digest changed")
        fields(record, {"schema", "kind", "software_qualified", "inputs"},
               "configure input snapshot")
        if not owned or type(record["schema"]) is not int or record["schema"] != 1:
            raise ValueError("Configure snapshot scope changed")
        expected = _snapshot_bytes(record["inputs"])
        if expected != _snapshot_bytes(_input_snapshot(source, lock_path, cmsis)):
            raise ValueError("Configure inputs changed since initial verification")
        result = _current_verification(source, lock_path, cmsis)
        if expected != _snapshot_bytes(_input_snapshot(source, lock_path, cmsis)):
            raise ValueError("Configure inputs changed during final verification")
        return result
    finally:
        if owned:
            pending.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--lock", required=True, type=Path)
    parser.add_argument("--cmsis-source", required=True, type=Path)
    session = parser.add_mutually_exclusive_group()
    session.add_argument("--begin-configure", type=Path)
    session.add_argument("--end-configure", type=Path)
    parser.add_argument("--snapshot-sha256")
    args = parser.parse_args(argv)
    if bool(args.end_configure) != bool(args.snapshot_sha256):
        parser.error("Final configure verification requires its snapshot digest")
    try:
        source, lock_path = args.source.absolute(), args.lock.absolute()
        cmsis = args.cmsis_source.absolute()
        if args.begin_configure:
            result = begin_configure(source, lock_path, cmsis, args.begin_configure)
        elif args.end_configure:
            result = end_configure(source, lock_path, cmsis, args.end_configure,
                                   args.snapshot_sha256)
        else:
            result = _current_verification(source, lock_path, cmsis)
    except (OSError, ValueError, TypeError, KeyError, IndexError, ImportError,
            subprocess.SubprocessError) as error:
        print("STM32F4 official source rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
