"""Cancellation preserves first failure while retiring owned host resources."""
import errno
import os
from pathlib import Path
import selectors
import tempfile
import unittest
from unittest.mock import patch

from tools.hil import serial_echo
from tools.hil.acquire import execute_acquisition
from tools.testing import test_hil_acquire


class HilCleanupCancellationTests(unittest.TestCase):
    def assert_descriptors_closed(self, descriptors):
        for descriptor in descriptors:
            with self.assertRaises(OSError) as error:
                os.fstat(descriptor)
            self.assertEqual(error.exception.errno, errno.EBADF)

    def test_fsync_cancellation_drains_streams_and_preserves_first_cause(self):
        for interruption in (KeyboardInterrupt("cancel"), SystemExit(17)):
            with self.subTest(error=type(interruption).__name__), \
                    tempfile.TemporaryDirectory() as directory:
                trace = serial_echo._Trace(Path(directory))
                descriptors = [stream.fileno()
                               for stream in trace.streams.values()]
                original_fsync = os.fsync

                def synchronize(descriptor):
                    if descriptor == descriptors[0]:
                        raise interruption
                    return original_fsync(descriptor)

                with patch.object(serial_echo.os, "fsync",
                                  side_effect=synchronize):
                    with self.assertRaises(type(interruption)) as error:
                        trace.close()
                self.assertIs(error.exception, interruption)
                self.assert_descriptors_closed(descriptors)

    def test_close_cancellation_drains_remaining_streams_and_reraises(self):
        for interruption in (KeyboardInterrupt("cancel"), SystemExit(17)):
            with self.subTest(error=type(interruption).__name__), \
                    tempfile.TemporaryDirectory() as directory:
                trace = serial_echo._Trace(Path(directory))
                descriptors = [stream.fileno()
                               for stream in trace.streams.values()]
                first = trace.streams["tx"]
                original_close = first.close

                def close():
                    original_close()
                    raise interruption

                with patch.object(first, "close", side_effect=close):
                    with self.assertRaises(type(interruption)) as error:
                        trace.close()
                self.assertIs(error.exception, interruption)
                self.assert_descriptors_closed(descriptors)

    def test_system_exit_capture_reaps_adapter_and_quarantines_station(self):
        fixture = test_hil_acquire.HilAcquireTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        pid = fixture.sleeping_adapter()
        original_select = selectors.EpollSelector.select
        interrupted = False

        def cancel(selector, timeout):
            nonlocal interrupted
            events = original_select(selector, timeout)
            if events and not interrupted:
                interrupted = True
                raise SystemExit(17)
            return events

        with patch.object(selectors.EpollSelector, "select", cancel):
            result = execute_acquisition(
                fixture.recipe_path, fixture.admission_path, fixture.output)
        self.assertTrue(interrupted)
        self.assertTrue(result["cancelled"])
        self.assertTrue(result["station_quarantined"])
        self.assertEqual(result["status"], "failed")
        self.assertEqual([step["name"] for step in result["operations"]],
                         ["identify"])
        self.assertIn("SystemExit: 17", result["operations"][0]["failure"])
        with self.assertRaises(ProcessLookupError):
            os.kill(int(pid.read_text()), 0)


if __name__ == "__main__":
    unittest.main()
