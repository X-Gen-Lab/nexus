"""Actual ARM CLI evidence uses the sole resolved CPU ABI, never ELF guessing."""
import contextlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from tests.contracts.test_evidence_support import link_fixture
from tools.evidence.common import EvidenceError, atomic_json, digest
from tools.evidence.identity import canonical_digest
from tools.measurement import resources, retention


BINDINGS = """static const nx_gpio_port_t s_nx_face_bus = {
    &nx_gd32_gpio_ops, &s_nx_port_bus
};
"""
IMAGE = """
typedef unsigned int u32;
extern u32 __nx_msp_end;
void Reset_Handler(void);
__attribute__((section(".isr_vector"), used))
const void* const vectors[] = {&__nx_msp_end, Reset_Handler};
volatile u32 initialized = 0x12345678;
volatile u32 zeroed[4];
int nx_gd32_gpio_write(void) { return 1; }
int nx_gd32_gpio_read(void) { return 2; }
int nx_gd32_gpio_toggle(void) { return 3; }
typedef struct { int (*write)(void); int (*read)(void); int (*toggle)(void); }
    nx_gpio_ops_t;
typedef struct { const nx_gpio_ops_t* ops; void* context; } nx_gpio_port_t;
const nx_gpio_ops_t nx_gd32_gpio_ops = {
    nx_gd32_gpio_write, nx_gd32_gpio_read, nx_gd32_gpio_toggle};
static u32 s_nx_port_bus;
__attribute__((used)) static const nx_gpio_port_t s_nx_face_bus = {
    &nx_gd32_gpio_ops, &s_nx_port_bus};
void Reset_Handler(void) { zeroed[0] = initialized; for (;;) {} }
"""


class ResolvedAbiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get("NEXUS_ARM_GCC") or shutil.which("arm-none-eabi-gcc")
        if not compiler:
            raise AssertionError("Actual ARM compiler is required for ABI tests")
        cls.compiler = str(Path(compiler).resolve())

    def setUp(self):
        self.storage = tempfile.TemporaryDirectory()
        self.addCleanup(self.storage.cleanup)
        self.root = Path(self.storage.name)
        self.elf, self.resolved, self.budget = link_fixture(self.root)
        self.bindings = self.root / "bindings.c"
        self.bindings.write_text(BINDINGS)
        self.report = self.root / "report.json"

    def prepare(self, selected, *, actual=None, cpu=True):
        actual = selected if actual is None else actual
        resolved = json.loads(self.resolved.read_text())
        if cpu:
            resolved["cpu"] = {"arch": "cortex-m3" if selected == "soft" else "cortex-m4",
                               "fpu": "none" if selected == "soft" else "fpv4-sp-d16",
                               "float_abi": selected}
        source = self.root / "image.c"
        source.write_text(IMAGE)
        resolved["inputs"][0]["sha256"] = digest(source)
        resolved["configuration_sha256"] = canonical_digest(
            {key: value for key, value in resolved.items()
             if key != "configuration_sha256"})
        atomic_json(self.resolved, resolved)
        linker = self.root / "memory.ld"
        text = re.sub(r"__nx_resolved_sha256_\d+ = 0x[0-9a-f]+;", "",
                      linker.read_text())
        text += "\n".join(f"__nx_resolved_sha256_{index} = 0x"
                         f"{resolved['configuration_sha256'][index*8:index*8+8]};"
                         for index in range(8))
        linker.write_text(text)
        command = [self.compiler, "-mcpu=" + ("cortex-m3" if actual == "soft" else "cortex-m4"),
                   "-mthumb", "-mfloat-abi=" + actual, "-nostdlib", "-Os",
                   "-T", str(linker), str(source), "-o", str(self.elf)]
        if actual == "hard":
            command.insert(3, "-mfpu=fpv4-sp-d16")
        subprocess.run(command, capture_output=True, check=True)

    def resources_cli(self, *extra):
        arguments = ["resources", "--elf", str(self.elf), "--resolved", str(self.resolved),
                     "--budget", str(self.budget), "--report", str(self.report), *extra]
        with mock.patch.object(sys, "argv", arguments), contextlib.redirect_stdout(io.StringIO()):
            return resources.main()

    def retention_cli(self, *, resolved=True):
        arguments = ["--elf", str(self.elf), "--bindings", str(self.bindings),
                     "--output", str(self.report)]
        if resolved:
            arguments += ["--resolved", str(self.resolved)]
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return retention.main(arguments)

    def test_resource_cli_admits_actual_soft_float_from_resolved_cpu(self):
        self.prepare("soft")
        self.assertEqual(self.resources_cli(), 0)
        self.assertEqual(json.loads(self.report.read_text())["status"], "passed")

    def test_resource_api_uses_soft_cpu_without_a_second_abi_authority(self):
        self.prepare("soft")
        self.assertEqual(resources.measure(self.elf, self.resolved, self.budget)["status"],
                         "passed")

    def test_resource_api_explicit_hard_override_cannot_hide_soft_ir(self):
        self.prepare("soft", actual="hard")
        with self.assertRaisesRegex(EvidenceError, "resolved CPU"):
            resources.measure(self.elf, self.resolved, self.budget, float_abi="hard")

    def test_resource_cli_rejects_hard_image_under_soft_ir(self):
        self.prepare("soft", actual="hard")
        self.assertEqual(self.resources_cli(), 1)
        self.assertEqual(json.loads(self.report.read_text())["status"], "failed")

    def test_resource_cli_rejects_soft_image_under_hard_ir(self):
        self.prepare("hard", actual="soft")
        self.assertEqual(self.resources_cli(), 1)

    def test_resource_cli_rejects_unknown_present_cpu_abi(self):
        self.prepare("hard")
        data = json.loads(self.resolved.read_text())
        data["cpu"]["float_abi"] = "softfp"
        data["configuration_sha256"] = canonical_digest(
            {key: value for key, value in data.items() if key != "configuration_sha256"})
        atomic_json(self.resolved, data)
        self.assertEqual(self.resources_cli(), 1)

    def test_current_firmware_resource_scope_requires_cpu_metadata(self):
        self.prepare("hard", cpu=False)
        self.assertEqual(self.resources_cli("--require-cpu"), 1)

    def test_legacy_cpu_absent_resource_scope_remains_explicitly_hard_only(self):
        self.prepare("hard", cpu=False)
        self.assertEqual(self.resources_cli(), 0)

    def test_retention_cli_admits_actual_soft_image_only_with_resolved_binding(self):
        self.prepare("soft")
        self.assertEqual(self.retention_cli(), 0)
        report = json.loads(self.report.read_text())
        self.assertEqual(report["face_count"], 1)
        self.assertEqual(report["method_count"], 3)
        self.assertEqual(report["resolved"]["sha256"], digest(self.resolved))

    def test_retention_cli_legacy_scope_does_not_autodetect_soft_image(self):
        self.prepare("soft")
        self.assertEqual(self.retention_cli(resolved=False), 1)
        self.assertFalse(self.report.exists())

    def test_retention_cli_keeps_legacy_hard_scope(self):
        self.prepare("hard")
        self.assertEqual(self.retention_cli(resolved=False), 0)

    def test_retention_cli_rejects_hard_image_under_soft_ir(self):
        self.prepare("soft", actual="hard")
        self.assertEqual(self.retention_cli(), 1)
        self.assertFalse(self.report.exists())

    def test_retention_cli_rejects_a_stale_configuration_digest(self):
        self.prepare("soft")
        data = json.loads(self.resolved.read_text())
        data["board"] = "foreign-board"
        atomic_json(self.resolved, data)
        self.assertEqual(self.retention_cli(), 1)

    def test_retention_cli_rejects_new_digest_not_bound_into_image(self):
        self.prepare("soft")
        data = json.loads(self.resolved.read_text())
        data["board"] = "foreign-board"
        data["configuration_sha256"] = canonical_digest(
            {key: value for key, value in data.items() if key != "configuration_sha256"})
        atomic_json(self.resolved, data)
        self.assertEqual(self.retention_cli(), 1)

    def test_retention_cli_cannot_overwrite_resolved_configuration(self):
        self.prepare("soft")
        before = self.resolved.read_bytes()
        self.report = self.resolved
        self.assertEqual(self.retention_cli(), 1)
        self.assertEqual(self.resolved.read_bytes(), before)

    def test_retention_cli_cannot_unlink_a_declared_source_input(self):
        self.prepare("soft")
        source = self.root / "image.c"
        before = source.read_bytes()
        self.report = source
        self.assertEqual(self.retention_cli(), 1)
        self.assertTrue(source.is_file())
        self.assertEqual(source.read_bytes(), before)

    def test_resource_cli_cannot_overwrite_a_declared_source_input(self):
        self.prepare("hard")
        source = self.root / "image.c"
        before = source.read_bytes()
        self.report = source
        with self.assertRaises(SystemExit) as stopped:
            self.resources_cli()
        self.assertEqual(stopped.exception.code, 1)
        self.assertEqual(source.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
