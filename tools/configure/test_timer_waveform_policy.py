"""Waveform policy consumes declared routes instead of a family module."""
from copy import deepcopy
import json
import unittest

from tools.configure.policies.timer_waveform import TimerWaveformPolicy
from tools.configure.tests.f4_fixture import load_f4_fixture


class WaveformPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture = load_f4_fixture("STM32F407ZGT6")

    def policy(self):
        return self.fixture.provider.WAVEFORM

    def facts(self):
        path = self.fixture.package.source_directory / "bindings/waveform_policy.json"
        return json.loads(self.fixture.snapshots[path])["domains"]["waveform"]

    def selection(self, route):
        result = dict(mode=self.policy().modes[route["kind"]], tick_hz=1000000,
            period_ticks=1000, deadtime_ticks=0, exclusive=True, poll_limit=128,
            break_active_high=True, idle_high=False, idle_complement_high=False,
            irq_priority=5, calls_os=False)
        if route["kind"] == "timer_sequence":
            result["burst_budget_hclk_cycles"] = 500
        return result

    def route(self, identity, facts):
        return dict(id=identity, controller=facts["controller"],
            kind=facts["kind"], pins=[dict(pin=pin, function=role, af=alternate)
                                    for pin, role, alternate in facts["pins"]])


    def test_deadtime_budgets_unknown_routes_and_mutation_are_rejected(self):
        policy = self.policy()
        identity, facts = next(iter(policy.routes.items()))
        route, selection = self.route(identity, facts), self.selection(facts)
        for change in ({"deadtime_ticks": 255}, {"exclusive": False},
                       {"calls_os": True}, {"period_ticks": 0}, {"tick_hz": True}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                policy.validate({**selection, **change}, route)
        with self.assertRaises(ValueError):
            policy.validate(selection, {**route, "id": "missing"})
        with self.assertRaises(ValueError):
            policy.resolve_fields({**selection, "burst_budget_hclk_cycles": 168000},
                route, {"core_hz": 168000000, "apb1_hz": 42000000, "apb2_hz": 84000000})

    def test_edges_use_declared_itr_sources_and_detect_cycles(self):
        policy = self.policy()
        selected = [dict(id="lead", kind="pwm_group", controller="TIM1",
                         waveform_resolution={"master_trigger": "enable", "sync_source": None}),
                    dict(id="follow", kind="pwm_group", controller="TIM3",
                         synchronization={"source": "lead", "mode": "trigger"},
                         waveform_resolution={"master_trigger": "update", "sync_source": None})]
        policy.resolve_edges(selected)
        self.assertEqual(selected[1]["waveform_resolution"], {
            "master_trigger": "update", "sync_source": "lead", "sync_mode": "trigger", "itr": 0})
        self.assertIsNone(selected[0]["waveform_resolution"]["sync_source"])
        selected[0]["synchronization"] = {"source": "follow", "mode": "trigger"}
        with self.assertRaisesRegex(ValueError, "cycle"):
            policy.resolve_edges(selected)

    def test_another_controller_layout_is_admitted_by_facts_without_name_inference(self):
        facts = self.facts()
        controller = deepcopy(facts["controllers"]["TIM1"])
        controller["channel_count"] = 8
        facts["controllers"] = {"TIMER_A": controller}
        facts["master_modes"].append("compare6")
        facts["internal_master_modes"] = ["compare6"]
        route = deepcopy(facts["routes"][0])
        route.update(id="reviewed-route", controller="TIMER_A", channels=[6],
            pins=[["P6", "ch6", 0]], internal_trigger=True, default_master="compare6")
        facts["routes"] = [route]
        policy = TimerWaveformPolicy.from_validated(facts)
        selection = dict(mode="coherent-pwm", tick_hz=1000000, period_ticks=1000,
            deadtime_ticks=0, exclusive=True, poll_limit=128, break_active_high=True,
            idle_high=False, idle_complement_high=False, irq_priority=5, calls_os=False)
        selected_route = dict(id="reviewed-route", kind="pwm_group", controller="TIMER_A",
                              pins=[dict(pin="P6", function="ch6", af=0)])
        policy.validate(selection, selected_route)
        self.assertEqual(policy.extra_pins("TIMER_A"), ("P6",))
        self.assertEqual(policy.extra_pins("UNKNOWN"), ())
        controller["channel_count"] = 0
        self.assertEqual(policy.resolve_fields(selection, selected_route, {
            "core_hz": 168000000, "apb2_hz": 84000000})["channels"], (6,))

    def test_malformed_structured_facts_raise_configuration_errors(self):
        original = self.facts()
        for index, mutate in enumerate((
                lambda value: value["master_modes"].append([]),
                lambda value: value["controllers"]["TIM1"]["dma"].append({}),
                lambda value: value["routes"][0].update(channels=[[1]]),
                lambda value: value["routes"][0].update(controller=[]),
                lambda value: value["routes"][0].update(kind=[]))):
            facts = deepcopy(original)
            mutate(facts)
            with self.subTest(mutation=index), self.assertRaises(ValueError):
                TimerWaveformPolicy.from_validated(facts)


if __name__ == "__main__":
    unittest.main()
