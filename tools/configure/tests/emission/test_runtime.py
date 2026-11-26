"""CPU runtime admission, input scopes and atomic bundle regressions."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure, dependencies, runtime
from tools.configure.tests.resources.peripherals.test_cpu_profiles import facts, irq


ROOT = Path(__file__).resolve().parents[4]


class RuntimeAssemblyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="CPU runtime ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.facts = self.root / "cpu.json"
        soc = json.loads((ROOT / "tests/support/native/soc.json").read_text())
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
        for old, new in (('"baremetal"', '"automatic"'), ('"Os"', '"O4"'),
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


PLATFORM_ONLY = (
    "providers/descriptor", "packages", "package_policy", "static_ir",
    "data_provider", "static_recipe", "static_endpoints", "static_fragments",
    "shared_irq", "domain_policies", "domain_bridge", "header_projection",
    "integration_package",
)


class ConfigurationInputScopeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        soc = json.loads((ROOT / "tests/support/native/soc.json").read_text())
        self.hardware = {
            "schema_version": 1, "cpu": soc["cpu"], "irq": soc["irq"],
            "clock_hz": 1000000}
        self.runtime_assembly = self.directory / "runtime.toml"
        self.platform_assembly = self.directory / "platform.toml"
        self.platform_assembly.write_text(
            'schema = 2\nboard = "native_reference"\nbackend = "native"\n'
            'clock = "model"\n[memory]\nmain_stack_bytes = 2048\n')

    def write_runtime(self, backend):
        hardware = (dict(self.hardware, cpu=facts("cortex-m4"), irq=irq("cortex-m4"))
                    if backend == "freertos" else self.hardware)
        (self.directory / "cpu.json").write_text(json.dumps(hardware))
        self.runtime_assembly.write_text(
            'schema_version = 1\ncpu_facts = "cpu.json"\n'
            f'backend = "{backend}"\noptimization = "Os"\n')

    def identity(self, mode, backend="baremetal"):
        output = self.directory / (mode + "-bundle")
        if mode == "runtime":
            self.write_runtime(backend)
            runtime.configure(self.runtime_assembly, output)
        else:
            configure.configure(self.platform_assembly, output, test_support=True)
        return json.loads((output / "generator-identity.json").read_text())

    def mirror_sources(self):
        mirror = self.directory / "generator"
        paths, _ = dependencies.snapshot(mode="platform", family="native",
                                         test_support=True)
        paths.update(dependencies.snapshot(mode="runtime")[0])
        for name, source in paths.items():
            target = mirror / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        return mirror

    def test_runtime_omits_platform_assembly_code_but_retains_imported_factory_code(self):
        names = {row["path"] for row in self.identity("runtime")["files"]}
        for module in PLATFORM_ONLY:
            self.assertNotIn("tools/configure/" + module + ".py", names)
        for module in ("factory", "usb_otg", "usb_host", "stream_profiles",
                       "uart_formats", "contracts", "calls"):
            self.assertIn("tools/configure/" + module + ".py", names)

    def test_threadx_code_is_selected_only_for_its_backend(self):
        path = "tools/configure/threadx.py"
        for mode, backend in (("runtime", "baremetal"), ("platform", "native")):
            with self.subTest(mode=mode):
                names = {row["path"] for row in self.identity(mode, backend)["files"]}
                self.assertNotIn(path, names)
        names = {row["path"] for row in self.identity("runtime", "threadx")["files"]}
        self.assertIn(path, names)

    def test_unselected_code_edit_does_not_change_runtime_generator_identity(self):
        mirror = self.mirror_sources()
        with patch.object(dependencies, "ROOT", mirror):
            before_runtime = self.identity("runtime")
            before_platform = self.identity("platform")
            source = mirror / "tools/configure/data_provider.py"
            source.write_bytes(source.read_bytes() + b"\n# platform engine edit\n")
            self.assertEqual(before_runtime, self.identity("runtime"))
            self.assertNotEqual(before_platform, self.identity("platform"))

    def test_threadx_edit_changes_only_selected_backend_identity(self):
        mirror = self.mirror_sources()
        with patch.object(dependencies, "ROOT", mirror):
            before_native = self.identity("platform")
            before_runtime = self.identity("runtime")
            before_threadx = self.identity("runtime", "threadx")
            source = mirror / "tools/configure/threadx.py"
            source.write_bytes(source.read_bytes() + b"\n# ThreadX policy edit\n")
            self.assertEqual(before_native, self.identity("platform"))
            self.assertEqual(before_runtime, self.identity("runtime"))
            self.assertNotEqual(before_threadx, self.identity("runtime", "threadx"))

    def test_lowpower_code_is_only_an_input_of_the_freertos_backend(self):
        mirror = self.mirror_sources()
        with patch.object(dependencies, "ROOT", mirror):
            before = {backend: self.identity("runtime", backend)
                      for backend in ("baremetal", "freertos", "threadx")}
            before_platform = self.identity("platform")
            for backend, identity in before.items():
                names = {row["path"] for row in identity["files"]}
                self.assertEqual("tools/configure/lowpower.py" in names,
                                 backend == "freertos")
            source = mirror / "tools/configure/lowpower.py"
            source.write_bytes(source.read_bytes() + b"\n# FreeRTOS policy edit\n")
            self.assertEqual(before_platform, self.identity("platform"))
            for backend, identity in before.items():
                changed = self.identity("runtime", backend)
                if backend == "freertos":
                    self.assertNotEqual(identity, changed)
                else:
                    self.assertEqual(identity, changed)

    def test_selected_backend_code_edit_during_resolution_withdraws_old_bundle(self):
        mirror = self.mirror_sources()
        original = runtime.resolve
        with patch.object(dependencies, "ROOT", mirror):
            for backend, module in (("baremetal", "factory"),
                                    ("freertos", "lowpower"), ("threadx", "threadx")):
                with self.subTest(backend=backend):
                    self.identity("runtime", backend)

                    def changing(*args, **kwargs):
                        result = original(*args, **kwargs)
                        source = mirror / ("tools/configure/" + module + ".py")
                        source.write_bytes(source.read_bytes() + b"\n# concurrent edit\n")
                        return result

                    with patch.object(runtime, "resolve", side_effect=changing):
                        with self.assertRaisesRegex(ValueError, "code changed"):
                            runtime.configure(self.runtime_assembly,
                                              self.directory / "runtime-bundle")
                    self.assertFalse((self.directory / "runtime-bundle").exists())

    def test_fresh_process_seals_every_executed_configuration_module(self):
        script = """
