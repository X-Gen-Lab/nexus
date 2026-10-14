"""The docs command preserves failures and only owns its explicit output."""

from contextlib import redirect_stderr, redirect_stdout
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.documentation import cli
from tools.dev import dev


class DocumentationCommandTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "docs").mkdir()
        (self.root / "docs/index.md").write_text("# Start\n")

    def test_valid_source_check_creates_explicit_report(self):
        report = self.root / "build/result.json"
        with redirect_stdout(io.StringIO()):
            result = cli.main(["check", "--report", str(report)], root=self.root)
        self.assertEqual(result, 0)
        self.assertIn('"status": "passed"', report.read_text())

    def test_bad_source_returns_failure_with_diagnostics(self):
        (self.root / "docs/index.md").write_text("# Start\n[bad](absent.md)\n")
        output = io.StringIO()
        with redirect_stdout(io.StringIO()), redirect_stderr(output):
            result = cli.main(["check"], root=self.root)
        self.assertNotEqual(result, 0)
        self.assertIn("missing", output.getvalue())

    def test_build_output_cannot_overlap_source_docs(self):
        with self.assertRaisesRegex(ValueError, "source|input"):
            cli.prepare_output(self.root, self.root / "docs")

    def test_existing_unowned_output_is_preserved(self):
        destination = self.root / "build/output"
        destination.mkdir(parents=True)
        (destination / "valuable.txt").write_text("keep")
        with self.assertRaisesRegex(ValueError, "owned|ownership"):
            cli.prepare_output(self.root, destination)
        self.assertEqual((destination / "valuable.txt").read_text(), "keep")

    def test_output_symlink_is_rejected(self):
        actual = self.root / "actual"
        actual.mkdir()
        destination = self.root / "linked"
        destination.symlink_to(actual, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            cli.prepare_output(self.root, destination)

    def test_owned_output_reuse_removes_stale_success(self):
        destination = self.root / "build/output"
        cli.prepare_output(self.root, destination)
        (destination / "documentation-build.json").write_text('{"status":"passed"}')
        cli.prepare_output(self.root, destination)
        self.assertFalse((destination / "documentation-build.json").exists())

    def test_thin_dev_route_returns_actual_docs_exit_code(self):
        with patch.object(dev, "execute", return_value=7) as execute:
            result = dev.main(["docs", "check"])
        self.assertEqual(result, 7)
        self.assertEqual(execute.call_args.args[0][1:],
                         ["tools/documentation/cli.py", "check"])

    def test_docs_doctor_route_does_not_require_mcu_build_tools(self):
        with patch.object(dev, "execute", return_value=9) as execute:
            result = dev.main(["doctor", "--target", "docs"])
        self.assertEqual(result, 9)
        self.assertEqual(execute.call_args.args[0][1:],
                         ["tools/documentation/cli.py", "doctor"])

    def test_failed_full_diagram_check_returns_failure(self):
        from tools.documentation import diagrams, install
        with patch.object(install, "environment", return_value={}), \
                patch.object(diagrams, "check", return_value={
                    "status": "failed", "errors": ["stale diagram"]}), \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = cli.main(["check", "--full"], root=self.root)
        self.assertNotEqual(result, 0)

    def test_staged_check_rejects_unstaged_source_repair(self):
        import subprocess
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        subprocess.run(["git", "-C", str(self.root), "add", "docs"], check=True)
        (self.root / "docs/index.md").write_text("# Changed outside index\n")
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = cli.main(["check", "--staged"], root=self.root)
        self.assertNotEqual(result, 0)

    def test_report_cannot_overwrite_a_document_input(self):
        page = self.root / "docs/index.md"
        original = page.read_bytes()
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = cli.main(["check", "--report", str(page)], root=self.root)
        self.assertNotEqual(result, 0)
        self.assertEqual(page.read_bytes(), original)

    def test_staged_check_rejects_a_missing_indexed_page(self):
        import subprocess
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        subprocess.run(["git", "-C", str(self.root), "add", "docs"], check=True)
        (self.root / "docs/index.md").unlink()
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = cli.main(["check", "--staged"], root=self.root)
        self.assertNotEqual(result, 0)

    def test_preview_requires_a_successful_build(self):
        output = self.root / "build/documentation"
        output.mkdir(parents=True)
        (output / "documentation-build.json").write_text('{"status":"failed"}')
        with patch.object(cli, "ThreadingHTTPServer") as server, \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = cli.main(["preview", "--build", str(output)], root=self.root)
        self.assertNotEqual(result, 0)
        server.assert_not_called()

    def test_staged_check_compares_svg_input_bytes(self):
        import subprocess
        image = self.root / "docs/assets/diagram.svg"
        image.parent.mkdir(parents=True)
        image.write_text('<svg xmlns="http://www.w3.org/2000/svg"><script/></svg>')
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        subprocess.run(["git", "-C", str(self.root), "add", "docs"], check=True)
        image.write_text('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"/>')
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = cli.main(["check", "--staged"], root=self.root)
        self.assertNotEqual(result, 0)

    def test_generated_svg_normalization_preserves_local_elements(self):
        folder = self.root / "output"
        folder.mkdir()
        image = folder / "doxygen.svg"
        image.write_text('<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" '
                         '"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">'
                         '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10">'
                         '<path id="local" d="M 0 0"/></svg>')
        cli.normalize_generated_svg(folder)
        self.assertNotIn(b"<!DOCTYPE", image.read_bytes())
        self.assertIn(b'id="local"', image.read_bytes())


if __name__ == "__main__":
    unittest.main()
