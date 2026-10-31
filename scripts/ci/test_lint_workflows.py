"""Real pinned actionlint against isolated Git indexes and local metadata."""

import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import tarfile
import uuid
import unittest
from unittest.mock import patch

from tools.dev import dependencies

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "lint_workflows", ROOT / "scripts/ci/lint_workflows.py")
LINT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LINT)
GOOD = """name: Fixture
on: push
jobs:
  check:
    runs-on: ubuntu-24.04
    steps:
      - run: echo ok
"""
BAD = GOOD.replace("    steps:",
                   "    env:\n      BAD: ${{ runner.temp }}\n    steps:")
METADATA = """name: Fixture
description: Local action
inputs:
  message:
    description: Message
    required: true
runs:
  using: composite
  steps:
    - run: echo '${{ inputs.message }}'
      shell: bash
"""
REUSABLE = """name: Reusable
on:
  workflow_call:
    inputs:
      message:
        required: true
        type: string
jobs:
  check:
    runs-on: ubuntu-24.04
    steps:
      - run: echo ok
"""


def isolated_environment():
    return {key: value for key, value in os.environ.items()
            if not key.startswith("GIT_")} | {
                "GIT_CONFIG_NOSYSTEM": "1",
                "GIT_CONFIG_GLOBAL": os.devnull,
                "GIT_CONFIG_SYSTEM": os.devnull,
            }


class WorkflowLintTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prefix = Path(os.environ.get(
            "NEXUS_ACTIONLINT_TEST_PREFIX", ROOT / ".venv/nexus-actionlint"))
        dependencies.verify_actionlint(
            cls.prefix, ROOT / "dependencies/actionlint.lock.json")

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="workflow-lint-test-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "project"
        self.root.mkdir()
        self.env = isolated_environment()
        self.git("init", "--quiet")
        self.write("dependencies/actionlint.lock.json", (
            ROOT / "dependencies/actionlint.lock.json").read_bytes())
        self.write("dependencies/workflow-tools.txt", (
            ROOT / "dependencies/workflow-tools.txt").read_bytes())
        self.write(".github/workflows/check.yml", GOOD)
        self.git("add", ".")
        self.index = self.root / ".git/index"
        self.before = self.index.read_bytes()

    def tearDown(self):
        destination = os.environ.get("NEXUS_ACTIONLINT_TEST_EVIDENCE")
        if destination:
            target = Path(destination) / (self.id() + "-" + uuid.uuid4().hex)
            target.mkdir(parents=True)
            for path in self.base.iterdir():
                if path.name == "project":
                    continue
                if path.is_dir():
                    shutil.copytree(path, target / path.name)
                else:
                    shutil.copyfile(path, target / path.name)
            records = self.git("ls-files", "--stage", "-z")
            (target / "index-records.raw").write_bytes(records)
            archive_path = target / "staged-inputs.tar.gz"
            with tarfile.open(archive_path, "w:gz") as archive:
                for row in records.split(b"\0"):
                    if not row:
                        continue
                    metadata, name = row.split(b"\t", 1)
                    blob = self.git("cat-file", "blob",
                                    metadata.split()[1].decode())
                    info = tarfile.TarInfo(name.decode())
                    info.size = len(blob)
                    archive.addfile(info, io.BytesIO(blob))

    def git(self, *arguments, input=None, env=None):
        return subprocess.run(
            ["git", *arguments], cwd=self.root,
            env=self.env if env is None else env, input=input,
            capture_output=True, check=True).stdout

    def write(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data.encode() if isinstance(data, str) else data)
        return path

    def stage(self, name, data):
        self.write(name, data)
        self.git("add", "--", name)
        self.before = self.index.read_bytes()

    def lint(self, **kwargs):
        with patch.dict(os.environ, self.env, clear=True):
            result = LINT.staged(
                self.root, self.prefix, self.base / "lint.log", **kwargs)
        self.assertEqual(self.index.read_bytes(), self.before)
        return result

    def reject(self, pattern):
        with self.assertRaisesRegex(LINT.WorkflowError, pattern):
            self.lint()

    def test_valid_full_index_ignores_bad_unstaged_workflow(self):
        self.write(".github/workflows/check.yml", BAD)
        self.assertEqual(self.lint(), 0)

    def test_bad_staged_context_cannot_hide_behind_good_worktree(self):
        self.stage(".github/workflows/check.yml", BAD)
        self.write(".github/workflows/check.yml", GOOD)
        self.assertEqual(self.lint(), 1)
        self.assertIn("context", (self.base / "lint.log").read_text())

    def test_replacement_refs_cannot_hide_bad_indexed_workflow(self):
        name = ".github/workflows/check.yml"
        self.stage(name, BAD)
        bad = self.git("rev-parse", ":" + name).strip().decode()
        good = self.git("hash-object", "-w", "--stdin", input=GOOD.encode())
        self.git("replace", bad, good.strip().decode())
        self.assertEqual(self.git("cat-file", "blob", bad), GOOD.encode())
        self.assertEqual(self.lint(), 1)

    def test_partial_promisor_roots_fail_before_any_object_read(self):
        settings = (("extensions.partialClone", "origin"),
                    ("remote.origin.promisor", "true"),
                    ("remote.origin.partialCloneFilter", "blob:none"))
        actual = subprocess.run
        protected = (self.root / ".github/workflows/check.yml",
                     self.root / "dependencies/actionlint.lock.json",
                     self.root / "dependencies/workflow-tools.txt")
        source_before = [path.read_bytes() for path in protected]
        for number, (key, value) in enumerate(settings):
            with self.subTest(setting=key):
                self.git("config", key, value)
                config = self.base / ("partial-config-" + str(number) + ".raw")
                config.write_bytes((self.root / ".git/config").read_bytes())
                calls = []
                def observe(command, **kwargs):
                    calls.append(list(command))
                    return actual(command, **kwargs)
                try:
                    with patch.object(LINT.subprocess, "run",
                                      side_effect=observe):
                        self.reject("promisor")
                    self.assertFalse(any("cat-file" in call for call in calls))
                    self.assertEqual([path.read_bytes() for path in protected],
                                     source_before)
                finally:
                    self.git("config", "--unset", key)
                    name = "partial-calls-" + str(number) + ".json"
                    path = self.base / name
                    path.write_text(json.dumps(calls, indent=2) + "\n")

    def test_all_workflows_are_checked_not_only_index_delta(self):
        self.stage(".github/workflows/unchanged.yml", BAD)
        self.git("-c", "user.name=Fixture", "-c", "user.email=f@example.org",
                 "commit", "--quiet", "-m", "fixture")
        self.before = self.index.read_bytes()
        self.assertEqual(self.lint(), 1)

    def test_alternate_index_selection_is_preserved_only_in_parent(self):
        alternate = self.base / "alternate-index"
        shutil.copyfile(self.index, alternate)
        env = {**self.env, "GIT_INDEX_FILE": str(alternate),
               "GIT_DIR": str(self.root / ".git"),
               "GIT_WORK_TREE": str(self.root)}
        self.write(".github/workflows/check.yml", BAD)
        self.git("add", ".github/workflows/check.yml", env=env)
        self.write(".github/workflows/check.yml", GOOD)
        before = alternate.read_bytes()
        with patch.dict(os.environ, env, clear=True):
            self.assertEqual(LINT.staged(
                self.root, self.prefix, self.base / "alternate.log"), 1)
        self.assertEqual(alternate.read_bytes(), before)
        self.assertEqual(self.index.read_bytes(), self.before)

    def test_unstaged_and_ambient_config_cannot_suppress_bad_context(self):
        self.stage(".github/workflows/check.yml", BAD)
        self.write(".github/actionlint.yaml", "[invalid config")
        self.env["XDG_CONFIG_HOME"] = str(self.base / "ambient")
        ambient = self.base / "ambient/actionlint/actionlint.yaml"
        ambient.parent.mkdir(parents=True)
        ambient.write_text("[invalid config")
        self.assertEqual(self.lint(), 1)

    def test_staged_config_bytes_are_used_over_worktree(self):
        self.stage(".github/actionlint.yaml", "{}\n")
        self.write(".github/actionlint.yaml", "[invalid config")
        self.assertEqual(self.lint(), 0)

    def test_invalid_staged_config_is_not_repaired(self):
        self.stage(".github/actionlint.yaml", "[invalid config")
        self.write(".github/actionlint.yaml", "{}\n")
        self.assertNotEqual(self.lint(), 0)

    def test_two_staged_config_names_are_rejected(self):
        self.stage(".github/actionlint.yaml", "{}\n")
        self.stage(".github/actionlint.yml", "{}\n")
        self.reject("config")

    def test_reusable_required_input_and_type_are_checked(self):
        self.stage(".github/workflows/reusable.yml", REUSABLE)
        caller = ("name: Caller\non: push\njobs:\n  call:\n"
                  "    uses: ./.github/workflows/reusable.yml\n")
        self.stage(".github/workflows/check.yml", caller)
        self.assertEqual(self.lint(), 1)
        self.stage(".github/workflows/check.yml",
                   caller + "    with:\n      message: hello\n")
        self.assertEqual(self.lint(), 0)
        self.stage(".github/workflows/reusable.yml",
                   REUSABLE.replace("type: string", "type: number"))
        self.stage(".github/workflows/check.yml",
                   caller + "    with:\n      message: hello\n")
        self.assertEqual(self.lint(), 1)

    def local_action(self, suffix="action.yml"):
        self.stage(".github/actions/local/" + suffix, METADATA)
        self.stage(".github/workflows/check.yml", GOOD.replace(
            "      - run: echo ok", "      - uses: ./.github/actions/local\n"
            "        with:\n          message: hello"))

    def test_local_action_staged_inputs_and_outputs_are_checked(self):
        self.local_action()
        self.write(".github/actions/local/action.yml", "[invalid")
        self.assertEqual(self.lint(), 0)
        self.stage(".github/workflows/check.yml", GOOD.replace(
            "      - run: echo ok", "      - uses: ./.github/actions/local\n"
            "        with:\n          unexpected: hello"))
        self.assertEqual(self.lint(), 1)

    def test_action_yaml_metadata_is_supported(self):
        self.local_action("action.yaml")
        self.assertEqual(self.lint(), 0)

    def test_root_local_action_is_supported(self):
        self.stage("action.yml", METADATA)
        self.stage(".github/workflows/check.yml", GOOD.replace(
            "      - run: echo ok", "      - uses: ./\n"
            "        with:\n          message: hello"))
        self.assertEqual(self.lint(), 0)

    def test_contained_local_action_trailing_slash_is_supported(self):
        self.local_action()
        path = ".github/workflows/check.yml"
        self.stage(path, (self.root / path).read_text().replace(
            "uses: ./.github/actions/local", "uses: ./.github/actions/local/"))
        self.assertEqual(self.lint(), 0)

    def test_missing_local_action_directory_fails_closed(self):
        self.stage(".github/workflows/check.yml", GOOD.replace(
            "      - run: echo ok", "      - uses: ./absent"))
        self.reject("metadata")

    def test_deleted_metadata_cannot_be_rescued_by_worktree(self):
        self.local_action()
        path = self.root / ".github/actions/local/action.yml"
        self.git("rm", "--cached", "--", str(path))
        self.before = self.index.read_bytes()
        self.reject("metadata")

    def test_missing_reusable_is_rejected(self):
        self.stage(".github/workflows/check.yml",
                   "name: Missing\non: push\njobs:\n  call:\n"
                   "    uses: ./.github/workflows/missing.yml\n")
        self.reject("reusable")

    def test_deleted_reusable_cannot_be_rescued_by_worktree(self):
        self.stage(".github/workflows/reusable.yml", REUSABLE)
        self.stage(".github/workflows/check.yml",
                   "name: Caller\non: push\njobs:\n  call:\n"
                   "    uses: ./.github/workflows/reusable.yml\n"
                   "    with:\n      message: hello\n")
        self.git("rm", "--cached", ".github/workflows/reusable.yml")
        self.before = self.index.read_bytes()
        self.reject("reusable")

    def test_local_action_unknown_output_is_checked(self):
        self.local_action()
        self.stage(".github/workflows/check.yml", GOOD.replace(
            "      - run: echo ok", "      - uses: ./.github/actions/local\n"
            "        id: local\n        with:\n          message: hello\n"
            "      - run: echo '${{ steps.local.outputs.absent }}'"))
        self.assertEqual(self.lint(), 1)

    def test_staged_yml_config_blocks_unstaged_yaml_default(self):
        self.stage(".github/actionlint.yml", "{}\n")
        self.write(".github/actionlint.yaml", "[invalid default")
        self.assertEqual(self.lint(), 0)

    def test_composite_direct_local_reference_closure_is_checked(self):
        self.local_action()
        self.stage(".github/actions/local/action.yml", METADATA.replace(
            "    - run: echo '${{ inputs.message }}'\n      shell: bash",
            "    - uses: ./missing-nested"))
        self.reject("metadata")

    def test_local_reference_escape_and_ambiguous_metadata_are_rejected(self):
        self.stage(".github/workflows/check.yml", GOOD.replace(
            "      - run: echo ok", "      - uses: ./../outside"))
        self.reject("local reference")
        self.local_action()
        self.stage(".github/actions/local/action.yaml", METADATA)
        self.reject("metadata")

    def test_symlink_metadata_and_case_aliases_are_rejected(self):
        self.local_action()
        blob = self.git("hash-object", "-w", "--stdin", input=b"outside\n")
        self.git("update-index", "--cacheinfo", "120000", blob.strip().decode(),
                 ".github/actions/local/action.yml")
        self.before = self.index.read_bytes()
        self.reject("regular")

    def test_case_alias_directory_is_rejected(self):
        self.stage(".github/actions/Case/action.yml", METADATA)
        self.stage(".github/actions/case/action.yaml", METADATA)
        self.reject("alias")

    def test_hardlink_output_cannot_modify_a_source_blob(self):
        source = self.root / ".github/workflows/check.yml"
        log = self.base / "hardlink.log"
        os.link(source, log)
        with self.assertRaisesRegex(LINT.WorkflowError, "output"):
            LINT.staged(self.root, self.prefix, log)
        self.assertEqual(source.read_text(), GOOD)

    def test_staged_lock_mismatch_cannot_hide_behind_worktree(self):
        lock = (self.root / "dependencies/actionlint.lock.json").read_bytes()
        data = json.loads(lock)
        data["version"] = "0.0.0"
        self.stage("dependencies/actionlint.lock.json", json.dumps(data))
        self.write("dependencies/actionlint.lock.json", lock)
        self.reject("lock")

    def test_pyyaml_uses_existing_staged_version_authority(self):
        self.stage("dependencies/workflow-tools.txt", "PyYAML==0.0.0\n")
        self.reject("PyYAML")

    def test_unmerged_entry_fails_before_tool_execution(self):
        name = ".github/workflows/check.yml"
        oid = self.git("rev-parse", ":" + name).strip()
        self.git("update-index", "--index-info", input=(
            b"0 " + b"0" * 40 + b"\t" + name.encode() + b"\n" +
            b"100644 " + oid + b" 1\t" + name.encode() + b"\n"))
        self.before = self.index.read_bytes()
        self.reject("unmerged")

    def test_actual_timeout_is_nonzero_and_retains_receipt(self):
        self.assertEqual(self.lint(timeout=0.000001), 124)
        receipt = json.loads((self.base / "lint.command.json").read_text())
        self.assertEqual(receipt["exit_code"], 124)

    def test_index_mutation_during_actual_tool_execution_is_rejected(self):
        actual = LINT.process.run
        def mutate(command, **kwargs):
            result = actual(command, **kwargs)
            self.write("unrelated.txt", "changed by another process\n")
            self.git("add", "unrelated.txt")
            return result
        with patch.object(LINT.process, "run", side_effect=mutate):
            with self.assertRaisesRegex(LINT.WorkflowError, "index changed"):
                self.lint()

    def test_missing_tool_is_setup_failure_without_network(self):
        with self.assertRaisesRegex(LINT.WorkflowError, "prepare"):
            LINT.staged(self.root, self.base / "absent",
                        self.base / "missing.log")

    def preserved_rejection(self, log, env=None):
        lock = self.root / "dependencies/actionlint.lock.json"
        lock_before = lock.read_bytes()
        index = self.index if env is None else Path(env["GIT_INDEX_FILE"])
        index_before = index.read_bytes()
        (self.base / "protected-index-before.raw").write_bytes(index_before)
        (self.base / "protected-lock-before.raw").write_bytes(lock_before)
        rejected = False
        try:
            with patch.dict(os.environ, self.env if env is None else env,
                            clear=True):
                LINT.staged(self.root, self.prefix, log)
        except (LINT.WorkflowError, OSError) as error:
            rejected = (isinstance(error, LINT.WorkflowError) and
                        "output" in str(error))
        after_index = self.base / "protected-index-after.raw"
        after_index.write_bytes(index.read_bytes())
        (self.base / "protected-lock-after.raw").write_bytes(lock.read_bytes())
        self.assertTrue(index.read_bytes() == index_before,
                        "Rejected output changed effective index bytes")
        self.assertTrue(lock.read_bytes() == lock_before,
                        "Rejected output changed worktree lock bytes")
        self.assertTrue(rejected, "Output must be rejected before any write")

    def test_dotdot_log_cannot_escape_build_and_overwrite_worktree_lock(self):
        self.preserved_rejection(
            self.root / "build/../dependencies/actionlint.lock.json")

    def test_external_effective_index_is_protected_from_output_aliases(self):
        for number, suffix in enumerate(
                (".log", ".command.json", ".attempts", ".inputs")):
            with self.subTest(index_alias=suffix):
                stem = "external-" + str(number)
                index = self.base / (stem + suffix)
                shutil.copyfile(self.index, index)
                env = {**self.env, "GIT_INDEX_FILE": str(index)}
                log = index if suffix == ".log" else self.base / (stem + ".log")
                self.preserved_rejection(log, env)

    def test_indexed_build_metadata_is_protected_from_log_alias(self):
        self.stage("build/action.yml", METADATA)
        target = self.root / "build/action.yml"
        before = target.read_bytes()
        rejected = False
        try:
            with patch.dict(os.environ, self.env, clear=True):
                LINT.staged(self.root, self.prefix, target)
        except LINT.WorkflowError as error:
            rejected = "output" in str(error)
        self.assertTrue(target.read_bytes() == before,
                        "Output changed indexed worktree metadata bytes")
        self.assertTrue(rejected)
        self.assertEqual(self.index.read_bytes(), self.before)

    def test_large_blob_is_rejected_before_git_payload_read(self):
        name = ".github/workflows/large.yml"
        self.stage(name, b"#" + b"x" * (4 * 1024 * 1024))
        oid = self.git("rev-parse", ":" + name).strip().decode()
        actual = subprocess.run
        calls = []
        def observe(command, **kwargs):
            calls.append(list(command))
            return actual(command, **kwargs)
        with patch.object(LINT.subprocess, "run", side_effect=observe):
            self.reject("bounded")
        self.assertFalse(any(command[1:] == ["cat-file", "blob", oid]
                             for command in calls),
                         "Oversized immutable OID payload was read")
        self.assertTrue(any(command[1:] == ["cat-file", "-s", oid]
                            for command in calls))

    def test_closure_limit_rejects_next_blob_before_payload_read(self):
        for number in range(4):
            self.stage(".github/workflows/limit-" + str(number) + ".yml",
                       b"#" + bytes([65 + number]) * (4 * 1024 * 1024 - 1))
        oid = self.git("rev-parse", ":.github/workflows/limit-3.yml")
        oid = oid.strip().decode()
        actual = subprocess.run
        calls = []
        def observe(command, **kwargs):
            calls.append(list(command))
            return actual(command, **kwargs)
        with patch.object(LINT.subprocess, "run", side_effect=observe):
            self.reject("bounded")
        self.assertFalse(any(command[1:] == ["cat-file", "blob", oid]
                             for command in calls),
                         "Over-budget closure payload was read")

    def test_report_cannot_overwrite_input_or_tool(self):
        with self.assertRaisesRegex(LINT.WorkflowError, "output"):
            LINT.staged(self.root, self.prefix,
                        self.root / ".github/workflows/check.yml")
        path = self.root / ".github/workflows/check.yml"
        self.assertEqual(path.read_text(), GOOD)


if __name__ == "__main__":
    unittest.main()
