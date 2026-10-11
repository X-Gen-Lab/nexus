"""Exercise actual tools and Git hooks in disposable repositories."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import yaml

from scripts.ci import style_gate as gate

ROOT = Path(__file__).resolve().parents[2]
GOOD = b"""/**
 * \\file            main.c
 * \\brief           Commit automation fixture
 * \\author          Nexus Team
 * \\version         1.0.0
 * \\date            2026-10-09
 *
 * \\copyright       Copyright (c) 2026 Nexus Team
 */

int main(void) {
    return 0;
}
"""


class StyleGateTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for name in (
            ".clang-format", ".pre-commit-config.yaml", "dependencies/development-tools.txt",
            "scripts/ci/style_gate.py", "scripts/ci/comment_style.py",
            "scripts/ci/check_commit_message.py", "scripts/tools/format.py",
            "scripts/setup/install_dev_tools.py",
        ):
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, destination)
        # This disposable project tests style and message hooks, not firmware.
        # Actual TDD configuration and execution are checked in the real tree.
        hook_path = self.root / ".pre-commit-config.yaml"
        hooks = yaml.safe_load(hook_path.read_text())
        for repository in hooks["repos"]:
            repository["hooks"] = [hook for hook in repository["hooks"]
                                   if hook["id"] != "nexus-tdd-gate"]
        hook_path.write_text(yaml.safe_dump(hooks, sort_keys=False))
        (self.root / ".clang-format-dirs").write_text(
            "src\n!vendors\n!ext\n!build\n[extensions]\n.c\n.h\n.inc\n"
        )
        (self.root / ".gitignore").write_text(".venv/\nbuild/\n")
        (self.root / "src").mkdir()
        (self.root / "src/main.c").write_bytes(GOOD)
        if not (ROOT / ".venv").is_dir():
            self.fail("Install locked development tools before running integration tests")
        os.symlink(ROOT / ".venv", self.root / ".venv", target_is_directory=True)
        self.baseline = {
            "schema_version": 1, "source_revision": gate.BOOTSTRAP_SOURCE,
            "formatter_version": "14.0.6", "format": {},
            "comments": {"schema_version": 1, "ruleset_version": 1, "files": {}},
        }
        self.write_baseline()
        self.git("init", "-q")
        self.git("config", "user.name", "Nexus hook test")
        self.git("config", "user.email", "hook-test@example.invalid")
        self.git("add", ".")
        self.git("commit", "-qm", "test: initialize disposable fixture")

    def git(self, *arguments, check=True):
        result = subprocess.run(
            ["git", *arguments], cwd=self.root, capture_output=True,
            env={key: value for key, value in os.environ.items() if not key.startswith("NEXUS_STYLE_")},
        )
        if check and result.returncode:
            self.fail(result.stderr.decode("utf-8", "replace") + result.stdout.decode("utf-8", "replace"))
        return result

    def write_baseline(self):
        (self.root / gate.BASELINE_PATH).write_text(json.dumps(self.baseline) + "\n")

    def install_hooks(self):
        python = ROOT / (".venv/Scripts/python.exe" if os.name == "nt" else ".venv/bin/python")
        result = subprocess.run(
            [str(python), "-m", "pre_commit", "install", "--hook-type", "pre-commit",
             "--hook-type", "commit-msg"], cwd=self.root, capture_output=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", "replace"))

    def check(self, *files, full=False):
        return gate.run_checks(self.root, list(files), all_files=full)

    def test_clean_source_passes_real_formatter_and_comments(self):
        result = self.check("src/main.c", full=True)
        self.assertFalse(result["errors"])
        self.assertEqual(result["source_files_checked"], 1)

    def test_real_repository_configures_tdd_hook_without_bypass(self):
        configuration = yaml.safe_load((ROOT / ".pre-commit-config.yaml").read_text())
        matches = [hook for repo in configuration["repos"] for hook in repo["hooks"]
                   if hook["id"] == "nexus-tdd-gate"]
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0]["entry"], "python scripts/ci/tdd_gate.py --staged")
        self.assertEqual(matches[0]["stages"], ["pre-commit"])
        self.assertTrue(matches[0]["always_run"])
        self.assertTrue(matches[0]["pass_filenames"])
        self.assertTrue((ROOT / "scripts/ci/tdd_gate.py").is_file())

    def test_authored_toml_is_checked_by_staged_text_gate(self):
        (self.root / "assembly.toml").write_bytes(b"schema = 2\r\n")
        self.git("add", "assembly.toml")
        result = self.check("assembly.toml")
        self.assertTrue(any("LF line endings" in error
                            for error in result["errors"]))

    def test_new_source_with_comment_violation_is_rejected(self):
        (self.root / "src/new.c").write_bytes(GOOD + b"// forbidden\n")
        self.git("add", "src/new.c")
        result = self.check("src/new.c")
        self.assertTrue(any("line_comment" in error for error in result["errors"]))

    def test_untracked_source_does_not_change_full_check_scope(self):
        (self.root / "src/untracked.c").write_text("not C and no header\n")
        result = self.check(full=True)
        self.assertEqual(result["source_files_checked"], 1)
        self.assertFalse(result["errors"])

    def test_unclassified_tracked_source_is_rejected(self):
        (self.root / "new.c").write_bytes(GOOD)
        self.git("add", "new.c")
        with self.assertRaisesRegex(ValueError, "Unclassified source"):
            self.check("new.c")

    def test_baseline_cannot_add_or_rebase_debt(self):
        self.baseline["format"]["src/main.c"] = gate.digest(GOOD)
        self.write_baseline()
        with self.assertRaisesRegex(ValueError, "cannot be added/rebased"):
            self.check(gate.BASELINE_PATH)

    def test_ownership_manifest_cannot_hide_existing_source(self):
        text = (self.root / ".clang-format-dirs").read_text()
        (self.root / ".clang-format-dirs").write_text(text.replace("!build\n", "!build\n!src/main.c\n"))
        (self.root / "src/remaining.c").write_bytes(GOOD)
        self.git("add", ".")
        with self.assertRaisesRegex(ValueError, "cannot be excluded"):
            self.check(".clang-format-dirs")

    def test_owned_source_cannot_be_renamed_into_vendor_exclusion(self):
        (self.root / "vendors").mkdir()
        self.git("mv", "src/main.c", "vendors/main.c")
        (self.root / "src/remaining.c").write_bytes(GOOD.replace(b"return 0;", b"return 1;"))
        self.git("add", ".")
        with self.assertRaisesRegex(ValueError, "cannot move outside ownership"):
            self.check("vendors/main.c")

    def test_space_in_owned_filename_is_supported(self):
        (self.root / "src/with space.c").write_bytes(GOOD)
        self.git("add", ".")
        self.assertFalse(self.check("src/with space.c")["errors"])

    def test_multiple_commits_cannot_reseal_new_debt_against_trusted_base(self):
        base_ref = self.git("rev-parse", "HEAD").stdout.decode().strip()
        self.baseline["format"]["src/main.c"] = gate.digest(GOOD)
        self.write_baseline()
        self.git("add", ".")
        self.git("commit", "-qm", "test: attempt to reseal disposable debt")
        (self.root / "README.md").write_text("Later commit\n")
        self.git("add", ".")
        self.git("commit", "-qm", "docs: later disposable commit")
        with self.assertRaisesRegex(ValueError, "cannot be added/rebased"):
            gate.run_checks(self.root, [], all_files=True, base_ref=base_ref)

    def test_initial_baseline_only_freezes_the_reviewed_source_snapshot(self):
        self.git("rm", gate.BASELINE_PATH)
        self.git("commit", "-qm", "test: disposable pre-automation source")
        snapshot = self.git("rev-parse", "HEAD").stdout.decode().strip()
        self.baseline["source_revision"] = snapshot
        self.write_baseline()
        self.git("add", ".")
        with patch.object(gate, "BOOTSTRAP_SOURCE", snapshot):
            result = gate.run_checks(self.root, [], all_files=True)
            initial_push = gate.run_checks(self.root, [], all_files=True, base_ref="0" * 40)
        self.assertFalse(result["errors"])
        self.assertEqual(result["debt_source_ref"], snapshot)
        self.assertFalse(initial_push["errors"])
        self.assertEqual(initial_push["base_ref"], snapshot)

    def test_comment_baseline_cannot_increase(self):
        bad = GOOD + b"// old debt\n"
        (self.root / "src/main.c").write_bytes(bad)
        facts = gate.comment_style.inspect_source("src/main.c", bad)
        self.baseline["comments"]["files"]["src/main.c"] = {
            "source_sha256": facts["source_sha256"], "rules": facts["rules"],
        }
        self.write_baseline()
        self.git("add", ".")
        self.git("commit", "-qm", "test: seal disposable historical debt")
        self.baseline["comments"]["files"]["src/main.c"]["rules"]["line_comment"] += 1
        self.write_baseline()
        with self.assertRaisesRegex(ValueError, "cannot increase"):
            self.check(gate.BASELINE_PATH)

    def test_restored_old_debt_after_a_fix_is_rejected(self):
        bad = GOOD + b"// old debt\n"
        (self.root / "src/main.c").write_bytes(bad)
        facts = gate.comment_style.inspect_source("src/main.c", bad)
        self.baseline["comments"]["files"]["src/main.c"] = {
            "source_sha256": facts["source_sha256"], "rules": facts["rules"],
        }
        self.write_baseline()
        self.git("add", ".")
        self.git("commit", "-qm", "test: seal disposable historical debt")
        (self.root / "src/main.c").write_bytes(GOOD)
        self.git("add", "src/main.c")
        self.git("commit", "-qm", "test: fix disposable historical debt")
        (self.root / "src/main.c").write_bytes(bad)
        result = self.check("src/main.c")
        self.assertTrue(any("line_comment" in error for error in result["errors"]))

    def test_control_only_change_checks_every_owned_source(self):
        (self.root / ".editorconfig").write_text("root = true\n")
        self.git("add", ".editorconfig")
        result = self.check(".editorconfig")
        self.assertTrue(result["full_source_check"])
        self.assertEqual(result["source_files_checked"], 1)

    def test_real_hook_rejects_staged_bad_format_and_keeps_index(self):
        self.install_hooks()
        before = self.git("rev-parse", "HEAD").stdout
        bad = GOOD.replace(b"return 0;", b"return  0;")
        (self.root / "src/main.c").write_bytes(bad)
        self.git("add", "src/main.c")
        # A clean working copy must not conceal the bad staged version.
        (self.root / "src/main.c").write_bytes(GOOD)
        result = self.git("commit", "-qm", "test: reject bad staged format", check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.git("rev-parse", "HEAD").stdout, before)
        self.assertEqual(self.git("show", ":src/main.c").stdout, bad)
        self.assertEqual((self.root / "src/main.c").read_bytes(), GOOD)

    def test_real_hook_accepts_clean_index_and_restores_unstaged_bad_format(self):
        self.install_hooks()
        staged = GOOD.replace(b"return 0;", b"return 1;")
        unstaged = staged.replace(b"return 1;", b"return  1;")
        (self.root / "src/main.c").write_bytes(staged)
        self.git("add", "src/main.c")
        (self.root / "src/main.c").write_bytes(unstaged)
        self.git("commit", "-qm", "test: commit only the clean index")
        self.assertEqual(self.git("show", "HEAD:src/main.c").stdout, staged)
        self.assertEqual((self.root / "src/main.c").read_bytes(), unstaged)

    def test_real_commit_message_hook_blocks_nonconventional_header(self):
        self.install_hooks()
        (self.root / "README.md").write_text("Disposable documentation\n")
        self.git("add", "README.md")
        before = self.git("rev-parse", "HEAD").stdout
        result = self.git("commit", "-qm", "update things", check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.git("rev-parse", "HEAD").stdout, before)

    @unittest.skipIf(os.name == "nt", "POSIX command-path fixture")
    def test_real_hooks_do_not_require_python_on_the_callers_path(self):
        self.install_hooks()
        commands = self.root / "git-command-path"
        commands.mkdir()
        for name in ("git", "bash", "sh", "dirname"):
            target = shutil.which(name)
            self.assertIsNotNone(target)
            (commands / name).symlink_to(target)
        (self.root / "README.md").write_text("GUI-style commit without activation\n")
        self.git("add", "README.md")
        environment = {
            key: value for key, value in os.environ.items()
            if not key.startswith("NEXUS_STYLE_")
        }
        environment["PATH"] = str(commands)
        result = subprocess.run(
            [str(commands / "git"), "commit", "-qm", "test: commit without shell activation"],
            cwd=self.root, capture_output=True, env=environment,
        )
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", "replace"))


if __name__ == "__main__":
    unittest.main()
