"""One typed decision owns both generation and the exported JSON projection."""
from tools.configure.tests.native_fixture import native_fixture
from dataclasses import FrozenInstanceError, fields, replace
from collections.abc import Mapping
from pathlib import Path
from operator import getitem
import tempfile
import unittest

from tools.configure import configure, emission, factory
from tools.configure.ir import ConfigurationIR, ControllerIR, EndpointIR


class UnifiedIrTests(unittest.TestCase):
    def resolve(self):
        return configure.resolve_ir(configure.ROOT /
            "tests/support/native/assemblies/native.toml", test_support=True)

    def test_resolver_returns_one_typed_authority(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/support/native/assemblies/native.toml", test_support=True)
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
            getitem(result, "board")

    def test_selected_claims_and_option_projection_have_one_owner(self):
        result = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/qiming-baremetal.toml", test_support=native_fixture(configure.ROOT /
            "tools/configure/assemblies/qiming-baremetal.toml"))
        self.assertNotIsInstance(result.claims[0], Mapping)
        uart = next(item for item in result.controllers if item.kind == "uart")
        changed = replace(uart, options=replace(uart.options, baud=57600))
        self.assertEqual(changed.to_dict()["baud"], 57600)
        self.assertNotIsInstance(changed, Mapping)
        self.assertEqual(changed.integration.resources, uart.integration.resources)

    def test_non_gpio_routes_preserve_their_declared_mask_and_safe_level(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/stm32_assembly/zg-pwm.toml")
        pwm = next(item for item in result.controllers if item.kind == "pwm")
        self.assertEqual(pwm.mask, 64)
        self.assertEqual(pwm.to_dict()["mask"], 64)

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
                configure.load_assembly(path, test_support=native_fixture(path))

    def test_serial_flash_endpoint_keeps_one_immutable_geometry_selection(self):
        # This is the typed boundary, not a substitute for exact Board/resource
        # resolution. The same C constructor receives these actual records.
        bus = ControllerIR.create(dict(id="bus", kind="external_bus",
            controller="QUADSPI", binding="control", mode="qspi-indirect-sdr",
            pins=[], route="quadspi-indirect-control", mask=0, initial=0),
            "stm32f4")
        parameters = dict(part="W25Q128JV", prescaler=1, cs_high_cycles=2,
            read_budget_us=1000, prepare_budget_us=2000, stop_budget_us=3000)
        values = dict(id="nor", controller="bus", driver="flash_async-endpoint",
            binding="flash1", mode="nor-indirect", parameters=parameters)
        endpoint = EndpointIR.create(values, (bus,))
        self.assertEqual("QspiFlashOptions", type(endpoint.options).__name__)
        selected = endpoint.options.parameters
        self.assertEqual("QspiParameters", type(selected).__name__)
        self.assertNotIsInstance(selected, Mapping)
        self.assertEqual(set(parameters), {item.name for item in fields(selected)})
        self.assertEqual(values, endpoint.to_dict())
        with self.assertRaises(FrozenInstanceError):
            selected.prescaler = 3


if __name__ == "__main__":
    unittest.main()
