"""Reject incomplete or inconsistent trace evidence using the production CLI."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "scripts/validation/check_coverage.py"
GOOD = "SF:osal/adapter.c\nDA:1,2\nDA:2,0\nLF:2\nLH:1\nend_of_record\n"


class CoverageGateContracts(unittest.TestCase):
    def run_checker(self, data, *, threshold="0.5", stale=False):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "coverage.info"
            start_ns = time.time_ns()
            if data is not None:
                report.write_text(data)
            if stale:
                os.utime(report, ns=(start_ns - 10_000_000_000,) * 2)
            return subprocess.run([sys.executable, str(CHECKER), "--coverage-file", str(report),
                "--threshold", threshold, "--not-before-ns", str(start_ns)],
                capture_output=True, text=True, timeout=10)

    def test_real_line_counts_and_threshold(self):
        result = self.run_checker(GOOD)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("1/2 = 50.00%", result.stdout)
        self.assertNotEqual(self.run_checker(GOOD, threshold="0.8").returncode, 0)

    def test_missing_empty_unterminated_and_zero_source_rejected(self):
        for data in (None, "", "SF:source.c\nDA:1,1\nLF:1\nLH:1\n",
                     "SF:source.c\nLF:0\nLH:0\nend_of_record\n"):
            with self.subTest(data=data):
                self.assertNotEqual(self.run_checker(data, threshold="0").returncode, 0)

    def test_false_negative_malformed_and_duplicate_counts_rejected(self):
        variants = (GOOD.replace("LH:1", "LH:2"), GOOD.replace("DA:1,2", "DA:1,-2"),
                    GOOD.replace("DA:1,2", "DA:garbage"), GOOD.replace("LF:2", "LF:2\nLF:2"))
        for data in variants:
            with self.subTest(data=data):
                self.assertNotEqual(self.run_checker(data).returncode, 0)

    def test_stale_trace_rejected(self):
        self.assertNotEqual(self.run_checker(GOOD, stale=True).returncode, 0)

    def test_nan_and_infinite_threshold_rejected(self):
        for value in ("nan", "inf", "-inf"):
            with self.subTest(value=value):
                self.assertNotEqual(self.run_checker(GOOD, threshold=value).returncode, 0)


if __name__ == "__main__":
    unittest.main()
