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

    def start(self, workers, timeout="10", allocation=None, files=(), explicit=True):
        env = dict(os.environ)
        env.pop("NEXUS_BUILD_LEASES", None)
        env.pop("NEXUS_TOOL_JOBS", None)
        env.update(NEXUS_COMMAND_TIMEOUT=timeout, NEXUS_BUILD_JOBS=str(workers if allocation is None else allocation))
        self.report.write_text("stale result")
        return subprocess.Popen([sys.executable, "-B",
            str(self.directory / "run_tool_tests.py"), "--suite", "testing",
            "--report", str(self.report),
            *(["--jobs", str(workers)] if explicit else []),
            *(["--files", *files] if files else [])],
            cwd=self.root, env=env, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True)

    def finish(self, proc):
        output, _ = proc.communicate(timeout=20)
        return proc.returncode, output

    def test_default_workers_inherit_the_callers_build_budget(self):
        for name in ("first", "second"):
            self.module(name, """
Path(f'{__name__}.budget').write_text(os.environ.get('NEXUS_BUILD_JOBS', ''))
""")
        status, log = self.finish(self.start(2, explicit=False))
        self.assertEqual(status, 0, log)
        record = json.loads(self.report.read_text())
        self.assertEqual(record.get("workers"), 2)
        self.assertEqual(record["executed"], 2)
        for budget in self.root.glob("*.budget"):
            self.assertEqual(budget.read_text(), "1")

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

    def test_nested_same_named_modules_have_independent_parallel_results(self):
        for package in ("first", "second"):
            directory = self.directory / package
            directory.mkdir()
            (directory / "__init__.py").write_text('"""Test group."""\n')
            (directory / "test_same.py").write_text(
                "import os, unittest\n"
                "class Nested(unittest.TestCase):\n"
                "    def test_complete(self):\n"
                "        self.assertEqual(os.environ['NEXUS_BUILD_JOBS'], '1')\n")
        records = []
        for workers in (1, 2):
            status, output = self.finish(self.start(workers, allocation=workers))
            self.assertEqual(status, 0, output)
            result = json.loads(self.report.read_text())
            self.assertEqual(result.get("workers"), workers)
            self.assertEqual(result["executed"], 2)
            self.assertEqual(set(result["modules"]), {
                "tools/testing/first/test_same.py",
                "tools/testing/second/test_same.py"})
            records.append(result["modules"])
        self.assertEqual(records[0], records[1])
        self.assertNotEqual(*records[0].values())

    def test_nested_selection_does_not_import_same_named_sibling(self):
        for package in ("chosen", "unselected"):
            directory = self.directory / package
            directory.mkdir()
            (directory / "__init__.py").write_text("")
            (directory / "test_same.py").write_text(
                "import unittest\nclass Selected(unittest.TestCase):\n"
                "    def test_complete(self): pass\n" if package == "chosen"
                else "raise RuntimeError('sibling must not import')\n")
        selected = ("tools/testing/chosen/test_same.py",)
        status, output = self.finish(self.start(2, files=selected))
        self.assertEqual(status, 0, output)
        result = json.loads(self.report.read_text())
        self.assertEqual(result["executed"], 1)
        self.assertEqual(result["files"], list(selected))

    def test_package_load_tests_hook_keeps_complete_discovery(self):
        directory = self.directory / "group"
        directory.mkdir()
        (directory / "__init__.py").write_text(
            "import unittest\n"
            "class PackageCase(unittest.TestCase):\n"
            "    def test_real_hook(self): pass\n"
            "def load_tests(loader, tests, pattern):\n"
            "    return loader.loadTestsFromTestCase(PackageCase)\n")
        (directory / "test_unselected.py").write_text(
            "raise RuntimeError('package hook owns discovery')\n")
        status, output = self.finish(self.start(2))
        self.assertEqual(status, 0, output)
        result = json.loads(self.report.read_text())
        self.assertEqual(result["executed"], 1)

    def test_unmarked_nested_fixture_is_outside_domain_discovery(self):
        self.module("owned", "self.assertEqual(2 + 2, 4)")
        fixture = self.directory / "manual_fixture"
        fixture.mkdir()
        (fixture / "test_supplier.py").write_text(
            "raise RuntimeError('manual fixture must not load')\n")
        private_package = fixture / "private_plugin"
        private_package.mkdir()
        (private_package / "__init__.py").write_text(
            "raise RuntimeError('unmarked fixture does not own discovery')\n")
        status, output = self.finish(self.start(2))
        self.assertEqual(status, 0, output)
        result = json.loads(self.report.read_text())
        self.assertEqual(result["executed"], 1)
        self.assertEqual(set(result["modules"]), {
            "tools/testing/test_owned.py"})

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
