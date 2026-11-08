"""Create projects from pinned Git bytes without overwriting user work."""

import hashlib
import importlib
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import shlex
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
        return projects.create(self.repo, self.revision, "native", self.output, test_support=True)

    def test_native_project_requires_explicit_test_support(self):
        from tools.dev import projects
        with self.assertRaisesRegex(ValueError, "test.support|test support"):
            projects.create(self.repo, self.revision, "native", self.output)
        self.assertFalse(self.output.exists())
        result = projects.create(self.repo, self.revision, "native", self.output,
                                 test_support=True)
        self.assertIn("--test-support", result["validation"]["argv"])
        self.assertIn("-DNEXUS_TEST_SUPPORT=ON",
                      result["commands"]["configure"])

    def selected_source(self, *, family="native", arch=None, mutation=""):
        source = self.root / "independent platform with spaces"
        (source / "dependencies").mkdir(parents=True)
        policy = json.loads((Path(__file__).resolve().parents[2] /
                             "dependencies/platform-compatibility.json")
                            .read_text())
        (source / "dependencies/platform-compatibility.json").write_text(
            json.dumps(policy))
        resolver = source / "tools/configure/configure.py"
        resolver.parent.mkdir(parents=True)
        resolver.write_text(
            'import argparse, hashlib, json, pathlib, sys\n'
            'parser = argparse.ArgumentParser()\n'
            'parser.add_argument("command", choices=["generate"])\n'
            'parser.add_argument("--assembly", type=pathlib.Path)\n'
            'parser.add_argument("--output", type=pathlib.Path)\n'
            'parser.add_argument("--source-root", type=pathlib.Path)\n'
            'parser.add_argument("--test-support", action="store_true")\n'
            'args = parser.parse_args()\n'
            'root = args.source_root\n'
            '(root / "execution.json").write_text(json.dumps(sys.argv))\n'
            'paths = {"assembly": str(args.assembly), "policy": '
            'str(root / "dependencies/platform-compatibility.json")}\n'
            'inputs = [{"path": key, "sha256": hashlib.sha256('
            'pathlib.Path(value).read_bytes()).hexdigest()} '
            'for key, value in paths.items()]\n'
            'args.output.mkdir()\n'
            '(args.output / "input_paths.json").write_text(json.dumps(paths))\n'
            '(args.output / "resolved.json").write_text(json.dumps({'
            '"configuration_sha256": "c" * 64, "inputs": inputs, '
            f'"soc_family": {family!r}, "backend": "native", '
            f'"cpu": {{"arch": {(arch or ("native" if family == "native" else "cortex-m4"))!r}}}}}))\n'
            + mutation)
        toolchain = source / "cmake/toolchains/arm-gcc.cmake"
        toolchain.parent.mkdir(parents=True)
        toolchain.write_text("# Independent test toolchain selection only.\n")
        return source

    def test_selected_source_executes_its_own_resolver_and_commands(self):
        from tools.dev import projects
        source = self.selected_source()
        result = projects.create(self.repo, self.revision, "native", self.output,
                                 source_root=source, test_support=True)
        self.assertTrue((source / "execution.json").is_file())
        self.assertEqual(result["validation"]["configuration_sha256"], "c" * 64)
        self.assertEqual(result["validation"]["resolver"]["path"],
                         str(source / "tools/configure/configure.py"))
        self.assertEqual(result["selected_platform"]["source_root"], str(source))
        self.assertEqual(result["expected_platform"]["commit"], "a" * 40)
        self.assertIn("-DNEXUS_SOURCE=" + str(source),
                      result["commands"]["configure"])
        self.assertNotIn("-DCMAKE_TOOLCHAIN_FILE=", " ".join(
            result["commands"]["configure"]))
        self.assertEqual(result["commands"]["test"][:2],
                         ["ctest", "--test-dir"])
        self.assertEqual(result["validation"]["argv"][1:3], ["-I", "-B"])
        self.assertTrue(result["validation"]["interpreter"]["sha256"])
        self.assertFalse(result["qualification"])

    def test_selected_mcu_source_uses_its_toolchain_without_native_run(self):
        from tools.dev import projects
        source = self.selected_source(family="stm32f4")
        result = projects.create(self.repo, self.revision, "native", self.output,
                                 source_root=source, test_support=True)
        self.assertIn("-DCMAKE_TOOLCHAIN_FILE=" +
                      str(source / "cmake/toolchains/arm-gcc.cmake"),
                      result["commands"]["configure"])
        self.assertNotIn("test", result["commands"])

    def test_native_cpu_in_external_family_does_not_select_arm_toolchain(self):
        from tools.dev import projects
        source = self.selected_source(family="independent_model", arch="native")
        result = projects.create(self.repo, self.revision, "native", self.output,
                                 source_root=source, test_support=True)
        self.assertNotIn("-DCMAKE_TOOLCHAIN_FILE=", " ".join(
            result["commands"]["configure"]))
        self.assertIn("test", result["commands"])

    def test_selected_resolver_change_during_creation_rolls_back(self):
        from tools.dev import projects
        source = self.selected_source(mutation=(
            'script = pathlib.Path(__file__)\n'
            'script.write_text(script.read_text() + "# changed\\n")\n'))
        with self.assertRaisesRegex(ValueError, "changed"):
            projects.create(self.repo, self.revision, "native", self.output,
                            source_root=source, test_support=True)
        self.assertFalse(self.output.exists())

    def test_resolver_cannot_change_copied_payload_or_add_project_artifacts(self):
        from tools.dev import projects
        mutations = {
            "bytes": '(args.assembly.parent / "main.c").write_text("changed\\n")\n',
            "build-definition":
                '(args.assembly.parent / "CMakeLists.txt").write_text("changed\\n")\n',
            "unknown-file":
                '(args.assembly.parent / "untracked.txt").write_text("foreign\\n")\n',
            "unknown-directory":
                '(args.assembly.parent / "foreign").mkdir()\n',
            "symlink":
                'target = args.assembly.parent / "main.c"\n'
                'target.unlink()\n'
                'target.symlink_to(root / "dependencies/platform-compatibility.json")\n',
            "missing-file": '(args.assembly.parent / "main.c").unlink()\n',
        }
        for name, mutation in mutations.items():
            with self.subTest(mutation=name):
                source = self.selected_source(mutation=mutation)
                try:
                    with self.assertRaisesRegex(ValueError, "copied|payload|project"):
                        projects.create(self.repo, self.revision, "native", self.output,
                                        source_root=source, test_support=True)
                    self.assertFalse(self.output.exists())
                    for filename, contents in self.payload.items():
                        self.assertEqual((self.template / filename).read_bytes(), contents)
                finally:
                    if self.output.exists():
                        shutil.rmtree(self.output)
                    shutil.rmtree(source)

    def replaced_output(self, *, symbolic, reject=False):
        external = self.root / "external user project"
        external.mkdir()
        for name, contents in self.payload.items():
            (external / name).write_bytes(contents)
        mutation = (
            'project = args.assembly.parent\n'
            'parked = root / "original-project"\n'
            'project.rename(parked)\n'
            f'external = pathlib.Path({str(external)!r})\n')
        mutation += ('project.symlink_to(external, target_is_directory=True)\n'
                     if symbolic else 'external.rename(project)\n')
        mutation += (
            'paths["assembly"] = str(parked / "assembly.toml")\n'
            '(args.output / "input_paths.json").write_text(json.dumps(paths))\n')
        if reject:
            mutation += 'raise SystemExit(7)\n'
        source = self.selected_source(mutation=mutation)
        return source, external

    def test_replaced_output_symlink_cannot_publish_origin_into_external_project(self):
        from tools.dev import projects
        source, external = self.replaced_output(symbolic=True)
        with self.assertRaisesRegex(ValueError, "root|replaced|owned"):
            projects.create(self.repo, self.revision, "native", self.output,
                            source_root=source, test_support=True)
        self.assertTrue(self.output.is_symlink())
        self.assertEqual({path.name for path in external.iterdir()}, set(self.payload))
        for name, contents in self.payload.items():
            self.assertEqual((external / name).read_bytes(), contents)
            self.assertEqual((self.template / name).read_bytes(), contents)

    def test_resolver_failure_preserves_replacement_directory_and_original_error(self):
        from tools.dev import projects
        source, _ = self.replaced_output(symbolic=False, reject=True)
        with self.assertRaisesRegex(ValueError, "Selected platform rejected"):
            projects.create(self.repo, self.revision, "native", self.output,
                            source_root=source, test_support=True)
        self.assertTrue(self.output.is_dir())
        self.assertEqual({path.name for path in self.output.iterdir()}, set(self.payload))
        for name, contents in self.payload.items():
            self.assertEqual((self.output / name).read_bytes(), contents)
            self.assertEqual((self.template / name).read_bytes(), contents)

    def sdk_prefix(self, *, publishable=True):
        source = self.selected_source()
        prefix = self.root / "installed SDK with spaces"
        sdk = prefix / "share/nexus/src"
        sdk.parent.mkdir(parents=True)
        shutil.move(source, sdk)
        identity = {"source_revision": "d" * 40, "source_tree": "e" * 40,
                    "publishable": publishable, "source_dirty": not publishable,
                    "snapshot_sha256": "f" * 64}
        (sdk / ".nexus-source-sdk.json").write_text(json.dumps(identity))
        verifier = sdk / "cmake/package/package_source_sdk.py"
        verifier.parent.mkdir(parents=True)
        verifier.write_text(
            'import json, pathlib, sys\n'
            'root = pathlib.Path(sys.argv[-1])\n'
            '(root / "verified.json").write_text(json.dumps(sys.argv))\n'
            'print((root / ".nexus-source-sdk.json").read_text())\n')
        config = prefix / "lib/cmake/Nexus/NexusConfig.cmake"
        config.parent.mkdir(parents=True)
        config.write_text("# SDK selection fixture only.\n")
        return prefix, sdk

    def test_sdk_selection_uses_its_verifier_resolver_and_cmake_package(self):
        from tools.dev import projects
        prefix, source = self.sdk_prefix()
        result = projects.create(self.repo, self.revision, "native", self.output,
                                 sdk_prefix=prefix, test_support=True)
        self.assertTrue((source / "verified.json").is_file())
        self.assertTrue((source / "execution.json").is_file())
        self.assertEqual(result["selected_platform"]["kind"], "sdk")
        self.assertEqual(result["selected_platform"]["source_revision"], "d" * 40)
        self.assertIn("-DNexus_DIR=" + str(prefix / "lib/cmake/Nexus"),
                      result["commands"]["configure"])
        self.assertNotIn("-DNEXUS_SOURCE=", " ".join(
            result["commands"]["configure"]))

    def test_development_sdk_is_explicitly_rejected_without_fallback(self):
        from tools.dev import projects
        prefix, _ = self.sdk_prefix(publishable=False)
        with self.assertRaisesRegex(ValueError, "publishable|development"):
            projects.create(self.repo, self.revision, "native", self.output,
                            sdk_prefix=prefix, test_support=True)
        self.assertFalse(self.output.exists())

    def test_installed_source_selection_records_its_verified_actual_revision(self):
        from tools.dev import projects
        _, source = self.sdk_prefix()
        result = projects.create(self.repo, self.revision, "native", self.output,
                                 source_root=source, test_support=True)
        self.assertTrue((source / "verified.json").is_file())
        self.assertEqual(result["selected_platform"]["kind"], "source")
        self.assertEqual(result["selected_platform"]["source_revision"], "d" * 40)
        self.assertIn("-DNEXUS_SOURCE=" + str(source),
                      result["commands"]["configure"])

    def test_source_and_sdk_conflict_is_rejected_before_creation(self):
        from tools.dev import projects
        with self.assertRaisesRegex(ValueError, "source.*SDK|mutually"):
            projects.create(self.repo, self.revision, "native", self.output,
                            source_root=self.root, sdk_prefix=self.root, test_support=True)
        self.assertFalse(self.output.exists())

    def test_cli_commands_quote_paths_and_json_retains_argument_boundaries(self):
        from tools.dev import projects
        source = self.selected_source()
        arguments = ["--templates", str(self.repo), "--revision", self.revision,
                     "--template", "native", "--output", str(self.output),
                     "--source", str(source), "--test-support"]
        stream = io.StringIO()
        with redirect_stdout(stream):
            self.assertEqual(projects.main(arguments), 0)
        origin = json.loads((self.output / ".nexus-template.json").read_text())
        commands = [line.partition(": ")[2] for line in stream.getvalue().splitlines()
                    if line.startswith(("Configure: ", "Build: ", "Test: "))]
        self.assertEqual([shlex.split(line) for line in commands],
                         [origin["commands"][name]
                          for name in ("configure", "build", "test")])
        shutil.rmtree(self.output)
        stream = io.StringIO()
        with redirect_stdout(stream):
            self.assertEqual(projects.main(arguments + ["--json"]), 0)
        self.assertEqual(json.loads(stream.getvalue())["commands"],
                         origin["commands"])

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
                                 "native", holder / ".." / self.output.name, test_support=True)
        self.assertEqual((self.output / "main.c").read_bytes(),
                         self.payload["main.c"])
        self.assertEqual(result["examples_commit"], self.revision)

    def test_parent_normalization_cannot_hide_a_symlink_component(self):
        projects = importlib.import_module("tools.dev.projects")
        alias = self.root / "alias"
        alias.symlink_to(self.repo, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            projects.create(alias / ".." / "examples", self.revision,
                            "native", self.output, test_support=True)
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
        shutil.copytree(dev.ROOT / "tests/support/native/board", shared)
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
