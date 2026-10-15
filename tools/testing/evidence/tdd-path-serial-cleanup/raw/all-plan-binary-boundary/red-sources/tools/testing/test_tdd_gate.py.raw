"""Real path boundaries in the existing gate, without running provider tests."""

import json
from pathlib import Path
import unittest

from tools.testing import test_tdd_matrix_integration as integration


class TDDBinaryPlanBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.fixture = integration.TDDMatrixIntegrationTests(
            "test_build_capture_execution_summary_strict_audit_order")
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)

    def reject_additional_binary(self, path):
        plan_path = self.fixture.build / "google-contracts.json"
        plan = json.loads(plan_path.read_text())
        plan["executables"].append(str(path))
        plan_path.write_text(json.dumps(plan))
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            return self.fixture.execute(command, environment, log)

        status, output, _, after = self.fixture.run_gate(execute=execute)
        self.assertEqual(status, 1, output)
        after.assert_not_called()
        self.assertFalse(any(command[0] == str(path) for command in commands))
        self.assertFalse((self.fixture.build / "tdd/execution.json").exists())

    def test_uncatalogued_parent_escape_rejected_before_subprocess(self):
        foreign = self.fixture.root / "foreign/nexus_extra_contract"
        foreign.parent.mkdir()
        foreign.write_bytes(self.fixture.binary.read_bytes())
        escaped = self.fixture.build / "../../foreign/nexus_extra_contract"
        self.assertTrue(escaped.is_file())
        self.assertFalse(escaped.is_symlink())
        self.assertFalse(escaped.resolve().is_relative_to(self.fixture.build))
        self.reject_additional_binary(escaped)

    def test_uncatalogued_file_symlink_rejected_before_subprocess(self):
        alias = self.fixture.binary.parent / "nexus_extra_contract"
        alias.symlink_to(self.fixture.binary)
        self.assertTrue(alias.is_file())
        self.assertTrue(alias.is_symlink())
        self.reject_additional_binary(alias)

    def test_uncatalogued_parent_symlink_rejected_before_subprocess(self):
        directory = self.fixture.build / "alias-bin"
        directory.symlink_to(self.fixture.binary.parent, target_is_directory=True)
        alias = directory / self.fixture.binary.name
        self.assertTrue(alias.is_file())
        self.assertFalse(alias.is_symlink())
        self.assertTrue(directory.is_symlink())
        self.reject_additional_binary(alias)


if __name__ == "__main__":
    unittest.main()
