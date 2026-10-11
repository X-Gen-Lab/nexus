"""Real PTY I/O verifies serial tooling, never physical MCU qualification."""

import copy
import errno
import fcntl
import json
import os
from pathlib import Path
import pty
import select
import termios
import tempfile
import threading
import time
import tty
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.hil import serial_echo


class HilSerialEchoTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.master, self.slave = pty.openpty()
        self.addCleanup(os.close, self.slave)
        self.addCleanup(os.close, self.master)
        self.device = Path(os.ttyname(self.slave))
        tty.setraw(self.slave)
        original = termios.tcgetattr(self.slave)
        original[4] = original[5] = termios.B9600
        termios.tcsetattr(self.slave, termios.TCSANOW, original)
        os.set_blocking(self.master, False)
        self.station = {"serial": {
            "path": "/dev/serial/by-id/software-only-pty",
            "baudrate": 115200, "electrical_mode": "ttl_3v3"},
            "scope": "software_only; no MCU or physical station operated"}
        self.identities = {}
        for name in ("station", "fixture", "elf", "resolved", "resources"):
            path = self.root / (name + ".json")
            atomic_json(path, self.station if name == "station" else
                        {"scope": "software_only serial tool fixture"})
            self.identities[name] = file_identity(path)
        self.admission = {
            "schema_version": 1, "kind": "hil_admission", "status": "ready",
            "physical_status": "not_executed", **self.identities}
        self.admission_path = self.root / "admission.json"
        atomic_json(self.admission_path, self.admission)
        self.record = self.root / "serial.observation.json"
        self.session = "a1" * 16
        admission = patch("tools.hil.serial_echo.admit",
                          return_value=copy.deepcopy(self.admission),
                          create=True)
        self.mock_admit = admission.start()
        self.addCleanup(admission.stop)
        alias = patch("tools.hil.serial_echo._alias_target",
                      return_value=self.device)
        self.mock_alias = alias.start()
        self.addCleanup(alias.stop)
        self.stop = threading.Event()
        self.observed = bytearray()
        self.attributes = None
        self.open_errno = None
        self.thread_error = None

    def emulator(self, mode="echo"):
        def run():
            try:
                while not self.stop.is_set():
                    if mode == "noise":
                        os.write(self.master, b"stale")
                    ready = select.select([self.master], [], [], 0.002)[0]
                    if not ready:
                        continue
                    try:
                        data = os.read(self.master, 4096)
                    except BlockingIOError:
                        continue
                    if not data:
                        continue
                    self.attributes = termios.tcgetattr(self.slave)
                    try:
                        descriptor = os.open(self.device, os.O_RDWR |
                                             os.O_NOCTTY | os.O_NONBLOCK)
                    except OSError as error:
                        self.open_errno = error.errno
                    else:
                        os.close(descriptor)
                    self.observed.extend(data)
                    if mode == "silent":
                        continue
                    if mode == "partial":
                        data = data[:max(1, len(data) // 2)]
                    elif mode == "wrong":
                        data = bytes([data[0] ^ 1]) + data[1:]
                    for offset in range(0, len(data), 3):
                        os.write(self.master, data[offset:offset + 3])
                        time.sleep(0.001)
            except BaseException as error:
                self.thread_error = error

        thread = threading.Thread(target=run, name="software-only-pty-echo")
        thread.start()

        def finish():
            self.stop.set()
            thread.join(timeout=1)
            self.assertFalse(thread.is_alive(), "PTY emulator did not join")
            self.assertIsNone(self.thread_error)

        self.addCleanup(finish)

    def capture(self, **options):
        return serial_echo.capture_serial(
            self.admission_path, self.session, self.record,
            timeout_s=options.pop("timeout_s", 0.3),
            quiet_s=options.pop("quiet_s", 0.01), **options)

    def retained(self):
        return self.record.with_name(self.record.name + ".serial")

    def test_actual_fragmented_echo_retains_bytes_and_host_times(self):
        os.write(self.master, b"old console output")
        self.emulator()
        result = self.capture()
        self.assertEqual(result["kind"], "hil_adapter_observation")
        self.assertEqual(result["operation"], "serial")
        self.assertEqual(result["session"], self.session)
        self.assertEqual(set(result), {"schema_version", "kind", "operation",
                                       "session", "observations"})
        facts = result["observations"]
        self.assertEqual(facts["status"], "captured")
        self.assertEqual(facts["backend"], "posix_termios")
        tx = Path(facts["tx"]["path"]).read_bytes()
        rx = Path(facts["rx"]["path"]).read_bytes()
        self.assertEqual(len(tx), 32)
        self.assertEqual(tx, rx)
        self.assertEqual(tx, bytes(self.observed))
        self.assertEqual(facts["tx"], file_identity(facts["tx"]["path"]))
        self.assertEqual(facts["rx"], file_identity(facts["rx"]["path"]))
        self.assertEqual(Path(facts["drain"]["path"]).read_bytes(),
                         b"old console output")
        events = json.loads(Path(facts["events"]["path"]).read_text())
        self.assertEqual(events["clock"], "host_monotonic_ns")
        self.assertEqual([event["sequence"] for event in events["events"]],
                         list(range(len(events["events"]))))
        for event in events["events"]:
            self.assertGreater(event["started_host_ns"], 0)
            self.assertGreaterEqual(event["finished_host_ns"],
                                    event["started_host_ns"])
        self.assertGreater(facts["elapsed_host_us"], 0)
        self.assertIn("not MCU", facts["timing_scope"])
        self.assertEqual(json.loads(self.record.read_text()), result)
        self.assertEqual(self.attributes[4:6], [termios.B115200] * 2)
        self.assertEqual(self.attributes[0:2], [0, 0])
        self.assertEqual(self.attributes[3], 0)
        self.assertEqual(self.attributes[2] & termios.CSIZE, termios.CS8)
        self.assertFalse(self.attributes[2] & (termios.PARENB | termios.CSTOPB))
        self.assertEqual(self.open_errno, errno.EBUSY)
        self.assertEqual(self.mock_admit.call_count, 2)
        self.mock_admit.assert_called_with(*[
            Path(self.identities[name]["path"]) for name in
            ("station", "fixture", "elf", "resolved", "resources")])

    def test_real_partial_writes_are_retried_and_logged(self):
        self.emulator()

        def partial(descriptor, payload):
            return os.write(descriptor, payload[:5])

        with patch("tools.hil.serial_echo._write", side_effect=partial):
            result = self.capture()
        facts = result["observations"]
        events = json.loads(Path(facts["events"]["path"]).read_text())
        writes = [event for event in events["events"]
                  if event["direction"] == "tx"]
        self.assertGreater(len(writes), 1)
        self.assertEqual(sum(event["size"] for event in writes), 32)
        self.assertEqual(Path(facts["tx"]["path"]).read_bytes(),
                         Path(facts["rx"]["path"]).read_bytes())

    def test_retryable_syscall_failures_do_not_replace_real_io(self):
        self.emulator()
        failures = [BlockingIOError(errno.EAGAIN, "retry"),
                    InterruptedError(errno.EINTR, "retry")]

        def interrupted(descriptor, payload):
            if failures:
                raise failures.pop(0)
            return os.write(descriptor, payload[:7])

        with patch("tools.hil.serial_echo._write", side_effect=interrupted):
            facts = self.capture()["observations"]
        self.assertFalse(failures)
        self.assertEqual(Path(facts["tx"]["path"]).read_bytes(),
                         Path(facts["rx"]["path"]).read_bytes())

    def test_repeated_write_interruptions_still_obey_deadline(self):
        self.emulator("silent")
        with patch("tools.hil.serial_echo._write",
                   side_effect=InterruptedError(errno.EINTR, "retry")):
            with self.assertRaisesRegex(EvidenceError, "deadline"):
                self.capture(timeout_s=0.03)
        self.assertEqual((self.retained() / "tx.bin").read_bytes(), b"")

    def test_raw_bound_retains_the_actual_overflow_byte_before_failure(self):
        self.emulator("noise")
        with patch("tools.hil.serial_echo.MAXIMUM_STREAM_BYTES", 8):
            with self.assertRaisesRegex(EvidenceError, "capture bound"):
                self.capture()
        # Reads request at most remaining capacity + one overflow byte.
        self.assertEqual(len((self.retained() / "drain.bin").read_bytes()), 9)
        report = json.loads((self.retained() / "capture.json").read_text())
        self.assertEqual(report["drain"]["size"], 9)
        self.assertEqual(report["status"], "failed")

    def test_each_capture_uses_fresh_random_challenge(self):
        self.emulator()
        first = self.capture()["observations"]
        self.record = self.root / "second.observation.json"
        second = self.capture()["observations"]
        self.assertNotEqual(Path(first["tx"]["path"]).read_bytes(),
                            Path(second["tx"]["path"]).read_bytes())

    def test_alias_swap_during_open_does_not_send_or_reconfigure(self):
        self.mock_alias.side_effect = [self.device, Path("/dev/null")]
        with self.assertRaisesRegex(EvidenceError, "identity changed"):
            self.capture()
        self.assertEqual((self.retained() / "tx.bin").read_bytes(), b"")
        self.assertEqual(termios.tcgetattr(self.slave)[4:6],
                         [termios.B9600] * 2)

    def test_existing_exclusive_owner_prevents_io(self):
        fcntl.ioctl(self.slave, termios.TIOCEXCL)
        self.addCleanup(fcntl.ioctl, self.slave, termios.TIOCNXCL)
        with self.assertRaises(EvidenceError):
            self.capture()
        self.assertEqual((self.retained() / "tx.bin").read_bytes(), b"")
        self.assertFalse(self.record.exists())

    def test_partial_echo_retains_failure_without_success_observation(self):
        self.emulator("partial")
        with self.assertRaisesRegex(EvidenceError, "deadline"):
            self.capture(timeout_s=0.08)
        self.assertFalse(self.record.exists())
        retained = self.retained()
        report = json.loads((retained / "capture.json").read_text())
        self.assertEqual(report["status"], "failed")
        self.assertEqual(len((retained / "tx.bin").read_bytes()), 32)
        self.assertGreater(len((retained / "rx.bin").read_bytes()), 0)
        self.assertLess(len((retained / "rx.bin").read_bytes()), 32)

    def test_no_echo_obeys_finite_host_capture_limit(self):
        self.emulator("silent")
        started = time.monotonic()
        with self.assertRaisesRegex(EvidenceError, "deadline"):
            self.capture(timeout_s=0.05)
        self.assertLess(time.monotonic() - started, 0.5)
        self.assertEqual((self.retained() / "rx.bin").read_bytes(), b"")
        self.assertFalse(self.record.exists())

    def test_wrong_echo_is_recorded_and_rejected(self):
        self.emulator("wrong")
        with self.assertRaisesRegex(EvidenceError, "echo"):
            self.capture()
        self.assertFalse(self.record.exists())
        self.assertTrue((self.retained() / "rx.bin").read_bytes())

    def test_continuous_prechallenge_noise_never_sends_challenge(self):
        self.emulator("noise")
        with self.assertRaisesRegex(EvidenceError, "deadline"):
            self.capture(timeout_s=0.04)
        self.assertEqual((self.retained() / "tx.bin").read_bytes(), b"")
        self.assertTrue((self.retained() / "drain.bin").read_bytes())
        self.assertFalse(self.record.exists())

    def test_changed_admission_input_rejects_before_device_open(self):
        Path(self.identities["fixture"]["path"]).write_text("changed")
        with self.assertRaises(EvidenceError):
            self.capture()
        self.mock_alias.assert_not_called()
        self.assertFalse(self.retained().exists())

    def test_full_admission_rebuild_rejects_inconsistent_record(self):
        self.mock_admit.return_value["physical_status"] = "changed"
        with self.assertRaises(EvidenceError):
            self.capture()
        self.mock_alias.assert_not_called()

    def test_admission_change_during_io_keeps_raw_and_rejects_success(self):
        self.emulator()
        self.mock_admit.side_effect = [copy.deepcopy(self.admission),
                                      {**self.admission, "status": "changed"}]
        with self.assertRaises(EvidenceError):
            self.capture()
        self.assertFalse(self.record.exists())
        self.assertEqual(len((self.retained() / "rx.bin").read_bytes()), 32)

    def test_record_cannot_overwrite_input_or_old_capture(self):
        for destination in (self.admission_path,
                            Path(self.identities["station"]["path"])):
            before = destination.read_bytes()
            with self.assertRaises(EvidenceError):
                serial_echo.capture_serial(self.admission_path, self.session,
                                           destination)
            self.assertEqual(destination.read_bytes(), before)
        self.record.write_text("old evidence")
        with self.assertRaises(EvidenceError):
            self.capture()
        self.assertEqual(self.record.read_text(), "old evidence")
        self.mock_alias.assert_not_called()

    def test_symlink_output_and_parent_are_rejected(self):
        self.record.symlink_to(self.root / "absent")
        with self.assertRaises(EvidenceError):
            self.capture()
        self.record.unlink()
        parent = self.root / "alias"
        parent.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(EvidenceError):
            serial_echo.capture_serial(self.admission_path, self.session,
                                       parent / "observation.json")
        self.mock_alias.assert_not_called()

    def test_existing_sidecar_and_symlink_input_are_rejected(self):
        self.retained().mkdir()
        with self.assertRaises(EvidenceError):
            self.capture()
        alias = self.root / "alias-admission.json"
        alias.symlink_to(self.admission_path)
        with self.assertRaises(EvidenceError):
            serial_echo.capture_serial(alias, self.session,
                                       self.root / "fresh.json")
        self.mock_alias.assert_not_called()

    def test_regular_file_cannot_be_used_as_serial_device(self):
        self.mock_alias.return_value = Path(self.identities["elf"]["path"])
        with self.assertRaisesRegex(EvidenceError, "character device"):
            self.capture()
        self.assertFalse(self.record.exists())

    def test_nonstable_and_traversal_device_names_are_rejected(self):
        with patch("tools.hil.serial_echo._alias_target",
                   wraps=self.mock_alias):
            for name in ("/dev/ttyUSB0", "/dev/serial/by-id/a/b",
                         "/dev/serial/by-id/../by-id/a", "relative"):
                with self.subTest(name=name):
                    self.station["serial"]["path"] = name
                    atomic_json(self.identities["station"]["path"],
                                self.station)
                    self.identities["station"] = file_identity(
                        self.identities["station"]["path"])
                    self.admission["station"] = self.identities["station"]
                    atomic_json(self.admission_path, self.admission)
                    self.mock_admit.return_value = copy.deepcopy(self.admission)
                    with self.assertRaises(EvidenceError):
                        self.capture()

    def test_invalid_session_and_capture_limits_reject_before_open(self):
        for session in ("", "../old", "0" * 31, "z" * 32):
            with self.assertRaises(EvidenceError):
                serial_echo.capture_serial(self.admission_path, session,
                                           self.record)
        for options in ({"timeout_s": 0}, {"timeout_s": True},
                        {"timeout_s": float("inf")}, {"quiet_s": 0},
                        {"quiet_s": 1}, {"quiet_s": float("nan")}):
            with self.assertRaises(EvidenceError):
                self.capture(**options)
        self.mock_alias.assert_not_called()

    def test_cli_runs_capture_and_returns_failure_without_qualification(self):
        self.emulator()
        self.assertEqual(serial_echo.main([
            "--admission", str(self.admission_path), "--session", self.session,
            "--record", str(self.record), "--quiet-s", "0.01",
            "--timeout-s", "0.3"]), 0)
        result = json.loads(self.record.read_text())
        self.assertNotIn("physical_status", result)
        self.assertNotIn("passed", result["observations"].values())
        self.assertEqual(serial_echo.main([
            "--admission", str(self.admission_path), "--session", self.session,
            "--record", str(self.record)]), 1)


if __name__ == "__main__":
    unittest.main()
