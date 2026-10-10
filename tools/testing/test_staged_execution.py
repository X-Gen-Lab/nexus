"""A late real source or index change must invalidate staged test evidence."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from scripts.ci import tdd_gate


class StagedExecutionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.build = self.root / "build" / "native-debug"
        self.binary = self.build / "bin" / "contract"
        self.binary.parent.mkdir(parents=True)
        self.binary.write_text("fixture executable\n")
        (self.root / ".gitignore").write_text("build/\n")
        (self.root / "core").mkdir()
        (self.root / "core" / "state.c").write_text("int state;\n")
        (self.root / "CMakePresets.json").write_text(json.dumps({
            "configurePresets": [{
                "name": "native-debug",
                "binaryDir": "${sourceDir}/build/native-debug",
            }],
        }))
        (self.build / "google-contracts.json").write_text(json.dumps({
            "schema": 1, "executables": [str(self.binary)],
        }))
        self.git("init", "-q")
        self.git("add", ".")

    def git(self, *arguments):
        return subprocess.run(["git", *arguments], cwd=self.root,
                              capture_output=True, text=True, check=True)

    def pipeline(self, mutation):
        def execute(command, _environment, _log):
            if "--report" in command:
                report = Path(command[command.index("--report") + 1])
                report.write_text(json.dumps({"passed": 1}))
            elif "--gtest_list_tests" in command:
                return "Example.\n  Runs\n"
            else:
                xml_option = next((argument for argument in command
                                   if argument.startswith("--gtest_output=")),
                                  None)
                if xml_option:
                    report = Path(xml_option.split("xml:", 1)[1])
                    report.write_text(
                        '<testsuites tests="1" failures="0" errors="0"'
                        ' disabled="0" name="AllTests"><testsuite'
                        ' name="Example" tests="1" failures="0" errors="0"'
                        ' skipped="0"><testcase name="Runs"'
                        ' classname="Example" status="run"'
                        ' result="completed"/></testsuite></testsuites>'
                    )
                    mutation()
            return ""

        output = io.StringIO()
        with patch.object(tdd_gate, "ROOT", self.root), \
                patch.object(tdd_gate, "execute", side_effect=execute), \
                patch.dict("os.environ", {}, clear=True), \
                redirect_stdout(output), redirect_stderr(output):
            result = tdd_gate.main(["--staged", "--all"])
        return result, output.getvalue()

    def assert_rejected(self, mutation):
        summary = self.build / "tdd" / "execution.json"
        summary.parent.mkdir(parents=True, exist_ok=True)
        summary.write_text('{"previous": "unrelated execution"}\n')
        result, output = self.pipeline(mutation)
        self.assertEqual(result, 1, output)
        self.assertFalse(summary.exists(), output)

    def test_unchanged_real_index_can_accept_completed_tests(self):
        result, output = self.pipeline(lambda: None)
        self.assertEqual(result, 0, output)
        summary = self.build / "tdd" / "execution.json"
        self.assertEqual(json.loads(summary.read_text())["passed"], 1)

    def test_late_unstaged_source_change_rejects_new_summary(self):
        self.assert_rejected(lambda: (self.root / "core" / "state.c")
                             .write_text("int state = 1;\n"))

    def test_late_untracked_source_rejects_new_summary(self):
        def mutation():
            (self.root / "io").mkdir()
            (self.root / "io" / "late.c").write_text("int late;\n")

        self.assert_rejected(mutation)

    def test_late_staged_source_change_rejects_new_summary(self):
        initial = self.git("write-tree").stdout

        def mutation():
            (self.root / "core" / "state.c").write_text("int state = 1;\n")
            self.git("add", "core/state.c")
            self.assertNotEqual(self.git("write-tree").stdout, initial)
            self.assertEqual(self.git("diff", "--name-only").stdout, "")

        self.assert_rejected(mutation)


if __name__ == "__main__":
    unittest.main()
