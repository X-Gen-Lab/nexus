"""Create projects from pinned Git bytes without overwriting user work."""

import hashlib
import importlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch


class ProjectCreationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.repo = self.root / "examples"
        self.repo.mkdir()
        self.git("init", "-q")
        self.git("config", "user.name", "Fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.payload = {
            "main.c": b"int main(void) { return 0; }\n",
            "CMakeLists.txt": b"cmake_minimum_required(VERSION 3.31)\n",
            "assembly.toml": (
                b'schema = 2\nboard = "native_reference"\n'
                b'backend = "native"\nclock = "model"\n'
                b'[memory]\nmain_stack_bytes = 2048\nlibc_heap_bytes = 0\n'),
            "README.md": b"An independent C11 consumer.\n",
        }
        self.template = self.repo / "templates/native"
        self.template.mkdir(parents=True)
        for name, data in self.payload.items():
            (self.template / name).write_bytes(data)
        self.manifest = {
            "schema": 1, "version": 1, "templates": {"native": {
                "files": {name: hashlib.sha256(data).hexdigest()
                          for name, data in self.payload.items()},
                "platform": {"interface_epoch": 1, "assembly_schema": 2}}}}
        self.manifest_path = self.repo / "templates/manifest.json"
        self.manifest_path.write_text(json.dumps(self.manifest))
        (self.repo / "dependencies").mkdir()
        (self.repo / "dependencies/nexus.lock.json").write_text(json.dumps({
            "schema_version": 1, "commit": "a" * 40, "tree": "b" * 40}))
        self.commit()
        self.output = self.root / "new product"

    def git(self, *args):
        return subprocess.check_output(
            ["git", "-C", str(self.repo), *args], text=True).strip()

    def commit(self):
        self.git("add", ".")
        self.git("commit", "-qm", "Synthetic template fixture")
        self.revision = self.git("rev-parse", "HEAD")

    def create(self):
        projects = importlib.import_module("tools.dev.projects")
        return projects.create(self.repo, self.revision, "native", self.output)

    def test_pinned_template_creates_exact_files_and_retains_origin(self):
        result = self.create()
        for name, data in self.payload.items():
            self.assertEqual((self.output / name).read_bytes(), data)
        origin = json.loads((self.output / ".nexus-template.json").read_text())
        self.assertEqual(origin["examples_commit"], self.revision)
        self.assertEqual(origin["expected_platform"]["commit"], "a" * 40)
        self.assertEqual(origin["validation"]["scope"],
                         "current resolver and declared inputs only")
        self.assertNotIn("commit", origin["validation"])
        self.assertTrue(origin["validation"]["inputs"])
        self.assertEqual(result["qualification"], False)
        self.assertFalse((self.output / ".git").exists())

    def test_existing_directory_is_never_modified(self):
        self.output.mkdir()
        retained = self.output / "retained"
        retained.write_bytes(b"user work")
        with self.assertRaisesRegex(ValueError, "exist"):
            self.create()
        self.assertEqual(retained.read_bytes(), b"user work")
        self.assertEqual(list(self.output.iterdir()), [retained])

    def test_parent_paths_create_the_same_project_as_normal_paths(self):
        projects = importlib.import_module("tools.dev.projects")
        holder = self.root / "holder"
        holder.mkdir()
        result = projects.create(holder / ".." / "examples", self.revision,
                                 "native", holder / ".." / self.output.name)
        self.assertEqual((self.output / "main.c").read_bytes(),
                         self.payload["main.c"])
        self.assertEqual(result["examples_commit"], self.revision)

    def test_parent_normalization_cannot_hide_a_symlink_component(self):
        projects = importlib.import_module("tools.dev.projects")
        alias = self.root / "alias"
        alias.symlink_to(self.repo, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            projects.create(alias / ".." / "examples", self.revision,
                            "native", self.output)
        self.assertFalse(self.output.exists())

    def test_dirty_selected_source_is_rejected_before_creation(self):
        (self.template / "main.c").write_text("uncommitted change")
        with self.assertRaisesRegex(ValueError, "changed|dirty|identity"):
            self.create()
        self.assertFalse(self.output.exists())

    def test_revision_must_be_exact_and_match_checked_out_head(self):
        for revision in ("HEAD", "a" * 40):
            with self.subTest(revision=revision):
                self.revision = revision
                with self.assertRaises(ValueError):
                    self.create()
                self.assertFalse(self.output.exists())

    def test_symlink_source_is_rejected_even_when_git_tracks_it(self):
        source = self.template / "main.c"
        source.unlink()
        source.symlink_to("README.md")
        self.commit()
        with self.assertRaisesRegex(ValueError, "symlink|regular"):
            self.create()
        self.assertFalse(self.output.exists())

    def test_manifest_cannot_escape_template_directory(self):
        self.manifest["templates"]["native"]["files"]["../escape.c"] = "a" * 64
        self.manifest_path.write_text(json.dumps(self.manifest))
        self.commit()
        with self.assertRaisesRegex(ValueError, "relative|path"):
            self.create()
        self.assertFalse(self.output.exists())

    def test_wrong_digest_is_rejected_before_creation(self):
        self.manifest["templates"]["native"]["files"]["main.c"] = "0" * 64
        self.manifest_path.write_text(json.dumps(self.manifest))
        self.commit()
        with self.assertRaisesRegex(ValueError, "digest|identity"):
            self.create()
        self.assertFalse(self.output.exists())

    def test_total_bound_rejects_next_blob_before_reading_its_payload(self):
        projects = importlib.import_module("tools.dev.projects")
        ordered = sorted(self.payload)
        second_blob = self.git("rev-parse", self.revision + ":templates/native/"
                               + ordered[1])
        run = subprocess.check_output
        payload_reads = []

        def observe(command, **kwargs):
            if "cat-file" in command and "blob" in command:
                payload_reads.append(command[-1])
            return run(command, **kwargs)

        with patch.object(projects, "MAXIMUM_PROJECT_BYTES",
                          len(self.payload[ordered[0]])), patch.object(
                subprocess, "check_output", side_effect=observe):
            with self.assertRaisesRegex(ValueError, "bound"):
                self.create()
        self.assertNotIn(second_blob, payload_reads)
        self.assertFalse(self.output.exists())

    def test_invalid_assembly_leaves_no_partial_project(self):
        data = b'schema = 2\nboard = "unknown_board"\n'
        (self.template / "assembly.toml").write_bytes(data)
        self.manifest["templates"]["native"]["files"]["assembly.toml"] = (
            hashlib.sha256(data).hexdigest())
        self.manifest_path.write_text(json.dumps(self.manifest))
        self.commit()
        with self.assertRaises(ValueError):
            self.create()
        self.assertFalse(self.output.exists())

    def test_relative_input_outside_copied_template_cannot_pass_creation(self):
        from tools.dev import dev
        shared = self.repo / "templates/shared-board"
        shutil.copytree(dev.ROOT / "boards/native_reference", shared)
        data = self.payload["assembly.toml"].replace(
            b'board = "native_reference"',
            b'board_package = "../shared-board"')
        (self.template / "assembly.toml").write_bytes(data)
        self.manifest["templates"]["native"]["files"]["assembly.toml"] = (
            hashlib.sha256(data).hexdigest())
        self.manifest_path.write_text(json.dumps(self.manifest))
        self.commit()
        with self.assertRaises(ValueError):
            self.create()
        self.assertFalse(self.output.exists())

    def test_failed_copy_cleans_only_new_owned_output(self):
        original = Path.write_bytes

        def fail(path, data):
            if path == self.output / "main.c":
                raise OSError("synthetic storage failure")
            return original(path, data)

        with patch.object(Path, "write_bytes", fail):
            with self.assertRaisesRegex(OSError, "synthetic"):
                self.create()
        self.assertFalse(self.output.exists())
        self.assertEqual((self.template / "main.c").read_bytes(),
                         self.payload["main.c"])


if __name__ == "__main__":
    unittest.main()
