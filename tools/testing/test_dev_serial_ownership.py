"""A monitor may clean only its own explicitly acquired terminal session."""

from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import termios
import unittest
from unittest.mock import patch

from tools.dev import device
from tools.dev import serial as serial_echo
from tools.testing import test_dev_serial_rates as serial_tests


FOREIGN_HELPER = """
import fcntl
import json
import os
import sys
import termios
fd = os.open(sys.argv[1], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK |
             os.O_CLOEXEC)
original = termios.tcgetattr(fd)
owned = False
try:
    attributes = termios.tcgetattr(fd)
    attributes[4] = attributes[5] = termios.B19200
    termios.tcsetattr(fd, termios.TCSANOW, attributes)
    fcntl.ioctl(fd, termios.TIOCEXCL)
    owned = True
    print(json.dumps({"phase": "owned", "pid": os.getpid(),
                      "speed": termios.tcgetattr(fd)[4]}), flush=True)
    if json.loads(sys.stdin.readline()) != {"operation": "release"}:
        raise ValueError("explicit release required")
finally:
    termios.tcsetattr(fd, termios.TCSANOW, original)
    if owned:
        fcntl.ioctl(fd, termios.TIOCNXCL)
    os.close(fd)
    print(json.dumps({"phase": "released"}), flush=True)
"""

MONITOR_HELPER = """
from pathlib import Path
import json
import runpy
import sys
sys.path.insert(0, sys.argv[1])
from tools.dev import serial as serial_echo
target = Path(sys.argv[2])
serial_echo._alias_target = lambda name: target
arguments = json.loads(sys.argv[3])
sys.argv = ["device.py", *arguments]
runpy.run_module("tools.dev.device", run_name="__main__", alter_sys=True)
"""


