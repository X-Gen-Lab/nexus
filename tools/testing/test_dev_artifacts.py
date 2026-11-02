"""Artifact navigation verifies real receipts without executing equipment."""

import importlib
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
from unittest.mock import patch
import unittest

from tools.testing import test_dev_device as fixtures


class DeveloperArtifactTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.DeveloperDeviceTests(methodName="runTest")
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)

    def module(self):
        return importlib.import_module("tools.dev.artifacts")

    def test_observation_lists_bound_outputs_without_launching_commands(self):
        with patch("subprocess.Popen") as launch:
            result = self.module().inspect(self.fixture.provenance)
        launch.assert_not_called()
        self.assertEqual(result["configuration"]["board"], "synthetic-board")
        self.assertEqual(result["artifacts"][0]["path"],
                         str(self.fixture.elf))
        self.assertEqual(result["artifacts"][0]["role"], "elf")
        self.assertFalse(result["software_qualified"])
        self.assertEqual(result["physical_status"], "not_assessed")

    def test_changed_artifact_rejects_before_navigation(self):
        self.fixture.elf.write_bytes(b"changed")
        with self.assertRaises(ValueError):
            self.module().inspect(self.fixture.provenance)

    def test_configure_or_unexecuted_record_is_not_a_completed_build(self):
        for change in ({"action": "configure"}, {"actual_execution": None}):
            with self.subTest(change=change):
                record = json.loads(self.fixture.provenance.read_text())
                record.update(change)
                self.fixture.provenance.write_text(json.dumps(record))
                with self.assertRaisesRegex(ValueError, "completed build"):
                    self.module().inspect(self.fixture.provenance)

    def test_changed_raw_execution_output_rejects(self):
        record = json.loads(self.fixture.provenance.read_text())
        raw = Path(record["actual_execution"]["stdout_stderr"][0]["path"])
        raw.write_bytes(b"changed raw result")
        with self.assertRaises(ValueError):
            self.module().inspect(self.fixture.provenance)

    def test_developer_cli_dispatches_read_only_artifact_navigation(self):
        from tools.dev import dev
        with redirect_stdout(io.StringIO()) as output:
            result = dev.main([
                "artifacts", "--provenance", str(self.fixture.provenance)])
        self.assertEqual(result, 0)
        self.assertEqual(json.loads(output.getvalue())["kind"],
                         "build_artifact_navigation")

    def test_output_cannot_overwrite_an_input_or_previous_observation(self):
        original = self.fixture.provenance.read_bytes()
        with self.assertRaises((ValueError, FileExistsError)):
            self.module().main([
                "--provenance", str(self.fixture.provenance),
                "--json", str(self.fixture.provenance)])
        self.assertEqual(self.fixture.provenance.read_bytes(), original)
        output = self.fixture.root / "navigation.json"
        self.assertEqual(self.module().main([
            "--provenance", str(self.fixture.provenance),
            "--json", str(output)]), 0)
        saved = output.read_bytes()
        with self.assertRaises((ValueError, FileExistsError)):
            self.module().main([
                "--provenance", str(self.fixture.provenance),
                "--json", str(output)])
        self.assertEqual(output.read_bytes(), saved)


if __name__ == "__main__":
    unittest.main()
