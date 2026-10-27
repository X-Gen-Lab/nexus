"""Providers explicitly own private storage and failed-init cleanup contracts."""
from dataclasses import FrozenInstanceError, replace
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.configure import bindings, configure as gate
from tools.configure.calls import DirectCall
from tools.configure.providers import common, maintained


class ConstructionContractTests(unittest.TestCase):
    def resolve(self, relative):
        return gate.resolve_ir(gate.ROOT / relative)

    def construction(self, result, item):
        return maintained(result.soc_family).constructor(
            item, result, result.board_bindings, result.routes,
            result.devices)

    def test_every_family_returns_explicit_immutable_construction(self):
        for relative in (
                "tools/configure/assemblies/native.toml",
                "tools/configure/assemblies/qiming-baremetal.toml",
                "tools/configure/assemblies/liangshan-baremetal.toml",
                "tests/contracts/g474_assembly/exti.toml",
                "tests/contracts/h743_assembly/dma-cache.toml",
                "tests/contracts/u575_assembly/io.toml"):
            with self.subTest(relative=relative):
                result = self.resolve(relative)
                for item in result.controllers:
                    construction = self.construction(result, item)
                    self.assertIsInstance(construction,
                                          common.Construction)
                    self.assertIsInstance(construction.state_type, str)
                    self.assertIs(type(construction.retain_on_failed_init),
                                  bool)
                    self.assertIsInstance(construction.definitions, tuple)
                    self.assertIsInstance(construction.stop, tuple)
                    self.assertIsInstance(construction.initialize, DirectCall)
                    self.assertTrue(all(isinstance(call, DirectCall)
                                        for call in construction.stop))
                    with self.assertRaises(TypeError):
                        construction["definitions"]
                    with self.assertRaises(FrozenInstanceError):
                        construction.state_type = "foreign_state_t"

    def test_shared_engines_and_secondary_controllers_keep_explicit_types(self):
        for relative in (
                "multi_controller_assembly/stm32-zg.toml",
                "multi_controller_assembly/gd32.toml",
                "multi_controller_assembly/stm32-zg-i2c-pair.toml",
                "multi_controller_assembly/gd32-i2c-pair.toml",
                "can_assembly/stm32-both.toml",
                "can_assembly/gd32-both.toml",
                "usb_assembly/stm32-zg.toml",
                "usb_assembly/gd32.toml",
                "ethernet_assembly/stm32-zg.toml",
                "ethernet_assembly/gd32.toml",
                "timer_assembly/stm32-zg.toml",
                "timer_assembly/gd32.toml"):
            with self.subTest(relative=relative):
                result = self.resolve("tests/contracts/" + relative)
                for item in result.controllers:
                    construction = self.construction(result, item)
                    self.assertIsInstance(construction,
                                          common.Construction)
                    self.assertTrue(construction.retain_on_failed_init)
                    source = bindings.emit(result)[1]
                    name = item.id.replace("-", "_")
                    self.assertIn(
                        f"static {construction.state_type} "
                        f"s_nx_port_{name};", source)
                    before = source.index(
                        f"s_nx_platform_initialized = "
                        f"{result.controllers.index(item) + 1}u;",
                        source.index("nx_platform_start(void)"))
                    initialize = source.index(
                        "result.primary = " + construction.initialize.render())
                    self.assertLess(before, initialize)

    def test_storage_type_is_provider_owned_without_prefix_inference(self):
        result = self.resolve("tools/configure/assemblies/native-gpio.toml")
        provider = maintained(result.soc_family)
        construction = self.construction(result, result.controllers[0])
        construction = replace(construction,
                               state_type="nx_alternate_gpio_state_t")
        explicit = SimpleNamespace(
            MODEL=True, includes=provider.includes,
            state_declarations=provider.state_declarations,
            instance_declarations=provider.instance_declarations,
            operations=provider.operations, shared_irqs=provider.shared_irqs,
            stop_system=provider.stop_system,
            start_system=provider.start_system,
            start_resources=provider.start_resources,
            constructor=lambda *args: construction)
        with patch.object(bindings, "maintained", return_value=explicit):
            source = bindings.emit(result)[1]
        self.assertIn("static nx_alternate_gpio_state_t s_nx_port_led0;",
                      source)

    def test_failed_initializer_is_in_cleanup_only_when_provider_requires(self):
        result = self.resolve("tools/configure/assemblies/native-gpio.toml")
        provider = maintained(result.soc_family)
        original = self.construction(result, result.controllers[0])
        for retained in (False, True):
            with self.subTest(retained=retained):
                construction = replace(original,
                                       retain_on_failed_init=retained)
                with patch.object(provider, "constructor",
                                  return_value=construction):
                    source = bindings.emit(result)[1]
                start = source[source.index("nx_platform_start(void)"):]
                initialize = start.index(
                    "result.primary = " + construction.initialize.render())
                self.assertEqual("s_nx_platform_initialized = 1u;" in
                                 start[:initialize], retained)
                self.assertIn("result.cleanup = "
                              "nx_platform_stop_impl(diagnostic, true);",
                              start[initialize:])

    def test_snapshot_storage_is_copied_and_readonly(self):
        result = self.resolve(
            "tests/contracts/multi_controller_assembly/stm32-zg.toml")
        original = self.construction(result, result.controllers[0])
        snapshots = dict(original.pin_snapshots)
        construction = replace(original, pin_snapshots=snapshots)
        snapshots.clear()
        self.assertEqual(dict(construction.pin_snapshots),
                         dict(original.pin_snapshots))
        with self.assertRaises(TypeError):
            construction.pin_snapshots["PH0"] = "foreign[0]"

    def test_cleanup_responsibility_cannot_be_missing_or_non_boolean(self):
        result = self.resolve("tools/configure/assemblies/native-gpio.toml")
        original = self.construction(result, result.controllers[0])
        for value in (None, 0, 1, "false"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                replace(original, retain_on_failed_init=value)


if __name__ == "__main__":
    unittest.main()
