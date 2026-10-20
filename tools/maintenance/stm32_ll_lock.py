#!/usr/bin/env python3
"""Verify fixed selected inline LL graphs paired with official Cube releases.

The verifier shares source-proof behavior, not register layouts or I/O policy.
Original inline APIs stay private; Nexus retains request, IRQ and DMA ownership.
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass
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


@dataclass(frozen=True)
class Profile:
    """One reviewed exact pairing and deliberately selected inline source graph."""
    family: str
    part: str
    commit: str
    cmsis_commit: str
    cube_release: str
    cube_commit: str
    units: tuple[str, ...]

    @property
    def directory(self):
        return f"vendors/st/stm32{self.family}_hal"

    @property
    def cmsis_directory(self):
        return f"vendors/st/cmsis_device_{self.family}"

    @property
    def lock_name(self):
        return f"dependencies/stm32{self.family}-hal.lock.json"

    @property
    def origin(self):
        return f"https://github.com/STMicroelectronics/stm32{self.family}xx_hal_driver.git"

    @property
    def cmsis_origin(self):
        return f"https://github.com/STMicroelectronics/cmsis-device-{self.family}.git"

    @property
    def pairing(self):
        result = {"repository": "https://github.com/STMicroelectronics/STM32Cube" +
            self.family.upper() + ".git", "release": self.cube_release,
            "commit": self.cube_commit, "cmsis_repository": self.cmsis_origin,
            "cmsis_commit": self.cmsis_commit}
        if self.family == "g4":
            # The paired release and the maintained CMSIS child differ only in
            # two apostrophes inside the file header's explanatory comment.
            result["compatibility"] = {
                "kind": "reviewed_comment_only_child",
                "paired_cmsis_commit": "25664ddc3a7624ae9627ae8c4c672073dc5b2539",
                "files": ["Include/stm32g4xx.h"],
                "scope": "two comment apostrophes; no declarations or macros changed"}
        return result

    @property
    def selected_files(self):
        return {"LICENSE.md", *(f"Inc/stm32{self.family}xx_ll_{unit}.h"
                                for unit in self.units)}

    def contract(self, lock):
        """Reject changed revisions, notices, pairing or a widened source graph."""
        fields(lock, {"schema_version", "kind", "repository", "commit", "release",
                      "selected_api", "pairing", "files", "license"},
               "official inline LL source lock")
        if (type(lock["schema_version"]) is not int or lock["schema_version"] != 1 or
                lock["kind"] != "nexus-reviewed-official-driver" or
                lock["repository"] != self.origin or lock["commit"] != self.commit or
                lock["release"] != "STM32Cube" + self.family.upper() + "-" +
                    self.cube_release + "-pinned-driver" or
                lock["selected_api"] != self.part + "-ll-inline" or
                lock["pairing"] != self.pairing):
            raise ValueError("Exact reviewed official LL release and pairing required")
        if not isinstance(lock["files"], dict) or set(lock["files"]) != self.selected_files:
            raise ValueError("Official LL identities must close the reviewed graph")
        for record in lock["files"].values():
            fields(record, {"bytes", "sha256"}, "official LL file identity")
            value = record["sha256"]
            if (type(record["bytes"]) is not int or record["bytes"] <= 0 or
                    not isinstance(value, str) or len(value) != 64 or
                    any(character not in "0123456789abcdef" for character in value)):
                raise ValueError("Nonempty exact official LL identity required")
        if lock["license"] != {"path": "LICENSE.md", "expression": "BSD-3-Clause"}:
            raise ValueError("Original official LL BSD-3-Clause notice required")
        return set(lock["files"])

    def verify_pair(self, source, cmsis):
        cmsis_device_lock.verify_git(source, {"commit": self.commit},
                                     {"origin": self.origin})
        cmsis_device_lock.verify_git(cmsis, {"commit": self.cmsis_commit},
                                     {"origin": self.cmsis_origin})

    def verify(self, source, lock_path, cmsis_source, *, packaged_commit=None,
               packaged_cmsis_commit=None):
        """Verify clean Git or both commits from a fully verified source SDK.

        Installed callers first verify the complete SDK manifest. Exact locked
        bytes alone never authorize an unobserved Git or package provenance.
        """
        source, cmsis = canonical_directory(source), canonical_directory(cmsis_source)
        raw, lock_identity = cmsis_device_lock.stable_snapshot(lock_path)
        lock = parse_json(raw)
        selected = self.contract(lock)
        packaged = packaged_commit is not None or packaged_cmsis_commit is not None
        if packaged:
            if (packaged_commit != self.commit or
                    packaged_cmsis_commit != self.cmsis_commit):
                raise ValueError("Packaged official LL needs both exact paired SDK commits")
            if any((path / ".git").exists() or (path / ".git").is_symlink()
                   for path in (source, cmsis)):
                raise ValueError("Packaged official LL cannot contain Git markers")
        else:
            self.verify_pair(source, cmsis)
        identities = {}
        for name in sorted(selected):
            data, identity = cmsis_device_lock.stable_snapshot(source / name)
            record = lock["files"][name]
            if (len(data) != record["bytes"] or
                    hashlib.sha256(data).hexdigest() != record["sha256"]):
                raise ValueError("Original official LL source/notice changed: " + name)
            identities[name] = identity
        if not packaged:
            self.verify_pair(source, cmsis)
        if cmsis_device_lock.stable_snapshot(lock_path)[1] != lock_identity or any(
                cmsis_device_lock.stable_snapshot(source / name)[1] != identity
                for name, identity in identities.items()):
            raise ValueError("Official LL source or lock changed during verification")
        return {"kind": "nexus-stm32-official-source-verification",
            "family": self.family, "commit": self.commit,
            "release": lock["release"], "cmsis_commit": self.cmsis_commit,
            "pairing": self.pairing, "verified_files": len(selected),
            "license": lock["license"]["expression"],
            "source_kind": "installed_source_sdk" if packaged else "git_checkout",
            "software_qualified": False}

    def main(self, argv=None):
        parser = argparse.ArgumentParser(description=__doc__)
        for name in ("source", "lock", "cmsis-source"):
            parser.add_argument("--" + name, required=True, type=Path)
        args = parser.parse_args(argv)
        try:
            source, lock = args.source.absolute(), args.lock.absolute()
            cmsis = args.cmsis_source.absolute()
            _, lock_identity = cmsis_device_lock.stable_snapshot(lock)
            sdk = source.parents[2]
            manifest_path = sdk / ".nexus-source-sdk.json"
            manifest_identity, packaged = None, {}
            if manifest_path.exists() or manifest_path.is_symlink():
                if (source != sdk / self.directory or lock != sdk / self.lock_name or
                        cmsis != sdk / self.cmsis_directory):
                    raise ValueError("Official LL requires exact SDK source and lock paths")
                _, manifest_identity = cmsis_device_lock.stable_snapshot(manifest_path)
                sys.path.insert(0, str(ROOT / "cmake/package"))
                import package_source_sdk
                manifest = package_source_sdk.verify(sdk)
                records = {path: [record for record in manifest["dependencies"]
                    if record["path"] == path]
                    for path in (self.directory, self.cmsis_directory)}
                if any(len(value) != 1 for value in records.values()):
                    raise ValueError("SDK lacks exact paired official LL dependencies")
                packaged = {"packaged_commit": records[self.directory][0]["commit"],
                    "packaged_cmsis_commit": records[self.cmsis_directory][0]["commit"]}
            result = self.verify(source, lock, cmsis, **packaged)
            if cmsis_device_lock.stable_snapshot(lock)[1] != lock_identity or (
                    manifest_identity is not None and
                    cmsis_device_lock.stable_snapshot(manifest_path)[1] != manifest_identity):
                raise ValueError("Official LL lock or SDK manifest changed during proof")
        except (OSError, ValueError, TypeError, KeyError, IndexError, ImportError,
                subprocess.SubprocessError) as error:
            print("Official LL source rejected: " + str(error), file=sys.stderr)
            return 1
        print(json.dumps(result, sort_keys=True))
        return 0


def canonical_directory(path):
    path = Path(path).absolute()
    if path.resolve() != path or path.is_symlink() or not path.is_dir():
        raise ValueError("Official LL sources require canonical regular directories")
    return path


PROFILES = {
    "g4": Profile("g4", "stm32g474", "f5929f431f9effe45fbe18f5337e4753ced9ac92",
        "626ee412334a5ed2e5b320af5a8d77d69f03a558", "v1.6.3",
        "d11b194a9f05d1b143d154771f3dbc282c8052a5", ("adc", "dmamux")),
    "h7": Profile("h7", "stm32h743", "a1996eed9172b59887bafaaa0ea1816ea14d48b5",
        "8f922cdc7cc6de2344e75ddd657889f4ff761790", "v1.13.0",
        "5abb9764b32e11a6557b90bf39531528019b5761", ("adc", "spi")),
    "u5": Profile("u5", "stm32u575", "2552682ee5a0d6f826a6435750fc9267be65868b",
        "624374fa1e21ca195d6f2102ac0caaa50d0ea4c8", "v1.9.0",
        "d88042df24f16799957c24f00c2b234c9e306188", ("adc", "spi", "i2c", "tim")),
}
