"""Bound independent Native executions without reusing successful reports."""

import hashlib
import json
from contextlib import chdir
import os
from pathlib import Path
import signal
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

from tools.dev import native_matrix


class NativeMatrixTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.output = self.root / "build/matrix"
        self.presets = ("native-debug", "native-release")
        (self.root / "tools/dev").mkdir(parents=True)
        (self.root / "scripts/ci").mkdir(parents=True)
        (self.root / "CMakePresets.json").write_text(json.dumps({
            "version": 6, "configurePresets": [{
                "name": name, "binaryDir": "${sourceDir}/build/" + name,
            } for name in self.presets],
            "buildPresets": [{"name": name, "configurePreset": name}
                             for name in self.presets],
            "testPresets": [{"name": name, "configurePreset": name}
                            for name in self.presets],
        }))
        (self.root / "tools/dev/dev.py").write_text(
            "import json, os, pathlib, sys, time\n"
            "root=pathlib.Path.cwd()\n"
            "action=sys.argv[1]\n"
            "if action=='doctor': raise SystemExit(0)\n"
            "preset=sys.argv[sys.argv.index('--preset')+1]\n"
            "build=root/'build'/preset\n"
            "build.mkdir(parents=True,exist_ok=True)\n"
            "(build/(action+'.json')).write_text(json.dumps({"
            "'jobs':os.environ['CMAKE_BUILD_PARALLEL_LEVEL']}))\n"
            "if action=='configure':\n"
            "    deadline=time.monotonic()+5\n"
            "    while len(list((root/'build').glob('native-*/configure.json')))"
            "<2 and time.monotonic()<deadline: time.sleep(0.005)\n"
            "    assert len(list((root/'build').glob("
            "'native-*/configure.json')))==2\n")
        (self.root / "scripts/ci/native_contracts.py").write_text(
            'import hashlib, json, pathlib, sys\n'
            "preset=sys.argv[sys.argv.index('--preset')+1]\n"
            "build=pathlib.Path.cwd()/'build'/preset\n"
            "root=build/'tdd'\n"
            'root.mkdir(parents=True,exist_ok=True)\n'
            "(root/'google').mkdir(exist_ok=True)\n"
            'google=(\'<testsuites name="AllTests">\' + \'\'.join(\n'
            '    \'<testcase classname="Probe" name="case%d" status="run" result="completed"/>\' % i\n'
            "    for i in range(4)) + '</testsuites>').encode()\n"
            'ctest=(\'<testsuite>\' + \'\'.join(\'<testcase name="contract%d"/>\' % i\n'
            "    for i in range(3)) + '</testsuite>').encode()\n"
            "(root/'google/probe.xml').write_bytes(google)\n"
            "(build/'ctest-results.xml').write_bytes(ctest)\n"
            "(root/'execution.json').write_text(json.dumps({\n"
            "    'kind':'development-contract-execution','preset':preset,'origin':'matrix-child',\n"
            "    'passed':4,'suites':[{'binary':'probe','passed':4,\n"
            "        'report_sha256':hashlib.sha256(google).hexdigest()}],\n"
            "    'ctest':{'passed':3,'report_sha256':hashlib.sha256(ctest).hexdigest()}}))\n"
        )

    def execute(self, *extra):
        with patch.object(native_matrix, "ROOT", self.root), patch.object(
                native_matrix, "capacity", return_value=(4, 8192)):
            return native_matrix.main([
                "--presets", *self.presets, "--jobs", "4",
                "--parallel-modes", "2", "--output", str(self.output),
                *extra])

    def test_two_modes_get_disjoint_builds_and_split_cpu_budget(self):
        self.assertEqual(self.execute(), 0)
        summary = json.loads((self.output / "execution.json").read_text())
        self.assertEqual(summary["status"], "passed")
        self.assertEqual(summary["parallel_modes"], 2)
        self.assertEqual(summary["jobs_per_mode"], 2)
        self.assertEqual(len(summary["modes"]), 2)
        receipts = []
        for result in summary["modes"]:
            self.assertEqual(result["status"], "passed")
            self.assertEqual(len(result["commands"]), 3)
            receipts += result["commands"]
            build = self.root / "build" / result["preset"]
            self.assertEqual(json.loads((build / "build.json").read_text())
                             ["jobs"], "2")
        self.assertEqual(len(set(receipts)), 6)
        for receipt in receipts:
            self.assertEqual(json.loads(Path(receipt).read_text())
                             ["exit_code"], 0)

    def test_finished_mode_is_snapshotted_before_another_cli_rewrites_its_reports(self):
        driver = self.root / "scripts/ci/native_contracts.py"
        driver.write_text(driver.read_text() +
            "import time\n"
            "if preset=='native-release':\n"
            "    deadline=time.monotonic()+5\n"
            "    marker=pathlib.Path.cwd()/'debug-overwritten'\n"
            "    while not marker.exists() and time.monotonic()<deadline: time.sleep(0.005)\n"
            "    assert marker.exists()\n")
        finished = threading.Event()
        changed = threading.Event()
        failures = []
        originals = {}
        execute_mode = native_matrix.execute_mode

        def observed_mode(*args, **kwargs):
            result = execute_mode(*args, **kwargs)
            if args[0] == "native-debug":
                finished.set()
                changed.wait(6)
            return result

        def ordinary_cli():
            try:
                if not finished.wait(6):
                    raise AssertionError("First mode did not finish")
                build = self.root / "build/native-debug"
                report = build / "tdd/execution.json"
                document = json.loads(report.read_text())
                for name, path in (("google", build / "tdd/google/probe.xml"),
                                   ("ctest", build / "ctest-results.xml")):
                    originals[name] = path.read_bytes()
                    payload = originals[name] + b"\n<!-- ordinary CLI report -->\n"
                    path.write_bytes(payload)
                    entry = (document["suites"][0] if name == "google"
                             else document["ctest"])
                    entry["report_sha256"] = hashlib.sha256(payload).hexdigest()
                document["origin"] = "ordinary-cli"
                report.write_text(json.dumps(document))
            except BaseException as error:
                failures.append(error)
            finally:
                (self.root / "debug-overwritten").write_text("complete")
                changed.set()

        trigger = threading.Thread(target=ordinary_cli)
        trigger.start()
        try:
            with patch.object(native_matrix, "execute_mode", side_effect=observed_mode):
                self.assertEqual(self.execute(), 0)
        finally:
            trigger.join(timeout=7)
        self.assertFalse(trigger.is_alive())
        self.assertEqual(failures, [])
        summary = json.loads((self.output / "execution.json").read_text())
        debug = next(item for item in summary["modes"]
                     if item["preset"] == "native-debug")
        retained = json.loads(Path(debug["runtime_report"]["path"]).read_text())
        self.assertEqual(retained["origin"], "matrix-child")
        self.assertEqual(Path(debug["ctest_report"]["path"]).read_bytes(),
                         originals["ctest"])
        self.assertEqual(Path(debug["google_reports"][0]["report"]["path"]).read_bytes(),
                         originals["google"])
        for artifact in (debug["runtime_report"], debug["ctest_report"],
                         debug["google_reports"][0]["report"]):
            self.assertEqual(hashlib.sha256(Path(artifact["path"]).read_bytes()).hexdigest(),
                             artifact["sha256"])

    def test_missing_or_changed_junit_never_publishes_a_passing_snapshot(self):
        driver = self.root / "scripts/ci/native_contracts.py"
        source = driver.read_text()
        for name, mutation in (
                ("missing", "(root/'google/probe.xml').unlink()"),
                ("changed", "(build/'ctest-results.xml').write_text('<other/>')")):
            with self.subTest(name=name):
                self.output = self.root / ("build/matrix-" + name)
                driver.write_text(source + "if preset=='native-debug': " + mutation + "\n")
                self.assertNotEqual(self.execute(), 0)
                summary = json.loads((self.output / "execution.json").read_text())
                debug = next(item for item in summary["modes"]
                             if item["preset"] == "native-debug")
                self.assertEqual(debug["status"], "failed")
                self.assertEqual(debug["failed_stage"], "snapshot")

    def test_invalid_cpu_or_memory_budgets_launch_no_children(self):
        for arguments in (("--jobs", "0"), ("--jobs", "5"),
                          ("--memory-per-mode-mib", "8193")):
            with self.subTest(arguments=arguments):
                self.assertNotEqual(self.execute(*arguments), 0)
                self.assertFalse((self.root / "build/native-debug/"
                                  "configure.json").exists())

    def test_duplicate_or_aliased_builds_reject_before_execution(self):
        self.presets = ("native-debug", "native-debug")
        self.assertNotEqual(self.execute(), 0)
        self.assertFalse((self.output / "execution.json").exists())

    def test_different_presets_cannot_own_the_same_build_tree(self):
        path = self.root / "CMakePresets.json"
        data = json.loads(path.read_text())
        for preset in data["configurePresets"]:
            preset["binaryDir"] = "${sourceDir}/build/shared"
        path.write_text(json.dumps(data))
        self.assertNotEqual(self.execute(), 0)
        self.assertFalse(self.output.exists())

    def test_busy_build_lock_rejects_before_any_child_is_launched(self):
        with native_matrix.build_lock(self.root / "build/native-debug"):
            self.assertNotEqual(self.execute(), 0)
        self.assertFalse(self.output.exists())

    def test_existing_output_cannot_reuse_a_successful_matrix_report(self):
        self.output.mkdir(parents=True)
        report = self.output / "execution.json"
        report.write_text('{"status":"passed","stale":true}')
        self.assertNotEqual(self.execute(), 0)
        self.assertTrue(json.loads(report.read_text())["stale"])
        self.assertFalse((self.root / "build/native-debug/"
                          "configure.json").exists())

    def test_failed_mode_cancels_other_owned_child_and_preserves_failure(self):
        driver = self.root / "tools/dev/dev.py"
        driver.write_text(driver.read_text() +
            "if action=='build' and preset=='native-debug': "
            "raise SystemExit(7)\n"
            "if action=='build' and preset=='native-release': "
            "time.sleep(30)\n")
        self.assertNotEqual(self.execute(), 0)
        summary = json.loads((self.output / "execution.json").read_text())
        self.assertEqual(summary["status"], "failed")
        records = [json.loads(Path(path).read_text())
                   for mode in summary["modes"] for path in mode["commands"]]
        self.assertTrue(any(record["exit_code"] == 7 for record in records))
        cancelled = [record for record in records
                     if record["outcome"] == "cancelled"]
        self.assertTrue(cancelled)
        self.assertTrue(all(record["withdrawal_complete"]
                            for record in cancelled))

    def test_interrupt_cancels_only_owned_children_and_returns_130(self):
        driver = self.root / "tools/dev/dev.py"
        driver.write_text(driver.read_text() +
                          "if action=='build': time.sleep(30)\n")
        observed = []

        def interrupt():
            deadline = time.monotonic() + 5
            ready = self.root / "build/native-debug/build.json"
            while not ready.exists() and time.monotonic() < deadline:
                time.sleep(0.005)
            if ready.exists():
                observed.append(True)
                if os.name == "posix":
                    os.kill(os.getpid(), signal.SIGINT)
                else:
                    signal.getsignal(signal.SIGINT)(signal.SIGINT, None)

        trigger = threading.Thread(target=interrupt)
        trigger.start()
        try:
            self.assertEqual(self.execute(), 130)
        finally:
            trigger.join(timeout=6)
        self.assertTrue(observed)
        summary = json.loads((self.output / "execution.json").read_text())
        self.assertEqual(summary["status"], "cancelled")
        records = [json.loads(Path(path).read_text())
                   for mode in summary["modes"] for path in mode["commands"]]
        self.assertTrue(any(record["outcome"] == "cancelled"
                            for record in records))
        self.assertTrue(all(record["withdrawal_complete"]
                            for record in records))

    def test_interrupted_preflight_keeps_real_130_and_withdrawal_receipt(self):
        driver = self.root / "tools/dev/dev.py"
        driver.write_text(driver.read_text().replace(
            "if action=='doctor': raise SystemExit(0)",
            "if action=='doctor': "
            "(root/'doctor-ready').write_text('started'); time.sleep(30)"))

        def interrupt():
            deadline = time.monotonic() + 5
            while not (self.root / "doctor-ready").exists() and (
                    time.monotonic() < deadline):
                time.sleep(0.005)
            if (self.root / "doctor-ready").exists():
                if os.name == "posix":
                    os.kill(os.getpid(), signal.SIGINT)
                else:
                    signal.getsignal(signal.SIGINT)(signal.SIGINT, None)

        trigger = threading.Thread(target=interrupt)
        trigger.start()
        try:
            self.assertEqual(self.execute(), 130)
        finally:
            trigger.join(timeout=6)
        record = json.loads((self.output / "doctor.command.json").read_text())
        self.assertEqual(record["outcome"], "cancelled")
        self.assertTrue(record["withdrawal_complete"])
        self.assertFalse((self.root / "build/native-debug/"
                          "configure.json").exists())

    def test_relative_output_receipts_work_from_another_working_directory(self):
        self.output = Path("build/matrix")
        with chdir(self.root):
            self.assertEqual(self.execute(), 0)
        summary = json.loads((self.root / self.output /
                              "execution.json").read_text())
        receipts = [summary["preflight_receipt"]] + [
            path for mode in summary["modes"] for path in mode["commands"]]
        for receipt in receipts:
            self.assertTrue(Path(receipt).is_absolute(), receipt)
            self.assertEqual(json.loads(Path(receipt).read_text())
                             ["exit_code"], 0)


if __name__ == "__main__":
    unittest.main()
