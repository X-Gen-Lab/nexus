"""DAC ownership follows finite package trigger facts, never alias spelling."""
from dataclasses import FrozenInstanceError
import re
from types import SimpleNamespace
import unittest

from tools.configure import dac


class DacTriggerFactTests(unittest.TestCase):
    @staticmethod
    def author():
        return {"idle": {"kind": "none"}, "manual": {"kind": "software"},
                "tick-alpha": {"kind": "timer", "owner": "TIMER_A"},
                "edge-beta": {"kind": "external_event", "owner": "EVENT_PORT"}}

    def provider(self):
        return SimpleNamespace(DAC_TRIGGERS=dac.trigger_facts(self.author()),
            DAC_PIN_CHANNELS={("OUTPUT_A", "PA4"): 1},
            DAC_DMA_ROUTES={1: ("DMA1:stream5:channel7",)})

    @staticmethod
    def selection(**changes):
        return dict(mode="direct", channels=[1], format="u12-right", idle_codes=[0],
                    output_buffer=True, trigger="idle", **changes)

    @staticmethod
    def route():
        return dict(kind="dac", controller="OUTPUT_A", pins=[{"pin": "PA4"}], resources=[])

    def test_trigger_kind_not_alias_controls_direct_software_and_timer_modes(self):
        provider = self.provider()
        baseline = self.selection()
        dac.validate_selection(baseline, self.route(), provider)
        dac.validate_selection({**baseline, "mode": "triggered", "trigger": "manual"},
                               self.route(), provider)
        selected = {**baseline, "mode": "trigger-dma", "trigger": "tick-alpha",
                    "irq_priority": 5}
        dac.validate_selection(selected, self.route(), provider)
        route = dac.selected_route(selected, self.route(), provider)
        self.assertEqual(["controller:TIMER_A", "timer:TIMER_A"], route["resources"])
        self.assertEqual({"trigger-dma": ["DMA1:stream5:channel7"]}, route["mode_dma"])

    def test_external_owner_and_recovery_timer_are_both_explicit_facts(self):
        provider = self.provider()
        selected = {**self.selection(), "id": "output", "kind": "dac",
            "mode": "trigger-dma-circular", "trigger": "edge-beta",
            "trigger_input": "event-instance", "recovery_trigger": "tick-alpha",
            "irq_priority": 5}
        dac.validate_selection(selected, self.route(), provider)
        route = dac.selected_route(selected, self.route(), provider)
        self.assertEqual(["controller:TIMER_A", "timer:TIMER_A"], route["resources"])
        source = dict(id="event-instance", kind="exti", controller="EVENT_PORT",
                      mode="hardware-event")
        controllers = [selected, source]
        dac.resolve_sources(controllers, triggers=provider.DAC_TRIGGERS)
        self.assertEqual(["event-instance", "output"], [item["id"] for item in controllers])
        for wrong in ({**source, "controller": "OTHER_EVENT"},
                      {**source, "mode": "edge-event"}):
            with self.subTest(source=wrong), self.assertRaises(ValueError):
                dac.resolve_sources([selected, wrong], triggers=provider.DAC_TRIGGERS)

    def test_external_finite_tail_and_non_timer_recovery_keep_their_rejections(self):
        provider = self.provider()
        baseline = {**self.selection(), "trigger": "edge-beta", "trigger_input": "source",
                    "mode": "trigger-dma-circular", "irq_priority": 5}
        for changes in ({"mode": "trigger-dma", "recovery_trigger": "tick-alpha"},
                        {"recovery_trigger": "manual"}, {"recovery_trigger": "missing"}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                dac.validate_selection({**baseline, **changes}, self.route(), provider)

    def test_missing_package_facts_do_not_fall_back_to_a_vendor_trigger_list(self):
        provider = self.provider()
        provider.DAC_TRIGGERS = {}
        with self.assertRaisesRegex(ValueError, "DAC.*trigger"):
            dac.validate_selection({**self.selection(), "trigger": "none"},
                                   self.route(), provider)

    def test_trigger_records_are_frozen_and_do_not_borrow_author_mutations(self):
        author = self.author()
        selected = dac.trigger_facts(author)
        author["tick-alpha"]["owner"] = "OTHER_TIMER"
        self.assertEqual("TIMER_A", selected["tick-alpha"].owner)
        with self.assertRaises(FrozenInstanceError):
            selected["tick-alpha"].owner = "OTHER_TIMER"
        with self.assertRaises(TypeError):
            selected["extra"] = selected["manual"]

    def test_strict_record_fields_kind_and_owner_values_reject_bad_author_data(self):
        invalid = ([], {"bad.alias": {"kind": "timer", "owner": "TIMER_A"}},
            {"timer": {"kind": "timer"}}, {"timer": {"kind": "timer", "owner": None}},
            {"timer": {"kind": "timer", "owner": "not-a-controller"}},
            {"timer": {"kind": "timer", "owner": "TIMER_A", "extra": True}},
            {"manual": {"kind": "software", "owner": "TIMER_A"}},
            {"unknown": {"kind": "unreviewed", "owner": "TIMER_A"}},
            {"unknown": {"kind": False}}, {"unknown": None})
        for value in invalid:
            with self.subTest(value=value), self.assertRaises(ValueError):
                dac.trigger_facts(value)

    def test_editor_accepts_ordinary_aliases_without_mcu_specific_enum(self):
        properties = {}
        dac.editor_schema(properties)
        for field in ("trigger", "recovery_trigger"):
            with self.subTest(field=field):
                self.assertNotIn("enum", properties[field])
                self.assertTrue(re.fullmatch(properties[field]["pattern"], "tick-alpha"))
                self.assertFalse(re.fullmatch(properties[field]["pattern"], "bad.alias"))


if __name__ == "__main__":
    unittest.main()