import importlib, json, sys
from pathlib import Path
mode, backend, assembly, output, root = sys.argv[1:]
root = Path(root)
module = importlib.import_module('tools.configure.' + mode)
arguments = ['--assembly', assembly, '--output', output]
if mode == 'configure':
    arguments.append('--test-support')
assert module.main(arguments) == 0
paths = json.loads((Path(output) / 'configuration_code_paths.json').read_text())
loaded = {Path(item.__file__).resolve().relative_to(root).as_posix()
          for item in list(sys.modules.values())
          if getattr(item, '__file__', None)
          and Path(item.__file__).resolve().is_relative_to(root)
          and not Path(item.__file__).resolve().is_relative_to(Path(sys.prefix).resolve())
          and Path(item.__file__).suffix == '.py'}
missing = loaded - paths.keys()
assert not missing, sorted(missing)
if backend != 'threadx':
    assert 'tools.configure.threadx' not in sys.modules
if backend != 'freertos':
    assert 'tools.configure.lowpower' not in sys.modules
"""
        for mode, backend in (("runtime", "baremetal"), ("runtime", "freertos"),
                              ("runtime", "threadx"),
                              ("configure", "native")):
            with self.subTest(mode=mode, backend=backend):
                self.write_runtime(backend)
                assembly = (self.runtime_assembly if mode == "runtime" else
                            self.platform_assembly)
                command = subprocess.run([
                    sys.executable, "-B", "-c", script, mode, backend, str(assembly),
                    str(self.directory / (mode + backend)), str(ROOT)],
                    cwd=ROOT, capture_output=True, text=True, timeout=30)
                self.assertEqual(command.returncode, 0, command.stdout + command.stderr)

    def test_sdk_runtime_keeps_the_imported_package_verifier_closure(self):
        names = dependencies.source_names("runtime", None, sdk=True)
        for path in ("tools/configure/packages.py",
                     "tools/configure/providers/descriptor.py",
                     "tools/dev/dependencies.py"):
            self.assertIn(path, names)
        script = """
import importlib.util, sys
from pathlib import Path
from tools.configure import dependencies, runtime
root = Path.cwd()
spec = importlib.util.spec_from_file_location(
    'sdk_verifier', root / 'cmake/package/package_source_sdk.py')
spec.loader.exec_module(importlib.util.module_from_spec(spec))
paths, _ = dependencies.snapshot(mode='runtime', sdk=True)
loaded = {Path(item.__file__).resolve().relative_to(root).as_posix()
          for item in list(sys.modules.values())
          if getattr(item, '__file__', None)
          and Path(item.__file__).resolve().is_relative_to(root)
          and not Path(item.__file__).resolve().is_relative_to(Path(sys.prefix).resolve())
          and Path(item.__file__).suffix == '.py'}
