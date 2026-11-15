"""Data bindings construct private C objects without family Python code."""
from dataclasses import dataclass, FrozenInstanceError
from types import SimpleNamespace
import unittest


def software():
    return {"enum_abi": "short-enums", "backends": ["baremetal"],
        "headers": ["provider.h"], "bindings": [{
            "kind": "gpio", "mode": "output", "implementation": "gpio",
            "state_type": "gpio_state_t", "config_type": "gpio_config_t",
            "ops": "gpio_ops", "prepare": "gpio_prepare", "close": "gpio_close",
            "fields": [{"name": "registers", "source": "controller", "type": "symbol"},
                       {"name": "mask", "source": "options.mask", "type": "u32"}]}],
        "platform": {name: "platform_" + name for name in (
            "system_start", "resources_acquire", "resources_resume",
            "resources_release", "system_release")}}


class DataProviderTests(unittest.TestCase):
    def provider(self, definition=None, policies=None):
        from tools.configure.data_provider import DataProvider
        package = SimpleNamespace(family="test_family", vendor="test_vendor")
        soc = {"cpu": {"arch": "cortex-m4", "fpu": "fpv4-sp-d16",
                       "float_abi": "hard", "dwt_cyccnt": True},
               "irq": {"priority_bits": 4, "external_count": 82},
               "clock_profiles": {}, "controllers": {}}
        return DataProvider(package, soc, software() if definition is None else definition,
                            policies)

    def test_private_config_is_a_finite_static_record_with_direct_c_lifecycle(self):
        provider = self.provider()
        item = SimpleNamespace(id="enable", kind="gpio", mode="output",
                               controller="GPIOA", options=SimpleNamespace(mask=16))
        selected = provider._construct_layout(item, None, {}, {}, ())
        self.assertFalse(selected.definitions)
        self.assertEqual(selected.state_type, "gpio_state_t")
        self.assertIn(".registers = GPIOA", selected.static_declarations[0].render()[0])
        self.assertEqual(selected.initialize.function, "gpio_prepare")
        self.assertEqual(selected.stop[0].function, "gpio_close")
        self.assertEqual(provider.operations(item), "gpio_ops")
        with self.assertRaises(FrozenInstanceError):
            provider.package = None

    def test_unknown_modes_and_c_fragments_reject(self):
        provider = self.provider()
        with self.assertRaises(ValueError):
            provider.mode_implementation("gpio", "invented")
        item = SimpleNamespace(id="enable", kind="gpio", mode="output",
                               controller="GPIOA; bad()", options=SimpleNamespace(mask=16))
        with self.assertRaises(ValueError):
            provider._construct_layout(item, None, {}, {}, ())

    def test_schema_and_unknown_field_types_do_not_become_code(self):
        value = software()
        value["bindings"][0]["fields"][0]["type"] = "C-expression"
        with self.assertRaises(ValueError):
            self.provider(value)
        value = software()
        value["bindings"][0]["fields"][0]["source"] = "options.__dict__"
        with self.assertRaises(ValueError):
            self.provider(value)

    def test_all_admission_calls_are_fixed_effect_free_c_entries(self):
        value = software()
        value["bindings"][0]["admit"] = "gpio_admit"
        selected = self.provider(value)._construct_layout(SimpleNamespace(
            id="enable", kind="gpio", mode="output", controller="GPIOA",
            options=SimpleNamespace(mask=16)), None, {}, {}, ())
        self.assertEqual(len(selected.admission), 1)
        self.assertEqual(selected.admission[0].render(),
                         "gpio_admit(&s_nx_port_enable, &s_nx_config_enable)")

    def test_static_recipe_uses_selected_layout_and_exact_lifecycle_arguments(self):
        value = software()
        value["bindings"] = [{"kind": "gpio", "mode": "output",
            "implementation": "gpio",
            "static_recipe": {"state_type": "gpio_state_t", "ops": "gpio_ops",
                "declarations": [{"role": "gpio_config", "type": "gpio_config_t",
                    "const": True, "value": {"record": [
                        ["registers", {"source": "item.controller", "as": "symbol"}],
                        ["mask", {"source": "item.options.mask"}]]}}],
                "calls": {"prepare": {"function": "gpio_prepare", "args": [
                    {"ref": "port", "as": "address"},
                    {"ref": "gpio_config", "as": "address"}]},
                    "close": [{"function": "gpio_close", "args": [
                        {"ref": "port", "as": "address"},
                        {"source": "item.options.initial"}]}]},
                "retain_on_failed_init": False}}]
        @dataclass(frozen=True)
        class Options:
            mask: int = 16
            initial: int = 16
        @dataclass(frozen=True)
        class Selected:
            id: str = "enable"
            kind: str = "gpio"
            mode: str = "output"
            controller: str = "GPIOA"
            options: Options = Options()
        item = Selected()
        selected = self.provider(value)._construct_layout(item, None, {}, {}, ())
        self.assertFalse(selected.definitions)
        self.assertIn(".registers = GPIOA", selected.static_declarations[0].render()[0])
        self.assertEqual(selected.initialize.render(),
            "gpio_prepare(&s_nx_port_enable, &s_nx_gpio_config_enable)")
        self.assertEqual(selected.stop[0].render(),
                         "gpio_close(&s_nx_port_enable, 16u)")

    def test_explicit_profile_key_selects_one_layout_without_fallback(self):
        @dataclass(frozen=True)
        class Integration:
            execution_profile: str
        @dataclass(frozen=True)
        class Item:
            integration: Integration
            id: str = "serial"
            kind: str = "uart"
            mode: str = "irq-byte-event"
            controller: str = "SERIAL_A"
        def recipe(state, ops):
            return {"state_type": state, "ops": ops, "declarations": [],
                "calls": {"prepare": {"function": "serial_prepare", "args": []},
                          "close": [{"function": "serial_close", "args": []}]}}
        value = software()
        value["bindings"] = [{"kind": "uart", "mode": "irq-byte-event",
            "implementation": "uart", "headers": ["serial_cold.h"],
            "profile_headers": {"events-default": ["events_only.h"]},
            "profiles": {"bytes-default": recipe("byte_state_t", "byte_ops"),
                         "events-default": recipe("event_state_t", "event_ops")}}]
        provider = self.provider(value)
        item = Item(Integration("events-default"))
        self.assertEqual(provider._construct_layout(item, None, {}, {}, ()).state_type,
                         "event_state_t")
        self.assertEqual(provider.operations(item), "event_ops")
        self.assertIn('#include "serial_cold.h"', provider.includes(
            SimpleNamespace(controllers=(item,))))
        self.assertIn('#include "events_only.h"', provider.includes(
            SimpleNamespace(controllers=(item,))))
        self.assertNotIn('#include "events_only.h"', provider.includes(
            SimpleNamespace(controllers=(Item(Integration("bytes-default")),))))
        with self.assertRaisesRegex(ValueError, "profile"):
            provider._construct_layout(Item(Integration("invented")), None, {}, {}, ())
        value["bindings"][0]["profile_headers"]["invented"] = ["bad.h"]
        with self.assertRaises(ValueError):
            self.provider(value)

    def test_mode_aliases_and_controller_scopes_require_unique_selection(self):
        value = software()
        first = value["bindings"][0]
        first["modes"] = [first.pop("mode"), "input"]
        first["controllers"] = ["GPIOA"]
        second = dict(first, controllers=["GPIOB"], implementation="second_gpio")
        value["bindings"] = [first, second]
        provider = self.provider(value)
        self.assertEqual(provider.mode_implementation("gpio", "input", "GPIOA"), "gpio")
        self.assertEqual(provider.mode_implementation("gpio", "output", "GPIOB"),
                         "second_gpio")
        with self.assertRaisesRegex(ValueError, "controller|Controller"):
            provider.mode_implementation("gpio", "output")
        with self.assertRaises(ValueError):
            provider.mode_implementation("gpio", "output", "GPIOC")
        value["bindings"][1]["controllers"] = ["GPIOA"]
        with self.assertRaisesRegex(ValueError, "[Dd]uplicate|[Aa]mbiguous"):
            self.provider(value)

    def test_package_fragments_reuse_finite_declarations_before_role_validation(self):
        @dataclass(frozen=True)
        class Item:
            id: str = "enable"
            kind: str = "gpio"
            mode: str = "output"
            controller: str = "GPIOA"
        value = software()
        value["static_fragments"] = {"declarations": {"gpio-config": [{
            "role": "config", "type": "gpio_config_t", "const": True,
            "value": {"record": [["mask", 16]]}}]}}
        value["bindings"] = [{"kind": "gpio", "mode": "output",
            "implementation": "gpio", "static_recipe": {
                "state_type": "gpio_state_t", "ops": "gpio_ops",
                "declarations": [{"use": "gpio-config"}],
                "calls": {"prepare": {"function": "gpio_prepare", "args": [
                    {"ref": "config", "as": "address"}]}, "close": []}}}]
        selected = self.provider(value)._construct_layout(Item(), None, {}, {}, ())
        self.assertEqual(selected.static_declarations[0].name, "s_nx_config_enable")
        self.assertIn(".mask = 16u", selected.static_declarations[0].render()[0])

    def test_exact_soc_view_preserves_the_already_validated_binding_snapshot(self):
        provider = self.provider()
        selected = dict(provider.soc)
        selected["irq"] = {"priority_bits": 3, "external_count": 97}
        selected["controllers"] = {"GPIOA": {"interfaces": {"signal": ("output",)}}}
        scoped = provider.for_soc(selected)
        self.assertEqual(scoped.EXTERNAL_IRQ_COUNT, 97)
        self.assertEqual(scoped.IRQ_PRIORITY_BITS, 3)
        self.assertEqual(scoped.CONTROLLER_INTERFACES["GPIOA"]["signal"], ("output",))
        self.assertEqual(provider.EXTERNAL_IRQ_COUNT, 82)
        self.assertIs(scoped.bindings, provider.bindings)
        self.assertIs(scoped.software, provider.software)
        self.assertIs(scoped.domain_policies, provider.domain_policies)

    def test_endpoint_capability_uses_declared_parent_binding_without_instantiation(self):
        @dataclass(frozen=True)
        class External:
            def interface_parent_modes(self, controller, kind, mode):
                return ("async16",) if (controller, kind, mode) == (
                    "BUS_A", "memory_window", "cpu-window") else ()
        @dataclass(frozen=True)
        class Policies:
            EXTERNAL_BUS: External = External()
        value = software()
        value["bindings"][0].update(kind="external_bus", mode="async16",
            implementation="private_memory", controllers=["BUS_A"])
        provider = self.provider(value, Policies())
        self.assertEqual(provider.mode_implementation("memory_window", "cpu-window", "BUS_A"),
                         "private_memory")
        with self.assertRaises(ValueError):
            provider.binding("memory_window", "cpu-window", "BUS_A")
        with self.assertRaises(ValueError):
            provider.mode_implementation("external_bus", "async16", "BUS_B")


if __name__ == "__main__":
    unittest.main()
