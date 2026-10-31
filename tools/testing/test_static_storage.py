"""Storage subsets come from real linked objects and never double-count aliases."""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from tests.contracts.test_evidence_support import link_fixture
from tools.evidence.common import EvidenceError, atomic_json, digest
from tools.evidence.identity import canonical_digest
from tools.measurement.elf import Elf32
from tools.measurement.resources import measure
try:
    from tools.measurement.static_storage import inspect_storage
except ImportError:
    inspect_storage = None


class StaticStorageTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(inspect_storage, "linked private-state cost observer missing")
        self.storage = tempfile.TemporaryDirectory()
        self.addCleanup(self.storage.cleanup)
        retained = os.environ.get("NEXUS_STATIC_STORAGE_TEST_ROOT")
        self.root = (Path(retained) / self._testMethodName if retained
                     else Path(self.storage.name))
        self.root.mkdir(parents=True, exist_ok=True)
        self.compiler = os.environ.get("NEXUS_ARM_GCC") or shutil.which("arm-none-eabi-gcc")
        self.assertIsNotNone(self.compiler, "real ARM compiler required")
        self.elf, self.resolved, self.budget = link_fixture(self.root)

    def linked(self, code):
        source = self.root / "image.c"
        source.write_text(source.read_text() + "\n" + code)
        resolved = json.loads(self.resolved.read_text())
        resolved["inputs"][0]["sha256"] = digest(source)
        resolved["configuration_sha256"] = canonical_digest({key: value
            for key, value in resolved.items() if key != "configuration_sha256"})
        atomic_json(self.resolved, resolved)
        linker = self.root / "memory.ld"
        script = re.sub(r"__nx_resolved_sha256_\d+ = 0x[0-9a-f]+;", "",
                        linker.read_text())
        script += "\n".join(f"__nx_resolved_sha256_{i} = 0x"
            f"{resolved['configuration_sha256'][i*8:i*8+8]};" for i in range(8))
        linker.write_text(script)
        subprocess.run([self.compiler, "-mcpu=cortex-m4", "-mthumb",
            "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-nostdlib", "-Os",
            "-T", str(self.root / "memory.ld"), str(source), "-o", str(self.elf)],
            check=True, capture_output=True, timeout=30)
        metrics = measure(self.elf, self.resolved, self.budget)["metrics"]
        return Elf32(self.elf.read_bytes()), metrics

    def test_actual_private_state_buffers_rtos_stack_and_control_are_disjoint_subsets(self):
        elf, metrics = self.linked("""
volatile unsigned char s_nx_port_serial[32];
volatile unsigned char s_nx_rx_serial[64];
volatile unsigned char uxIdleTaskStack[128];
volatile unsigned char xIdleTaskTCB[24];
""")
        report = inspect_storage(elf, metrics)
        self.assertEqual(report["bytes"]["provider_state"], 32)
        self.assertEqual(report["bytes"]["buffers"], 64)
        self.assertEqual(report["bytes"]["rtos_stack"], 128)
        self.assertEqual(report["bytes"]["rtos_control"], 24)
        self.assertEqual(report["bytes"]["other_static"], 20)
        self.assertEqual(report["msp_reserved_bytes"], 1024)
        self.assertEqual(report["heap_reserved_bytes"], 64)
        self.assertEqual(report["dynamic_stack_high_water"], "not_measured")

    def test_same_class_exact_alias_is_counted_once(self):
        elf, metrics = self.linked("""
volatile unsigned char s_nx_port_serial[32];
extern volatile unsigned char s_nx_port_alias[32]
    __attribute__((alias("s_nx_port_serial")));
""")
        self.assertEqual(inspect_storage(elf, metrics)["bytes"]["provider_state"], 32)

    def test_cross_class_alias_is_rejected_instead_of_double_counted(self):
        elf, metrics = self.linked("""
volatile unsigned char s_nx_port_serial[32];
extern volatile unsigned char s_nx_rx_alias[32]
    __attribute__((alias("s_nx_port_serial")));
""")
        with self.assertRaises(EvidenceError):
            inspect_storage(elf, metrics)

    def test_read_only_object_cannot_be_misreported_as_private_ram(self):
        elf, metrics = self.linked("const unsigned char s_nx_port_serial[32] = {1};")
        with self.assertRaises(EvidenceError):
            inspect_storage(elf, metrics)

    def test_read_only_chip_select_face_is_separate_from_mutable_state(self):
        elf, metrics = self.linked("""
struct gpio_face { const void* operations; void* context; };
static const unsigned operations[] = {1};
volatile unsigned char s_nx_cs_device4[20];
__attribute__((used))
static const struct gpio_face s_nx_cs_face_device4 = {
    operations, (void*)s_nx_cs_device4
};
""")
        face, = (symbol for symbol in elf.defined_symbols
                 if symbol["name"] == "s_nx_cs_face_device4")
        self.assertEqual(face["binding"], 0)
        self.assertEqual(face["size"], 8)
        report = inspect_storage(elf, metrics)
        self.assertEqual(report["bytes"]["provider_state"], 20)
        self.assertEqual(report["bytes"]["other_static"], 20)
        self.assertEqual(report["read_only_chip_select_faces"], [{
            "name": "s_nx_cs_face_device4", "bytes": 8,
            "section": elf.sections[face["section"]]["name"]}])
        self.assertNotIn("s_nx_cs_face_device4",
                         [row["name"] for row in report["objects"]])

    def test_chip_select_face_cannot_be_admitted_as_writable_state(self):
        elf, metrics = self.linked(
            "volatile unsigned char s_nx_cs_face_device4[8];")
        with self.assertRaises(EvidenceError):
            inspect_storage(elf, metrics)

    def test_chip_select_state_still_requires_actual_writable_ram(self):
        elf, metrics = self.linked(
            "const unsigned char s_nx_cs_device4[20] = {1};")
        with self.assertRaises(EvidenceError):
            inspect_storage(elf, metrics)

    def test_read_only_chip_select_face_requires_two_arm_pointers(self):
        elf, metrics = self.linked(
            "const unsigned char s_nx_cs_face_device4[4] = {1};")
        with self.assertRaises(EvidenceError):
            inspect_storage(elf, metrics)

    def test_read_only_chip_select_face_requires_pointer_alignment(self):
        linker = self.root / "memory.ld"
        linker.write_text(linker.read_text().replace(
            ".text : { *(.text*) *(.rodata*) } > FLASH",
            ".text : { *(.text*) *(.rodata*) . = ALIGN(4); . += 1; "
            "KEEP(*(.cs_bad)) } > FLASH"))
        elf, metrics = self.linked("""
__attribute__((section(".cs_bad"), aligned(1)))
const unsigned char s_nx_cs_face_device4[8] = {1};
""")
        face, = (symbol for symbol in elf.defined_symbols
                 if symbol["name"] == "s_nx_cs_face_device4")
        self.assertEqual(face["value"] % 4, 1)
        with self.assertRaises(EvidenceError):
            inspect_storage(elf, metrics)

    def test_inconsistent_ram_metrics_cannot_make_subset_larger_than_total(self):
        elf, metrics = self.linked("volatile unsigned char s_nx_port_serial[32];")
        metrics["ram"]["sram"]["ranges"] = [[0x20000000, 0x20000004]]
        with self.assertRaises(EvidenceError):
            inspect_storage(elf, metrics)


if __name__ == "__main__":
    unittest.main()
