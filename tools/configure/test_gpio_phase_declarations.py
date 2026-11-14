"""Cold GPIO composition projects finite records and selected resource facts."""
import copy
import json
import unittest

from tools.configure import configure, gpio_phase
from tools.configure.calls import Symbol
from tools.configure.static_ir import ArrayElementAddress, Declaration


class GpioPhaseDeclarationTests(unittest.TestCase):
    def result(self, name):
        return configure.resolve_ir(configure.ROOT / name)

    def test_selected_pin_and_admission_records_are_finite_typed_declarations(self):
        for name in (
                "tools/configure/assemblies/qiming-baremetal.toml",
                "tests/contracts/stm32_assembly/ve-dma.toml",
                "tests/contracts/multi_controller_assembly/stm32-ve-adc-internal-baremetal.toml"):
            with self.subTest(assembly=name):
                declarations = gpio_phase.declarations(self.result(name))
                self.assertTrue(declarations)
                self.assertTrue(all(isinstance(value, Declaration)
                                    for value in declarations))

    def test_admission_rejects_raw_snapshot_initializer_fragments(self):
        result = self.result("tools/configure/assemblies/qiming-baremetal.toml")
        pin = gpio_phase.selected_pins(result)[0]
        for slot in ("s_nx_slot[0u] + side_effect()", "s_nx_slot[0u]", 1):
            with self.subTest(slot=slot), self.assertRaisesRegex(
                    ValueError, "snapshot.*typed|snapshot.*reference"):
                gpio_phase.declarations(result, {pin: slot})

    def test_existing_typed_snapshot_is_reused_without_duplicate_storage(self):
        result = self.result("tools/configure/assemblies/qiming-baremetal.toml")
        pins = gpio_phase.selected_pins(result)
        slots = {pin: ArrayElementAddress(Symbol("s_nx_selected_slots"), index)
                 for index, pin in enumerate(pins)}
        declarations = gpio_phase.declarations(result, slots)
        self.assertTrue(all(isinstance(value, Declaration)
                            for value in declarations))
        self.assertFalse(any(value.name == "s_nx_platform_pin_state"
                             for value in declarations))
        source = "\n".join(line for value in declarations for line in value.render())
        for slot in slots.values():
            self.assertIn(slot.render(), source)


class GpioPhasePolicyTests(unittest.TestCase):
    def facts(self):
        path = configure.ROOT / "soc/stm32/stm32f4/bindings/gpio_policy.json"
        return json.loads(path.read_text())["domains"]["gpio_phase"]

    def test_output_speed_limits_are_declared_pad_facts_not_mcu_names(self):
        from tools.configure.policies.gpio_phase import GpioPhasePolicy
        facts = self.facts()
        facts["output_speeds"] = {"PAD_IO7": ["low"]}
        policy = GpioPhasePolicy.from_validated(facts)
        facts["output_speeds"]["PAD_IO7"].append("high")
        route = {"kind": "gpio", "pins": [{"pin": "PAD_IO7"}]}
        policy.validate_output({"mode": "output", "speed": "low"}, route)
        with self.assertRaisesRegex(ValueError, "output speed.*PAD_IO7"):
            policy.validate_output({"mode": "output", "speed": "high"}, route)
        policy.validate_output({"mode": "output", "speed": "high"},
                               {"kind": "gpio", "pins": [{"pin": "PC13"}]})
        for invalid in ([], ["preserve"], ["low", "low"], "low"):
            facts["output_speeds"] = {"PAD_IO7": invalid}
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                GpioPhasePolicy.from_validated(facts)

    def test_authored_clock_and_exti_masks_project_without_tool_changes(self):
        from tools.configure.policies.gpio_phase import GpioPhasePolicy
        result = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/qiming-baremetal.toml")
        facts = self.facts()
        facts["controller_gates"]["USART1"] = {
            "apb1_clocks": ["REVIEWED_ALTERNATE_GATE"]}
        policy = GpioPhasePolicy.from_validated(facts)
        declaration = policy.admission_declarations(result, None, 0)[-1]
        fields = dict(declaration.initializer.fields)
        self.assertEqual(fields["apb1_clocks"].values,
                         (Symbol("REVIEWED_ALTERNATE_GATE"),))
        self.assertEqual(fields["apb2_clocks"], 0)
        exti = configure.resolve_ir(configure.ROOT /
            "tests/contracts/stm32_assembly/ve-exti.toml")
        facts["exti_lines"]["EXTI5"] = 1 << 7
        policy = GpioPhasePolicy.from_validated(facts)
        fields = dict(policy.admission_declarations(exti, None, 0)[-1].initializer.fields)
        self.assertEqual(fields["exti_lines"], (1 << 7) | (1 << 6))

    def test_gate_policy_rejects_unknown_types_fields_and_nonconstant_masks(self):
        from tools.configure.policies.gpio_phase import GpioPhasePolicy
        facts = self.facts()
        for change in (
                lambda d: d.update(behavior="write_register()"),
                lambda d: d["types"].update(pin="void*"),
                lambda d: d["controller_gates"]["USART1"].update(unknown=["FLAG"]),
                lambda d: d["controller_gates"]["USART1"].update(
                    apb2_clocks=["FLAG | side_effect()"]),
                lambda d: d["controller_gates"]["USART1"].update(apb2_clocks=[True]),
                lambda d: d["exti_lines"].update(EXTI0=True),
                lambda d: d["exti_lines"].update(EXTI0=3)):
            with self.subTest(change=change):
                changed = copy.deepcopy(facts)
                change(changed)
                with self.assertRaises(ValueError):
                    GpioPhasePolicy.from_validated(changed)


if __name__ == "__main__":
    unittest.main()
