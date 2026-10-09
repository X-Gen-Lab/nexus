"""Development-tool installation tests; no real pip, venv or Git writes."""

from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "setup/install_dev_tools.py"
SPEC = importlib.util.spec_from_file_location("install_dev_tools", SCRIPT)
INSTALLER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(INSTALLER)


class InstallDevToolsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.lock = self.root / INSTALLER.LOCK_NAME
        self.lock.parent.mkdir()
        # Versions deliberately differ from the repository's initial lock.
        self.pins = {"pre-commit": "5.2.1", "clang-format": "19.3.2"}
        self.lock.write_text("# Tool pins\npre-commit==5.2.1\nclang-format==19.3.2\n", encoding="utf-8")
        self.installed = dict(self.pins)
        self.hooks_path = None
        self.python_version = [3, 10, 1]
        self.prefix = self.root / ".venv"
        self.formatter_version = self.pins["clang-format"]
        self.pre_commit_version = self.pins["pre-commit"]
        self.fail_pip = False
        self.fail_pre_commit = False
        self.update_after_pip = True
        self.calls = []

    def create_venv(self, directory):
        directory.mkdir()
        (directory / "pyvenv.cfg").write_text("home = fake-python\n", encoding="utf-8")
        python, formatter = INSTALLER.venv_paths(directory)
        python.parent.mkdir()
        python.touch()
        formatter.touch()

    def fake_run(self, command, **kwargs):
        command = [str(part) for part in command]
        self.calls.append(command)
        self.assertEqual(Path(kwargs["cwd"]), self.root)
        if command == ["git", "rev-parse", "--show-toplevel"]:
            return subprocess.CompletedProcess(command, 0, str(self.root) + "\n", "")
        if command == ["git", "config", "--get", "core.hooksPath"]:
            code = 1 if self.hooks_path is None else 0
            return subprocess.CompletedProcess(command, code, self.hooks_path or "", "")
        if "-c" in command:
            data = {
                "python": self.python_version,
                "prefix": str(self.prefix),
                "base_prefix": str(self.root / "base-python"),
                "versions": self.installed,
            }
            return subprocess.CompletedProcess(command, 0, json.dumps(data), "")
        if "pip" in command:
            if self.fail_pip:
                raise subprocess.CalledProcessError(7, command, stderr="package download failed")
            if self.update_after_pip:
                self.installed = dict(self.pins)
            return subprocess.CompletedProcess(command, 0, "", "")
        if "pre_commit" in command and "--version" in command:
            if self.fail_pre_commit:
                raise subprocess.CalledProcessError(1, command, stderr="missing dependency")
            return subprocess.CompletedProcess(command, 0, f"pre-commit {self.pre_commit_version}\n", "")
        if command[-1] == "--version":
            return subprocess.CompletedProcess(command, 0, f"clang-format version {self.formatter_version}\n", "")
        if "pre_commit" in command and "install" in command:
            return subprocess.CompletedProcess(command, 0, "", "")
        raise AssertionError(f"Unexpected command: {command}")

    def invoke(self, arguments=()):
        output, errors = io.StringIO(), io.StringIO()
        with patch.object(INSTALLER, "ROOT", self.root), \
                patch.object(INSTALLER.subprocess, "run", side_effect=self.fake_run), \
                patch.object(INSTALLER.venv, "EnvBuilder") as builder, \
                redirect_stdout(output), redirect_stderr(errors):
            builder.return_value.create.side_effect = self.create_venv
            result = INSTALLER.main(list(arguments))
            created = builder.return_value.create.call_count
        return result, created, output.getvalue(), errors.getvalue()

    def test_lock_versions_are_authoritative_and_allow_comments(self):
        self.lock.write_text(
            "# Locked\n\npre-commit==5.2.1 # reviewed\nclang-format==19.3.2\n",
            encoding="utf-8",
        )
        self.assertEqual(INSTALLER.read_lock(self.lock), self.pins)

    def test_invalid_lock_is_rejected(self):
        invalid = (
            "", "pre-commit==5.2.1\n", "pre-commit>=5.2.1\nclang-format==19.3.2\n",
            "pre-commit==5.2.1\npre-commit==5.2.1\nclang-format==19.3.2\n",
            "pre-commit==5.2.1\nclang-format==19.3.2\nother==1.2.3\n",
            "-r other.txt\npre-commit==5.2.1\nclang-format==19.3.2\n",
            "pre-commit==05.2.1\nclang-format==19.3.2\n",
            "pre-commit==5.2.1#not-a-comment\nclang-format==19.3.2\n",
        )
        for contents in invalid:
            with self.subTest(contents=contents):
                self.lock.write_text(contents, encoding="utf-8")
                with self.assertRaises(INSTALLER.InstallError):
                    INSTALLER.read_lock(self.lock)

    def test_missing_lock_fails_without_commands_or_venv(self):
        self.lock.unlink()
        result, created, _, error = self.invoke()
        self.assertEqual(result, 1)
        self.assertEqual(created, 0)
        self.assertEqual(self.calls, [])
        self.assertIn("Cannot read tool lock", error)

    def test_cross_platform_venv_paths(self):
        directory = self.root / ".venv"
        self.assertEqual(INSTALLER.venv_paths(directory, "nt"), (
            directory / "Scripts/python.exe", directory / "Scripts/clang-format.exe",
        ))
        self.assertEqual(INSTALLER.venv_paths(directory, "posix"), (
            directory / "bin/python", directory / "bin/clang-format",
        ))

    def test_fresh_install_uses_lock_and_default_hook_migration(self):
        self.installed = {name: None for name in self.pins}
        result, created, _, error = self.invoke()
        self.assertEqual((result, created, error), (0, 1, ""))
        python, _ = INSTALLER.venv_paths(self.root / ".venv")
        self.assertIn([str(python), "-m", "pip", "install", "-r", str(self.lock)], self.calls)
        self.assertIn([
            str(python), "-I", "-m", "pre_commit", "install", "--install-hooks",
            "--hook-type", "pre-commit", "--hook-type", "commit-msg",
        ], self.calls)
        self.assertTrue(all("--overwrite" not in call for call in self.calls))
        self.assertTrue(all("--global" not in call for call in self.calls))

    def test_matching_tools_reuse_venv_without_pip(self):
        self.create_venv(self.root / ".venv")
        result, created, output, error = self.invoke()
        self.assertEqual((result, created, error), (0, 0, ""))
        self.assertFalse(any("pip" in call for call in self.calls))
        self.assertIn("skipping pip", output)

    def test_skip_hooks_requires_no_git_and_still_checks_binaries(self):
        result, _, output, error = self.invoke(["--skip-hooks"])
        self.assertEqual((result, error), (0, ""))
        self.assertFalse(any(call[0] == "git" for call in self.calls))
        self.assertFalse(any("--install-hooks" in call for call in self.calls))
        self.assertEqual(sum(call[-1] == "--version" for call in self.calls), 2)
        self.assertIn("hook installation skipped", output)

    def test_configured_hook_path_is_rejected_before_any_install(self):
        self.hooks_path = ".custom-hooks\n"
        result, created, _, error = self.invoke()
        self.assertEqual((result, created), (1, 0))
        self.assertIn("core.hooksPath", error)
        self.assertIn("--skip-hooks", error)
        self.assertFalse(any("pip" in call or "--install-hooks" in call for call in self.calls))

    def test_existing_non_venv_is_never_overwritten(self):
        directory = self.root / ".venv"
        directory.mkdir()
        marker = directory / "user-file"
        marker.write_text("preserve me", encoding="utf-8")
        result, created, _, error = self.invoke(["--skip-hooks"])
        self.assertEqual((result, created), (1, 0))
        self.assertEqual(marker.read_text(encoding="utf-8"), "preserve me")
        self.assertIn("not overwritten", error)

    def test_existing_venv_python_too_old_fails_without_pip(self):
        self.create_venv(self.root / ".venv")
        self.python_version = [3, 9, 20]
        result, created, _, error = self.invoke(["--skip-hooks"])
        self.assertEqual((result, created), (1, 0))
        self.assertIn("Python >=3.10", error)
        self.assertFalse(any("pip" in call for call in self.calls))

    def test_wrong_python_prefix_is_rejected(self):
        self.prefix = self.root / "some-other-venv"
        result, _, _, error = self.invoke(["--skip-hooks"])
        self.assertEqual(result, 1)
        self.assertIn("not running inside", error)

    def test_package_mismatch_after_install_fails_before_hooks(self):
        self.installed["clang-format"] = None
        self.update_after_pip = False
        result, _, _, error = self.invoke()
        self.assertEqual(result, 1)
        self.assertIn("Installed tools do not match lock", error)
        self.assertFalse(any("--install-hooks" in call for call in self.calls))

    def test_formatter_binary_mismatch_fails_without_network_or_hooks(self):
        self.formatter_version = "18.0.0"
        result, _, _, error = self.invoke()
        self.assertEqual(result, 1)
        self.assertIn("executable version", error)
        self.assertFalse(any("pip" in call or "--install-hooks" in call for call in self.calls))

    def test_missing_runtime_dependency_reports_failure_without_pip(self):
        self.fail_pre_commit = True
        result, _, _, error = self.invoke(["--skip-hooks"])
        self.assertEqual(result, 1)
        self.assertIn("missing dependency", error)
        self.assertFalse(any("pip" in call for call in self.calls))

    def test_pip_failure_is_nonzero_and_does_not_install_hooks(self):
        self.installed["pre-commit"] = None
        self.fail_pip = True
        result, _, _, error = self.invoke()
        self.assertEqual(result, 1)
        self.assertIn("package download failed", error)
        self.assertFalse(any("--install-hooks" in call for call in self.calls))

    def test_host_python_too_old_fails_before_commands(self):
        with patch.object(INSTALLER.sys, "version_info", (3, 9, 20)):
            result, created, _, error = self.invoke(["--skip-hooks"])
        self.assertEqual((result, created), (1, 0))
        self.assertIn("Python >=3.10", error)
        self.assertEqual(self.calls, [])


if __name__ == "__main__":
    unittest.main()
