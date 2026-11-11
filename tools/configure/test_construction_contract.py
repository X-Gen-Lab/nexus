"""Providers explicitly own private storage and failed-init cleanup contracts."""
from tools.configure.tests.native_fixture import native_fixture
from dataclasses import FrozenInstanceError, replace
from types import SimpleNamespace
from operator import getitem
import unittest
from unittest.mock import patch

from tools.configure import bindings, configure as gate
from tools.configure.calls import DirectCall, Symbol
from tools.configure.providers import common, descriptor, maintained


class ConstructionContractTests(unittest.TestCase):
    def resolve(self, relative):
        return gate.resolve_ir(gate.ROOT / relative, test_support=native_fixture(gate.ROOT / relative))

    def construction(self, result, item):
        return maintained(result.soc_family, test_support=(result.soc_family == "native")).constructor(
            item, result, result.board_bindings, result.routes,
            result.devices)

    def test_every_family_returns_explicit_immutable_construction(self):
        seen = set()
        for relative in ("tests/support/native/assemblies/native.toml", "tools/configure/assemblies/qiming-baremetal.toml",):
            with self.subTest(relative=relative):
                result = self.resolve(relative)
                seen.add(result.soc_family)
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
                        getitem(construction, "definitions")
                    with self.assertRaises(FrozenInstanceError):
                        construction.state_type = "foreign_state_t"

        self.assertEqual(seen, {item.family for item in descriptor.families()} | {"native"})

    def test_shared_engines_and_secondary_controllers_keep_explicit_types(self):
        for relative in ("multi_controller_assembly/stm32-zg.toml", "multi_controller_assembly/stm32-zg-i2c-pair.toml", "can_assembly/stm32-both.toml", "usb_assembly/stm32-zg.toml", "ethernet_assembly/stm32-zg.toml", "timer_assembly/stm32-zg.toml",):
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
                    self.assertIn(
                        "(nx_lifecycle_acquisition_t){ " +
                        construction.initialize.render() + ", true }", source)

    def test_storage_type_is_provider_owned_without_prefix_inference(self):
        result = self.resolve("tests/support/native/assemblies/native-gpio.toml")
        provider = maintained(result.soc_family, test_support=(result.soc_family == "native"))
        construction = self.construction(result, result.controllers[0])
        construction = replace(construction,
                               state_type="nx_alternate_gpio_state_t")
        explicit = SimpleNamespace(
            MODEL=True, includes=provider.includes,
            state_declarations=provider.state_declarations,
            instance_declarations=provider.instance_declarations,
            operations=provider.operations, shared_irqs=provider.shared_irqs,
            constructor=lambda *args: construction)
        with patch.object(bindings, "for_selection", return_value=explicit):
            source = bindings.emit(result)[1]
        self.assertIn("static nx_alternate_gpio_state_t s_nx_port_led0;",
                      source)

    def test_failed_initializer_is_in_cleanup_only_when_provider_requires(self):
        result = self.resolve("tests/support/native/assemblies/native-gpio.toml")
        provider = maintained(result.soc_family, test_support=(result.soc_family == "native"))
        original = self.construction(result, result.controllers[0])
        for retained in (False, True):
            with self.subTest(retained=retained):
                construction = replace(original,
                                       retain_on_failed_init=retained)
                with patch.object(provider, "constructor",
                                  return_value=construction):
                    source = bindings.emit(result)[1]
                self.assertIn("(nx_lifecycle_acquisition_t){ " +
                              construction.initialize.render() + ", " +
                              str(retained).lower() + " }", source)
                self.assertIn(".acquire = nx_platform_acquire", source)

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
        result = self.resolve("tests/support/native/assemblies/native-gpio.toml")
        original = self.construction(result, result.controllers[0])
        for value in (None, 0, 1, "false"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                replace(original, retain_on_failed_init=value)

    def test_selected_pre_c_owner_uses_only_a_typed_static_initializer(self):
        result = self.resolve("tests/support/native/assemblies/native-gpio.toml")
        original = self.construction(result, result.controllers[0])
        self.assertIsNone(original.initial_state)
        self.assertFalse(original.pre_c_effects)
        marker = Symbol("NX_TEST_PERMANENT_STATE")
        selected = replace(original, initial_state=marker, pre_c_effects=True)
        self.assertIs(selected.initial_state, marker)
        provider = maintained(result.soc_family, test_support=(result.soc_family == "native"))
        with patch.object(provider, "constructor", return_value=selected):
            source = bindings.emit(result)[1]
        self.assertIn("s_nx_port_led0 = NX_TEST_PERMANENT_STATE;", source)
        self.assertIn(".permanent_effects = true", source)
        for invalid in ("{ .boot_owned = true }", {}, 1, True):
            with self.subTest(initial=invalid), self.assertRaises(ValueError):
                replace(original, initial_state=invalid)
        for invalid in (None, 0, 1, "false"):
            with self.subTest(effects=invalid), self.assertRaises(ValueError):
                replace(original, pre_c_effects=invalid)

    def test_admission_calls_are_typed_immutable_and_copied(self):
        result = self.resolve("tests/support/native/assemblies/native-gpio.toml")
        original = self.construction(result, result.controllers[0])
        calls = [DirectCall("nx_test_admit")]
        construction = replace(original, admission=calls)
        calls.clear()
        self.assertEqual(construction.admission, (DirectCall("nx_test_admit"),))
        with self.assertRaises(FrozenInstanceError):
            construction.admission = ()
        for invalid in (None, "nx_test_admit()", ("nx_test_admit()",)):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                replace(original, admission=invalid)

    def test_admission_precedes_owned_effects_and_never_enters_cleanup(self):
        result = self.resolve("tests/support/native/assemblies/native-gpio.toml")
        provider = maintained(result.soc_family, test_support=(result.soc_family == "native"))
        original = self.construction(result, result.controllers[0])
        check = DirectCall("nx_test_admit")
        constructed = replace(original, admission=(check,),
                              retain_on_failed_init=True)
        with patch.object(provider, "constructor", return_value=constructed):
            source = bindings.emit(result)[1]
        start = source[source.index("nx_platform_admit(size_t index)"):
                       source.index("nx_platform_acquire(size_t index)")]
        self.assertIn("case 0u:", start)
        self.assertIn("return " + check.render() + ";", start)
        self.assertIn(".admit = nx_platform_admit", source)
        self.assertNotIn(original.initialize.render(), start)
        self.assertNotIn("nx_platform_lifecycle_stop", start)




class InstanceConstructionIndependenceTests(unittest.TestCase):
    def check_independent(self, kind, removed, selected):
        import tempfile
        from pathlib import Path
        fixture = gate.ROOT / "tests/contracts/multi_controller_assembly"
        original = (fixture / "stm32-ve.toml").read_text().replace(
            'board_package = "stm32-ve-board"',
            'board_package = "' + str(fixture / "stm32-ve-board") + '"')
        retained, discard = [], False
        for line in original.splitlines(keepends=True):
            if line.startswith("["):
                section = line.strip()[1:-1]
                discard = any(section == prefix or section.startswith(prefix + ".")
                              for prefix in removed)
            if not discard:
                retained.append(line)
        with tempfile.TemporaryDirectory() as directory:
            constructions = []
            for name, contents in (("single", "".join(retained)), ("peer", original)):
                path = Path(directory) / (name + ".toml")
                path.write_text(contents)
                result = gate.resolve_ir(path)
                item = next(item for item in result.controllers
                            if item.kind == kind and item.id == selected)
                provider = maintained(result.soc_family)
                constructions.append(provider.constructor(
                    item, result, result.board_bindings, result.routes, result.devices))
            self.assertEqual(constructions[0], constructions[1],
                             "An unrelated controller changed static cold ownership")

    def test_uart_cold_configuration_is_independent_of_a_peer(self):
        self.check_independent("uart", ("uart.secondary_uart",), "primary_uart")

    def test_spi_cold_ownership_is_independent_of_a_peer(self):
        self.check_independent("spi", ("spi.secondary_spi", "spi_device.secondary_sensor"),
                               "primary_spi")


if __name__ == "__main__":
    unittest.main()
