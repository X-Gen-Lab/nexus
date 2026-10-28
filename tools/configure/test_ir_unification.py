"""One typed decision owns both generation and the exported JSON projection."""
from dataclasses import fields, replace
from collections.abc import Mapping
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure, emission, factory
from tools.configure.ir import ConfigurationIR


class UnifiedIrTests(unittest.TestCase):
    def resolve(self):
        return configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/native.toml")

    def test_resolver_returns_one_typed_authority(self):
        result = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/native.toml")
        self.assertIsInstance(result, ConfigurationIR)
        self.assertFalse(hasattr(configure, "resolve"))

    def test_named_decisions_are_the_only_serialized_values(self):
        result = self.resolve()
        changed = replace(result, board="reviewed-other-board")
        self.assertEqual(changed.to_dict()["board"], changed.board)
        self.assertNotIn("values", {item.name for item in fields(result)})
        cpu = replace(result.cpu_profile, enum_abi="wide")
        self.assertEqual(cpu.to_dict()["enum_abi"], "wide")
        self.assertNotIn("values", {item.name for item in fields(cpu)})
        changed_cpu = replace(result, cpu_profile=cpu)
        self.assertEqual(changed_cpu.enum_abi, cpu.enum_abi)

    def test_ir_is_not_a_mapping_compatibility_facade(self):
        result = self.resolve()
        self.assertNotIsInstance(result, Mapping)
        self.assertNotIsInstance(result.cpu_profile, Mapping)
        with self.assertRaises(TypeError):
            result["board"]

    def test_selected_claims_and_option_projection_have_one_owner(self):
        result = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/qiming-baremetal.toml")
        self.assertNotIsInstance(result.claims[0], Mapping)
        uart = next(item for item in result.controllers if item.kind == "uart")
        changed = replace(uart, options=replace(uart.options, baud=57600))
        self.assertEqual(changed.to_dict()["baud"], 57600)
        self.assertNotIsInstance(changed, Mapping)
        self.assertEqual(changed.integration.resources, uart.integration.resources)

    def test_non_gpio_routes_preserve_their_declared_mask_and_safe_level(self):
        h743 = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/h743-high-peripherals.toml")
        pwm = next(item for item in h743.controllers if item.kind == "pwm")
        self.assertEqual(pwm.mask, 64)
        gd470 = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/liangshan-baremetal-spi.toml")
        spi = next(item for item in gd470.controllers if item.kind == "spi")
        self.assertEqual(spi.initial, 1)
        self.assertEqual(spi.to_dict()["initial"], 1)

    def test_emitters_reject_exported_mapping_instead_of_reinterpreting_it(self):
        projected = self.resolve().to_dict()
        with self.assertRaises(TypeError):
            factory.emit(projected)
        with tempfile.TemporaryDirectory() as name:
            with self.assertRaises(TypeError):
                emission.emit_bundle(projected, Path(name))
            self.assertEqual(list(Path(name).iterdir()), [])

    def test_json_is_not_an_authored_platform_entry(self):
        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "assembly.json"
            path.write_text("{}", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "TOML"):
                configure.load_assembly(path)


if __name__ == "__main__":
    unittest.main()
