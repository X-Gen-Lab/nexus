"""Reject incomplete or stale selected-provider assembly evidence."""

from pathlib import Path
import contextlib
import hashlib
import io
import json
import os
import signal
import sys
import tempfile
import unittest.mock

from scripts.ci import provider_assemblies as gate
from tools.evidence.common import EvidenceError


ROOT = Path(__file__).resolve().parents[2]
USB = ROOT / "tests/contracts/usb_assembly/stm32-zg.toml"


class FixtureAdmission(unittest.TestCase):
    def test_empty_matrix_is_a_failure(self):
        with self.assertRaisesRegex(EvidenceError, "nonempty"):
            gate.validate_fixtures(ROOT, [])

    def test_duplicate_input_is_not_two_executions(self):
        with self.assertRaisesRegex(EvidenceError, "duplicate"):
            gate.validate_fixtures(ROOT, [USB, USB])

    def test_family_filter_must_match_resolved_authority(self):
        with self.assertRaisesRegex(EvidenceError, "family"):
            gate.validate_fixtures(ROOT, [USB], family="gd32f470")

    def test_real_authored_input_is_resolved_without_rewriting(self):
        before = USB.read_bytes()
        rows = gate.validate_fixtures(ROOT, [USB], family="stm32f407")
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["resolved"]["part"], "STM32F407ZGT6")
        self.assertEqual(rows[0]["resolved"]["backend"], "baremetal")
        self.assertEqual(USB.read_bytes(), before)

    def test_model_only_directory_is_not_an_assembly_graph(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "tests/contracts") as directory:
            path = Path(directory) / "fixture.toml"
            path.write_text('schema=2\n')
            with self.assertRaisesRegex(EvidenceError, "CMakeLists"):
                gate.validate_fixtures(ROOT, [path])

    def test_unowned_external_fixture_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.toml"
            path.write_text('schema=2\n')
            with self.assertRaisesRegex(EvidenceError, "contracts"):
                gate.validate_fixtures(ROOT, [path])

    def test_symlink_input_is_refused_before_resolution(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "tests/contracts") as directory:
            path = Path(directory) / "alias.toml"
            path.symlink_to(USB)
            with self.assertRaisesRegex(EvidenceError, "symlink"):
                gate.validate_fixtures(ROOT, [path])