class MonitorTerminalOwnershipTests(unittest.TestCase):
    setUp = serial_tests.DeveloperSerialRateTests.setUp
    write_target = serial_tests.DeveloperSerialRateTests.write_target
    plan = serial_tests.DeveloperSerialRateTests.plan
    terminal = serial_tests.DeveloperSerialRateTests.terminal

    def line(self, stream):
        self.assertTrue(select.select([stream], [], [], 5)[0],
                        "foreign helper handshake expired")
        return json.loads(stream.readline())

    @contextmanager
    def foreign_owner(self, path):
        child = subprocess.Popen(
            [sys.executable, "-B", "-c", FOREIGN_HELPER, str(path)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, env=os.environ.copy())
        try:
            ready = self.line(child.stdout)
            self.assertEqual(ready["phase"], "owned")
            self.assertEqual(ready["speed"], termios.B19200)
            yield ready
        finally:
            child.stdin.write(json.dumps({"operation": "release"}) + "\n")
            child.stdin.flush()
            child.stdin.close()
            released = self.line(child.stdout)
            child.wait(timeout=5)
            errors = child.stderr.read()
            child.stdout.close()
            child.stderr.close()
            self.assertEqual(released["phase"], "released")
            self.assertEqual(child.returncode, 0, errors)

    def execute_with_foreign(self, *, acquire_before_child, tamper_phase=False):
        slave, path, _ = self.terminal()
        sentinel, _, sentinel_original = self.terminal()
        real_run = device.process.run
        owners = []

        def launch(command, **options):
            def acquire():
                owner = self.foreign_owner(path)
                owner.__enter__()
                owners.append(owner)

            if acquire_before_child:
                acquire()
            result = real_run([
                sys.executable, "-B", "-c", MONITOR_HELPER, str(device.ROOT),
                str(path), json.dumps(command[3:])], **options)
            if not acquire_before_child:
                acquire()
            if tamper_phase:
                control = json.loads(command[-1])
                Path(control["phase"]["path"]).write_bytes(b"invalid phase")
            return result

        try:
            with patch.object(serial_echo, "_alias_target", return_value=path):
                with patch.object(device.process, "run", launch):
                    report = device.execute(self.plan(),
                                            self.root / "operation")
            self.assertEqual(termios.tcgetattr(slave)[4:6],
                             [termios.B19200] * 2,
                             "monitor parent changed a foreign terminal")
            with self.assertRaises(OSError):
                descriptor = os.open(path, os.O_RDONLY | os.O_NOCTTY |
                                     os.O_NONBLOCK)
                os.close(descriptor)
            self.assertEqual(termios.tcgetattr(sentinel), sentinel_original)
            return report
        finally:
            for owner in reversed(owners):
                owner.__exit__(None, None, None)

    def test_busy_child_cannot_authorize_parent_to_clear_foreign_owner(self):
        report = self.execute_with_foreign(acquire_before_child=True)
        self.assertEqual(report["child_exit_code"], 2)
        self.assertTrue(report["station_quarantined"])

    def test_released_child_never_authorizes_parent_to_repeat_terminal_cleanup(
            self):
        report = self.execute_with_foreign(acquire_before_child=False)
        self.assertEqual(report["child_exit_code"], 0)
        self.assertEqual(report["exit_code"], 0)

    def test_corrupt_phase_never_authorizes_parent_to_touch_foreign_terminal(
            self):
        report = self.execute_with_foreign(acquire_before_child=False,
                                          tamper_phase=True)
        self.assertEqual(report["child_exit_code"], 0)
        self.assertNotEqual(report["exit_code"], 0)
        self.assertTrue(report["station_quarantined"])
        self.assertIn("ownership record rejected", report["validation_error"])

    def test_phase_is_published_before_configuration_and_before_release(self):
        slave, path, original = self.terminal()
        binding = serial_tests.DeveloperSerialRateTests.binding(self, path)
        control = {"path": str(self.root / "phase.json"), "nonce": "a" * 32}
        serial_echo._begin_monitor_phase(control, binding)
        real_ioctl = fcntl.ioctl
        phases = []

        def checked_ioctl(descriptor, operation):
            phase = json.loads(Path(control["path"]).read_text())["phase"]
            if operation == termios.TIOCEXCL:
                self.assertEqual(phase, "acquiring")
            elif operation == termios.TIOCNXCL:
                self.assertEqual(phase, "releasing")
            phases.append(phase)
            return real_ioctl(descriptor, operation)

        with patch.object(serial_echo, "_alias_target", return_value=path):
            with patch.object(fcntl, "ioctl", checked_ioctl):
                with serial_echo._device(
                        self.target["serial"]["path"], baudrate=9600,
                        read_only=True, expected_device=binding,
                        phase_control=control):
                    state = serial_echo._read_monitor_phase(control, binding)
                    self.assertEqual(state["phase"], "owned")
                    self.assertEqual(termios.tcgetattr(slave)[4:6],
                                     [termios.B9600] * 2)
        self.assertEqual(phases, ["acquiring", "releasing"])
        self.assertEqual(termios.tcgetattr(slave), original)
        self.assertEqual(serial_echo._read_monitor_phase(control, binding)[
            "phase"], "released")

    def test_changed_nonce_rejects_before_terminal_access(self):
        _, path, _ = self.terminal()
        binding = serial_tests.DeveloperSerialRateTests.binding(self, path)
        control = {"path": str(self.root / "phase.json"), "nonce": "a" * 32}
        serial_echo._begin_monitor_phase(control, binding)
        record = json.loads(Path(control["path"]).read_text())
        record["nonce"] = "b" * 32
        Path(control["path"]).write_text(json.dumps(record))
        with patch.object(serial_echo, "_alias_target",
                          side_effect=AssertionError("terminal accessed")):
            with self.assertRaisesRegex(ValueError, "session|identity"):
                with serial_echo._device(
                        self.target["serial"]["path"], baudrate=9600,
                        read_only=True, expected_device=binding,
                        phase_control=control):
                    self.fail("changed nonce was accepted")

    def test_failed_restore_readback_cannot_publish_released(self):
        slave, path, original = self.terminal()
        binding = serial_tests.DeveloperSerialRateTests.binding(self, path)
        control = {"path": str(self.root / "phase.json"), "nonce": "a" * 32}
        serial_echo._begin_monitor_phase(control, binding)
        real_set = termios.tcsetattr

        def ignore_restore(descriptor, when, attributes):
            if attributes[4] != termios.B38400:
                return real_set(descriptor, when, attributes)
            return None

        try:
            with patch.object(serial_echo, "_alias_target", return_value=path):
                with patch.object(termios, "tcsetattr", ignore_restore):
                    with self.assertRaisesRegex(ValueError, "restor"):
                        with serial_echo._device(
                                self.target["serial"]["path"], baudrate=9600,
                                read_only=True, expected_device=binding,
                                phase_control=control):
                            pass
            self.assertEqual(serial_echo._read_monitor_phase(control, binding)[
                "phase"], "releasing")
        finally:
            fcntl.ioctl(slave, termios.TIOCNXCL)
            real_set(slave, termios.TCSANOW, original)

    def test_helper_retains_primary_eof_and_cleanup_phase_failure(self):
        slave, path, original = self.terminal()
        real_run = device.process.run
        helper = """
from pathlib import Path
import json
import os
import runpy
import select
import sys
sys.path.insert(0, sys.argv[1])
from tools.dev import serial as serial_echo
target = Path(sys.argv[2])
serial_echo._alias_target = lambda name: target
arguments = json.loads(sys.argv[3])
control = json.loads(arguments[-1])
reader, writer = os.pipe()
os.close(writer)
real_read = os.read
def disconnected_read(descriptor, maximum):
    Path(control["phase"]["path"]).write_bytes(b"invalid cleanup phase")
    return real_read(reader, maximum)
os.read = disconnected_read
select.select = lambda readers, writers, errors, timeout: (readers, [], [])
sys.argv = ["device.py", *arguments]
runpy.run_module("tools.dev.device", run_name="__main__", alter_sys=True)
"""

        def launch(command, **options):
            return real_run([
                sys.executable, "-B", "-c", helper, str(device.ROOT), str(path),
                json.dumps(command[3:])], **options)

        try:
            with patch.object(serial_echo, "_alias_target", return_value=path):
                with patch.object(device.process, "run", launch):
                    report = device.execute(self.plan(),
                                            self.root / "double-failure")
            receipt = json.loads(Path(report["receipt"]).read_text())
            raw = Path(receipt["stderr"]["path"]).read_text()
            self.assertIn("Serial monitor disconnected (EOF)", raw)
            self.assertIn("invalid JSON document", raw)
            self.assertEqual(report["child_exit_code"], 2)
            self.assertNotEqual(report["exit_code"], 0)
            self.assertTrue(report["station_quarantined"])
        finally:
            fcntl.ioctl(slave, termios.TIOCNXCL)
            termios.tcsetattr(slave, termios.TCSANOW, original)


if __name__ == "__main__":
    unittest.main()
