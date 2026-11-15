"""Compile-time resource claims retain exclusive defaults and exact sharing."""

import json
import unittest

from tools.configure import resource_graph as graph


def claim(key, owner="control", **options):
    return {"key": key, "owner": owner, "mode": "test", "source": "model",
            **options}


class ResourceGraphTests(unittest.TestCase):
    def test_legacy_claims_remain_exclusive(self):
        with self.assertRaisesRegex(ValueError, "conflict"):
            graph.resolve([claim("controller:TIM3"),
                           claim("controller:TIM3", "other")])

    def test_conflicts_retain_both_canonical_claims_for_diagnostics(self):
        cases = (
            ([claim("controller:TIM3", "owner-a", source="route-a"),
              claim("controller:TIM3/channel/1", "owner-b", source="route-b")], {}),
            ([claim("controller:TIM3", "owner-a", source="route-a"),
              claim("controller:TIM3", "owner-b", source="route-b")], {}),
            ([claim("controller:UART", "owner-a", source="route-a",
                    personality="uart"),
              claim("controller:SPI", "owner-b", source="route-b",
                    personality="spi")],
             {"controller:UART": "ip:FLEXCOMM0", "controller:SPI": "ip:FLEXCOMM0"}),
        )
        for claims, aliases in cases:
            with self.subTest(claims=claims), self.assertRaises(ValueError) as error:
                graph.resolve(claims, aliases=aliases)
            details = error.exception.details
            self.assertEqual(details["code"], "resource_conflict")
            self.assertEqual([item["owner"] for item in details["claims"]],
                             ["owner-a", "owner-b"])
            self.assertEqual([item["source"] for item in details["claims"]],
                             ["route-a", "route-b"])

    def test_parent_claim_blocks_other_owner_child(self):
        for records in ([claim("controller:TIM3"),
                         claim("controller:TIM3/channel/1", "other")],
                        [claim("controller:TIM3/channel/1", "other"),
                         claim("controller:TIM3")]):
            with self.subTest(records=records), self.assertRaises(ValueError):
                graph.resolve(records)

    def test_distinct_channels_and_same_owner_parent_are_valid(self):
        plan = graph.resolve([claim("controller:TIM3/channel/1"),
                              claim("controller:TIM3/channel/2", "other")])
        self.assertEqual(len(plan.claims), 2)
        graph.resolve([claim("controller:TIM3"),
                       claim("controller:TIM3/channel/1")])

    def test_timer_base_requires_explicit_fact_and_identical_config(self):
        first = claim("controller:TIM3/base", policy="compatible",
                      attributes={"tick_hz": 1000000, "period_ticks": 1000})
        second = {**first, "owner": "other"}
        shared = {"controller:TIM3/base": ("period_ticks", "tick_hz")}
        plan = graph.resolve([first, second], shared=shared)
        self.assertEqual(plan.claims[0].attributes,
                         (("period_ticks", 1000), ("tick_hz", 1000000)))
        with self.assertRaises(ValueError):
            graph.resolve([first, second])
        for attrs in ({"tick_hz": 999999, "period_ticks": 1000},
                      {"tick_hz": 1000000},
                      {"tick_hz": 1000000, "period_ticks": 1000, "extra": 1}):
            with self.subTest(attrs=attrs), self.assertRaises(ValueError):
                graph.resolve([first, {**second, "attributes": attrs}],
                              shared=shared)

    def test_boolean_is_not_an_integer_configuration_match(self):
        shared = {"controller:T/base": ("value",)}
        with self.assertRaises(ValueError):
            graph.resolve([claim("controller:T/base", policy="compatible",
                                 attributes={"value": 1}),
                           claim("controller:T/base", "other",
                                 policy="compatible",
                                 attributes={"value": True})], shared=shared)

    def test_flexcomm_personalities_alias_to_same_ip(self):
        aliases = {"controller:USART0": "ip:FLEXCOMM0",
                   "controller:SPI0": "ip:FLEXCOMM0"}
        with self.assertRaisesRegex(ValueError, "conflict"):
            graph.resolve([claim("controller:USART0", personality="usart"),
                           claim("controller:SPI0", "other",
                                 personality="spi")],
                          aliases=aliases)

    def test_nordic_shared_id_conflicts_even_for_same_owner(self):
        with self.assertRaisesRegex(ValueError, "personality"):
            graph.resolve([claim("controller:UARTE0", personality="uarte"),
                           claim("controller:SPIM0", personality="spim")],
                          aliases={"controller:UARTE0": "ip:ID8",
                                   "controller:SPIM0": "ip:ID8"})

    def test_sibling_channels_share_the_same_physical_ip_personality(self):
        for owner in ("control", "other"):
            with self.subTest(owner=owner), self.assertRaisesRegex(
                    ValueError, "personality"):
                graph.resolve([
                    claim("controller:USART0/channel/0", personality="usart"),
                    claim("controller:SPI0/channel/1", owner,
                          personality="spi")], aliases={
                              "controller:USART0": "ip:FLEXCOMM0",
                              "controller:SPI0": "ip:FLEXCOMM0"})

    def test_personality_requires_an_explicit_physical_ip_identity(self):
        with self.assertRaisesRegex(ValueError, "physical IP"):
            graph.resolve([claim("controller:UNKNOWN/channel/0",
                                 personality="uart")])

    def test_same_owner_duplicate_attributes_are_type_strict(self):
        for policy in ("exclusive", "compatible"):
            first = claim("controller:T/base", policy=policy,
                          attributes={"value": 1})
            with self.subTest(policy=policy), self.assertRaises(ValueError):
                graph.resolve([first, {**first, "attributes": {"value": True}}],
                              shared={"controller:T/base": ("value",)})

    def test_alias_applies_to_child_and_is_frozen(self):
        attrs = {"tick": 1}
        plan = graph.resolve([claim("controller:T/channel/1",
                                    attributes=attrs)],
                             aliases={"controller:T": "ip:T"})
        attrs["tick"] = 2
        self.assertEqual(plan.claims[0].key, "ip:T/channel/1")
        self.assertEqual(plan.claims[0].attributes, (("tick", 1),))
        with self.assertRaises(AttributeError):
            plan.claims[0].owner = "changed"

    def test_unknown_fields_and_malformed_claims_reject(self):
        for record in (claim("controller:T", typo=True),
                       claim("controller:T", policy="shared"),
                       claim("controller:T", policy=[]),
                       claim("controller:T/../channel"),
                       claim("controller:T", owner=""),
                       claim("controller:T", attributes={"bad": []})):
            with self.subTest(record=record), self.assertRaises(ValueError):
                graph.resolve([record])

    def test_alias_cycles_and_ambiguous_duplicates_reject(self):
        with self.assertRaises(ValueError):
            graph.resolve([], aliases={"controller:A": "controller:B",
                                       "controller:B": "controller:A"})
        with self.assertRaises(ValueError):
            graph.resolve([claim("controller:T"),
                           claim("controller:T", mode="changed")])

    def test_growing_alias_cycle_rejects_without_unbounded_expansion(self):
        with self.assertRaises(ValueError):
            graph.resolve([], aliases={"controller:A": "controller:B/child",
                                       "controller:B": "controller:A/child"})

    def test_malformed_alias_target_rejects_before_canonicalization(self):
        with self.assertRaises(ValueError):
            graph.resolve([], aliases={"controller:A": "controller:B",
                                       "controller:B": []})

    def test_identical_duplicate_is_idempotent_and_projects_json(self):
        record = claim("controller:T")
        plan = graph.resolve([record, record])
        self.assertEqual(len(plan.claims), 1)
        self.assertEqual(plan.to_dict()["claims"][0]["key"], "controller:T")

    def test_validated_json_projection_freezes_deep_attributes(self):
        plan = graph.resolve([claim("controller:T", attributes={"tick": 1})])
        values = json.loads(json.dumps(plan.to_dict()))
        self.assertEqual(plan.to_dict(), values)
        frozen = graph.ResourcePlanIR.from_validated(values)
        values["claims"][0]["attributes"][0][1] = 2
        self.assertEqual(frozen, plan)


if __name__ == "__main__":
    unittest.main()
