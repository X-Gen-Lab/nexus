"""Reject an incomplete SDK manifest before opening its payload contents."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch as mock_patch

ROOT = Path(__file__).resolve().parents[2]


class SDKRequiredAdmissionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        specification = importlib.util.spec_from_file_location(
            "sdk_required_admission",
            ROOT / "cmake/package/package_source_sdk.py")
        cls.sdk = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(cls.sdk)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="SDK admission ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "share/nexus/src"
        self.root.mkdir(parents=True)
        self.payload = self.root / "ordinary.c"
        self.payload.write_bytes(b"reviewed payload bytes\n")
        self.files = {"ordinary.c": self.sdk.digest(self.payload.read_bytes())}
        self.identity = {
            "schema": 2, "profile": self.sdk.source_profile(),
            "kind": "nexus-installed-source-sdk",
            "source_revision": "a" * 40, "source_tree": "b" * 40,
            "publishable": True, "source_dirty": False,
            "files_sha256": self.files, "dependencies": [],
            "reviewed_imports": [], "toolchains_lock_sha256": "c" * 64,
            "snapshot_sha256": "", "package_files_sha256": {},
        }

    def write_manifest(self):
        self.identity["snapshot_sha256"] = self.sdk.digest(json.dumps(
            self.files, sort_keys=True, separators=(",", ":")).encode())
        (self.root / self.sdk.MANIFEST).write_text(json.dumps(self.identity))

    def complete_required_map(self):
        # Portable rejection fixtures do not claim a complete installed SDK.
        # Retain the actual policy; no vendor checkout or reduced checker graph.
        for name in self.sdk.required_files(ROOT, self.identity["profile"]):
            self.files[name] = self.sdk.digest(b"fixture placeholder")
        # Selection admission reads the real supplier lock before payload
        # traversal. Placeholder payload files remain deliberate rejection data.
        name = "dependencies/stm32f4-hal.lock.json"
        lock = self.root / name
        lock.parent.mkdir(parents=True, exist_ok=True)
        data = (ROOT / name).read_bytes()
        lock.write_bytes(data)
        self.files[name] = self.sdk.digest(data)

    def test_missing_required_names_reject_without_opening_payload(self):
        self.write_manifest()
        reads = []
        original_open = Path.open

        def observe_open(path, *args, **kwargs):
            if path == self.payload:
                reads.append(path)
            return original_open(path, *args, **kwargs)

        with mock_patch.object(Path, "open", autospec=True,
                               side_effect=observe_open):
            with self.assertRaisesRegex(self.sdk.SDKError,
                                        "missing required sources/licenses"):
                self.sdk.verify(self.root)
        self.assertEqual(reads, [],
                         "An unqualified manifest opened payload bytes")

    def test_complete_required_map_still_checks_payload_bytes(self):
        self.complete_required_map()
        self.write_manifest()
        self.payload.write_bytes(b"changed bytes after manifest publication\n")
        with self.assertRaisesRegex(self.sdk.SDKError,
                                    "file identity changed: ordinary.c"):
            self.sdk.verify(self.root)


    def test_qiming_device_omissions_reject_before_payload_reads(self):
        paths = (
            "boards/stm32f407zg_qiming_core/board.json",
            "boards/stm32f407zg_qiming_v41/board.json",
            "io/include/nexus/io/block.h", "io/src/api/block.c",
            "cmake/platform/STM32FSMC.cmake",
            "cmake/platform/STM32SDIO.cmake",
            "soc/stm32/stm32f4/drivers/fsmc.c",
            "soc/stm32/stm32f4/drivers/fsmc_boot.c",
            "soc/stm32/stm32f4/sram_boot.c",
            "soc/stm32/stm32f4/private/stm32f407_sram_boot.h",
            "soc/stm32/stm32f4/private/stm32f407_fsmc_profile.h",
            "io/include/nexus/io/parallel16.h",
            "io/include/nexus/io/memory_window.h",
            "soc/stm32/stm32f4/drivers/rtc.c",
            "soc/stm32/stm32f4/drivers/sdio.c",
            "soc/stm32/stm32f4/drivers/encoder.c",
            "soc/stm32/stm32f4/private/stm32f4_encoder.h",
            "soc/stm32/stm32f4/private/stm32f407_timer_hardware.h",
            "tools/configure/stm32f4/encoder.py",
            "components/at24c02/src/at24c02.c",
            "components/w25/src/w25.c",
            "components/ili9341/src/ili9341.c",
            "components/xpt2046/src/xpt2046.c",
            "components/software_spi/src/software_spi.c",
            "components/software_i2c/src/software_i2c.c",
            "components/infrared/src/infrared.c",
            "components/onewire/src/onewire.c",
            "components/onewire/src/crc8.c",
            "components/ds18b20/src/ds18b20.c")
        self.assertTrue(set(paths).issubset(self.sdk.REQUIRED_FILES),
                        "Qiming source SDK must close device dependencies")
        self.complete_required_map()
        for name in paths:
            with self.subTest(missing=name):
                original = self.files.pop(name)
                try:
                    self.write_manifest()
                    reads = []
                    original_read = Path.read_bytes

                    def observe_read(path):
                        if path != self.root / self.sdk.MANIFEST:
                            reads.append(path)
                        return original_read(path)

                    with mock_patch.object(Path, "read_bytes", autospec=True,
                                           side_effect=observe_read):
                        with self.assertRaisesRegex(
                                self.sdk.SDKError,
                                "missing required sources/licenses"):
                            self.sdk.verify(self.root)
                        self.assertEqual(reads, [])
                finally:
                    self.files[name] = original

    def test_complete_required_map_still_rejects_unsafe_payload_paths(self):
        self.complete_required_map()
        self.files = {"../outside.c": "d" * 64, **self.files}
        self.identity["files_sha256"] = self.files
        self.write_manifest()
        with self.assertRaisesRegex(self.sdk.SDKError,
                                    "Unsafe source SDK path"):
            self.sdk.verify(self.root)

    def test_schema_and_provenance_precede_required_name_admission(self):
        for changes, message in (
                ({"schema": 1}, "Unsupported installed source SDK identity"),
                ({"source_revision": "invalid"}, "Invalid source Git identity"),
                ({"publishable": "true"},
                 "Invalid source SDK provenance flags"),
                ({"source_dirty": True},
                 "dirty source SDK cannot be publishable")):
            with self.subTest(changes=changes):
                original = dict(self.identity)
                try:
                    self.identity.update(changes)
                    self.write_manifest()
                    with self.assertRaisesRegex(self.sdk.SDKError, message):
                        self.sdk.verify(self.root)
                finally:
                    self.identity = original


if __name__ == "__main__":
    unittest.main()