assert not loaded - paths.keys(), sorted(loaded - paths.keys())
"""
        command = subprocess.run([sys.executable, "-B", "-c", script],
                                 cwd=ROOT, capture_output=True, text=True, timeout=30)
        self.assertEqual(command.returncode, 0, command.stdout + command.stderr)

    def test_unknown_backend_scope_keeps_conservative_policy_inputs(self):
        for backend in (None, "future-backend"):
            with self.subTest(backend=backend):
                self.assertIn("tools/configure/threadx.py",
                    dependencies.source_names("runtime", None, backend=backend))
                self.assertIn("tools/configure/lowpower.py",
                    dependencies.source_names("runtime", None, backend=backend))

    def test_narrowed_platform_receipt_preserves_strict_graph_binding(self):
        from tools.evidence.identity import resolved_identity
        from tools.measurement import build_resources

        self.identity("platform")
        output = self.directory / "platform-bundle"
        path = output / "resolved.json"
        selected, _ = resolved_identity(path)
        self.assertEqual(selected["backend"], "native")
        anchor = build_resources.bind(path, allow_reference=False)
        with patch.object(configure, "resolve_ir", side_effect=AssertionError(
                "A bound source-only relink must not replay the resolver")):
            self.assertEqual(build_resources.from_graph(path, anchor,
                             allow_reference=False)[0], selected)
        paths = output / "configuration_code_paths.json"
        identity = output / "generator-identity.json"
        catalog, code = json.loads(paths.read_text()), json.loads(identity.read_text())
        name = "tools/configure/factory.py"
        replacement = self.directory / "replacement.py"
        replacement.write_bytes(Path(catalog[name]).read_bytes())
        catalog[name] = str(replacement)
        paths.write_text(json.dumps(catalog))
        with self.assertRaisesRegex(ValueError, "selected sources"):
            build_resources.bind(path, allow_reference=False)
        del catalog[name]
        paths.write_text(json.dumps(catalog))
        code["files"] = [row for row in code["files"] if row["path"] != name]
        identity.write_text(json.dumps(code))
        with self.assertRaisesRegex(ValueError, "selected sources"):
            build_resources.bind(path, allow_reference=False)

    def test_narrowed_runtime_receipts_retain_independent_resolved_identity(self):
        from tools.evidence.identity import resolved_identity

        for backend in ("baremetal", "freertos", "threadx"):
            with self.subTest(backend=backend):
                code = self.identity("runtime", backend)
                path = self.directory / "runtime-bundle/resolved-cpu.json"
                selected, _ = resolved_identity(path)
                self.assertEqual(selected["cpu_profile"]["backend"], backend)
                self.assertEqual(code["scope"], "runtime")
                selected["cpu_profile"]["backend"] = "changed-backend"
                path.write_text(json.dumps(selected))
                with self.assertRaisesRegex(ValueError, "digest mismatch"):
                    resolved_identity(path)

    def test_narrowed_platform_receipt_rejects_rewritten_resolved_backend(self):
        from tools.evidence.identity import resolved_identity

        self.identity("platform")
        path = self.directory / "platform-bundle/resolved.json"
        selected, _ = resolved_identity(path)
        selected["backend"] = "threadx"
        path.write_text(json.dumps(selected))
        with self.assertRaisesRegex(ValueError, "projection"):
            resolved_identity(path)

    def test_unselected_domain_input_still_rejects_and_withdraws_old_bundle(self):
        self.identity("platform")
        self.platform_assembly.write_text(self.platform_assembly.read_text() +
            '[usb_host.unselected]\nbinding = "missing"\nmode = "invalid"\n'
            'hidden_policy = true\n')
        with self.assertRaisesRegex(ValueError, "unknown"):
            configure.configure(self.platform_assembly,
                                self.directory / "platform-bundle", test_support=True)
        self.assertFalse((self.directory / "platform-bundle").exists())
        for backend, fields, error in (
                ("baremetal", "tick_hz = 1000", "OS policy requires"),
                ("freertos", 'interrupt_mask = "primask"', "unknown"),
                ("threadx", 'lowpower_port = "timebase"', "unknown")):
            with self.subTest(backend=backend):
                self.identity("runtime", backend)
                self.runtime_assembly.write_text(self.runtime_assembly.read_text() +
                                                 "[os]\n" + fields + "\n")
                with self.assertRaisesRegex(ValueError, error):
                    runtime.configure(self.runtime_assembly,
                                      self.directory / "runtime-bundle")
                self.assertFalse((self.directory / "runtime-bundle").exists())


if __name__ == "__main__":
    unittest.main()
