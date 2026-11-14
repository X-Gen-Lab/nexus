"""Explicit provider selection in Board-first authoring commands."""
from tools.configure.tests.native_fixture import native_fixture, with_native_support
from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import shutil
import tempfile
import tomllib
import unittest
from unittest.mock import patch

from tools.configure import configure
from tools.configure import test_external_package as external_tests


class ExternalProviderAuthoringTests(unittest.TestCase):
    reseal = staticmethod(external_tests.ExternalPackageTests.reseal)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.package, self.assembly = external_tests.ExternalPackageTests.package(self)
        self.board = self.root / "partner-board"

    def command(self, *arguments):
        output, errors = io.StringIO(), io.StringIO()
        with redirect_stdout(output), redirect_stderr(errors):
            try:
                status = configure.main([*arguments, "--test-support"])
            except SystemExit as error:
                status = error.code
        return status, output.getvalue(), errors.getvalue()

    def test_board_first_query_uses_explicit_provider_without_execution_choices(self):
        with patch.object(configure.integration_package, "select",
                          wraps=configure.integration_package.select) as select:
            status, output, errors = self.command(
                "list-bindings", "--board-package", str(self.board),
                "--soc-package", str(self.package), "--kind", "gpio", "--json")
        self.assertEqual(status, 0, errors)
        self.assertEqual(select.call_count, 1)
        view = json.loads(output)
        self.assertEqual(view["selection"]["family"], "partner_model")
        self.assertIsNone(view["selection"]["backend"])
        self.assertIsNone(view["selection"]["clock_profile"])
        self.assertEqual(view["physical_verification"], "not_applicable")
        self.assertEqual(view["bindings"][0]["binding"], "led0")
        self.assertIn(str(self.package / "nexus-soc.json"), view["fact_sources"])

    def test_missing_or_mismatched_provider_never_falls_back(self):
        status, _, _ = self.command("list-bindings", "--board-package", str(self.board))
        self.assertEqual(status, 1)
        facts = self.board / "board.json"
        document = json.loads(facts.read_text())
        document["soc_family"] = "native"
        facts.write_text(json.dumps(document))
        status, _, errors = self.command(
            "list-bindings", "--board-package", str(self.board),
            "--soc-package", str(self.package))
        self.assertEqual(status, 1)
        self.assertIn("differs from Board family", errors)

    def test_assembly_query_rejects_an_independent_provider_override(self):
        status, _, errors = self.command(
            "list-bindings", "--assembly", str(self.assembly),
            "--soc-package", str(self.package))
        self.assertEqual(status, 1)
        self.assertIn("assembly", errors)

    def test_relative_init_preserves_both_package_paths_after_relocation(self):
        output = Path("product/config/assembly.toml")
        previous = Path.cwd()
        try:
            os.chdir(self.root)
            status, _, errors = self.command(
                "init", "--board-package", self.board.name,
                "--soc-package", self.package.name, "--backend", "native",
                "--clock", "model", "--profile", "gpio", "--binding", "led0",
                "--output", str(output))
        finally:
            os.chdir(previous)
        self.assertEqual(status, 0, errors)
        authored = tomllib.loads((self.root / output).read_text())
        self.assertEqual(authored["soc_package"], "../../partner")
        self.assertEqual(authored["board_package"], "../../partner-board")
        relocated = self.root.with_name(self.root.name + " relocated")
        self.root.rename(relocated)
        self.addCleanup(shutil.rmtree, relocated, ignore_errors=True)
        resolved = configure.resolve_ir(relocated / output, test_support=native_fixture(relocated / output))
        self.assertEqual(resolved.soc_family, "partner_model")
        self.assertEqual(resolved.controllers[0].kind, "gpio")

    def test_init_keeps_explicit_backend_clock_and_removes_failed_output(self):
        output = self.root / "new.toml"
        arguments = ("init", "--board-package", str(self.board),
                     "--soc-package", str(self.package), "--output", str(output))
        for additional in ((), ("--backend", "native"), ("--clock", "model")):
            status, _, _ = self.command(*arguments, *additional)
            self.assertNotEqual(status, 0)
            self.assertFalse(output.exists())
        status, _, errors = self.command(
            *arguments, "--backend", "native", "--clock", "model",
            "--profile", "gpio", "--binding", "missing")
        self.assertEqual(status, 1)
        self.assertIn("binding", errors.lower())
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
