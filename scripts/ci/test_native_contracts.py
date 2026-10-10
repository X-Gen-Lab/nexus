"""Exercise single-pass CI execution and complete case-level admission."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from scripts.ci import native_contracts


class NativeExecutionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(
            prefix="nexus CI contracts ")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.build = self.root / "build" / "native-debug"
        self.build.mkdir(parents=True)
        self.output = self.build / "tdd"
        self.output.mkdir()
        self.summary = self.output / "execution.json"
        self.summary.write_text('{"stale": true}\n')
        self.binaries = []
        cmake = ["cmake_minimum_required(VERSION 3.31)",
                 "project(execution NONE)", "enable_testing()"]
        for name in ("first", "second"):
            binary = self.build / name
            binary.write_text(
                f"#!{sys.executable}\n"
                "import os, pathlib, sys\n"
                "if '--gtest_list_tests' in sys.argv:\n"
                "    print('Contract.\\n  Normal\\n  DISABLED_Explicit')\n"
                "    raise SystemExit(0)\n"
                "root = pathlib.Path(__file__).parent\n"
                "name = pathlib.Path(__file__).name\n"
                "count = root / (name + '.count')\n"
                "count.write_text(str(int(count.read_text()) + 1) "
                "if count.exists() else '1')\n"
                "for key in ('GTEST_FILTER', 'GTEST_TOTAL_SHARDS', "
                "'GTEST_SHARD_INDEX'):\n"
                "    assert key not in os.environ\n"
                "assert os.environ['GTEST_ALSO_RUN_DISABLED_TESTS'] == '1'\n"
                "assert os.environ['GTEST_REPEAT'] == '1'\n"
                "output = pathlib.Path(os.environ['GTEST_OUTPUT'][4:])\n"
                "output.mkdir(parents=True, exist_ok=True)\n"
                "mode = (root / 'mode').read_text() "
                "if (root / 'mode').exists() else ''\n"
                "cases = ['Normal', 'DISABLED_Explicit']\n"
                "if mode == 'partial': cases.pop()\n"
                "if mode == 'wrong': cases[1] = 'Other'\n"
                "if mode == 'duplicate': cases[1] = 'Normal'\n"
                "if mode == 'extra': cases.append('Other')\n"
                "if mode == 'empty': cases.clear()\n"
                "xml = '<testsuites name=\"AllTests\" tests=\"' "
                "+ str(len(cases)) + '\"><testsuite name=\"Contract\">'\n"
                "xml += ''.join('<testcase classname=\"Contract\" name=\"' "
                "+ case + '\" status=\"run\" result=\"completed\"/>' "
                "for case in cases)\n"
                "xml += '</testsuite></testsuites>'\n"
                "if mode != 'missing': "
                "(output / (name + '.xml')).write_text(xml)\n"
                "if mode == 'extra-report': "
                "(output / 'unplanned.xml').write_text(xml)\n"
            )
            binary.chmod(0o755)
            self.binaries.append(binary)
            cmake.append(f'add_test(NAME {name} COMMAND "{binary}")')
        cmake.append(f'add_test(NAME ordinary COMMAND "{sys.executable}" '
                     '-c "raise SystemExit(0)")')
        self.cmake = cmake
        self.configure()
        (self.root / "CMakePresets.json").write_text(json.dumps({
            "version": 6,
            "configurePresets": [{
                "name": "native-debug", "generator": "Unix Makefiles",
                "binaryDir": "${sourceDir}/build/native-debug",
            }],
            "testPresets": [{
                "name": "native-debug", "configurePreset": "native-debug",
                "execution": {"noTestsAction": "error"},
            }],
        }))
        (self.build / "google-contracts.json").write_text(json.dumps({
            "schema": 1, "executables": list(map(str, self.binaries)),
        }))

    def configure(self):
        (self.root / "CMakeLists.txt").write_text("\n".join(self.cmake) + "\n")
        cmake = shutil.which("cmake")
        self.assertIsNotNone(cmake)
        result = subprocess.run([cmake, "-S", str(self.root), "-B",
                                 str(self.build)], capture_output=True,
                                text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def execute(self):
        output = io.StringIO()
        with patch.object(native_contracts, "ROOT", self.root), \
                redirect_stdout(output), redirect_stderr(output):
            result = native_contracts.main(["--preset", "native-debug"])
        return result, output.getvalue()

    def assert_rejected(self):
        result, output = self.execute()
        self.assertEqual(result, 1, output)
        self.assertFalse(self.summary.exists(), output)

    def test_all_ctest_and_google_cases_execute_once_with_clean_settings(self):
        with patch.dict(os.environ, {"GTEST_FILTER": "Missing*",
                                     "GTEST_TOTAL_SHARDS": "2",
                                     "GTEST_SHARD_INDEX": "1"}):
            result, output = self.execute()
        self.assertEqual(result, 0, output)
        record = json.loads(self.summary.read_text())
        self.assertEqual(record["passed"], 4)
        self.assertEqual(record["ctest"]["passed"], 3)
        for binary in self.binaries:
            self.assertEqual(binary.with_suffix(".count").read_text(), "1")

    def test_partial_google_run_rejects_even_if_ctest_succeeds(self):
        (self.build / "mode").write_text("partial")
        self.assert_rejected()

    def test_equal_count_with_different_case_identity_rejects(self):
        (self.build / "mode").write_text("wrong")
        self.assert_rejected()

    def test_duplicate_case_identity_rejects(self):
        (self.build / "mode").write_text("duplicate")
        self.assert_rejected()

    def test_extra_case_rejects(self):
        (self.build / "mode").write_text("extra")
        self.assert_rejected()

    def test_empty_google_run_rejects(self):
        (self.build / "mode").write_text("empty")
        self.assert_rejected()

    def test_unplanned_google_report_rejects(self):
        (self.build / "mode").write_text("extra-report")
        self.assert_rejected()

    def test_missing_google_xml_cannot_reuse_previous_success(self):
        old = self.output / "google" / "first.xml"
        old.parent.mkdir()
        old.write_text('<testsuites name="AllTests"/>')
        os.utime(old, (1, 1))
        (self.build / "mode").write_text("missing")
        self.assert_rejected()
        self.assertFalse(old.exists())

    def test_failed_ordinary_ctest_rejects_google_success(self):
        self.cmake[-1] = self.cmake[-1].replace(
            "SystemExit(0)", "SystemExit(1)")
        self.configure()
        self.assert_rejected()

    def test_mixed_skipped_ctest_cannot_establish_complete_execution(self):
        self.cmake[-1] = self.cmake[-1].replace(
            "SystemExit(0)", "SystemExit(7)")
        self.cmake.append("set_tests_properties(ordinary PROPERTIES "
                          "SKIP_RETURN_CODE 7)")
        self.configure()
        self.assert_rejected()

    def test_unregistered_google_binary_rejects_before_execution(self):
        self.cmake = [line for line in self.cmake
                      if not line.startswith("add_test(NAME second ")]
        self.configure()
        self.assert_rejected()
        for binary in self.binaries:
            self.assertFalse(binary.with_suffix(".count").exists())

    def test_duplicate_executable_manifest_rejects(self):
        (self.build / "google-contracts.json").write_text(json.dumps({
            "schema": 1,
            "executables": [str(self.binaries[0]), str(self.binaries[0])],
        }))
        self.assert_rejected()

    def test_preset_filter_cannot_hide_an_ordinary_contract(self):
        path = self.root / "CMakePresets.json"
        presets = json.loads(path.read_text())
        presets["testPresets"][0]["filter"] = {
            "exclude": {"name": "ordinary"},
        }
        path.write_text(json.dumps(presets))
        self.assert_rejected()


if __name__ == "__main__":
    unittest.main()
