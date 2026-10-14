"""The docs command preserves failures and only owns its explicit output."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import subprocess
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

    def test_owned_output_reuse_cannot_remove_rendered_inputs(self):
        destination = self.root / "build/output"
        cli.prepare_output(self.root, destination)
        included = destination / "consumer.c"
        original = "int retained_rendered_input;\n"
        included.write_text(original)
        (self.root / "docs/index.md").write_text(
            "# Start\n\n```{literalinclude} ../build/output/consumer.c\n```\n")
        with self.assertRaisesRegex(ValueError, "input"):
            cli.prepare_output(self.root, destination)
        self.assertEqual(included.read_text(), original)

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

    def staged_include(self, *, target="tests/consumer.c", stage_target=False):
        """Exercise Git admission using a real include outside fixed roots."""
        page = self.root / "docs/index.md"
        page.write_text("# Start\n\n```{literalinclude} ../" + target
                        + "\n```\n")
        included = self.root / target
        included.parent.mkdir(parents=True, exist_ok=True)
        included.write_text("int consumer(void) { return 0; }\n")
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        subprocess.run(["git", "-C", str(self.root), "add", "docs"], check=True)
        if stage_target:
            subprocess.run(["git", "-C", str(self.root), "add", target],
                           check=True)
        return included

    def run_staged_check(self):
        output = io.StringIO()
        with redirect_stdout(io.StringIO()), redirect_stderr(output):
            result = cli.main(["check", "--staged"], root=self.root)
        return result, output.getvalue()

    def test_staged_check_rejects_untracked_literalinclude_dependency(self):
        self.staged_include()
        result, errors = self.run_staged_check()
        self.assertNotEqual(result, 0)
        self.assertIn("tests/consumer.c", errors)

    def test_failed_staged_admission_cannot_retain_a_passed_report(self):
        report = self.root / "build/result.json"
        with redirect_stdout(io.StringIO()):
            result = cli.main(["check", "--report", str(report)], root=self.root)
        self.assertEqual(result, 0)
        self.staged_include()
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = cli.main(["check", "--staged", "--report", str(report)],
                              root=self.root)
        self.assertNotEqual(result, 0)
        record = json.loads(report.read_text())
        self.assertEqual(record["status"], "failed")
        self.assertTrue(record["errors"])

    def test_report_cannot_create_a_missing_rendered_build_input(self):
        target = self.root / "build/input.json"
        (self.root / "docs/index.md").write_text(
            "# Start\n\n```{literalinclude} ../build/input.json\n```\n")
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = cli.main(["check", "--report", str(target)], root=self.root)
        self.assertEqual(result, 2)
        self.assertFalse(target.exists())

    def test_missing_include_still_replaces_previous_passed_report(self):
        report = self.root / "build/result.json"
        with redirect_stdout(io.StringIO()):
            result = cli.main(["check", "--report", str(report)], root=self.root)
        self.assertEqual(result, 0)
        (self.root / "docs/index.md").write_text(
            "# Start\n\n```{literalinclude} ../tests/absent.c\n```\n")
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = cli.main(["check", "--report", str(report)], root=self.root)
        self.assertEqual(result, 1)
        self.assertEqual(json.loads(report.read_text())["status"], "failed")

    def test_staged_check_rejects_unstaged_literalinclude_dependency_bytes(self):
        included = self.staged_include(stage_target=True)
        included.write_text("int consumer(void) { return 1; }\n")
        result, errors = self.run_staged_check()
        self.assertNotEqual(result, 0)
        self.assertIn("tests/consumer.c", errors)

    def test_staged_check_accepts_exact_literalinclude_dependency_bytes(self):
        self.staged_include(stage_target=True)
        result, errors = self.run_staged_check()
        self.assertEqual((result, errors), (0, ""))

    def test_staged_check_rejects_transitive_include_dependency(self):
        included = self.staged_include(target="examples/included.md",
                                       stage_target=True)
        included.write_text("```{literalinclude} private/consumer.c\n```\n")
        (self.root / "docs/index.md").write_text(
            "# Start\n\n```{include} ../examples/included.md\n```\n")
        child = included.parent / "private/consumer.c"
        child.parent.mkdir()
        child.write_text("int consumer(void) { return 0; }\n")
        subprocess.run(["git", "-C", str(self.root), "add", "docs",
                        "examples/included.md"], check=True)
        result, errors = self.run_staged_check()
        self.assertNotEqual(result, 0)
        self.assertIn("examples/private/consumer.c", errors)

    def test_staged_rst_include_uses_parent_syntax_for_text_fragment(self):
        (self.root / "docs/index.md").write_text(
            "# Start\n\n```{toctree}\npage\n```\n")
        (self.root / "docs/page.rst").write_text(
            "Page\n====\n\n.. include:: ../examples/fragment.txt\n")
        fragment = self.root / "examples/fragment.txt"
        fragment.parent.mkdir()
        fragment.write_text(".. literalinclude:: private/consumer.c\n")
        consumer = fragment.parent / "private/consumer.c"
        consumer.parent.mkdir()
        consumer.write_text("int consumer(void) { return 0; }\n")
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        subprocess.run(["git", "-C", str(self.root), "add", "docs",
                        "examples/fragment.txt"], check=True)
        result, errors = self.run_staged_check()
        self.assertNotEqual(result, 0)
        self.assertIn("examples/private/consumer.c", errors)

    def test_staged_check_rejects_untracked_markdown_image_dependency(self):
        image = self.staged_include(target="examples/image.png")
        (self.root / "docs/index.md").write_text(
            "# Start\n\n![example](../examples/image.png)\n")
        subprocess.run(["git", "-C", str(self.root), "add", "docs"], check=True)
        result, errors = self.run_staged_check()
        self.assertNotEqual(result, 0)
        self.assertIn(image.relative_to(self.root).as_posix(), errors)

    def test_staged_check_compares_local_static_asset_bytes(self):
        folder = self.root / "docs/assets/styles"
        folder.mkdir(parents=True)
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        for filename in ("search.js", "font.woff2"):
            with self.subTest(filename=filename):
                asset = folder / filename
                original = b"static asset bytes\0\xff"
                asset.write_bytes(original)
                subprocess.run(["git", "-C", str(self.root), "add", "docs"],
                               check=True)
                asset.write_bytes(b"different unpublished bytes\0\xff")
                result, errors = self.run_staged_check()
                self.assertNotEqual(result, 0)
                self.assertIn(filename, errors)
                asset.write_bytes(original)

    def test_fast_check_rejects_external_resource_in_registered_dot(self):
        folder = self.root / "docs/assets/diagrams"
        folder.mkdir(parents=True)
        (folder / "diagram-index.csv").write_text(
            "diagram_id,source_dot,rendered_svg,render_status\n"
            "FIG-01,one.dot,one.svg,rendered\n")
        (folder / "one.dot").write_text(
            'digraph one { a [href="https://example.invalid/x"]; }\n')
        (folder / "one.svg").write_text(
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"/>')
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = cli.main(["check"], root=self.root)
        self.assertNotEqual(result, 0)

    def test_fast_check_rejects_malformed_results_index(self):
        index = self.root / "docs/status/documentation-results.json"
        index.parent.mkdir()
        index.write_text("{ broken json\n")
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = cli.main(["check"], root=self.root)
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
