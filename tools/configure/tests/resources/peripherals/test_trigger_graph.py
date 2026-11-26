"""Hardware execution cycles and startup dependencies are separate."""

import json
import unittest

from tools.configure import trigger_graph as graph


def node(name, **options):
    return {"id": name, "owner": "control", "clock_domain": "apb",
            "events": ["done"], "actions": ["start", "stop"],
            "stop_actions": ["stop"], **options}


def edge(source, target, **options):
    return {"source": source, "event": "done", "target": target,
            "action": "start", "route": source + "-" + target, **options}


class TriggerGraphTests(unittest.TestCase):
    def test_startup_order_is_independent_of_runtime_edges(self):
        plan = graph.resolve([node("timer"), node("adc"), node("dma")],
                             [edge("timer", "adc"), edge("adc", "dma")],
                             startup=[("dma", "adc"), ("adc", "timer")])
        self.assertEqual(plan.startup_order, ("dma", "adc", "timer"))
        self.assertEqual(len(plan.edges), 2)

    def test_undeclared_cycle_rejects(self):
        with self.assertRaisesRegex(ValueError, "cycle"):
            graph.resolve([node("a"), node("b")],
                          [edge("a", "b"), edge("b", "a")])

    def test_explicit_autonomous_cycle_has_stop_owner_and_gate(self):
        plan = graph.resolve([node("a"), node("b")],
                             [edge("a", "b"), edge("b", "a")],
                             loops=[{"nodes": ["a", "b"],
                                     "stop_owner": "control",
                                     "stop_node": "a", "stop_action": "stop"}])
        self.assertEqual(plan.loops[0].stop_owner, "control")

    def test_declared_loop_must_have_real_stoppable_owned_node(self):
        base = {"nodes": ["a", "b"], "stop_owner": "control",
                "stop_node": "a", "stop_action": "stop"}
        for override in ({"stop_owner": "other"}, {"stop_node": "missing"},
                         {"stop_action": "missing"}, {"nodes": ["a"]}):
            with self.subTest(override=override), self.assertRaises(ValueError):
                graph.resolve([node("a"), node("b")],
                              [edge("a", "b"), edge("b", "a")],
                              loops=[{**base, **override}])

    def test_startup_cycle_always_rejects(self):
        with self.assertRaisesRegex(ValueError, "startup"):
            graph.resolve([node("a"), node("b")], [],
                          startup=[("a", "b"), ("b", "a")])

    def test_unknown_event_action_node_and_route_reject(self):
        for bad in (edge("a", "missing"), edge("a", "b", event="missing"),
                    edge("a", "b", action="missing"), edge("a", "b", route=""),
                    edge("a", "b", typo=True)):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                graph.resolve([node("a"), node("b")], [bad])

    def test_clock_crossing_requires_explicit_route_fact(self):
        nodes = [node("a"), node("b", clock_domain="ahb")]
        with self.assertRaises(ValueError):
            graph.resolve(nodes, [edge("a", "b")])
        graph.resolve(nodes, [edge("a", "b", cross_domain=True)])
        with self.assertRaises(ValueError):
            graph.resolve(nodes, [edge("a", "b", cross_domain=1)])

    def test_duplicate_routes_and_nodes_reject(self):
        with self.assertRaises(ValueError):
            graph.resolve([node("a"), node("a")], [])
        with self.assertRaises(ValueError):
            graph.resolve([node("a"), node("b")],
                          [edge("a", "b"), edge("a", "b")])

    def test_unused_loop_declaration_rejects(self):
        with self.assertRaises(ValueError):
            graph.resolve([node("a")], [], loops=[{
                "nodes": ["a"], "stop_owner": "control", "stop_node": "a",
                "stop_action": "stop"}])

    def test_start_action_cannot_claim_to_stop_a_loop(self):
        with self.assertRaises(ValueError):
            graph.resolve([node("a")], [edge("a", "a")], loops=[{
                "nodes": ["a"], "stop_owner": "control", "stop_node": "a",
                "stop_action": "start"}])

    def test_stop_gate_must_interrupt_every_cycle_in_component(self):
        with self.assertRaises(ValueError):
            graph.resolve([node("a"), node("b"), node("c")],
                          [edge("a", "b"), edge("b", "a"),
                           edge("b", "c"), edge("c", "b")], loops=[{
                              "nodes": ["a", "b", "c"], "stop_owner": "control",
                              "stop_node": "a", "stop_action": "stop"}])

    def test_loop_stop_owner_cannot_borrow_another_owners_event_state(self):
        with self.assertRaises(ValueError):
            graph.resolve([node("a"), node("b", owner="other")],
                          [edge("a", "b"), edge("b", "a")], loops=[{
                              "nodes": ["a", "b"], "stop_owner": "control",
                              "stop_node": "a", "stop_action": "stop"}])

    def test_malformed_startup_dependencies_are_configuration_errors(self):
        with self.assertRaises(ValueError):
            graph.resolve([node("a")], [], startup=[([], "a")])

    def test_selected_f407_adc_events_keep_owned_start_and_stop_order(self):
        selected = [{"id": "samples", "kind": "adc", "mode": "trigger-dma",
                     "controller": "ADC2", "trigger": "tim5-ch2",
                     "dma": ["DMA2:stream2:channel1"]}]
        plan = graph.resolve_fixed(selected, adc_triggers={"tim5-ch2": "TIM5"})
        self.assertEqual(len(plan.edges), 2)
        self.assertEqual(plan.edges[0].event, "compare2")
        self.assertEqual(plan.edges[0].action, "sample")
        self.assertEqual(plan.startup_order[-1], "samples:trigger")
        self.assertEqual(len({item.clock_domain for item in plan.nodes}), 3)
        self.assertTrue(all(item.cross_domain for item in plan.edges))

    def test_fixed_non_trigger_modes_have_no_invented_event_graph(self):
        selected = [{"id": "uart", "kind": "uart", "mode": "dma-tx"}]
        self.assertEqual(graph.resolve_fixed(selected).edges, ())

    def test_validated_json_projection_freezes_sequences(self):
        plan = graph.resolve([node("a")], [edge("a", "a")], loops=[{
            "nodes": ["a"], "stop_owner": "control", "stop_node": "a",
            "stop_action": "stop"}])
        values = json.loads(json.dumps(plan.to_dict()))
        self.assertEqual(plan.to_dict(), values)
        frozen = graph.TriggerPlanIR.from_validated(values)
        values["nodes"][0]["actions"].append("changed")
        values["loops"][0]["nodes"].append("changed")
        self.assertEqual(frozen, plan)


if __name__ == "__main__":
    unittest.main()
