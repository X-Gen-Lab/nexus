"""Selected artifacts remain bound to full configuration and declared bytes."""
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure, integration_package, runtime
from tools.evidence.common import EvidenceError, file_identity
from tools.evidence.identity import resolved_identity


class ResolvedIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.assembly = self.root / "assembly.toml"
        original = configure.ROOT / (
            "tools/configure/assemblies/qiming-baremetal-empty.toml")
        self.assembly.write_bytes(original.read_bytes())
        output = self.root / "generated"
        self.full = configure.configure(self.assembly, output)
        self.path = output / "resolved.json"
        self.selected = json.loads(self.path.read_text())

    def write(self, value):
        self.path.write_text(json.dumps(value))

    def test_selected_file_preserves_full_configuration_and_file_identity(self):
        selected, artifact = resolved_identity(self.path)
        self.assertEqual(selected, self.selected)
        self.assertEqual(selected["schema_version"], 2)
        self.assertEqual(selected["configuration_sha256"],
                         self.full["configuration_sha256"])
        self.assertEqual(artifact, file_identity(self.path))

    def test_selected_fields_cannot_reuse_a_valid_full_configuration_digest(self):
        self.selected["clock"]["core_hz"] -= 1
        self.write(self.selected)
        with self.assertRaisesRegex(EvidenceError, "projection"):
            resolved_identity(self.path)

    def test_unknown_configuration_digest_is_rejected(self):
        self.selected["configuration_sha256"] = "0" * 64
        self.write(self.selected)
        with self.assertRaisesRegex(EvidenceError, "configuration"):
            resolved_identity(self.path)

    def test_changed_input_is_rejected_before_resolving(self):
        self.assembly.write_text(self.assembly.read_text() + "\n# changed\n")
        with patch.object(configure, "resolve_ir") as resolve:
            with self.assertRaisesRegex(EvidenceError, "input changed"):
                resolved_identity(self.path)
        resolve.assert_not_called()

    def test_output_cannot_enable_legacy_provider_execution(self):
        from tools.configure.tests.test_external_package import ExternalPackageTests

        fixture = ExternalPackageTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        _, assembly = fixture.package()
        output = self.root / "legacy"
        configure.configure(assembly, output, test_support=True)
        path = output / "resolved.json"
        selected = json.loads(path.read_text())
        selected["soc_family"] = "native"
        path.write_text(json.dumps(selected))
        with patch.object(integration_package, "_load_provider",
                          side_effect=AssertionError("legacy provider executed")) as load:
            with self.assertRaises(EvidenceError):
                resolved_identity(path)
        load.assert_not_called()

    def test_builtin_native_with_external_json_board_remains_supported(self):
        board = self.root / "native-board"
        board.mkdir()
        shutil.copyfile(configure.ROOT / "tests/support/native/board/board.json",
                        board / "board.json")
        assembly = self.root / "native.toml"
        assembly.write_text('schema=2\nboard_package="native-board"\n'
                            'backend="native"\nclock="model"\n'
                            '[memory]\nmain_stack_bytes=2048\n')
        output = self.root / "native"
        expected = configure.configure(assembly, output, test_support=True)
        selected, _ = resolved_identity(output / "resolved.json")
        self.assertEqual(selected["configuration_sha256"],
                         expected["configuration_sha256"])

    def test_native_intent_replaced_before_resolution_cannot_load_legacy_code(self):
        from tools.configure.tests.test_external_package import ExternalPackageTests

        fixture = ExternalPackageTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        _, assembly = fixture.package()
        legacy = assembly.read_bytes()
        assembly.write_text('schema=2\nboard="native_reference"\n'
                            'backend="native"\nclock="model"\n'
                            '[memory]\nmain_stack_bytes=2048\n')
        output = self.root / "native-race"
        configure.configure(assembly, output, test_support=True)
        read = configure.load_assembly
        replaced = False

        def replace_after_read(*args, **kwargs):
            nonlocal replaced
            intent = read(*args, **kwargs)
            if not replaced:
                assembly.write_bytes(legacy)
                replaced = True
            return intent

        with patch.object(configure, "load_assembly", side_effect=replace_after_read):
            with patch.object(integration_package, "_load_provider",
                              side_effect=AssertionError("legacy provider executed")) as load:
                with self.assertRaises(EvidenceError):
                    resolved_identity(output / "resolved.json")
        load.assert_not_called()

    def check_mutation_after_resolution(self, mutate, reason):
        resolve = configure.resolve_ir

        def change(*args, **kwargs):
            selected = resolve(*args, **kwargs)
            mutate()
            return selected

        with patch.object(configure, "resolve_ir", side_effect=change):
            with self.assertRaisesRegex(EvidenceError, reason):
                resolved_identity(self.path)

    def test_input_changed_after_resolution_is_rejected(self):
        def change():
            self.assembly.write_text(self.assembly.read_text() + "\n# changed\n")

        self.check_mutation_after_resolution(change, "input changed")

    def test_artifact_changed_after_resolution_is_rejected(self):
        def change():
            selected = json.loads(self.path.read_text())
            selected["clock"]["core_hz"] -= 1
            self.write(selected)

        self.check_mutation_after_resolution(change, "artifact changed")

    def test_navigation_changed_after_resolution_is_rejected(self):
        alternate = self.root / "alternate.toml"
        alternate.write_bytes(self.assembly.read_bytes())
        navigation = self.path.parent / "input_paths.json"

        def change():
            paths = json.loads(navigation.read_text())
            paths["external/assembly.toml"] = str(alternate)
            navigation.write_text(json.dumps(paths))

        self.check_mutation_after_resolution(change, "navigation changed")

    def test_cpu_runtime_retains_its_independent_schema_one_identity(self):
        facts = json.loads((configure.ROOT /
                            "tests/support/native/soc.json").read_text())
        cpu = self.root / "cpu.json"
        cpu.write_text(json.dumps({"schema_version": 1, "cpu": facts["cpu"],
                                   "irq": facts["irq"], "clock_hz": 1000000}))
        assembly = self.root / "runtime.toml"
        assembly.write_text('schema_version=1\ncpu_facts="cpu.json"\n'
                            'backend="baremetal"\noptimization="Os"\n')
        output = self.root / "runtime"
        expected = runtime.configure(assembly, output)
        selected, _ = resolved_identity(output / "resolved-cpu.json")
        self.assertEqual(selected["schema_version"], 1)
        self.assertEqual(selected["configuration_sha256"],
                         expected.configuration_sha256)

    def test_unknown_and_boolean_schema_versions_are_rejected(self):
        for version in (True, False, 0, 3, "2"):
            with self.subTest(version=version):
                self.write({**self.selected, "schema_version": version})
                with self.assertRaisesRegex(EvidenceError, "schema"):
                    resolved_identity(self.path)


if __name__ == "__main__":
    unittest.main()
