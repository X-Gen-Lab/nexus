"""Reject incomplete or stale selected-provider assembly evidence."""

from pathlib import Path
import json
import os
import signal
import sys
import tempfile
import unittest
from unittest import mock

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
            with mock.patch.dict(os.environ, {"GTEST_FILTER": "nothing", "CTEST_PARALLEL_LEVEL": "9"}):
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
        elf = mock.Mock()
        elf.defined_symbols = [{"name": "nx_any_uart_ops"}]
        with mock.patch.object(gate.retention, "source_contracts", return_value=(
                {}, {}, {"nx_any_uart_ops": {}}, [])):
            with self.assertRaisesRegex(EvidenceError, "unselected"):
                gate.empty_retention(elf, "", {"controllers": [], "devices": []})


if __name__ == "__main__":
    unittest.main()
