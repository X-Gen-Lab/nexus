"""Independent regressions for literal source-input snapshot admission."""

import subprocess
import contextlib
import io
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from tools.documentation import cli, results


class LiteralFilenameReviewTests(unittest.TestCase):
    def fixture(self, root, rendered_name, checked_name):
        """Stage a decoy; keep the actual literal filename untracked."""
        (root / "docs").mkdir()
        (root / "tests").mkdir()
        page = root / "docs/index.md"
        page.write_text("# Literal source input\n\n"
                        "```{literalinclude} ../tests/" + rendered_name
                        + "\n```\n", encoding="utf-8")
        (root / "tests" / checked_name).write_text(
            "int staged_decoy;\n", encoding="utf-8")
        (root / "tests" / rendered_name).write_text(
            "int actual_rendered_input;\n", encoding="utf-8")
        subprocess.run(["git", "init", "-q", str(root)], check=True)
        subprocess.run(["git", "-C", str(root), "add", "docs/index.md",
                        "tests/" + checked_name], check=True)

    def test_literal_filename_fragment_cannot_select_a_staged_decoy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root, "consumer.c#extra", "consumer.c")
            with self.assertRaisesRegex(ValueError, "input|index|literal"):
                cli._staged_sources(root)

    def test_literal_percent_filename_cannot_select_a_staged_decoy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root, "consumer%2Ec", "consumer.c")
            with self.assertRaisesRegex(ValueError, "input|index|literal"):
                cli._staged_sources(root)

    def test_escaped_rst_colon_keeps_real_nested_include_in_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "docs").mkdir()
            (root / "tests").mkdir()
            (root / "docs/index.rst").write_text(
                "Escaped punctuation \\::\n\n"
                "   .. include:: ../tests/consumer.c\n",
                encoding="utf-8")
            (root / "tests/consumer.c").write_text(
                "Actual included input.\n", encoding="utf-8")
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            subprocess.run(["git", "-C", str(root), "add", "docs/index.rst"],
                           check=True)
            with self.assertRaisesRegex(ValueError, "input|index|include"):
                cli._staged_sources(root)

    def test_report_cannot_delete_or_overwrite_a_rendered_build_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "docs").mkdir()
            (root / "build").mkdir()
            (root / "docs/index.md").write_text(
                "# Retained generated input\n\n"
                "```{literalinclude} ../build/input.json\n```\n",
                encoding="utf-8")
            input_path = root / "build/input.json"
            original = b'{"retained_documentation_input": true}\n'
            input_path.write_bytes(original)
            with contextlib.redirect_stdout(io.StringIO()), \
                    contextlib.redirect_stderr(io.StringIO()):
                result = cli.main(["check", "--report", str(input_path)],
                                  root=root)
            self.assertEqual(result, 2)
            self.assertEqual(input_path.read_bytes(), original)

    def test_escaping_input_cannot_leave_a_previous_passed_report(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "docs").mkdir()
            (root / "build").mkdir()
            (root / "docs/index.md").write_text(
                "# Rejected dependency\n\n"
                "```{literalinclude} ../../outside.c\n```\n",
                encoding="utf-8")
            report = root / "build/result.json"
            report.write_text('{"status": "passed"}\n', encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()), \
                    contextlib.redirect_stderr(io.StringIO()):
                result = cli.main(["check", "--report", str(report)],
                                  root=root)
            self.assertNotEqual(result, 0)
            self.assertEqual(json.loads(report.read_text())["status"], "failed")


class RetainedIndexUrlReviewTests(unittest.TestCase):
    def test_immutable_storage_url_rejects_a_non_numeric_port(self):
        source_root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in (results.INDEX_PATH, results.TASK_AUTHORITY,
                         results.QUALIFIED_INDEX):
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source_root / name, target)
            path = root / results.INDEX_PATH
            document = json.loads(path.read_text(encoding="utf-8"))
            old = document["evidence_delivery"]["parts_url"]
            document["evidence_delivery"]["parts_url"] = old.replace(
                "raw.githubusercontent.com/",
                "raw.githubusercontent.com:not-a-port/")
            path.write_text(json.dumps(document), encoding="utf-8")
            self.assertTrue(results.validate(root))


if __name__ == "__main__":
    unittest.main()
