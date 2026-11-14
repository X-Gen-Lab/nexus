"""Common ADC event metadata follows the admitted converter identity."""
import copy
import unittest

from tools.configure import trigger_graph as graph


def selected(mode="regular-sequence-dma", trigger="pulse-trgo"):
    return {"id": "samples", "kind": "adc_group", "controller": "COMMON_A",
            "mode": mode, "trigger": trigger,
            "dma": ["ENGINE_A:stream3:channel0"]}


class AdcGroupTriggerIdentityTests(unittest.TestCase):
    def test_regular_event_uses_declared_converter_timer_and_dma_engine(self):
        plan = graph.resolve_fixed([selected()],
            adc_triggers={"pulse-trgo": "COUNTER_A"},
            adc_group_converter="SAMPLE_A")
        self.assertEqual(tuple(node.clock_domain for node in plan.nodes),
                         ("controller:SAMPLE_A", "controller:COUNTER_A",
                          "engine:ENGINE_A"))
        self.assertEqual(tuple(edge.route for edge in plan.edges),
                         ("COUNTER_A:update:SAMPLE_A:regular-sequence-dma",
                          "ENGINE_A:stream3:channel0"))
        self.assertEqual(plan.startup_order,
                         ("samples:dma", "samples:adc", "samples:trigger"))

    def test_regular_and_injected_events_share_admitted_converter(self):
        item = selected("regular-timer-injected-blocks")
        item["injected"] = {"trigger": "kick-ch4"}
        plan = graph.resolve_fixed([item],
            adc_triggers={"pulse-trgo": "COUNTER_A"},
            adc_injected_triggers={"kick-ch4": "COUNTER_B"},
            adc_group_converter="SAMPLE_A")
        self.assertEqual(tuple(node.clock_domain for node in plan.nodes
                               if node.id in {"samples:adc", "samples:injected"}),
                         ("controller:SAMPLE_A", "controller:SAMPLE_A"))
        self.assertEqual(plan.edges[1].route,
                         "COUNTER_B:compare4:SAMPLE_A:injected")
        self.assertLess(plan.startup_order.index("samples:injected"),
                        plan.startup_order.index("samples:injected-trigger"))

    def test_auto_injected_sequence_keeps_same_converter_and_owned_stop_order(self):
        plan = graph.resolve_fixed(
            [selected("regular-auto-injected", "software")],
            adc_group_converter="SAMPLE_A")
        self.assertEqual(plan.edges[0].route,
                         "SAMPLE_A:regular:JAUTO:injected")
        self.assertFalse(plan.edges[0].cross_domain)
        self.assertEqual(plan.nodes[0].events,
                         ("sample-ready", "sequence-complete"))
        self.assertEqual(plan.startup_order,
                         ("samples:dma", "samples:injected", "samples:adc"))

    def test_missing_converter_rejects_without_mutating_selected_graph(self):
        items = [selected("injected-sequence", "software")]
        before = copy.deepcopy(items)
        with self.assertRaisesRegex(ValueError, "converter"):
            graph.resolve_fixed(items)
        self.assertEqual(items, before)

    def test_converter_requires_exact_c_identifier(self):
        for invalid in (None, "", 7, "bad-name", "SAMPLE_A;", "struct", "a.b"):
            with self.subTest(invalid=invalid), self.assertRaisesRegex(
                    ValueError, "converter"):
                graph.resolve_fixed([selected("injected-sequence", "software")],
                                    adc_group_converter=invalid)

    def test_existing_f4_converter_keeps_exact_reviewed_event_route(self):
        plan = graph.resolve_fixed([selected()],
            adc_triggers={"pulse-trgo": "TIM3"}, adc_group_converter="ADC1")
        self.assertEqual(plan.edges[0].route,
                         "TIM3:update:ADC1:regular-sequence-dma")
        self.assertEqual(plan.nodes[0].clock_domain, "controller:ADC1")


if __name__ == "__main__":
    unittest.main()
