"""Thin routing and coherent preset metadata for focused feedback."""

from contextlib import redirect_stdout
from concurrent.futures import ThreadPoolExecutor
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

from tools.dev import dev, device, projects, timings


class DeveloperFeedbackTests(unittest.TestCase):
    def test_parallel_commands_publish_one_complete_immutable_latest_view(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rendezvous = threading.Barrier(3)
            cancellation = threading.Event()
            ready = root / "cancel-ready"

            def execute(label, code):
                rendezvous.wait(timeout=5)
                return dev.execute([sys.executable, "-B", "-c", code],
                                   os.environ.copy(), cancel_event=(
                                       cancellation if label == "third"
                                       else None))

            with patch.object(dev, "ROOT", root), ThreadPoolExecutor(
                    max_workers=3) as pool:
                futures = [pool.submit(execute, label,
                    f"import time; time.sleep(0.05); print('{label}'); "
                    f"raise SystemExit({exit_code})")
                    for label, exit_code in (("first", 0), ("second", 7))]
                futures.append(pool.submit(execute, "third",
                    "import pathlib, time; pathlib.Path(" + repr(str(ready)) +
                    ").write_text('started'); time.sleep(30)"))
                deadline = time.monotonic() + 5
                while not ready.exists() and time.monotonic() < deadline:
                    if any(future.done() and future.exception()
                           for future in futures):
                        break
                    time.sleep(0.005)
                cancellation.set()
                results = [future.result(timeout=10) for future in futures]
            self.assertTrue(ready.exists())
            self.assertEqual(list(map(int, results)), [0, 7, 130])
            receipts = [result.receipt_path for result in results]
            self.assertEqual(len(set(receipts)), 3)
            view = json.loads((root / "build/quality/"
                               "developer-command.command.json").read_text())
            self.assertIn(Path(view["attempt_receipt"]), receipts)
            log = view["log"]
            path = Path(log["path"])
            self.assertNotEqual(path, root / "build/quality/"
                                "developer-command.txt")
            self.assertEqual(log["bytes"], path.stat().st_size)
            self.assertEqual(log["sha256"], hashlib.sha256(
                path.read_bytes()).hexdigest())
            attempt = json.loads(Path(view["attempt_receipt"]).read_text())
            self.assertEqual(view["command"], attempt["command"])
            self.assertEqual(path.read_text(),
                Path(attempt["stdout"]["path"]).read_text() +
                Path(attempt["stderr"]["path"]).read_text())
            for result in results:
                retained = json.loads(result.receipt_path.read_text())
                if int(result) == 130:
                    self.assertEqual(retained["outcome"], "cancelled")
                    self.assertTrue(retained["withdrawal_complete"])
                combined = result.receipt_path.parents[2] / "command.txt"
                self.assertEqual(combined.read_text(),
                    Path(retained["stdout"]["path"]).read_text() +
                    Path(retained["stderr"]["path"]).read_text())

    def test_new_commands_delegate_and_preserve_actual_failures(self):
        for action, module in (("create", projects), ("timings", timings),
                               ("flash", device), ("debug", device),
                               ("monitor", device)):
            with self.subTest(action=action), patch.object(
                    module, "main", return_value=9) as execute:
                self.assertEqual(dev.main(
                    [action, "--unknown-native-option"]), 9)
                arguments = execute.call_args.args
                expected = (["--unknown-native-option"],)
                if module is device:
                    expected = (action, *expected)
                self.assertEqual(arguments, expected)

    def test_one_preset_snapshot_keeps_mapping_and_filter_consistent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            preset = root / "CMakePresets.json"
            data = {"version": 6, "configurePresets": [
                {"name": "config", "binaryDir": "${sourceDir}/build/config"}],
                "testPresets": [{
                    "name": "contracts", "configurePreset": "config",
                    "filter": {"include": {"name": "Selected"}}}]}
            preset.write_text(json.dumps(data))
            read_text = Path.read_text
            reads = []

            def changing_file(path, *args, **kwargs):
                value = read_text(path, *args, **kwargs)
                if path == preset:
                    reads.append(value)
                    # Simulate an editor replacing metadata after observation.
                    data["testPresets"][0].pop("filter", None)
                    preset.write_text(json.dumps(data))
                return value

            report = root / "build/config/ctest-results.xml"

            def execute(command, environment):
                del environment
                self.assertIn(str(report), command)
                report.write_text(
                    '<testsuites tests="1" failures="0" errors="0">'
                    '<testsuite name="selected" tests="1" failures="0" '
                    'errors="0"><testcase name="works"/>'
                    '</testsuite></testsuites>')
                return 0

            with patch.object(dev, "ROOT", root), patch.object(
                    dev, "executable", return_value="ctest"), patch.object(
                    Path, "read_text", changing_file), patch.object(
                    dev, "execute", side_effect=execute), redirect_stdout(
                        io.StringIO()):
                self.assertEqual(dev.main(["test", "--preset", "contracts"]), 0)
            record = json.loads(
                report.with_suffix(".execution.json").read_text())
            self.assertEqual(record["kind"], "focused-development")
            self.assertFalse(record["qualification"])
            self.assertEqual(len(reads), 1)


if __name__ == "__main__":
    unittest.main()
