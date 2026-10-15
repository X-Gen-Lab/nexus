"""Generated identities bind the canonical source-interface metadata bytes."""
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure as gate, cpu, factory, metadata, runtime


class MetadataBindingTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for name in (metadata.POLICY, "soc/native/soc.json", "soc/native/routes.json",
                     "boards/native_reference/board.json"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(gate.ROOT / name, target)
        self.policy_path = self.root / metadata.POLICY
        self.version = json.loads(self.policy_path.read_text())["semantic_version"]
        major, minor, patch_version = map(int, self.version.split("."))
        self.other_version = f"{major}.{minor}.{patch_version + 1}"
        self.assembly = self.root / "assembly.toml"
        self.assembly.write_text('schema=2\nboard="native_reference"\n'
                                 'backend="native"\nclock="model"\n'
                                 '[memory]\nmain_stack_bytes=2048\n')
        facts = json.loads((self.root / "soc/native/soc.json").read_text())
        self.cpu_facts = self.root / "cpu.json"
        self.cpu_facts.write_text(json.dumps({"schema_version": 1,
            "cpu": facts["cpu"], "irq": facts["irq"], "clock_hz": 1000000}))
        self.runtime = self.root / "runtime.toml"
        self.runtime.write_text('schema_version=1\ncpu_facts="cpu.json"\n'
                                'backend="baremetal"\noptimization="Os"\n')

    def change_policy(self, **changes):
        value = json.loads(self.policy_path.read_text())
        value.update(changes)
        self.policy_path.write_text(json.dumps(value))

    def test_owner_observer_is_an_explicit_optional_component(self):
        self.assembly.write_text(self.assembly.read_text().replace(
            '[memory]', 'components=["owner-observer"]\n[memory]'))
        result = gate.resolve_ir(self.assembly, self.root)
        self.assertEqual(result["components"], ("owner-observer",))

    def test_assembly_header_and_input_closure_bind_policy(self):
        output = self.root / "platform"
        result = gate.configure(self.assembly, output, self.root)
        identity = next(record for record in result["inputs"]
                        if record["path"] == metadata.POLICY)
        self.assertEqual(identity["sha256"], hashlib.sha256(
            self.policy_path.read_bytes()).hexdigest())
        header = (output / "nexus_config.h").read_text()
        self.assertIn(f'#define NEXUS_PLATFORM_VERSION "{self.version}"', header)
        self.assertIn('#define NEXUS_FACTORY_ABI_EPOCH 1U', header)
        self.assertEqual(factory.FACTORY_ABI_EPOCH, 1)

    def test_assembly_policy_change_invalidates_configuration_identity(self):
        before = gate.resolve_ir(self.assembly, self.root)["configuration_sha256"]
        self.change_policy(semantic_version=self.other_version)
        result = gate.configure(self.assembly, self.root / "changed", self.root)
        self.assertNotEqual(before, result["configuration_sha256"])
        self.assertIn(f'#define NEXUS_PLATFORM_VERSION "{self.other_version}"',
                      (self.root / "changed/nexus_config.h").read_text())

    def test_unsupported_factory_epoch_rejects_owned_assembly_bundle(self):
        output = self.root / "platform"
        gate.configure(self.assembly, output, self.root)
        self.change_policy(factory_abi_epoch=2)
        with self.assertRaisesRegex(ValueError, "factory epoch"):
            gate.configure(self.assembly, output, self.root)
        self.assertFalse(output.exists())

    def test_owned_output_marker_cannot_authorize_policy_input_deletion(self):
        directory = self.policy_path.parent
        original = self.policy_path.read_bytes()
        (directory / ".nexus-config-bundle").write_text("old bundle")
        with self.assertRaises(ValueError):
            gate.configure(self.assembly, directory, self.root)
        self.assertTrue(self.policy_path.is_file())
        self.assertEqual(self.policy_path.read_bytes(), original)

    def test_runtime_uses_same_policy_snapshot_and_version_macros(self):
        output = self.root / "runtime-output"
        with patch.object(runtime, "ROOT", self.root, create=True):
            before = runtime.configure(self.runtime, output)
            self.assertEqual(before.inputs["platform_metadata"], str(self.policy_path))
            self.assertIn(str(self.policy_path), before.snapshots)
            header = (output / "nexus_config.h").read_text()
            self.assertIn(f'#define NEXUS_PLATFORM_VERSION "{self.version}"', header)
            self.assertIn('#define NEXUS_FACTORY_ABI_EPOCH 1U', header)
            self.change_policy(semantic_version=self.other_version)
            after = runtime.configure(self.runtime, output)
            self.assertNotEqual(before.input_sha256, after.input_sha256)
            self.assertNotEqual(before.configuration_sha256,
                                after.configuration_sha256)

    def test_runtime_epoch_mismatch_invalidates_owned_bundle(self):
        output = self.root / "runtime-output"
        with patch.object(runtime, "ROOT", self.root, create=True):
            runtime.configure(self.runtime, output)
            self.change_policy(factory_abi_epoch=2)
            with self.assertRaisesRegex(ValueError, "factory epoch"):
                runtime.configure(self.runtime, output)
        self.assertFalse(output.exists())

    def test_assembly_policy_change_during_emission_is_rejected(self):
        output = self.root / "platform"
        original = gate.generate

        def changing(result, directory):
            original(result, directory)
            self.change_policy(semantic_version=self.other_version)

        with patch.object(gate, "generate", changing):
            with self.assertRaisesRegex(ValueError, "changed"):
                gate.configure(self.assembly, output, self.root)
        self.assertFalse(output.exists())

    def test_runtime_policy_change_during_emission_is_rejected(self):
        output = self.root / "runtime-output"
        original = cpu.emit_profile

        def changing(*args, **kwargs):
            result = original(*args, **kwargs)
            self.change_policy(semantic_version=self.other_version)
            return result

        with patch.object(runtime, "ROOT", self.root, create=True):
            with patch.object(cpu, "emit_profile", changing):
                with self.assertRaisesRegex(ValueError, "changed"):
                    runtime.configure(self.runtime, output)
        self.assertFalse(output.exists())

    def test_direct_cpu_cli_emits_and_binds_policy(self):
        output = self.root / "cpu-output"
        args = ['--facts', str(self.cpu_facts), '--backend', 'baremetal',
                '--output', str(output)]
        with patch.object(cpu, "ROOT", self.root, create=True):
            self.assertEqual(cpu.main(args), 0)
            before = json.loads((output / "resolved-cpu.json").read_text())
            self.assertIn('#define NEXUS_FACTORY_ABI_EPOCH 1U',
                          (output / "nexus_config.h").read_text())
            self.change_policy(semantic_version=self.other_version)
            self.assertEqual(cpu.main(args), 0)
            after = json.loads((output / "resolved-cpu.json").read_text())
            self.assertNotEqual(before['input_sha256'], after['input_sha256'])


if __name__ == '__main__':
    unittest.main()