class ExplicitProjectAdmission(unittest.TestCase):
    def setUp(self):
        self.assembly = ROOT / (
            "tools/configure/assemblies/nucleo-h743-baremetal-uart.toml")
        self.project = ROOT / "tests/contracts/h743_assembly"

    def test_canonical_input_uses_the_explicit_owned_project(self):
        before = self.assembly.read_bytes()
        rows = gate.validate_fixtures(
            ROOT, [self.assembly], project=self.project, family="stm32h743")
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["project"], str(self.project))
        self.assertEqual(rows[0]["resolved"]["part"], "STM32H743ZIT6")
        self.assertEqual(self.assembly.read_bytes(), before)

    def test_canonical_input_without_project_remains_rejected(self):
        with self.assertRaises(EvidenceError):
            gate.validate_fixtures(ROOT, [self.assembly])

    def test_legacy_input_can_use_an_explicit_owned_project(self):
        rows = gate.validate_fixtures(ROOT, [USB], project=USB.parent)
        self.assertEqual(rows[0]["project"], str(USB.parent))

    def test_external_project_cannot_select_a_source_graph(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            (project / "CMakeLists.txt").write_text("project(foreign)\n")
            with self.assertRaisesRegex(EvidenceError, "project"):
                gate.validate_fixtures(ROOT, [self.assembly], project=project)

    def test_missing_and_symlink_project_are_rejected(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "tests/contracts") as directory:
            project = Path(directory)
            with self.assertRaisesRegex(EvidenceError, "CMakeLists"):
                gate.validate_fixtures(ROOT, [self.assembly], project=project)
            alias = project / "alias"
            alias.symlink_to(self.project, target_is_directory=True)
            with self.assertRaisesRegex(EvidenceError, "symlink"):
                gate.validate_fixtures(ROOT, [self.assembly], project=alias)

    def test_dotdot_fixture_alias_cannot_count_as_an_execution(self):
        alias = self.assembly.parent / ".." / "assemblies" / self.assembly.name
        with self.assertRaisesRegex(EvidenceError, "path|alias"):
            gate.validate_fixtures(ROOT, [alias], project=self.project)

    def test_canonical_duplicate_is_not_another_execution(self):
        with self.assertRaisesRegex(EvidenceError, "duplicate"):
            gate.validate_fixtures(ROOT, [self.assembly, self.assembly],
                                   project=self.project)

    def test_external_fixture_is_rejected_with_an_owned_project(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "foreign.toml"
            source.write_text('schema = 2\n')
            with self.assertRaisesRegex(EvidenceError, "owned|fixture"):
                gate.validate_fixtures(ROOT, [source], project=self.project)

    def test_native_canonical_is_not_an_arm_fixture(self):
        source = self.assembly.with_name("native.toml")
        with self.assertRaisesRegex(EvidenceError, "host model"):
            gate.validate_fixtures(ROOT, [source], project=self.project)

    def test_explicit_canonical_input_requires_schema_two(self):
        with tempfile.NamedTemporaryFile(
                dir=self.assembly.parent, suffix=".toml", mode="w") as stream:
            stream.write('schema = 1\n')
            stream.flush()
            with self.assertRaisesRegex(EvidenceError, "schema"):
                gate.validate_fixtures(ROOT, [Path(stream.name)],
                                       project=self.project)

    def test_explicit_project_files_belong_to_the_stale_input_guard(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text("project(root)\n")
            project = root / "tests/contracts/explicit"
            project.mkdir(parents=True)
            (project / "CMakeLists.txt").write_text("project(selected)\n")
            main = project / "main.c"
            main.write_text("int main(void) { return 0; }\n")
            fixture = {"assembly": str(root / "tools/configure/assembly.toml"),
                       "project": str(project), "inputs": [],
                       "resolved": {"source_dependencies": []}}
            before = gate.source_inputs(root, [fixture])
            self.assertIn(str(main), {row["path"] for row in before})
            main.write_text("int main(void) { return 1; }\n")
            with self.assertRaisesRegex(EvidenceError, "snapshot"):
                gate.verify_snapshot(root, [fixture], before)

    def test_dotdot_output_is_refused_before_creating_any_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "build" / ".." / "source" / "generated"
            with self.assertRaisesRegex(EvidenceError, "path|source"):
                gate.fresh_output(output, root)
            self.assertFalse((root / "source").exists())

    def test_cli_repeated_project_rejects_before_running_the_matrix(self):
        with unittest.mock.patch.object(gate, "run") as run, \
                contextlib.redirect_stderr(io.StringIO()), \
                self.assertRaises(SystemExit) as failure:
            gate.main(["--fixture", str(self.assembly), "--output", "unused",
                       "--project", str(self.project),
                       "--project", str(self.project)])
        self.assertEqual(failure.exception.code, 2)
        run.assert_not_called()

    def test_cli_default_inherits_outer_build_jobs(self):
        with unittest.mock.patch.dict(os.environ, {"NEXUS_BUILD_JOBS": "1",
                                         "CMAKE_BUILD_PARALLEL_LEVEL": "9"}), \
                unittest.mock.patch.object(gate, "run", return_value=0) as run:
            result = gate.main(["--fixture", str(USB), "--output", "unused"])
        self.assertEqual(result, 0)
        self.assertEqual(run.call_args.kwargs["parallel"], 1)

    def test_explicit_cli_allocation_overrides_outer_default(self):
        with unittest.mock.patch.dict(os.environ, {"NEXUS_BUILD_JOBS": "1"}), \
                unittest.mock.patch.object(gate, "run", return_value=0) as run:
            result = gate.main(["--fixture", str(USB), "--output", "unused",
                                "--parallel", "3"])
        self.assertEqual(result, 0)
        self.assertEqual(run.call_args.kwargs["parallel"], 3)

    def test_cli_passes_the_explicit_project_without_family_inference(self):
        with unittest.mock.patch.object(gate, "run", return_value=0) as run:
            self.assertEqual(gate.main([
                "--fixture", str(self.assembly), "--output", "unused",
                "--project", str(self.project)]), 0)
        self.assertEqual(run.call_args.kwargs["project"], self.project)
        self.assertIsNone(run.call_args.kwargs["family"])


    def test_hardlinked_canonical_aliases_are_rejected_before_resolution(self):
        descriptor, name = tempfile.mkstemp(
            dir=self.assembly.parent, suffix=".toml")
        os.close(descriptor)
        alias = Path(name)
        alias.unlink()
        os.link(self.assembly, alias)
        try:
            for selected in ([self.assembly], [alias],
                             [self.assembly, alias]):
                with self.subTest(selected=[path.name for path in selected]):
                    with self.assertRaisesRegex(EvidenceError, "hardlink"):
                        gate.validate_fixtures(
                            ROOT, selected, project=self.project)
        finally:
            alias.unlink()

    def test_hardlinked_project_entry_cannot_authorize_a_source_graph(self):
        with tempfile.TemporaryDirectory(
                dir=ROOT / "tests/contracts") as name:
            project = Path(name)
            original = project / "selected.cmake"
            original.write_text("project(selected)\n")
            os.link(original, project / "CMakeLists.txt")
            with self.assertRaisesRegex(EvidenceError, "hardlink"):
                gate.validate_fixtures(
                    ROOT, [self.assembly], project=project)

    def test_every_owned_project_input_invalidates_a_stale_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text("project(root)\n")
            project = root / "tests/contracts/explicit"
            project.mkdir(parents=True)
            (project / "CMakeLists.txt").write_text(
                "project(selected)\n"
                "add_executable(selected main.cpp)\n"
                "configure_file(template.in generated.h)\n"
                "file(STRINGS build-options SELECTED_OPTIONS)\n")
            inputs = ("main.cpp", "selected.inc", "template.in",
                      "build-options")
            for name in inputs:
                (project / name).write_text("original selected input\n")
            fixture = {"assembly": str(root / "tools/configure/assembly.toml"),
                       "project": str(project), "inputs": [],
                       "resolved": {"source_dependencies": []}}
            for name in inputs:
                with self.subTest(name=name):
                    path = project / name
                    before = gate.source_inputs(root, [fixture])
                    path.write_text("changed selected input\n")
                    with self.assertRaisesRegex(EvidenceError, "snapshot"):
                        gate.verify_snapshot(root, [fixture], before)
                    self.assertIn(str(path), {row["path"] for row in before})
                    path.write_text("original selected input\n")

    def test_symlink_project_subtree_cannot_hide_selected_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text("project(root)\n")
            project = root / "tests/contracts/explicit"
            project.mkdir(parents=True)
            (project / "CMakeLists.txt").write_text("project(selected)\n")
            external = root / "outside"
            external.mkdir()
            (external / "template.in").write_text("external input\n")
            (project / "linked").symlink_to(external,
                                            target_is_directory=True)
            fixture = {"assembly": str(root / "tools/configure/assembly.toml"),
                       "project": str(project), "inputs": [],
                       "resolved": {"source_dependencies": []}}
            with self.assertRaisesRegex(EvidenceError, "symlink"):
                gate.source_inputs(root, [fixture])

class ActualCommandRecording(unittest.TestCase):
    def test_nonzero_real_tool_is_not_a_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            recorder = gate.Recorder(Path(directory), ROOT)
            with self.assertRaisesRegex(EvidenceError, "exit 7"):
                recorder.execute([sys.executable, "-c",
                                  "import sys; print('raw failure'); sys.exit(7)"],
                                 "failed-tool")
            self.assertEqual(recorder.commands[0]["returncode"], 7)
            log = Path(recorder.commands[0]["stdout"]["path"])
            self.assertEqual(log.read_bytes(), b"raw failure\n")

    def test_missing_tool_keeps_failed_command_record(self):
        with tempfile.TemporaryDirectory() as directory:
            recorder = gate.Recorder(Path(directory), ROOT)
            with self.assertRaises(EvidenceError):
                recorder.execute([str(Path(directory) / "missing")], "missing-tool")
            self.assertIsNone(recorder.commands[0]["returncode"])
            self.assertIn("error", recorder.commands[0])

    def test_timeout_retains_actual_output_and_terminates(self):
        with tempfile.TemporaryDirectory() as directory:
            recorder = gate.Recorder(Path(directory), ROOT, timeout=0.1)
            with self.assertRaisesRegex(EvidenceError, "timed out"):
                recorder.execute([sys.executable, "-u", "-c",
                                  "import time; print('started'); time.sleep(5)"],
                                 "timeout-tool")
            self.assertEqual(Path(recorder.commands[0]["stdout"]["path"]).read_bytes(),
                             b"started\n")
            self.assertNotEqual(recorder.commands[0]["returncode"], 0)

    def test_command_environment_does_not_inherit_test_filters(self):
        with tempfile.TemporaryDirectory() as directory:
            with unittest.mock.patch.dict(os.environ, {"GTEST_FILTER": "nothing", "CTEST_PARALLEL_LEVEL": "9"}):
                recorder = gate.Recorder(Path(directory), ROOT)
            output = recorder.execute([sys.executable, "-c",
                "import os; print(int(any(k.startswith(('GTEST_', 'CTEST_')) for k in os.environ)))"],
                "environment")
            self.assertEqual(output.strip(), "0")

    def test_successful_parent_cannot_leave_artifact_writer_running(self):
        with tempfile.TemporaryDirectory() as directory:
            recorder = gate.Recorder(Path(directory), ROOT)
            try:
                with self.assertRaisesRegex(EvidenceError, "background"):
                    recorder.execute([sys.executable, "-u", "-c",
                        "import subprocess,sys; p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(5)']); print(p.pid)"],
                        "background-writer")
            finally:
                log = Path(recorder.commands[0]["stdout"]["path"])
                if log.read_text().strip():
                    try:
                        os.kill(int(log.read_text().strip()), signal.SIGKILL)
                    except ProcessLookupError:
                        # The recorder may already have withdrawn this child.
                        # Other cleanup errors must still fail the test.
                        pass


class OutputAdmission(unittest.TestCase):
    def test_alternate_source_root_cannot_qualify_current_tool_code(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "other-source"
            root.mkdir()
            output = Path(directory) / "audit"
            self.assertEqual(gate.run(root, [], "unused", output), 1)
            self.assertIn("tool source", json.loads(
                (output / "report.json").read_text())["error"])

    def test_shared_job_budget_helper_is_a_snapshotted_input(self):
        from tools.dev import process
        inputs = {row["path"] for row in gate.source_inputs(ROOT, [])}
        self.assertIn(str(Path(process.__file__).resolve()), inputs)

    def test_imported_compiler_identity_helper_is_a_snapshotted_input(self):
        inputs = gate.source_inputs(ROOT, [])
        self.assertIn(str(Path(gate.arch_compile.__file__).resolve()),
                      {item["path"] for item in inputs})

    def test_new_source_file_cannot_escape_snapshot_verification(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text("project(test)\n")
            original = gate.source_inputs(root, [])
            source = root / "io/src/new.c"
            source.parent.mkdir(parents=True)
            source.write_text("void new_source(void) {}\n")
            with self.assertRaisesRegex(EvidenceError, "snapshot"):
                gate.verify_snapshot(root, [], original)

    def test_shared_driver_bytes_are_in_the_independent_source_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text("project(test)\n")
            for suffix in (".c", ".h"):
                with self.subTest(suffix=suffix):
                    source = root / ("drivers/i2c/reviewed_ip/driver" + suffix)
                    source.parent.mkdir(parents=True, exist_ok=True)
                    source.write_text("/* Reviewed driver bytes. */\n")
                    original = gate.source_inputs(root, [])
                    source.write_text("/* Changed during observation. */\n")
                    with self.assertRaisesRegex(EvidenceError, "input changed"):
                        gate.verify_inputs(original)
                    with self.assertRaisesRegex(EvidenceError, "snapshot"):
                        gate.verify_snapshot(root, [], original)

    def test_new_driver_cannot_escape_independent_snapshot_verification(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text("project(test)\n")
            original = gate.source_inputs(root, [])
            source = root / "drivers/i2c/reviewed_ip/new_driver.c"
            source.parent.mkdir(parents=True)
            source.write_text("/* New source after observation. */\n")
            with self.assertRaisesRegex(EvidenceError, "snapshot"):
                gate.verify_snapshot(root, [], original)

    def test_existing_output_is_not_fresh_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "output"
            path.mkdir()
            (path / "old-positive.json").write_text('{"status":"passed"}\n')
            with self.assertRaisesRegex(EvidenceError, "fresh"):
                gate.fresh_output(path, ROOT)

    def test_symlink_output_cannot_overwrite_other_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            (base / "protected").mkdir()
            (base / "output").symlink_to(base / "protected", target_is_directory=True)
            with self.assertRaisesRegex(EvidenceError, "symlink"):
                gate.fresh_output(base / "output", ROOT)

    def test_output_cannot_live_inside_an_authored_fixture(self):
        with self.assertRaisesRegex(EvidenceError, "source"):
            gate.fresh_output(USB.parent / "generated-audit", ROOT)

    def test_stale_source_identity_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "source.c"
            path.write_text("old\n")
            identities = [gate.arch_compile.digest(path)]
            path.write_text("new\n")
            with self.assertRaisesRegex(EvidenceError, "changed"):
                gate.verify_inputs(identities)

    def test_wrong_built_configuration_is_rejected(self):
        with self.assertRaisesRegex(EvidenceError, "configuration"):
            gate.require_configuration({"configuration_sha256": "a", "soc_family": "stm32f407"},
                                       {"configuration_sha256": "b", "soc_family": "stm32f407"})

    def test_wrong_built_family_is_rejected_even_if_hash_is_claimed_equal(self):
        with self.assertRaisesRegex(EvidenceError, "family"):
            gate.require_configuration({"configuration_sha256": "a", "soc_family": "stm32f407"},
                                       {"configuration_sha256": "a", "soc_family": "gd32f470"})

    def test_budget_failure_is_never_admitted(self):
        with self.assertRaisesRegex(EvidenceError, "budget"):
            gate.require_budget({"status": "failed", "violations": [{"metric": "ram.sram"}]})

    def test_positive_status_with_violations_is_not_a_budget_pass(self):
        with self.assertRaisesRegex(EvidenceError, "budget"):
            gate.require_budget({"status": "passed", "violations": ["overflow"]})

    def test_ninja_dependency_report_requires_actual_valid_objects(self):
        with self.assertRaisesRegex(EvidenceError, "dependencies"):
            gate.dependency_paths("", ROOT)
        with self.assertRaisesRegex(EvidenceError, "stale"):
            gate.dependency_paths("a.o: #deps 1, deps mtime 1 (STALE)\n    file.h\n", ROOT)

    def test_empty_retention_rejects_retained_concrete_provider(self):
        elf = unittest.mock.Mock()
        elf.defined_symbols = [{"name": "nx_any_uart_ops"}]
        with unittest.mock.patch.object(gate.retention, "source_contracts", return_value=(
                {}, {}, {"nx_any_uart_ops": {}}, [])):
            with self.assertRaisesRegex(EvidenceError, "unselected"):
                gate.empty_retention(elf, "", {"controllers": [], "devices": []})


class EmptySourceInputs(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "CMakeLists.txt").write_text("project(test)\n")
        self.source = self.root / "io/src/empty.c"
        self.source.parent.mkdir(parents=True)
        self.source.write_bytes(b"")

    def test_empty_source_and_header_remain_in_the_input_closure(self):
        header = self.source.with_suffix(".h")
        header.write_bytes(b"")
        inputs = gate.source_inputs(self.root, [])
        for path in (self.source, header):
            identity = next(item for item in inputs if item["path"] == str(path))
            self.assertEqual(identity["bytes"], 0)
            self.assertEqual(identity["sha256"], hashlib.sha256(b"").hexdigest())
        gate.verify_inputs(inputs)
        gate.verify_snapshot(self.root, [], inputs)

    def test_empty_locked_vendor_source_is_not_dropped(self):
        vendor = self.root / "vendors/locked/component.h"
        vendor.parent.mkdir(parents=True)
        vendor.write_bytes(b"")
        fixture = {"assembly": str(self.root / "tests/contracts/fixture/a.toml"),
                   "resolved": {"source_dependencies": [
                       {"path": "vendors/locked"}]}, "inputs": []}
        inputs = gate.source_inputs(self.root, [fixture])
        self.assertIn(str(vendor), {item["path"] for item in inputs})
        gate.verify_snapshot(self.root, [fixture], inputs)

    def test_empty_source_content_change_is_rejected(self):
        inputs = gate.source_inputs(self.root, [])
        self.source.write_text("void changed(void) {}\n")
        with self.assertRaisesRegex(EvidenceError, "changed"):
            gate.verify_inputs(inputs)
        with self.assertRaisesRegex(EvidenceError, "snapshot"):
            gate.verify_snapshot(self.root, [], inputs)

    def test_removed_empty_source_is_not_an_unchanged_snapshot(self):
        inputs = gate.source_inputs(self.root, [])
        self.source.unlink()
        with self.assertRaisesRegex(EvidenceError, "snapshot"):
            gate.verify_snapshot(self.root, [], inputs)

    def test_declared_missing_input_is_still_rejected(self):
        self.source.unlink()
        fixture = {"assembly": str(self.root / "tests/contracts/fixture/a.toml"),
                   "resolved": {"source_dependencies": []},
                   "inputs": [{"path": str(self.source)}]}
        with self.assertRaisesRegex(EvidenceError, "missing"):
            gate.source_inputs(self.root, [fixture])

    def test_symlink_cannot_substitute_for_an_empty_input(self):
        inputs = gate.source_inputs(self.root, [])
        target = self.root / "other-empty.c"
        target.write_bytes(b"")
        self.source.unlink()
        self.source.symlink_to(target)
        with self.assertRaisesRegex(EvidenceError, "symlink|changed"):
            gate.verify_inputs(inputs)
        with self.assertRaisesRegex(EvidenceError, "symlink"):
            gate.verify_snapshot(self.root, [], inputs)

    def test_actual_empty_compiler_dependency_remains_recorded(self):
        text = f"source.o: #deps 1, deps mtime 1 (VALID)\n    {self.source}\n"
        self.assertEqual(gate.dependency_paths(text, self.root), [self.source])

    def test_missing_compiler_dependency_is_still_rejected(self):
        missing = self.root / "missing.h"
        text = f"source.o: #deps 1, deps mtime 1 (VALID)\n    {missing}\n"
        with self.assertRaisesRegex(EvidenceError, "missing"):
            gate.dependency_paths(text, self.root)

    def test_symlink_compiler_dependency_is_still_rejected(self):
        target = self.root / "target.h"
        target.write_bytes(b"")
        alias = self.root / "alias.h"
        alias.symlink_to(target)
        text = f"source.o: #deps 1, deps mtime 1 (VALID)\n    {alias}\n"
        with self.assertRaisesRegex(EvidenceError, "symlink"):
            gate.dependency_paths(text, self.root)

    def test_empty_firmware_artifact_is_still_rejected(self):
        with self.assertRaisesRegex(EvidenceError, "missing or empty"):
            gate.one([self.source], "firmware ELF")


if __name__ == "__main__":
    unittest.main()
