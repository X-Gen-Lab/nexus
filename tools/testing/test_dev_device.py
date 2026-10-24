"""Daily hardware commands bind explicit equipment and actual build bytes."""

import importlib
from contextlib import contextmanager
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.dev import process, qualification
from tools.evidence.common import file_identity
from tools.evidence.identity import canonical_digest


class DeveloperDeviceTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.tool = self.root / "synthetic-openocd"
        self.tool.write_text("#!/bin/sh\nprintf '%s\\n' \"$@\"\nexit 0\n")
        self.tool.chmod(0o700)
        interface = self.root / "interface.cfg"
        target = self.root / "target.cfg"
        interface.write_text("# synthetic test interface\n")
        target.write_text("# synthetic test target\n")
        self.target = {
            "schema": 1, "kind": "nexus_development_target",
            "board": "synthetic-board", "part": "synthetic-part",
            "station_lock": str(self.root / "station.lock"),
            "probe": {"serial": "probe-123", "transport": "swd",
                      "tool": file_identity(self.tool),
                      "interface_config": file_identity(interface),
                      "target_config": file_identity(target)},
            "serial": {"path": "/dev/serial/by-id/synthetic-123",
                       "baudrate": 115200}}
        self.target_path = self.root / "target.json"
        self.write_target()
        self.elf = self.root / 'image [literal];$name.elf'
        self.elf.write_bytes(b"\x7fELF\x01\x01" + b"\0" * 12 + b"\x28\0")
        self.resolved = self.root / "resolved.json"
        resolved = {"schema_version": 1, "board": "synthetic-board",
                    "part": "synthetic-part"}
        resolved["configuration_sha256"] = canonical_digest(resolved)
        self.resolved.write_text(json.dumps(resolved))
        command = [sys.executable, "-c", "print('synthetic build receipt')"]
        result = process.run(command, cwd=self.root, env=os.environ,
                             log=self.root / "build.log", stage="build")
        receipt = json.loads(result.receipt_path.read_text())
        provenance = qualification.provenance(
            {"kind": "synthetic"}, [self.resolved], [self.elf], [command],
            configuration=self.resolved)
        provenance["action"] = "build"
        provenance["actual_execution"] = {
            "receipt": file_identity(result.receipt_path),
            "stdout_stderr": [receipt["stdout"], receipt["stderr"]],
            "duration_seconds": receipt["duration_seconds"]}
        self.provenance = self.root / "build-provenance.json"
        self.provenance.write_text(json.dumps(provenance))

    def write_target(self):
        self.target_path.write_text(json.dumps(self.target))

    def module(self):
        return importlib.import_module("tools.dev.device")

    def plan(self, action="flash"):
        return self.module().plan(action, self.target_path, self.provenance,
                                  elf=self.elf, timeout=2, port=3333)

    def test_plan_binds_probe_and_quotes_tcl_without_running_equipment(self):
        with patch("subprocess.Popen") as execute:
            result = self.plan()
        execute.assert_not_called()
        self.assertEqual(result["physical_status"], "not_executed")
        self.assertIn('adapter serial "probe-123"', result["command"])
        self.assertIn('program "' + str(self.elf).replace("[", "\\[").replace(
            "]", "\\]").replace("$", "\\$") + '" verify reset exit',
            result["command"])
        self.assertFalse(Path(self.target["station_lock"]).exists())

    def test_probe_selector_must_be_explicit(self):
        self.target["probe"]["serial"] = ""
        self.write_target()
        with self.assertRaisesRegex(ValueError, "probe"):
            self.plan()

    def test_target_part_mismatch_rejects_before_execution(self):
        self.target["part"] = "different-part"
        self.write_target()
        with self.assertRaisesRegex(ValueError, "part|target"):
            self.plan()

    def test_changed_elf_is_not_the_built_artifact(self):
        self.elf.write_bytes(self.elf.read_bytes() + b"changed")
        with self.assertRaises(ValueError):
            self.plan()

    def test_explicit_elf_parent_alias_matches_without_hiding_symlinks(self):
        holder = self.root / "holder"
        holder.mkdir()
        result = self.module().plan(
            "flash", self.target_path, self.provenance,
            elf=holder / ".." / self.elf.name, timeout=2)
        self.assertEqual(result["elf"], file_identity(self.elf))
        link = self.root / "alias"
        link.symlink_to(holder, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.module().plan("flash", self.target_path, self.provenance,
                               elf=link / ".." / self.elf.name, timeout=2)

    def test_debug_disables_external_control_and_has_finite_budget(self):
        result = self.plan("debug")
        self.assertIn("bindto 127.0.0.1", result["command"])
        self.assertIn("gdb_port 3333", result["command"])
        self.assertIn("telnet_port disabled", result["command"])
        self.assertEqual(result["timeout_seconds"], 2)

    def test_actual_adapter_execution_preserves_exit_and_logs(self):
        output = self.root / "run"
        result = self.module().execute(self.plan(), output)
        self.assertEqual(result["exit_code"], 0)
        self.assertFalse(result["qualification"])
        self.assertEqual(result["physical_status"], "executed_unqualified")
        self.assertTrue(Path(result["receipt"]).is_file())
        self.assertFalse(Path(self.target["station_lock"]).exists())

    def test_nonzero_command_is_retained_and_station_stays_quarantined(self):
        self.tool.write_text("#!/bin/sh\necho observed-failure >&2\nexit 9\n")
        self.target["probe"]["tool"] = file_identity(self.tool)
        self.write_target()
        result = self.module().execute(self.plan(), self.root / "failed")
        self.assertEqual(result["exit_code"], 9)
        self.assertTrue(result["station_quarantined"])
        self.assertTrue(Path(self.target["station_lock"]).exists())
        receipt = json.loads(Path(result["receipt"]).read_text())
        self.assertIn("observed-failure",
                      Path(receipt["stderr"]["path"]).read_text())

    def test_existing_station_lock_blocks_all_equipment_commands(self):
        lock = Path(self.target["station_lock"])
        lock.write_text("another operator")
        with patch("subprocess.Popen") as execute:
            with self.assertRaisesRegex(ValueError, "lock|busy"):
                self.module().execute(self.plan(), self.root / "blocked")
        execute.assert_not_called()
        self.assertEqual(lock.read_text(), "another operator")

    def test_lost_lock_after_command_is_not_reported_as_success(self):
        lock = Path(self.target["station_lock"])
        real_run = process.run

        def changed_owner(*args, **kwargs):
            result = real_run(*args, **kwargs)
            lock.write_text("changed operator")
            return result

        with patch.object(process, "run", side_effect=changed_owner):
            result = self.module().execute(self.plan(), self.root / "lost")
        self.assertNotEqual(result["exit_code"], 0)
        self.assertEqual(result["child_exit_code"], 0)
        self.assertTrue(result["station_quarantined"])
        self.assertEqual(lock.read_text(), "changed operator")

    def test_changed_image_during_command_retains_failure_and_lock(self):
        real_run = process.run

        def changed_image(*args, **kwargs):
            result = real_run(*args, **kwargs)
            self.elf.write_bytes(self.elf.read_bytes() + b"changed")
            return result

        with patch.object(process, "run", side_effect=changed_image):
            result = self.module().execute(self.plan(), self.root / "changed")
        self.assertNotEqual(result["exit_code"], 0)
        self.assertTrue(result["station_quarantined"])
        self.assertIn("identity", result["validation_error"].lower())

    def test_monitor_plan_does_not_open_serial_and_reports_host_scope(self):
        with patch("os.open") as opened:
            result = self.plan("monitor")
        opened.assert_not_called()
        self.assertEqual(result["serial"]["baudrate"], 115200)
        self.assertIn("host", result["scope"])

    def test_swd_action_does_not_require_an_unrelated_serial_device(self):
        del self.target["serial"]
        self.write_target()
        self.assertIsNone(self.plan()["serial"])

    def test_monitor_without_probe_or_image_retains_unassociated_identity(self):
        del self.target["probe"]
        del self.target["part"]
        self.write_target()
        result = self.module().plan("monitor", self.target_path, None)
        self.assertIsNone(result["provenance"])
        self.assertIsNone(result["elf"])
        self.assertEqual(result["image_linkage"], "unassociated")
        self.assertIsNone(result["part"])

    def test_flash_without_build_identity_rejects_before_probe_access(self):
        with self.assertRaisesRegex(ValueError, "provenance|build"):
            self.module().plan("flash", self.target_path, None)

    def test_monitor_eof_reports_disconnect_instead_of_busy_loop(self):
        descriptor, writer = os.pipe()
        os.close(writer)
        self.addCleanup(os.close, descriptor)

        @contextmanager
        def disconnected_device(name):
            del name
            yield descriptor, {}

        with patch("tools.hil.serial_echo._device", disconnected_device):
            with self.assertRaisesRegex(ValueError, "disconnect|EOF"):
                self.module()._monitor("/dev/serial/by-id/synthetic", 1)


if __name__ == "__main__":
    unittest.main()
