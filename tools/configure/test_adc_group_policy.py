"""Reviewed common ADC mechanism uses package facts instead of an MCU branch."""
import copy
import json
from pathlib import Path
import unittest

from tools.configure import configure as gate

ROOT = Path(__file__).resolve().parents[2]


class AdcGroupPolicyTests(unittest.TestCase):
    def policy(self, *, renamed=False):
        from tools.configure.policies.adc_group import AdcGroupPolicy
        facts = json.loads((ROOT / "soc/stm32/stm32f4/bindings/adc_policy.json").read_text())["domains"]["adc_group"]
        hardware = gate.load(ROOT / "soc/stm32/stm32f4/bindings.json")["hardware"]
        if renamed:
            facts = copy.deepcopy(facts)
            hardware = copy.deepcopy(hardware)
            facts["controller"] = "SAMPLE_COMMON"
            facts["converter_names"] = ["SAMPLE_A", "SAMPLE_B", "SAMPLE_C"]
            for row in hardware["adc_channels"]:
                row["controllers"] = [dict(ADC1="SAMPLE_A", ADC2="SAMPLE_B", ADC3="SAMPLE_C")[name] for name in row["controllers"]]
        return AdcGroupPolicy.from_validated(facts, hardware)

    @staticmethod
    def selection():
        return dict(mode="regular-sequence-dma", operation="dual-simultaneous",
                    channels=[0, 1], sample_times=[7, 7], reference_mv=3300,
                    exclusive=True, irq_priority=5, period_us=1000,
                    trigger="tim3-trgo")

    def test_same_mechanism_uses_renamed_common_and_converter_identities(self):
        policy = self.policy(renamed=True)
        selected = self.selection()
        route = dict(controller="SAMPLE_COMMON", pins=[
            dict(pin="PA0", function="analog", af=0),
            dict(pin="PA1", function="analog", af=0)])
        policy.validate(selected, route)
        self.assertEqual(["PA0", "PA1"], [pin["pin"] for pin in policy.selected_route(selected, route)["pins"]])
        self.assertEqual(("controller:TIM3", "timer:TIM3"), policy.trigger_resources(selected))
        plan = policy.resolve(selected, part={}, clock={"apb2_hz": 84000000, "apb1_hz": 42000000}, clock_plan=None)
        self.assertEqual((1, 2), plan["converters"])
        self.assertEqual((4, 2, 21000000, 84000000), (plan["scan_stride"], plan["sample_stride"], plan["adc_clock_hz"], plan["timer_clock_hz"]))

    def test_interleaved_admission_uses_physical_pad_identity(self):
        policy = self.policy()
        selected = {**self.selection(), "operation": "triple-interleaved",
                    "channels": [4, 4, 4], "sample_times": [7, 7, 7],
                    "trigger": "software", "period_us": 0}
        with self.assertRaisesRegex(ValueError, "physical"):
            policy.validate(selected, {"controller": "ADC123"})

    def test_missing_sample_cycle_or_unreviewed_clock_domain_rejects_facts(self):
        from tools.configure.policies.adc_group import AdcGroupPolicy
        facts = json.loads((ROOT / "soc/stm32/stm32f4/bindings/adc_policy.json").read_text())["domains"]["adc_group"]
        hardware = gate.load(ROOT / "soc/stm32/stm32f4/bindings.json")["hardware"]
        for field, bad in (("sample_cycles", []), ("timer_domains", {"TIM3": "missing"})):
            with self.subTest(field=field):
                with self.assertRaisesRegex(ValueError, "ADC"):
                    AdcGroupPolicy.from_validated({**facts, field: bad}, hardware)

    def test_channel_missing_on_one_converter_reports_an_admission_error(self):
        from tools.configure.policies.adc_group import AdcGroupPolicy
        facts = json.loads((ROOT / "soc/stm32/stm32f4/bindings/adc_policy.json").read_text())["domains"]["adc_group"]
        hardware = copy.deepcopy(gate.load(ROOT / "soc/stm32/stm32f4/bindings.json")["hardware"])
        del hardware["adc_channels"][1]["pins"]["PF5"]
        policy = AdcGroupPolicy.from_validated(facts, hardware)
        selected = {**self.selection(), "operation": "triple-simultaneous",
                    "channels": [0, 1, 15], "sample_times": [7, 7, 7]}
        with self.assertRaisesRegex(ValueError, "physical"):
            policy.validate(selected, {"controller": "ADC123"})

    def test_board_authorization_selects_an_alternate_pad_for_the_same_channel(self):
        from tools.configure.policies.adc_group import AdcGroupPolicy
        facts = json.loads((ROOT / "soc/stm32/stm32f4/bindings/adc_policy.json").read_text())["domains"]["adc_group"]
        hardware = copy.deepcopy(gate.load(ROOT / "soc/stm32/stm32f4/bindings.json")["hardware"])
        hardware["adc_channels"][0]["pins"]["PB12"] = 0
        policy = AdcGroupPolicy.from_validated(facts, hardware)
        selected = self.selection()
        route = dict(controller="ADC123", pins=[
            dict(pin="PB12", function="analog", af=0),
            dict(pin="PA1", function="analog", af=0)])
        policy.validate(selected, route)
        self.assertEqual(["PB12", "PA1"], [pin["pin"] for pin in
            policy.selected_route(selected, route)["pins"]])

    def test_regular_injected_and_combined_modes_keep_their_layout_contracts(self):
        policy = self.policy()
        clock = {"apb1_hz": 42000000, "apb2_hz": 84000000}
        for operation, count, stride in (("dual-simultaneous", 2, 4),
                ("triple-simultaneous", 3, 6),
                ("dual-interleaved", 2, 4), ("triple-interleaved", 3, 12)):
            with self.subTest(operation=operation):
                interleaved = "interleaved" in operation
                selected = {**self.selection(), "operation": operation,
                    "channels": [0] * count if interleaved else list(range(count)),
                    "sample_times": [7] * count,
                    "trigger": "software" if interleaved else "tim3-trgo",
                    "period_us": 0 if interleaved else 1000}
                policy.validate(selected, {"controller": "ADC123"})
                plan = policy.resolve(selected, part={}, clock=clock, clock_plan=None)
                self.assertEqual(stride, plan["scan_stride"])
        for operation, count in (("single", 1), ("dual", 2), ("triple", 3)):
            with self.subTest(injected=operation):
                selected = {**self.selection(), "mode": "injected-sequence",
                    "operation": operation, "channels": list(range(count)),
                    "sample_times": [7] * count, "trigger": "software", "period_us": 0}
                policy.validate(selected, {"controller": "ADC123"})
                self.assertEqual(1, policy.resolve(selected, part={}, clock=clock,
                                                   clock_plan=None)["max_frames"])
        for mode in ("regular-blocks", "regular-auto-injected",
                     "regular-software-injected-blocks", "regular-timer-injected-blocks"):
            with self.subTest(mode=mode):
                selected = {**self.selection(), "mode": mode}
                if mode.endswith("blocks"):
                    selected["max_service_gap_us"] = 1000
                if mode != "regular-blocks":
                    selected["injected"] = dict(channels=[2, 3], sample_times=[7, 7])
                    if mode != "regular-auto-injected":
                        selected["injected"]["min_interval_us"] = 2000
                    if mode == "regular-timer-injected-blocks":
                        selected["injected"].update(trigger="tim4-trgo", delay_us=10)
                policy.validate(selected, {"controller": "ADC123"})
                plan = policy.resolve(selected, part={}, clock=clock, clock_plan=None)
                self.assertEqual((1, 2), plan["converters"])
                if mode == "regular-timer-injected-blocks":
                    self.assertEqual("TIM4", plan["injected_timer"]["timer"])
                    self.assertEqual(("controller:TIM3", "timer:TIM3",
                                      "controller:TIM4", "timer:TIM4"),
                                     policy.trigger_resources(selected))


if __name__ == "__main__":
    unittest.main()
