import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from quality_tools import commands, run


@unittest.skipUnless(os.name == "posix", "analyzer process fixtures require POSIX; CI analysis runs on Linux")
class RequiredAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        self.build = self.root / "build"
        self.build.mkdir()
        self.source = self.root / "services" / "sample.c"
        self.source.parent.mkdir()
        self.source.write_text("int sample(void) { return 1; }\n")
        self.report = self.root / "reports" / "analysis.txt"
        self.database([self.source])

    def database(self, paths):
        (self.build / "compile_commands.json").write_text(json.dumps([
            {"file": str(p), "directory": str(self.root), "arguments": ["cc", "-c", str(p)]}
            for p in paths]))

    def tool(self, code, version=0):
        path = self.root / "analyzer"
        path.write_text(f"#!{sys.executable}\nimport sys\nprint('model analyzer executed')\nsys.exit({version} if '--version' in sys.argv else {code})\n")
        path.chmod(0o700)
        return str(path)

    def test_success_requires_nonempty_scope(self):
        self.assertEqual(run("tidy", self.root, self.build, self.tool(0), self.report), 0)
        self.assertIn("Exit code: 0", self.report.read_text())

    def test_diagnostic_exit_fails(self):
        self.assertEqual(run("cppcheck", self.root, self.build, self.tool(2), self.report), 1)
        self.assertIn("Exit code: 2", self.report.read_text())

    def test_crashed_tool_fails(self):
        self.assertEqual(run("tidy", self.root, self.build, self.tool(70), self.report), 1)

    def test_tool_version_failure_cannot_pass(self):
        self.assertEqual(run("tidy", self.root, self.build, self.tool(0, 1), self.report), 1)

    def test_missing_tool_fails_and_retains_reason(self):
        self.assertEqual(run("tidy", self.root, self.build, str(self.root / "missing"), self.report), 1)
        self.assertIn("Analysis execution failed", self.report.read_text())

    def test_zero_owned_scope_fails(self):
        external = self.root / "ext" / "vendor.c"
        external.parent.mkdir()
        external.write_text("int vendor;\n")
        self.database([external])
        self.assertEqual(run("cppcheck", self.root, self.build, self.tool(0), self.report), 1)
        self.assertIn("zero owned translation units", self.report.read_text())

    def test_external_vendor_commands_excluded(self):
        self.database([self.source, Path("/tmp/foreign.c"), self.root / "vendors" / "sdk.c"])
        self.assertEqual(len(commands(self.root, self.build)), 1)

    def test_malformed_database_fails(self):
        (self.build / "compile_commands.json").write_text("{}")
        self.assertEqual(run("tidy", self.root, self.build, self.tool(0), self.report), 1)

    def test_missing_owned_source_fails(self):
        self.source.unlink()
        self.assertEqual(run("tidy", self.root, self.build, self.tool(0), self.report), 1)


if __name__ == "__main__":
    unittest.main()
