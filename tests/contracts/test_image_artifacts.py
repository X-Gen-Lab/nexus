"""Real ARM image-window boundaries; these fixtures grant no qualification."""

from copy import deepcopy
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tests.contracts.test_evidence_support import link_fixture
from tools.dev.process import run
from tools.evidence import candidate, reproduce
from tools.evidence.common import EvidenceError, atomic_json, digest, file_identity
from tools.evidence.identity import canonical_digest
from tools.measurement import elf as elf_tools
from tools.measurement import resources


class ImageArtifactTests(unittest.TestCase):
    def setUp(self):
        retained = os.environ.get("NEXUS_OFFSET_ARTIFACT_TEST_ROOT")
        if retained:
            self.root = Path(retained) / self._testMethodName
            self.root.mkdir(parents=True)
        else:
            temporary = tempfile.TemporaryDirectory()
            self.addCleanup(temporary.cleanup)
            self.root = Path(temporary.name)

    def fixture(self, *, offset=0x20000, size=0x20000, outside=None,
                family="stm32f407", vector_prefix=False):
        compiler = os.environ.get("NEXUS_ARM_GCC") or shutil.which("arm-none-eabi-gcc")
        self.assertIsNotNone(compiler, "Prepared ARM compiler required")
        path = self.root / "image"
        path.mkdir()
        elf, config, budget = link_fixture(path)
        resolved = json.loads(config.read_text())
        resolved["soc_family"] = family
        blocks = [16384] * 4 + [65536] + [131072] * 7
        resolved["flash"]["erase_blocks"] = blocks
        resolved["layout"] = {
            **resolved["flash"], "id": "application", "kind": "image",
            "origin": resolved["flash"]["origin"] + offset, "size": size}
        source = path / "image.c"
        text = source.read_text().replace("vectors[]", "g_pfnVectors[]")
        if vector_prefix:
            text = text.replace(
                "const void* const g_pfnVectors[]",
                "const void* const vector_prefix[] = {&__nx_msp_end, Reset_Handler};\n"
                "__attribute__((section(\".isr_vector\"), used))\n"
                "const void* const g_pfnVectors[]")
        if outside is not None:
            text += ("\n__attribute__((section(\".outside\"), used))\n"
                     "const unsigned outside_image[4] = {1, 2, 3, 4};\n")
        source.write_text(text)
        resolved["inputs"][0]["sha256"] = digest(source)
        resolved.pop("configuration_sha256")
        resolved["configuration_sha256"] = canonical_digest(resolved)
        atomic_json(config, resolved)
        script = path / "memory.ld"
        text = script.read_text().replace(
            "ORIGIN = 0x08000000, LENGTH = 1024K",
            "ORIGIN = 0x%08x, LENGTH = %d" % (
                resolved["layout"]["origin"], size))
        if outside is not None:
            text = text.replace(
                "RAM (rw)",
                "EXTRA (rx) : ORIGIN = 0x%08x, LENGTH = 4096\nRAM (rw)" % outside)
            text = text.replace(".data :", ".outside : { KEEP(*(.outside)) } > EXTRA\n.data :")
        for index in range(8):
            text = re.sub(
                r"(__nx_resolved_sha256_%d = )0x[0-9a-f]+;" % index,
                r"\g<1>0x%s;" % resolved["configuration_sha256"][index * 8:index * 8 + 8],
                text)
        script.write_text(text)
        command = [compiler, "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16",
                   "-mfloat-abi=hard", "-nostdlib", "-Os", "-fno-toplevel-reorder",
                   "-Wl,-Map=" + str(path / "image.map"),
                   "-T", str(script), str(source), "-o", str(elf)]
        result = run(command, cwd=path, env=os.environ.copy(),
                     log=path / "compile.log", timeout=60,
                     stage="image-artifact-arm-fixture")
        self.assertEqual(result.returncode, 0, result.stderr)
        binary = path / "image.bin"
        objcopy = str(Path(compiler).with_name("arm-none-eabi-objcopy"))
        result = run([objcopy, "-O", "binary", str(elf), str(binary)],
                     cwd=path, env=os.environ.copy(), log=path / "objcopy.log",
                     timeout=30, stage="image-artifact-objcopy")
        self.assertEqual(result.returncode, 0, result.stderr)
        return elf, config, budget, binary

    def test_offset_resources_count_image_footprint_and_preserve_physical_flash(self):
        elf, config, budget, _ = self.fixture()
        before = config.read_bytes()
        try:
            report = resources.measure(elf, config, budget)
        except EvidenceError as error:
            self.fail("Reviewed offset allocation must validate: " + str(error))
        self.assertEqual(report["status"], "passed")
        self.assertLess(report["metrics"]["flash"]["footprint_bytes"], 4096)
        self.assertEqual(report["metrics"]["flash"]["capacity_bytes"], 0x20000)
        self.assertEqual(json.loads(config.read_text())["flash"]["size"], 0x100000)
        self.assertEqual(config.read_bytes(), before)

    def test_offset_reproduction_validates_actual_objcopy_bytes(self):
        elf, config, _, binary = self.fixture()
        artifacts = {"elf": file_identity(elf), "bin": file_identity(binary),
                     "map": file_identity(elf.with_suffix(".map"))}
        try:
            reproduce.validate_build_artifacts(config.parent, artifacts, "resolved.json")
        except EvidenceError as error:
            self.fail("Actual offset objcopy bytes must validate: " + str(error))

    def test_offset_candidate_reaches_required_qualification_rejection(self):
        elf, config, _, binary = self.fixture()
        lock = self.root / "fixture-lock.json"
        environment = self.root / "fixture-environment.json"
        atomic_json(lock, {"kind": "unit-test-input-only"})
        atomic_json(environment, {"kind": "mocked-environment-not-qualification"})
        manifest = {
            "schema_version": 1, "candidate_id": "offset-artifact-rejection",
            "source": {}, "dependencies": [file_identity(lock)],
            "environment": file_identity(environment),
            "configuration": file_identity(config),
            "artifacts": {"elf": file_identity(elf), "bin": file_identity(binary),
                          "map": file_identity(elf.with_suffix(".map"))},
            "qualifications": []}
        # Only unrelated source/environment boundaries are mocked. The actual
        # ELF/configuration/BIN must pass, and this never admits qualification.
        with patch.object(candidate, "verify_source"), \
                patch.object(candidate, "verify_environment"):
            with self.assertRaisesRegex(EvidenceError,
                                        "incomplete required software qualification"):
                candidate.validate_manifest(manifest)

    def test_offset_file_load_before_image_is_rejected(self):
        elf, config, budget, _ = self.fixture(outside=0x08010000)
        with self.assertRaisesRegex(EvidenceError, "selected Flash image"):
            resources.measure(elf, config, budget)

    def test_offset_file_load_after_image_is_rejected(self):
        elf, config, budget, _ = self.fixture(outside=0x08040000)
        with self.assertRaisesRegex(EvidenceError, "selected Flash image"):
            resources.measure(elf, config, budget)

    def test_reset_image_cannot_load_into_reserved_following_flash(self):
        elf, config, budget, _ = self.fixture(offset=0, outside=0x08030000)
        with self.assertRaisesRegex(EvidenceError, "selected Flash image"):
            resources.measure(elf, config, budget)

    def test_offset_vector_prefix_cannot_displace_vendor_table(self):
        elf, config, budget, _ = self.fixture(vector_prefix=True)
        linked = elf_tools.Elf32(elf.read_bytes())
        vectors = next(section for section in linked.sections
                       if section["name"] == ".isr_vector")
        self.assertNotEqual(linked.symbol("g_pfnVectors"), vectors["address"])
        with self.assertRaisesRegex(EvidenceError, "startup vector"):
            resources.measure(elf, config, budget)

    def test_unreviewed_family_cannot_gain_offset_support_from_artifact_metadata(self):
        elf, config, budget, _ = self.fixture(family="stm32f103")
        with self.assertRaisesRegex(EvidenceError, "reviewed.*offset|offset.*reviewed"):
            resources.measure(elf, config, budget)

    def test_default_full_reset_image_retains_existing_accounting(self):
        elf, config, budget, _ = self.fixture(offset=0, size=0x100000)
        report = resources.measure(elf, config, budget)
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["metrics"]["flash"]["capacity_bytes"], 0x100000)
        self.assertEqual(report["metrics"]["reservations"]["msp"]["bytes"], 1024)

    def test_window_metadata_is_strict_and_physical_geometry_remains_authoritative(self):
        selected = getattr(elf_tools, "image_window", None)
        self.assertTrue(callable(selected), "Shared image-window validation is missing")
        _, config, _, _ = self.fixture()
        original = json.loads(config.read_text())
        for key, value in (("origin", True), ("origin", -1),
                           ("origin", 2 ** 32), ("size", 0),
                           ("size", 2 ** 32), ("size", 0x100000)):
            changed = deepcopy(original)
            changed["layout"][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(EvidenceError):
                selected(changed)
        for origin, size in ((0x08020004, 0x1fffc), (0x08020000, 0x20004)):
            changed = deepcopy(original)
            changed["layout"].update(origin=origin, size=size)
            with self.subTest(origin=origin, size=size), self.assertRaises(EvidenceError):
                selected(changed)
        changed = deepcopy(original)
        changed["layout"]["erase_blocks"] = [changed["flash"]["size"]]
        with self.assertRaises(EvidenceError):
            selected(changed)
        for value in (8, 4.0, True):
            changed = deepcopy(original)
            changed["flash"]["program_unit"] = 4
            changed["layout"]["program_unit"] = value
            with self.subTest(program_unit=value), self.assertRaises(EvidenceError):
                selected(changed)
        changed = deepcopy(original)
        changed["part"] = "STM32F103RBT6"
        with self.assertRaisesRegex(EvidenceError, "reviewed"):
            selected(changed)


if __name__ == "__main__":
    unittest.main()
