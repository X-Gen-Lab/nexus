"""Owned configuration transactions, dependencies and incremental contracts."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure as gate
from tools.configure import runtime


class BundleTransactionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="Nexus bundle ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for name in ("dependencies/platform-compatibility.json",
                     "soc/native/soc.json", "soc/native/routes.json",
                     "boards/native_reference/board.json"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(gate.ROOT / name, target)
        self.assembly = self.root / "assembly.json"
        self.authored = {
            "schema_version": 1,
            "board_package": "boards/native_reference",
            "backend": "native", "clock_profile": "model",
            "controllers": [], "devices": [],
            "memory_budgets": {"main_stack_bytes": 2048}, "layout": None}
        self.assembly.write_text(json.dumps(self.authored))
        self.output = self.root / "generated"

    def configure(self):
        return gate.configure(self.assembly, self.output, self.root)

    def timestamps(self):
        return {path.name: (path.read_bytes(), path.stat().st_mtime_ns)
                for path in self.output.iterdir()}

    def test_same_inputs_preserve_every_generated_file_timestamp(self):
        self.configure()
        first = self.timestamps()
        self.configure()
        self.assertEqual(first, self.timestamps())

    def test_owned_marker_never_authorizes_deleting_authored_input(self):
        self.output.mkdir()
        (self.output / ".nexus-config-bundle").write_text("owned")
        authored = self.output / "assembly.json"
        authored.write_bytes(self.assembly.read_bytes())
        expected = authored.read_bytes()
        with self.assertRaisesRegex(gate.ConfigurationError, "input"):
            gate.configure(authored, self.output, self.root)
        self.assertEqual(authored.read_bytes(), expected)

    def test_owned_marker_never_authorizes_deleting_board_package(self):
        package = self.root / "external-board"
        shutil.copytree(self.root / "boards/native_reference", package)
        (package / ".nexus-config-bundle").write_text("owned")
        self.authored["board_package"] = "external-board"
        self.assembly.write_text(json.dumps(self.authored))
        expected = (package / "board.json").read_bytes()
        with self.assertRaisesRegex(gate.ConfigurationError, "input"):
            gate.configure(self.assembly, package, self.root)
        self.assertEqual((package / "board.json").read_bytes(), expected)

    def test_driver_package_is_protected_before_owned_bundle_withdrawal(self):
        package = self.root / "drivers/i2c/i2c_v1_async"
        package.mkdir(parents=True)
        (package / ".nexus-config-bundle").write_text("owned")
        source = package / "i2c_v1_async.c"
        source.write_text("/* Reviewed hardware algorithm. */\n")
        expected = source.read_bytes()
        with self.assertRaisesRegex(gate.ConfigurationError, "input package"):
            gate.configure(self.assembly, package, self.root)
        self.assertEqual(source.read_bytes(), expected)

    def test_alternate_fact_root_cannot_replace_executed_soc_adapter(self):
        code_root = self.root / "generator"
        package = code_root / "soc/native/configure"
        package.mkdir(parents=True)
        (package / ".nexus-config-bundle").write_text("owned")
        source = package / "bindings.py"
        source.write_text("# Reviewed static configuration adapter.\n")
        expected = source.read_bytes()
        with patch.object(gate, "ROOT", code_root):
            with self.assertRaisesRegex(gate.ConfigurationError,
                                        "input package"):
                gate.configure(self.assembly, package, self.root)
        self.assertEqual(source.read_bytes(), expected)

    def test_nested_declared_board_fact_is_protected_before_withdrawal(self):
        package = self.root / "boards/native_reference"
        output = package / "generated"
        output.mkdir()
        (output / ".nexus-config-bundle").write_text("owned")
        fact = output / "reviewed-board-input.txt"
        fact.write_text("Reviewed electrical source, keep byte-for-byte\n")
        expected = fact.read_bytes()
        board_path = package / "board.json"
        board = json.loads(board_path.read_text())
        board["inputs"] = ["generated/reviewed-board-input.txt"]
        board_path.write_text(json.dumps(board))
        with self.assertRaises(gate.ConfigurationError):
            gate.configure(self.assembly, output, self.root)
        self.assertTrue(fact.is_file())
        self.assertEqual(fact.read_bytes(), expected)

    def test_publication_failure_invalidates_old_bundle(self):
        self.configure()
        with patch.object(gate, "generate", side_effect=OSError("disk full")):
            with self.assertRaises(OSError):
                self.configure()
        self.assertFalse(self.output.exists())
        self.assertFalse(list(self.root.glob(".generated.nexus-previous*")))

    def test_configure_emits_complete_code_dependency_and_identity_sidecars(self):
        self.configure()
        paths = json.loads((self.output / "configuration_code_paths.json").read_text())
        identity = json.loads((self.output / "generator-identity.json").read_text())
        self.assertIn("tools/configure/factory.py", paths)
        self.assertIn("tools/configure/bundle.py", paths)
        self.assertIn("tools/configure/emission.py", paths)
        self.assertIn("soc/native/configure/emission.py", paths)
        self.assertNotIn("soc/gd32/gd32f470/configure/emission.py", paths)
        self.assertEqual(set(paths), {item["path"] for item in identity["files"]})
        for record in identity["files"]:
            self.assertEqual(gate.file_digest(Path(paths[record["path"]])),
                             record["sha256"])

    def test_output_parent_symlink_is_rejected_without_mutation(self):
        real = self.root / "real"
        real.mkdir()
        alias = self.root / "alias"
        alias.symlink_to(real, target_is_directory=True)
        with self.assertRaisesRegex(gate.ConfigurationError, "symlink"):
            gate.configure(self.assembly, alias / "generated", self.root)
        self.assertEqual(list(real.iterdir()), [])

    def test_same_output_concurrent_writer_is_rejected_without_damage(self):
        from tools.configure import bundle
        with bundle.transaction(self.output, ".nexus-config-bundle",
                                inputs=(self.assembly,), fail=gate.fail) as pending:
            (pending / ".nexus-config-bundle").write_text("owned")
            command = [sys.executable, "-B", str(gate.ROOT / "tools/configure/configure.py"),
                       "--source-root", str(self.root), "--assembly", str(self.assembly),
                       "--output", str(self.output)]
            process = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(process.returncode, 0)
            self.assertIn("already being configured", process.stderr)
        self.configure()

    def test_interruption_has_no_old_public_bundle_and_next_run_recovers(self):
        self.configure()
        script = ("import os; from tools.configure import configure\n"
                  "configure.generate = lambda *args: os._exit(73)\n"
                  f"configure.configure(__import__('pathlib').Path({str(self.assembly)!r}), "
                  f"__import__('pathlib').Path({str(self.output)!r}), "
                  f"__import__('pathlib').Path({str(self.root)!r}))\n")
        process = subprocess.run([sys.executable, "-B", "-c", script],
                                 env={**os.environ, "PYTHONPATH": str(gate.ROOT)})
        self.assertEqual(process.returncode, 73)
        self.assertFalse(self.output.exists())
        self.configure()
        self.assertTrue((self.output / "nexus_config.h").is_file())

    def test_budget_change_rewrites_only_affected_generated_files(self):
        self.configure()
        before = self.timestamps()
        self.authored["memory_budgets"]["main_stack_bytes"] = 4096
        self.assembly.write_text(json.dumps(self.authored))
        self.configure()
        after = self.timestamps()
        self.assertNotEqual(before["memory.ld"][0], after["memory.ld"][0])
        self.assertEqual(before["nexus_factory.h"], after["nexus_factory.h"])
        self.assertEqual(before["nexus_bindings.h"], after["nexus_bindings.h"])

    def test_instance_error_includes_file_instance_binding_mode_and_range(self):
        authored = self.root / "bad-uart.toml"
        authored.write_text(
            'schema = 2\nboard = "native_reference"\nbackend = "native"\n'
            'clock = "model"\n[memory]\nmain_stack_bytes = 2048\n'
            '[uart.console]\nbinding = "uart0"\nmode = "irq"\nbaud = 1\n'
            '[uart.console.rx]\nformat = "bytes"\ncapacity = 32\n'
            '[uart.console.irq]\npriority = 5\n')
        with self.assertRaises(gate.ConfigurationError) as rejected:
            gate.configure(authored, self.output, self.root)
        detail = str(rejected.exception)
        for expected in (str(authored), "console", "uart0", "irq-byte-event", "1200"):
            self.assertIn(expected, detail)

    def test_configuration_code_changed_during_resolution_is_rejected(self):
        code_root = self.root / "generator"
        paths, _ = gate.dependencies.snapshot(mode="platform")
        for name, source in paths.items():
            target = code_root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        original = gate.resolve_ir
        for module in ("factory", "emission"):
            with self.subTest(module=module):
                def changing(*args, **kwargs):
                    result = original(*args, **kwargs)
                    path = code_root / f"tools/configure/{module}.py"
                    path.write_bytes(path.read_bytes() + b"\n# concurrent edit\n")
                    return result
                self.configure()
                with patch.object(gate.dependencies, "ROOT", code_root):
                    with patch.object(gate, "resolve_ir", side_effect=changing):
                        with self.assertRaisesRegex(ValueError, "code changed"):
                            self.configure()
                self.assertFalse(self.output.exists())

    def test_endpoint_constructor_decisions_are_typed_and_immutable(self):
        self.authored["controllers"] = [{"id": "bus", "binding": "spi0",
                                          "mode": "short-poll", "max_hz": 1000000}]
        self.authored["devices"] = [{"id": "sensor", "controller": "bus",
                                      "driver": "spi-endpoint", "mode": 3,
                                      "max_hz": 1000000, "model_bytes": 32}]
        self.assembly.write_text(json.dumps(self.authored))
        result = gate.resolve_ir(self.assembly, self.root)
        options = result.devices[0].options
        self.assertEqual(options.mode, 3)
        self.assertEqual(options.max_hz, 1000000)
        self.assertEqual(options.model_bytes, 32)
        with self.assertRaises((AttributeError, TypeError)):
            options.mode = 1

    def test_sdk_marker_does_not_bypass_full_byte_verification(self):
        self.configure()
        (self.root / ".nexus-source-sdk.json").write_text(
            '{"schema": 1, "dependencies": []}\n')
        with self.assertRaisesRegex(ValueError, "SDK"):
            self.configure()
        self.assertFalse(self.output.exists())


class RuntimeIncrementalTests(unittest.TestCase):
    def test_runtime_sidecar_includes_factory_and_preserves_timestamps(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            soc = json.loads((gate.ROOT / "soc/native/soc.json").read_text())
            (root / "cpu.json").write_text(json.dumps({
                "schema_version": 1, "cpu": soc["cpu"], "irq": soc["irq"],
                "clock_hz": 1000000}))
            assembly = root / "runtime.toml"
            assembly.write_text('schema_version = 1\ncpu_facts = "cpu.json"\n'
                                'backend = "baremetal"\noptimization = "Os"\n')
            output = root / "generated"
            runtime.configure(assembly, output)
            before = {p.name: (p.read_bytes(), p.stat().st_mtime_ns)
                      for p in output.iterdir()}
            paths = json.loads((output / "configuration_code_paths.json").read_text())
            self.assertIn("tools/configure/factory.py", paths)
            runtime.configure(assembly, output)
            after = {p.name: (p.read_bytes(), p.stat().st_mtime_ns)
                     for p in output.iterdir()}
            self.assertEqual(before, after)

    def test_explicit_step_optimization_profiles(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            soc = json.loads((gate.ROOT / "soc/native/soc.json").read_text())
            (root / "cpu.json").write_text(json.dumps({
                "schema_version": 1, "cpu": soc["cpu"], "irq": soc["irq"],
                "clock_hz": 1000000}))
            assembly, output = root / "runtime.toml", root / "generated"
            for optimization in ("Og", "O0"):
                with self.subTest(optimization=optimization):
                    assembly.write_text(
                        'schema_version = 1\ncpu_facts = "cpu.json"\n'
                        'backend = "baremetal"\noptimization = "' +
                        optimization + '"\n')
                    self.assertEqual(runtime.configure(assembly, output).optimization,
                                     optimization)
            for optimization in ("Og-lto", "O0-lto"):
                assembly.write_text(assembly.read_text().replace(
                    'optimization = "O0"', 'optimization = "' + optimization + '"'))
                with self.assertRaisesRegex(runtime.RuntimeError, "optimization"):
                    runtime.configure(assembly, output)

    def test_runtime_code_changed_during_resolution_is_rejected(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            soc = json.loads((gate.ROOT / "soc/native/soc.json").read_text())
            (root / "cpu.json").write_text(json.dumps({
                "schema_version": 1, "cpu": soc["cpu"], "irq": soc["irq"],
                "clock_hz": 1000000}))
            assembly, output = root / "runtime.toml", root / "generated"
            assembly.write_text('schema_version = 1\ncpu_facts = "cpu.json"\n'
                                'backend = "baremetal"\noptimization = "Os"\n')
            paths, _ = runtime.dependencies.snapshot(mode="runtime")
            code_root = root / "generator"
            for relative, source in paths.items():
                target = code_root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
            original = runtime.resolve
            def changing(*args, **kwargs):
                result = original(*args, **kwargs)
                path = code_root / "tools/configure/factory.py"
                path.write_bytes(path.read_bytes() + b"\n# concurrent edit\n")
                return result
            with patch.object(runtime.dependencies, "ROOT", code_root):
                with patch.object(runtime, "resolve", side_effect=changing):
                    with self.assertRaisesRegex(runtime.RuntimeError, "code changed"):
                        runtime.configure(assembly, output)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
