"""Formatter selection and failure propagation; no clang dependency required."""

import contextlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


REPO = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("nexus_format", REPO / "scripts/tools/format.py")
FORMAT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FORMAT)


class FormatToolTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name).resolve()
        (self.root / "arch").mkdir()
        (self.root / "soc").mkdir()
        self.config = self.root / ".clang-format-dirs"
        self.config.write_text("arch\nsoc\n!vendor\n!ext\n[extensions]\n.c\n.h\n", encoding="utf-8")
        (self.root / ".clang-format").write_text("BasedOnStyle: LLVM\n", encoding="utf-8")
        self.source = self.root / "arch/source with space.c"
        self.source.write_text("int value=1;\n", encoding="utf-8")

    def invoke(self, arguments, results=None):
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(FORMAT, "get_project_root", return_value=self.root), \
             patch.object(FORMAT, "tracked_source_files", return_value=[self.source]), \
             patch.object(FORMAT.shutil, "which", return_value="selected-clang-format"), \
             patch.object(FORMAT.subprocess, "run", side_effect=results or []) as run, \
             contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            status = FORMAT.main(arguments)
        return status, stdout.getvalue(), stderr.getvalue(), run

    @staticmethod
    def tool_results(last_status=0, diagnostics=""):
        return [
            subprocess.CompletedProcess([], 0, "clang-format version 14.0.6\n", ""),
            subprocess.CompletedProcess([], 0, "BasedOnStyle: LLVM\n", ""),
            subprocess.CompletedProcess([], last_status, "", diagnostics),
        ]

    def test_repo_policy_covers_owned_layers_and_excludes_third_party(self):
        includes, exclusions, _ = FORMAT.parse_format_config(REPO / ".clang-format-dirs")
        self.assertTrue({"core", "io", "os", "components", "arch", "soc", "boards", "tests", "tools/measurement"}.issubset(includes))
        for path in ("vendors/sdk/a.c", "ext/library/a.h", "soc/chip/vendor/a.c"):
            self.assertTrue(FORMAT.should_exclude(REPO / path, REPO, exclusions))

    def test_formatted_content_is_bytes_independent_of_locale(self):
        contents = "/* 中文注释 */\r\n".encode("utf-8")
        with patch.object(FORMAT.subprocess, "run", return_value=
                          subprocess.CompletedProcess([], 0, contents, b"")) as run:
            self.assertEqual(FORMAT.formatted_source(self.source, "tool", self.root / ".clang-format"), contents)
            self.assertNotIn("text", run.call_args.kwargs)

    def test_index_selection_excludes_vendor_and_deduplicates_roots(self):
        vendor = self.root / "soc/vendor"
        vendor.mkdir()
        (vendor / "library.c").write_text("int vendor;\n", encoding="utf-8")
        (self.root / "soc/owned.h").write_text("extern int value;\n", encoding="utf-8")
        self.config.write_text(self.config.read_text().replace("arch\nsoc\n", "arch\narch\nsoc\n"))
        untracked = self.root / "arch/untracked.c"
        untracked.write_text("int untracked;\n")
        tracked = [self.source, self.root / "soc/owned.h", vendor / "library.c"]
        with patch.object(FORMAT, "tracked_source_files", return_value=tracked):
            self.assertEqual(FORMAT.find_source_files(self.root),
                             sorted([self.source, self.root / "soc/owned.h"]))

    def test_index_is_nul_delimited_and_failed_git_does_not_scan_files(self):
        result = subprocess.CompletedProcess([], 0, b"arch/source with space.c\0soc/staged.inc\0", b"")
        with patch.object(FORMAT.subprocess, "run", return_value=result) as run:
            self.assertEqual(FORMAT.tracked_source_files(self.root),
                             [self.source, self.root / "soc/staged.inc"])
            self.assertIn("--cached", run.call_args.args[0])
            self.assertIn("-z", run.call_args.args[0])
        with patch.object(FORMAT.subprocess, "run", return_value=
                          subprocess.CompletedProcess([], 128, b"", b"not a git repository")):
            with self.assertRaisesRegex(FORMAT.FormatError, "not a git repository"):
                FORMAT.find_source_files(self.root)

    def test_missing_and_malformed_config_have_no_default_fallback(self):
        for content in (None, "arch\n", "[extensions]\n.c\n", "arch\n[unknown]\n.c\n"):
            with self.subTest(content=content):
                if content is None:
                    self.config.unlink()
                else:
                    self.config.write_text(content, encoding="utf-8")
                status, _, error, run = self.invoke(["--check"])
                self.assertEqual(status, 2)
                self.assertIn("ERROR", error)
                run.assert_not_called()

    def test_missing_style_and_configured_directory_fail_preflight(self):
        (self.root / ".clang-format").unlink()
        status, _, error, run = self.invoke(["--check"])
        self.assertEqual(status, 2)
        self.assertIn("Root clang-format configuration", error)
        run.assert_not_called()
        (self.root / ".clang-format").write_text("BasedOnStyle: LLVM\n")
        self.config.write_text("renamed-directory\n[extensions]\n.c\n")
        status, _, error, run = self.invoke(["--check"])
        self.assertEqual(status, 2)
        self.assertIn("Configured directory not found", error)
        run.assert_not_called()

    def test_zero_files_is_failure_not_a_pass(self):
        self.config.write_text("arch\nsoc\n[extensions]\n.hpp\n")
        status, output, error, run = self.invoke(["--check"])
        self.assertEqual(status, 2)
        self.assertNotIn("Checked", output)
        self.assertIn("No owned source files", error)
        run.assert_not_called()

    def test_explicit_files_cannot_override_owned_roots_or_exclusions(self):
        bad_paths = [self.root / "outside.c", self.root / "soc/ext/library.c", self.root / "arch/note.txt"]
        for path in bad_paths:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("not to be formatted", encoding="utf-8")
            with self.subTest(path=path):
                status, _, _, run = self.invoke(["--files", str(self.source), str(path)])
                self.assertEqual(status, 2)
                run.assert_not_called()
                self.assertEqual(path.read_text(), "not to be formatted")

    def test_symlink_cannot_escape_project(self):
        with tempfile.TemporaryDirectory() as external:
            target = Path(external) / "outside.c"
            target.write_text("int outside;\n")
            link = self.root / "arch/escape.c"
            try:
                link.symlink_to(target)
            except OSError as error:
                self.skipTest(f"Symlinks unavailable: {error}")
            with self.assertRaisesRegex(FORMAT.FormatError, "escapes"):
                FORMAT.find_source_files(self.root, explicit_files=[str(link)])

    def test_tracked_symlink_does_not_select_an_untracked_owned_target(self):
        link = self.root / "soc/alias.c"
        try:
            link.symlink_to(self.source)
        except OSError as error:
            self.skipTest(f"Symlinks unavailable: {error}")
        with patch.object(FORMAT, "tracked_source_files", return_value=[link]):
            with self.assertRaisesRegex(FORMAT.FormatError, "symlinks"):
                FORMAT.find_source_files(self.root)

    def test_check_uses_root_style_and_preserves_tool_diagnostics(self):
        (self.root / "arch/.clang-format").write_text("BasedOnStyle: Google\n")
        status, _, error, run = self.invoke(
            ["check", "--files", str(self.source)], self.tool_results(1, "formatting diagnostic"))
        self.assertEqual(status, 1)
        self.assertIn("formatting diagnostic", error)
        command = run.call_args_list[-1].args[0]
        self.assertIn(f"--style=file:{self.root / '.clang-format'}", command)
        self.assertIn("--fallback-style=none", command)
        self.assertIn("--dry-run", command)
        self.assertIn("--Werror", command)
        self.assertNotIn("-i", command)
        self.assertEqual(command[-1], str(self.source))

    def test_invalid_root_style_is_rejected_before_in_place_format(self):
        results = self.tool_results()
        results[1] = subprocess.CompletedProcess([], 1, "", "unknown style key")
        status, _, error, run = self.invoke([], results)
        self.assertEqual(status, 2)
        self.assertIn("unknown style key", error)
        self.assertEqual(run.call_count, 2)
        self.assertEqual(self.source.read_text(), "int value=1;\n")

    def test_show_config_does_not_require_tool_and_explicit_files_deduplicate(self):
        status, output, _, run = self.invoke(
            ["--show-config", "--files", str(self.source), "--files", str(self.source)])
        self.assertEqual(status, 0)
        self.assertIn("Selected owned files: 1", output)
        run.assert_not_called()

    def test_explicit_missing_tool_is_not_replaced_by_default(self):
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(FORMAT, "get_project_root", return_value=self.root), \
             patch.object(FORMAT.shutil, "which", return_value=None) as which, \
             patch.object(FORMAT, "default_tool") as default, \
             contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            self.assertEqual(FORMAT.main(["--check", "--tool", "missing-tool", "--files",
                                          str(self.source)]), 2)
        which.assert_called_once_with("missing-tool")
        default.assert_not_called()

    def test_locked_environment_precedes_path_and_output_helper_is_nonmutating(self):
        executable = self.root / ".venv/bin/clang-format"
        executable.parent.mkdir(parents=True)
        executable.touch()
        self.assertEqual(FORMAT.default_tool(self.root), str(executable))
        with patch.object(FORMAT.subprocess, "run", return_value=
                          subprocess.CompletedProcess([], 0, "int value = 1;\n", "")):
            self.assertEqual(FORMAT.formatted_source(self.source, "tool", self.root / ".clang-format"),
                             "int value = 1;\n")
        self.assertEqual(self.source.read_text(), "int value=1;\n")

    @unittest.skipUnless(os.name == "posix", "Shell wrapper requires POSIX")
    def test_shell_wrapper_preserves_foreign_working_directory_and_failure(self):
        command = ["bash", str(REPO / "scripts/tools/format.sh"), "check", "--tool",
                   "nexus-missing-clang-format", "--files",
                   str(REPO / "arch/cortex_m4/nx_arch_cortex_m4.c")]
        result = subprocess.run(command, cwd=self.root, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 2)
        self.assertIn("nexus-missing-clang-format", result.stderr)
        self.assertNotIn("Checked", result.stdout)


if __name__ == "__main__":
    unittest.main()
