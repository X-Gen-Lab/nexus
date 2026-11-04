"""Linux monitor rates retain terminal ownership and restoration."""

from contextlib import contextmanager
import errno
import fcntl
import json
import os
from pathlib import Path
import pty
import sys
import termios
import tempfile
import threading
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.dev import device
from tools.hil import serial_echo


class DeveloperSerialRateTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.target_path = self.root / "target.json"
        self.target = {
            "schema": 1, "kind": "nexus_development_target",
            "board": "software-only-board",
            "station_lock": str(self.root / "station.lock"),
            "serial": {"path": "/dev/serial/by-id/software-only-monitor",
                       "baudrate": 9600},
        }
        self.write_target()

    def write_target(self):
        self.target_path.write_text(json.dumps(self.target))

    def plan(self):
        return device.plan("monitor", self.target_path, timeout=1)

    def terminal(self):
        master, slave = pty.openpty()
        self.addCleanup(os.close, slave)
        self.addCleanup(os.close, master)
        original = termios.tcgetattr(slave)
        original[4] = original[5] = termios.B38400
        termios.tcsetattr(slave, termios.TCSANOW, original)
        return slave, Path(os.ttyname(slave)), termios.tcgetattr(slave)

    def test_plan_keeps_explicit_supported_rate_without_opening_equipment(self):
        for baudrate in (9600, 19200, 115200):
            with self.subTest(baudrate=baudrate):
                self.target["serial"]["baudrate"] = baudrate
                self.write_target()
                with patch("os.open", side_effect=AssertionError("opened")):
                    result = self.plan()
                self.assertEqual(result["serial"]["baudrate"], baudrate)
                self.assertEqual(result["command"][-3:], [
                    self.target["serial"]["path"], "1", str(baudrate)])
                self.assertEqual(result["physical_status"], "not_executed")
                self.assertEqual(result["image_linkage"], "unassociated")

    def test_invalid_or_nonstandard_rates_reject_before_device_access(self):
        for baudrate in (True, False, 0, -9600, "9600", 9600.0, None, 123456):
            with self.subTest(baudrate=baudrate):
                self.target["serial"]["baudrate"] = baudrate
                self.write_target()
                with patch("os.open", side_effect=AssertionError("opened")):
                    with self.assertRaisesRegex(ValueError, "baud|rate"):
                        self.plan()

    def test_host_unavailable_rate_rejects_before_alias_resolution(self):
        with patch.object(termios, "B9600", None), patch.object(
                serial_echo, "_alias_target",
                side_effect=AssertionError("device identity accessed")):
            with self.assertRaisesRegex(ValueError, "baud|rate"):
                self.plan()
            with self.assertRaisesRegex(ValueError, "baud|rate"):
                with serial_echo._device(
                        self.target["serial"]["path"], baudrate=9600):
                    self.fail("unsupported rate opened a terminal")

    def test_pty_uses_selected_speed_and_restores_all_terminal_attributes(self):
        slave, path, original = self.terminal()
        with patch.object(serial_echo, "_alias_target", return_value=path):
            with serial_echo._device(
                    self.target["serial"]["path"], baudrate=9600) as (
                        descriptor, observed):
                attributes = termios.tcgetattr(descriptor)
                self.assertEqual(attributes[4:6], [termios.B9600] * 2)
                self.assertEqual(observed["baudrate"], 9600)
                self.assertEqual(attributes[2] & termios.CSIZE, termios.CS8)
                self.assertFalse(attributes[2] & (termios.PARENB |
                                                 termios.CSTOPB))
                unexpected = None
                try:
                    with self.assertRaises(OSError) as failure:
                        unexpected = os.open(
                            path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
                finally:
                    if unexpected is not None:
                        os.close(unexpected)
                self.assertEqual(failure.exception.errno, errno.EBUSY)
        self.assertEqual(termios.tcgetattr(slave), original)
        descriptor = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        os.close(descriptor)

    def test_default_primitive_rate_preserves_fixed_hil_console_contract(self):
        slave, path, original = self.terminal()
        with patch.object(serial_echo, "_alias_target", return_value=path):
            with serial_echo._device(self.target["serial"]["path"]) as (
                    descriptor, observed):
                self.assertEqual(termios.tcgetattr(descriptor)[4:6],
                                 [termios.B115200] * 2)
                self.assertEqual(observed["baudrate"], 115200)
        self.assertEqual(termios.tcgetattr(slave), original)

    def test_interruption_restores_terminal_and_releases_exclusive_access(self):
        slave, path, original = self.terminal()
        with patch.object(serial_echo, "_alias_target", return_value=path):
            with self.assertRaises(KeyboardInterrupt):
                with serial_echo._device(
                        self.target["serial"]["path"], baudrate=9600):
                    raise KeyboardInterrupt
        self.assertEqual(termios.tcgetattr(slave), original)
        descriptor = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        os.close(descriptor)

    def test_driver_rejection_is_not_silently_retried_at_another_rate(self):
        slave, path, original = self.terminal()
        real_set = termios.tcsetattr
        speeds = []

        def rejecting_set(descriptor, when, attributes):
            speeds.append(attributes[4])
            if attributes[4] == termios.B9600:
                raise termios.error(errno.EINVAL, "driver rejected baud")
            return real_set(descriptor, when, attributes)

        with patch.object(serial_echo, "_alias_target", return_value=path):
            with patch.object(termios, "tcsetattr", rejecting_set):
                with self.assertRaises(termios.error):
                    with serial_echo._device(
                            self.target["serial"]["path"], baudrate=9600):
                        self.fail("driver rejection was ignored")
        self.assertEqual(speeds, [termios.B9600, termios.B38400])
        self.assertEqual(termios.tcgetattr(slave), original)

    def test_ignored_speed_setting_fails_readback_and_restores_terminal(self):
        slave, path, original = self.terminal()
        real_set = termios.tcsetattr

        def ignoring_set(descriptor, when, attributes):
            if attributes[4] != termios.B9600:
                return real_set(descriptor, when, attributes)
            return None

        with patch.object(serial_echo, "_alias_target", return_value=path):
            with patch.object(termios, "tcsetattr", ignoring_set):
                with self.assertRaisesRegex(ValueError, "baud|speed|rate"):
                    with serial_echo._device(
                            self.target["serial"]["path"], baudrate=9600):
                        self.fail("unapplied speed was reported as configured")
        self.assertEqual(termios.tcgetattr(slave), original)

    def test_monitor_forwards_rate_and_keeps_eof_a_failure(self):
        descriptor, writer = os.pipe()
        self.addCleanup(os.close, descriptor)
        os.close(writer)
        rates = []

        @contextmanager
        def disconnected(name, *, baudrate=115200, read_only=False,
                         expected_device=None, phase_control=None):
            self.assertEqual(name, self.target["serial"]["path"])
            self.assertTrue(read_only)
            self.assertIsNone(expected_device)
            self.assertIsNone(phase_control)
            rates.append(baudrate)
            yield descriptor, {}

        with patch.object(serial_echo, "_device", disconnected):
            with self.assertRaisesRegex(ValueError, "disconnect|EOF"):
                device._monitor(self.target["serial"]["path"], 1, 9600)
        self.assertEqual(rates, [9600])

    def test_changed_rate_invalidates_prepared_operation_before_launch(self):
        prepared = self.plan()
        self.target["serial"]["baudrate"] = 19200
        self.write_target()
        with patch.object(device.process, "run",
                          side_effect=AssertionError("launched")):
            with self.assertRaisesRegex(ValueError, "changed"):
                device.execute(prepared, self.root / "run")
        self.assertFalse(Path(self.target["station_lock"]).exists())

    def test_unreviewed_host_is_rejected_without_claiming_serial_support(self):
        with patch.object(device.sys, "platform", "darwin"):
            with self.assertRaisesRegex(ValueError, "Linux"):
                self.plan()

    def interrupted_monitor(self, mode):
        slave, path, original = self.terminal()
        marker = self.root / "helper-ready"
        real_run = device.process.run
        cancel = threading.Event()
        stop = threading.Event()
        helper = """
from contextlib import contextmanager
from pathlib import Path
import sys
import termios
sys.path.insert(0, sys.argv[1])
from tools.dev import device
from tools.hil import serial_echo
serial_echo._alias_target = lambda name: Path(sys.argv[2])
original_device = serial_echo._device
@contextmanager
def observed_device(name, **options):
    with original_device(name, **options) as opened:
        descriptor, _ = opened
        Path(sys.argv[4]).write_text(str(termios.tcgetattr(descriptor)[4]))
        yield opened
serial_echo._device = observed_device
import json
control = json.loads(sys.argv[5])
device._monitor(sys.argv[3], 30, 9600,
                expected_device=control["device"], phase_control=control["phase"])
"""

        def execute_helper(command, **options):
            self.assertIn("_monitor", command)
            self.assertEqual(command[-2], "9600")
            options["cancel_event"] = cancel
            options["timeout"] = 0.4 if mode == "timeout" else 3
            return real_run([
                sys.executable, "-B", "-c", helper, str(device.ROOT), str(path),
                self.target["serial"]["path"], str(marker), command[-1]],
                **options)

        def cancel_after_configuration():
            deadline = time.monotonic() + 2
            while not stop.is_set() and time.monotonic() < deadline:
                if marker.exists():
                    cancel.set()
                    return
                stop.wait(0.002)
            cancel.set()

        thread = None
        if mode == "cancel":
            thread = threading.Thread(target=cancel_after_configuration)
            thread.start()
        try:
            with patch.object(serial_echo, "_alias_target", return_value=path):
                with patch.object(device.process, "run", execute_helper):
                    report = device.execute(self.plan(), self.root / mode)
        finally:
            stop.set()
            if thread is not None:
                thread.join(timeout=3)
                self.assertFalse(thread.is_alive())
            # A RED implementation can leave TIOCEXCL set on this test PTY.
            self.addCleanup(fcntl.ioctl, slave, termios.TIOCNXCL)
        self.assertTrue(marker.is_file(), "monitor never configured its PTY")
        self.assertEqual(int(marker.read_text()), termios.B9600)
        self.assertEqual(termios.tcgetattr(slave), original)
        descriptor = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        os.close(descriptor)
        self.assertEqual(report["exit_code"],
                         124 if mode == "timeout" else 130)
        self.assertTrue(report["station_quarantined"])
        self.assertEqual(report["terminal_restore"]["status"], "restored")
        self.assertEqual(report["terminal_restore"]["errors"], [])
        receipt = json.loads(Path(report["receipt"]).read_text())
        self.assertEqual(receipt["outcome"],
                         "timed_out" if mode == "timeout" else "cancelled")
        self.assertTrue(receipt["withdrawal_complete"])

    def test_cancelled_monitor_restores_pty_after_helper_withdrawal(self):
        self.interrupted_monitor("cancel")

    def test_owned_monitor_timeout_restores_pty_after_helper_withdrawal(self):
        self.interrupted_monitor("timeout")

    def test_parent_restore_failure_is_reported_and_quarantines_station(self):
        slave, path, _ = self.terminal()
        real_run = device.process.run

        def completed_helper(command, **options):
            self.claim_ownership(command)
            fcntl.ioctl(slave, termios.TIOCEXCL)
            return real_run([sys.executable, "-B", "-c", "print('completed')"],
                            **options)

        with patch.object(serial_echo, "_alias_target", return_value=path):
            with patch.object(device.process, "run", completed_helper):
                with patch.object(termios, "tcsetattr", side_effect=
                                  termios.error(errno.EIO, "disconnected")):
                    report = device.execute(self.plan(), self.root / "restore")
        self.addCleanup(fcntl.ioctl, slave, termios.TIOCNXCL)
        self.assertEqual(report["child_exit_code"], 0)
        self.assertNotEqual(report["exit_code"], 0)
        self.assertTrue(report["station_quarantined"])
        self.assertEqual(report["terminal_restore"]["status"], "failed")
        self.assertIn("disconnected", str(report["terminal_restore"]["errors"]))

    def test_byid_change_never_restores_the_replacement_device(self):
        slave, path, original = self.terminal()
        other_slave, other_path, other_original = self.terminal()
        selected = {"path": path}
        real_run = device.process.run

        def changed_alias(command, **options):
            self.claim_ownership(command)
            fcntl.ioctl(slave, termios.TIOCEXCL)
            attributes = termios.tcgetattr(slave)
            attributes[4] = attributes[5] = termios.B9600
            termios.tcsetattr(slave, termios.TCSANOW, attributes)
            selected["path"] = other_path
            return real_run([sys.executable, "-B", "-c", "print('completed')"],
                            **options)

        with patch.object(serial_echo, "_alias_target",
                          side_effect=lambda name: selected["path"]):
            with patch.object(device.process, "run", changed_alias):
                report = device.execute(self.plan(), self.root / "alias-change")
        self.assertEqual(termios.tcgetattr(slave), original)
        self.assertEqual(termios.tcgetattr(other_slave), other_original)
        self.assertNotEqual(report["exit_code"], 0)
        self.assertTrue(report["station_quarantined"])
        self.assertIn("identity changed", str(report["terminal_restore"]))

    def test_incomplete_withdrawal_keeps_terminal_unchanged_and_exclusive(self):
        slave, path, original = self.terminal()
        receipt = self.root / "incomplete-withdrawal.json"
        receipt.write_text(json.dumps({"withdrawal_complete": False,
                                       "outcome": "timed_out",
                                       "exit_code": 124}))

        def incomplete_withdrawal(command, **options):
            self.claim_ownership(command)
            del options
            attributes = termios.tcgetattr(slave)
            attributes[4] = attributes[5] = termios.B9600
            termios.tcsetattr(slave, termios.TCSANOW, attributes)
            fcntl.ioctl(slave, termios.TIOCEXCL)
            return SimpleNamespace(returncode=124, stdout="", stderr="",
                                   receipt_path=receipt)

        try:
            with patch.object(serial_echo, "_alias_target", return_value=path):
                with patch.object(device.process, "run", incomplete_withdrawal):
                    report = device.execute(self.plan(), self.root / "owned")
            self.assertEqual(termios.tcgetattr(slave)[4:6],
                             [termios.B9600] * 2)
            unexpected = None
            try:
                with self.assertRaises(OSError) as failure:
                    unexpected = os.open(
                        path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            finally:
                if unexpected is not None:
                    os.close(unexpected)
            self.assertEqual(failure.exception.errno, errno.EBUSY)
            self.assertTrue(report["station_quarantined"])
            self.assertEqual(report["terminal_restore"]["status"], "failed")
            self.assertIn("withdrawal", str(report["terminal_restore"]))
        finally:
            fcntl.ioctl(slave, termios.TIOCNXCL)
            termios.tcsetattr(slave, termios.TCSANOW, original)

    def test_helper_rejects_byid_replacement_after_parent_snapshot(self):
        slave, path, original = self.terminal()
        other_slave, other_path, other_original = self.terminal()
        selected = {"path": path}
        real_run = device.process.run
        helper = """
from pathlib import Path
import json
import runpy
import sys
sys.path.insert(0, sys.argv[1])
from tools.hil import serial_echo
replacement = Path(sys.argv[2])
serial_echo._alias_target = lambda name: replacement
arguments = json.loads(sys.argv[3])
sys.argv = ["device.py", *arguments]
runpy.run_module("tools.dev.device", run_name="__main__", alter_sys=True)
"""

        def replace_before_child_open(command, **options):
            selected["path"] = other_path
            options["timeout"] = 0.4
            return real_run([
                sys.executable, "-B", "-c", helper, str(device.ROOT),
                str(other_path), json.dumps(command[3:])], **options)

        try:
            with patch.object(serial_echo, "_alias_target",
                              side_effect=lambda name: selected["path"]):
                with patch.object(device.process, "run",
                                  replace_before_child_open):
                    report = device.execute(self.plan(), self.root / "replaced")
            self.assertEqual(termios.tcgetattr(other_slave), other_original)
            self.assertEqual(termios.tcgetattr(slave), original)
            descriptor = os.open(other_path, os.O_RDWR | os.O_NOCTTY |
                                 os.O_NONBLOCK)
            os.close(descriptor)
            self.assertEqual(report["child_exit_code"], 2)
            self.assertTrue(report["station_quarantined"])
        finally:
            for descriptor, attributes in (
                    (slave, original), (other_slave, other_original)):
                fcntl.ioctl(descriptor, termios.TIOCNXCL)
                termios.tcsetattr(descriptor, termios.TCSANOW, attributes)

    def binding(self, path):
        observed = path.stat()
        return {"path": str(path), "st_dev": observed.st_dev,
                "st_ino": observed.st_ino, "st_rdev": observed.st_rdev}

    def claim_ownership(self, command):
        control = json.loads(command[-1])
        record_path = Path(control["phase"]["path"])
        record = json.loads(record_path.read_text())
        record.update(phase="owned", sequence=1)
        record_path.write_text(json.dumps(record))

    def test_tampered_binding_rejects_before_terminal_access(self):
        slave, path, original = self.terminal()
        binding = self.binding(path)
        changes = ({"path": "/dev/pts/../pts/0"}, {"st_ino": True},
                   {"st_rdev": binding["st_rdev"] + 1}, {"extra": 1})
        with patch.object(serial_echo, "_alias_target", return_value=path):
            for change in changes:
                with self.subTest(change=change):
                    with patch("os.open", side_effect=
                               AssertionError("terminal unexpectedly opened")):
                        with self.assertRaises(ValueError):
                            with serial_echo._device(
                                    self.target["serial"]["path"],
                                    baudrate=9600, read_only=True,
                                    expected_device={**binding, **change}):
                                self.fail("changed binding was admitted")
        self.assertEqual(termios.tcgetattr(slave), original)

    def test_opened_replacement_is_read_only_and_rejected_before_config(self):
        slave, path, original = self.terminal()
        other_slave, other_path, other_original = self.terminal()
        binding = self.binding(path)
        real_open = os.open
        flags_seen = []

        def replaced_open(name, flags):
            self.assertEqual(Path(name), path)
            flags_seen.append(flags)
            return real_open(other_path, flags)

        with patch.object(serial_echo, "_alias_target", return_value=path):
            with patch("os.open", replaced_open):
                with self.assertRaisesRegex(ValueError, "identity changed"):
                    with serial_echo._device(
                            self.target["serial"]["path"], baudrate=9600,
                            read_only=True, expected_device=binding):
                        self.fail("replacement fd was admitted")
        self.assertEqual(len(flags_seen), 1)
        self.assertEqual(flags_seen[0] & os.O_ACCMODE, os.O_RDONLY)
        self.assertEqual(termios.tcgetattr(slave), original)
        self.assertEqual(termios.tcgetattr(other_slave), other_original)
        descriptor = os.open(other_path, os.O_RDWR | os.O_NOCTTY |
                             os.O_NONBLOCK)
        os.close(descriptor)


class UnexpectedSuccessfulOpenFDTests(unittest.TestCase):
    def check_existing_case(self, method_name):
        existing = DeveloperSerialRateTests(method_name)
        original_terminal = existing.terminal
        original_open = os.open
        terminal = {}
        returned = []
        result = unittest.TestResult()

        def observed_terminal():
            value = original_terminal()
            terminal.update(slave=value[0], path=value[1])
            return value

        def one_unexpected_success(path, flags, *arguments, **keywords):
            expected = os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK
            if (flags == expected and terminal and not returned
                    and Path(path) == terminal["path"]):
                descriptor = os.dup(terminal["slave"])
                returned.append(descriptor)
                return descriptor
            return original_open(path, flags, *arguments, **keywords)

        existing.terminal = observed_terminal
        try:
            # The production device open also requires CLOEXEC and NOFOLLOW.
            # Only the test's exact expected-EBUSY open may unexpectedly succeed.
            with patch.object(os, "open", new=one_unexpected_success):
                existing.run(result)
            self.assertEqual(result.testsRun, 1)
            self.assertEqual(result.errors, [])
            self.assertEqual(len(result.failures), 1)
            self.assertIn("OSError not raised", result.failures[0][1])
            self.assertEqual(len(returned), 1)
            with self.assertRaises(OSError) as closed:
                os.fstat(returned[0])
            self.assertEqual(closed.exception.errno, errno.EBADF)
        finally:
            for descriptor in returned:
                try:
                    os.close(descriptor)
                except OSError as error:
                    if error.errno != errno.EBADF:
                        raise

    def test_expected_busy_failure_closes_unexpected_fd_in_speed_case(self):
        self.check_existing_case(
            "test_pty_uses_selected_speed_and_restores_all_terminal_attributes")

    def test_expected_busy_failure_closes_unexpected_fd_in_incomplete_withdrawal_case(
            self):
        self.check_existing_case(
            "test_incomplete_withdrawal_keeps_terminal_unchanged_and_exclusive")

if __name__ == "__main__":
    unittest.main()
