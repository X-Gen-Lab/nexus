"""Actual child accounting keeps execution and cancellation contracts."""

import json
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import sys
import tempfile
import unittest

from tools.dev import process, timings


class ProcessResourceTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def execute(self, code, *, timeout=10):
        result = process.run([sys.executable, "-B", "-c", code],
                             cwd=self.root, env=os.environ, timeout=timeout,
                             log=self.root / "child.log")
        return result, json.loads(result.receipt_path.read_text())

    def test_actual_cpu_and_memory_are_bound_to_waited_child(self):
        result, record = self.execute(
            "import time; data=bytearray(24*1024*1024); "
            "end=time.process_time()+0.04\n"
            "while time.process_time()<end: pass\n"
            "print(len(data))")
        self.assertEqual(result.returncode, 0)
        usage = record["resource_usage"]
        if not sys.platform.startswith("linux") or not hasattr(os, "wait4"):
            self.assertIsNone(usage)
            return
        self.assertEqual(usage["method"], "linux_wait4")
        self.assertEqual(usage["scope"],
                         "waited_process_and_reaped_descendants")
        self.assertGreaterEqual(usage["user_cpu_seconds"] +
                                usage["system_cpu_seconds"], 0.03)
        self.assertGreaterEqual(usage["maximum_process_rss_bytes"],
                                24 * 1024 * 1024)
        summary = timings.summarize([result.receipt_path])
        self.assertEqual(summary["attempts"][0]["resource_usage"], usage)
        self.assertEqual(summary["measured_resource_attempts"], 1)

    def test_nonzero_exit_retains_its_actual_resource_observation(self):
        result, record = self.execute("raise SystemExit(7)")
        self.assertEqual(result.returncode, 7)
        self.assertEqual(record["child_returncode"], 7)
        if sys.platform.startswith("linux") and hasattr(os, "wait4"):
            self.assertEqual(record["resource_usage"]["method"], "linux_wait4")
        else:
            self.assertIsNone(record["resource_usage"])

    def test_timeout_keeps_withdrawal_and_unknown_resource_cost(self):
        result, record = self.execute("import time; time.sleep(10)",
                                      timeout=0.1)
        self.assertEqual(result.returncode, 124)
        self.assertEqual(record["outcome"], "timed_out")
        self.assertTrue(record["withdrawal_complete"])
        self.assertIsNone(record["resource_usage"])

    def test_missing_executable_has_no_invented_resource_sample(self):
        result = process.run([str(self.root / "absent")], cwd=self.root,
                             env=os.environ, log=self.root / "missing.log")
        record = json.loads(result.receipt_path.read_text())
        self.assertEqual(result.returncode, 127)
        self.assertIsNone(record["resource_usage"])

    def test_concurrent_waiters_do_not_mix_global_child_cpu(self):
        def execute(name, work):
            code = ("import time; " + work +
                    "; print(time.process_time(), flush=True)")
            result = process.run([sys.executable, "-B", "-c", code],
                                 cwd=self.root, env=os.environ, timeout=10,
                                 log=self.root / (name + ".log"))
            self.assertEqual(result.returncode, 0)
            return result, json.loads(result.receipt_path.read_text())

        with ThreadPoolExecutor(max_workers=2) as pool:
            sleeper = pool.submit(execute, "sleep", "time.sleep(0.4)")
            burner = pool.submit(execute, "burn",
                                 "exec('end=time.process_time()+0.1\\n'"
                                 "+'while time.process_time()<end: pass')")
            busy, busy_record = burner.result()
            idle, idle_record = sleeper.result()
        if not sys.platform.startswith("linux") or not hasattr(os, "wait4"):
            self.assertIsNone(idle_record["resource_usage"])
            return
        for result, record in ((busy, busy_record), (idle, idle_record)):
            usage = record["resource_usage"]
            observed = usage["user_cpu_seconds"] + usage["system_cpu_seconds"]
            own = float(result.stdout.strip())
            self.assertGreaterEqual(observed + 0.002, own)
            self.assertLess(observed - own, 0.05)


if __name__ == "__main__":
    unittest.main()
