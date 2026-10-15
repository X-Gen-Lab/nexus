"""CPU-only assembly admission and atomic bundle regressions."""
import json
from pathlib import Path
import tempfile
import unittest

import runtime


ROOT = Path(__file__).resolve().parents[2]


class RuntimeAssemblyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="CPU runtime ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.facts = self.root / "cpu.json"
        soc = json.loads((ROOT / "soc/native/soc.json").read_text())
        self.hardware = {"schema_version": 1, "cpu": soc["cpu"],
                         "irq": soc["irq"], "clock_hz": 1000000}
        self.facts.write_text(json.dumps(self.hardware) + "\n")
        self.assembly = self.root / "runtime.toml"
        self.write_assembly()
        self.output = self.root / "generated"

    def write_assembly(self, extra=""):
        self.assembly.write_text(
            'schema_version = 1\ncpu_facts = "cpu.json"\n'
            'backend = "baremetal"\noptimization = "Os"\n' + extra)

    def configure(self):
        return runtime.configure(self.assembly, self.output)

    def test_one_toml_resolves_reviewed_facts_into_cpu_bundle(self):
        result = self.configure()
        self.assertEqual(result.profile.arch, "native")
        self.assertEqual(result.backend, "baremetal")
        self.assertEqual(result.optimization, "Os")
        self.assertEqual(len(result.configuration_sha256), 64)
        for name in ("selection.cmake", "nexus_config.h", "resolved-cpu.json",
                     "input_paths.json", ".nexus-runtime-bundle"):
            self.assertTrue((self.output / name).is_file(), name)
        selected = (self.output / "selection.cmake").read_text()
        self.assertIn('set(NEXUS_CPU_ARCH "native")', selected)
        self.assertIn('set(NEXUS_SOC_FAMILY "cpu-runtime")', selected)

    def test_ir_and_input_identities_are_read_only(self):
        result = runtime.resolve(self.assembly)
        with self.assertRaises((AttributeError, TypeError)):
            result.backend = "freertos"
        with self.assertRaises(TypeError):
            result.inputs["assembly"] = "other"

    def test_input_changes_change_identity_without_runtime_defaults(self):
        before = self.configure().configuration_sha256
        self.hardware["clock_hz"] += 1
        self.facts.write_text(json.dumps(self.hardware))
        after = self.configure().configuration_sha256
        self.assertNotEqual(before, after)

    def test_unknown_authored_fields_do_not_override_chip_capabilities(self):
        for extra in ('arch = "cortex-m85"\n', 'fpu = "none"\n',
                      'priority_bits = 8\n', 'worker = true\n'):
            with self.subTest(extra=extra):
                self.write_assembly(extra)
                with self.assertRaisesRegex(runtime.RuntimeError, "unknown"):
                    self.configure()

    def test_backend_optimization_and_schema_are_explicit(self):
        original = self.assembly.read_text()
        for old, new in (('"baremetal"', '"automatic"'), ('"Os"', '"O0"'),
                         ('schema_version = 1', 'schema_version = 2')):
            with self.subTest(new=new):
                self.assembly.write_text(original.replace(old, new))
                with self.assertRaises(runtime.RuntimeError):
                    self.configure()

    def test_duplicate_json_keys_and_unknown_facts_are_rejected(self):
        self.facts.write_text('{"schema_version":1,"schema_version":1}')
        with self.assertRaisesRegex(runtime.RuntimeError, "Duplicate"):
            self.configure()
        self.hardware["automatic_dma"] = True
        self.facts.write_text(json.dumps(self.hardware))
        with self.assertRaisesRegex(runtime.RuntimeError, "unknown"):
            self.configure()

    def test_rejected_reconfiguration_invalidates_owned_old_bundle(self):
        self.configure()
        self.write_assembly('hidden_heap = 1024\n')
        with self.assertRaises(runtime.RuntimeError):
            self.configure()
        self.assertFalse(self.output.exists())

    def test_unowned_output_is_preserved(self):
        self.output.mkdir()
        marker = self.output / "consumer-owned.txt"
        marker.write_text("keep")
        with self.assertRaisesRegex(runtime.RuntimeError, "owned"):
            self.configure()
        self.assertEqual(marker.read_text(), "keep")

    def test_symlink_facts_and_output_are_rejected(self):
        actual = self.root / "actual-cpu.json"
        self.facts.rename(actual)
        self.facts.symlink_to(actual)
        with self.assertRaisesRegex(runtime.RuntimeError, "symlink"):
            self.configure()
        self.facts.unlink()
        actual.rename(self.facts)
        destination = self.root / "consumer"
        destination.mkdir()
        self.output.symlink_to(destination, target_is_directory=True)
        with self.assertRaisesRegex(runtime.RuntimeError, "symlink"):
            self.configure()
        self.assertTrue(destination.is_dir())

    def test_source_parent_and_file_output_are_rejected(self):
        for output in (self.root, self.assembly, self.facts):
            with self.subTest(output=output):
                with self.assertRaises(runtime.RuntimeError):
                    runtime.configure(self.assembly, output)
        self.assertTrue(self.assembly.is_file())
        self.assertTrue(self.facts.is_file())

    def test_bad_clock_fact_has_no_usable_generated_result(self):
        for value in (0, True, -1, 1 << 32, "1000000"):
            with self.subTest(value=value):
                self.hardware["clock_hz"] = value
                self.facts.write_text(json.dumps(self.hardware))
                with self.assertRaises(runtime.RuntimeError):
                    self.configure()
                self.assertFalse(self.output.exists())

    def test_owned_marker_does_not_authorize_deleting_external_facts(self):
        external = self.root / "external"
        external.mkdir()
        source = external / "cpu.json"
        self.facts.rename(source)
        (external / ".nexus-runtime-bundle").write_text("old output")
        self.assembly.write_text(self.assembly.read_text().replace(
            '"cpu.json"', '"external/cpu.json"'))
        with self.assertRaises(runtime.RuntimeError):
            runtime.configure(self.assembly, external)
        self.assertTrue(source.is_file())

    def test_symlink_alias_cannot_erase_owned_directory_containing_facts(self):
        external = self.root / "external"
        external.mkdir()
        source = external / "cpu.json"
        self.facts.rename(source)
        (external / ".nexus-runtime-bundle").write_text("old output")
        (self.root / "alias").symlink_to(external, target_is_directory=True)
        self.assembly.write_text(self.assembly.read_text().replace(
            '"cpu.json"', '"alias/cpu.json"'))
        with self.assertRaises(runtime.RuntimeError):
            runtime.configure(self.assembly, external)
        self.assertTrue(source.is_file())

    def test_valid_bundle_has_only_selected_cpu_runtime_outputs(self):
        self.configure()
        self.assertFalse((self.output / "bindings.c").exists())
        self.assertFalse((self.output / "memory.ld").exists())
        self.assertFalse((self.output / "nexus_factory.h").exists())


if __name__ == "__main__":
    unittest.main()
