"""Real complete module executions share a finite build allocation."""

import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

from tools.testing import run_tool_tests


class ToolWorkersTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="nexus module workers ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.directory = self.root / "tools/testing"
        self.directory.mkdir(parents=True)
        for name in ("testing/run_tool_tests.py", "testing/module_workers.py",
                     "dev/process.py", "dev/build_lock.py"):
            source = run_tool_tests.ROOT / "tools" / name
            if source.is_file():
                target = self.root / "tools" / name
                target.parent.mkdir(exist_ok=True)
                shutil.copyfile(source, target)
        self.report = self.root / "result.json"

    def module(self, name, body):
        (self.directory / f"test_{name}.py").write_text(
            "import os, sys, time, unittest, subprocess\n"
            "from pathlib import Path\n"
            "class Contract(unittest.TestCase):\n"
            "    def test_behavior(self):\n" +
            "".join("        " + line + "\n" for line in body.splitlines()))

    def start(self, workers, timeout="10", allocation=None, files=()):
        env = dict(os.environ)
        env.pop("NEXUS_BUILD_LEASES", None)
        env.update(NEXUS_COMMAND_TIMEOUT=timeout, NEXUS_BUILD_JOBS=str(workers if allocation is None else allocation))
        self.report.write_text("stale result")
        return subprocess.Popen([sys.executable, "-B",
            str(self.directory / "run_tool_tests.py"), "--suite", "testing",
            "--report", str(self.report), "--jobs", str(workers),
            *(["--files", *files] if files else [])],
            cwd=self.root, env=env, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True)

    def finish(self, proc):
        output, _ = proc.communicate(timeout=20)
        return proc.returncode, output

    def test_workers_preserve_complete_serial_case_sets_and_split_budget(self):
        for name in ("first", "second"):
            self.module(name, """
self.assertEqual(os.environ['CMAKE_BUILD_PARALLEL_LEVEL'], '1')
self.assertEqual(os.environ['NEXUS_BUILD_JOBS'], '1')
""")
        (self.directory / "test_fixture.py").write_text("# Shared test fixture.\n")
        results = []
        for workers, budget in ((1, 1), (2, 2), (4, 1)):
            status, log = self.finish(self.start(workers, allocation=budget))
            self.assertEqual(status, 0, log)
            result = json.loads(self.report.read_text())
            self.assertEqual(result["workers"], min(workers, budget))
            self.assertEqual(result["executed"], 2)
            self.assertEqual(result["scope"], "full-tool-domain")
            results.append(result["modules"])
        self.assertTrue(all(result == results[0] for result in results))

    def test_empty_domain_and_explicit_empty_module_cannot_pass(self):
        (self.directory / "test_fixture.py").write_text("# Shared test fixture.\n")
        selected = ("tools/testing/test_fixture.py",)
        for files in ((), selected):
            with self.subTest(files=files):
                status, log = self.finish(self.start(2, files=files))
                self.assertNotEqual(status, 0, log)
                self.assertIn("zero tests", log)
                self.assertFalse(self.report.exists())

    def test_failed_module_withdraws_active_sibling_and_hides_stale_pass(self):
        self.module("a", """
deadline = time.monotonic() + 5
while not Path('started').exists() and time.monotonic() < deadline: time.sleep(.02)
self.assertTrue(Path('started').exists(), 'workers must overlap')
self.fail('injected module failure')
""")
        self.module("b", """
Path('started').write_text(str(os.getpid()))
time.sleep(30)
""")
        status, log = self.finish(self.start(2))
        self.assertNotEqual(status, 0)
        self.assertIn("injected module failure", log)
        self.assertFalse(self.report.exists())
        self.assertFalse(self.alive())

    def alive(self):
        pid = int((self.root / "started").read_text())
        if os.name == "posix":
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                return False
            return True
        return False

    def test_timeout_and_signal_withdraw_owned_worker(self):
        self.module("blocking", """
Path('started').write_text(str(os.getpid()))
time.sleep(30)
""")
        for mode in (("timeout", "signal") if os.name == "posix" else ("timeout",)):
            with self.subTest(mode=mode):
                started = self.root / "started"
                started.unlink(missing_ok=True)
                proc = self.start(2, timeout="2" if mode == "timeout" else "15")
                deadline = time.monotonic() + 5
                while not started.exists() and proc.poll() is None and time.monotonic() < deadline:
                    time.sleep(.02)
                if mode == "signal" and proc.poll() is None:
                    proc.send_signal(signal.SIGTERM)
                status, log = self.finish(proc)
                self.assertTrue(started.exists(), log)
                self.assertNotEqual(status, 0)
                self.assertFalse(self.report.exists())
                self.assertFalse(self.alive())
