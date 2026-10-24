"""Thin routing and coherent preset metadata for focused feedback."""

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.dev import dev, device, projects, timings


class DeveloperFeedbackTests(unittest.TestCase):
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
