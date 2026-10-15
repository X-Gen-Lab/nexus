"""Reviewed Linux timer derivation is strict and leaves upstream bytes intact."""
import hashlib
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "nexus_threadx_linux_prepare", ROOT / "os/threadx/prepare_linux.py")
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)
SOURCE = Path(os.environ.get("NEXUS_THREADX_SOURCE", ROOT / "ext/threadx")) / "ports/linux/gnu/src/tx_initialize_low_level.c"
SCHEDULER = SOURCE.with_name("tx_thread_schedule.c")


class ThreadXLinuxOverlayTests(unittest.TestCase):
    def fixture(self, directory):
        source, output = Path(directory) / "source.c", Path(directory) / "overlay.c"
        source.write_bytes(SOURCE.read_bytes())
        return source, output

    def test_actual_reviewed_source_uses_monotonic_clock_and_clockwait(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            raw = source.read_bytes()
            prepare.prepare(source, output)
            text = output.read_text()
            self.assertIn("clock_gettime(CLOCK_MONOTONIC, &ts);", text)
            self.assertIn("sem_clockwait(&_tx_linux_timer_semaphore, CLOCK_MONOTONIC, &ts)", text)
            self.assertIn("if (ts.tv_nsec >= 1000000000)", text)
            self.assertEqual(source.read_bytes(), raw)
            self.assertIn("SPDX-License-Identifier: MIT", text)

    def test_unreviewed_source_bytes_fail_before_output(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            source.write_bytes(source.read_bytes() + b"\n")
            with self.assertRaisesRegex(ValueError, "identity"):
                prepare.prepare(source, output)
            self.assertFalse(output.exists())

    def test_mismatching_exact_patch_boundary_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "boundary"):
            prepare.replace_once("unrelated", "old", "new")
        with self.assertRaisesRegex(ValueError, "boundary"):
            prepare.replace_once("old old", "old", "new")

    def test_incomplete_patch_cannot_be_published(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            with patch.object(prepare, "replace_once", side_effect=lambda text, old, new: text):
                with self.assertRaisesRegex(ValueError, "incomplete"):
                    prepare.prepare(source, output)
            self.assertFalse(output.exists())

    def test_existing_different_output_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            output.write_text("reserved")
            with self.assertRaises(ValueError): prepare.prepare(source, output)
            self.assertEqual(output.read_text(), "reserved")

    def test_existing_exact_output_is_idempotent_without_rewriting(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            prepare.prepare(source, output)
            before = output.stat().st_mtime_ns
            prepare.prepare(source, output)
            self.assertEqual(output.stat().st_mtime_ns, before)

    def test_reviewed_exact_predecessor_is_atomically_upgraded(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            previous = (ROOT / "tests/contracts/threadx_linux_port_predecessor.c.raw").read_bytes()
            output.write_bytes(previous)
            before = output.stat().st_ino
            raw = source.read_bytes()
            prepare.prepare(source, output)
            self.assertNotEqual(output.stat().st_ino, before)
            self.assertEqual(output.read_bytes(), prepare.derive(raw))
            self.assertEqual(source.read_bytes(), raw)

    def test_changed_predecessor_is_never_upgraded(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            previous = (ROOT / "tests/contracts/threadx_linux_port_predecessor.c.raw").read_bytes()
            output.write_bytes(previous + b"\n")
            with self.assertRaisesRegex(ValueError, "different contents"):
                prepare.prepare(source, output)
            self.assertEqual(output.read_bytes(), previous + b"\n")

    def test_input_and_output_symlinks_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            alias = Path(directory) / "alias.c"; alias.symlink_to(source)
            with self.assertRaises(ValueError): prepare.prepare(alias, output)
            output.symlink_to(source)
            with self.assertRaises(ValueError): prepare.prepare(source, output)
            self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), prepare.SOURCE_SHA256)

    def test_parent_symlink_and_traversal_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            alias = Path(directory) / "alias"; alias.symlink_to(Path(directory), target_is_directory=True)
            with self.assertRaises(ValueError): prepare.prepare(source, alias / "overlay.c")
            with self.assertRaises(ValueError): prepare.prepare(source, Path(directory) / "child/../overlay.c")

    def test_source_change_during_derivation_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            actual = prepare.replace_once
            def changing(text, old, new):
                source.write_bytes(source.read_bytes() + b"\n")
                return actual(text, old, new)
            with patch.object(prepare, "replace_once", side_effect=changing):
                with self.assertRaisesRegex(ValueError, "changed"):
                    prepare.prepare(source, output)
            self.assertFalse(output.exists())


class ThreadXLinuxSchedulerOverlayTests(unittest.TestCase):
    def fixture(self, directory):
        source, output = Path(directory) / "schedule.c", Path(directory) / "overlay.c"
        source.write_bytes(SCHEDULER.read_bytes())
        return source, output

    def test_actual_scheduler_preserves_ack_and_timer_origin_handshake(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            raw = source.read_bytes()
            prepare.prepare(source, output, kind="scheduler")
            text = output.read_text()
            self.assertNotIn("_tx_linux_thread_suspend(_tx_linux_timer_id);", text)
            self.assertNotIn("_tx_linux_thread_resume(_tx_linux_timer_id);", text)
            self.assertIn("tx_linux_sem_post_nolock(&_tx_linux_isr_semaphore);", text)
            self.assertIn("tx_linux_sem_wait(&_tx_linux_semaphore);", text)
            self.assertIn("SPDX-License-Identifier: MIT", text)
            self.assertEqual(source.read_bytes(), raw)

    def test_scheduler_rejects_modified_official_bytes_before_output(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            source.write_bytes(source.read_bytes() + b"\n")
            with self.assertRaisesRegex(ValueError, "identity"):
                prepare.prepare(source, output, kind="scheduler")
            self.assertFalse(output.exists())

    def test_scheduler_never_admits_lowlevel_predecessor_or_polluted_output(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            original = (ROOT / "tests/contracts/threadx_linux_port_predecessor.c.raw").read_bytes()
            output.write_bytes(original)
            with self.assertRaisesRegex(ValueError, "different contents"):
                prepare.prepare(source, output, kind="scheduler")
            self.assertEqual(output.read_bytes(), original)
            output.write_bytes(b"unknown schedule")
            with self.assertRaisesRegex(ValueError, "different contents"):
                prepare.prepare(source, output, kind="scheduler")
            self.assertEqual(output.read_bytes(), b"unknown schedule")

    def test_scheduler_exact_output_is_idempotent_without_rewrite(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            prepare.prepare(source, output, kind="scheduler")
            before = output.stat().st_mtime_ns
            prepare.prepare(source, output, kind="scheduler")
            self.assertEqual(output.stat().st_mtime_ns, before)

    def test_scheduler_symlink_fails_without_touching_official_input(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            output.symlink_to(source)
            raw = source.read_bytes()
            with self.assertRaises(ValueError):
                prepare.prepare(source, output, kind="scheduler")
            self.assertEqual(source.read_bytes(), raw)

    def test_unknown_derivation_kind_rejects_before_output(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = self.fixture(directory)
            with self.assertRaisesRegex(ValueError, "kind"):
                prepare.prepare(source, output, kind="unsupported")
            self.assertFalse(output.exists())


if __name__ == "__main__": unittest.main()
