"""CAN IP rules consume explicit controller clocks and interrupt sources."""
import copy
from types import SimpleNamespace
import unittest

from tools.configure import can
from tools.configure.domain_bridge import DomainBridge
from tools.configure.domain_policies import DomainPolicies
from tools.configure.ir import ClockDomainIR
from tools.configure.ir import thaw
from tools.configure.tests.f4_fixture import provider as f4_provider


class CanPolicyBindingTests(unittest.TestCase):
    def facts(self):
        names = ("BUS_A", "BUS_B")
        controllers = {}
        for index, name in enumerate(names):
            irqs = [f"LINE_{index}_{role}" for role in ("T", "A", "B", "E")]
            controllers[name] = {"kind": "can", "modes": ["classic-irq"],
                "irq": irqs, "pins": [], "clock_domain": name.lower(),
                "irq_sources": dict(zip(("tx", "rx0", "rx1", "error"), irqs))}
        return {"id": "host_clock_fixture", "controllers": controllers,
                "can_domain": {"primary": names[0], "controllers": list(names),
                               "filter_banks": 28}}

    def policy(self, facts):
        return DomainPolicies.from_validated(SimpleNamespace(), facts,
            {"domains": {"schema_version": 1, "can": {
                "model": "bxcan", "fifo_order_supported": True,
                "transport": {name: {"clock_domain": row["clock_domain"],
                    "irq_sources": row["irq_sources"]}
                    for name, row in facts["controllers"].items()}}}})

    def provider(self, facts):
        class Provider(DomainBridge):
            pass
        provider = Provider()
        provider.soc = facts
        provider.domain_policies = None
        return provider

    def plan(self):
        return SimpleNamespace(clock={"core_hz": 160000000, "apb1_hz": 42000000},
            clock_plan=SimpleNamespace(domains={
                "bus_a": ClockDomainIR("independent_a", 24000000, 0, 0),
                "bus_b": ClockDomainIR("independent_b", 12000000, 0, 0)}))

    def test_each_controller_uses_its_exact_selected_clock_domain(self):
        provider = self.provider(self.facts())
        plan = self.plan()
        self.assertEqual(provider.controller_clock_hz(plan, "BUS_A"), 24000000)
        self.assertEqual(provider.controller_clock_hz(plan, "BUS_B"), 12000000)

    def test_missing_declared_clock_never_falls_back_to_core_or_apb(self):
        provider = self.provider(self.facts())
        plan = self.plan()
        del plan.clock_plan.domains["bus_a"]
        with self.assertRaisesRegex(ValueError, "clock domain"):
            provider.controller_clock_hz(plan, "BUS_A")

    def test_bxcan_policy_is_explicit_and_requires_no_global_frequency(self):
        facts = self.facts()
        policy = self.policy(facts)
        can.validate_soc(facts, policy)
        self.assertEqual(type(policy.CAN_INTEGRATION).__name__, "BxCanPolicy")
        self.assertFalse(hasattr(policy, "CAN_CLOCK_HZ"))

    def test_missing_transport_cannot_weaken_package_admission(self):
        with self.assertRaisesRegex(ValueError, "transport"):
            DomainPolicies.from_validated(SimpleNamespace(), self.facts(),
                {"domains": {"schema_version": 1, "can": {
                    "model": "bxcan", "fifo_order_supported": True}}})
        from tools.configure.policies.bxcan import BxCanPolicy
        with self.assertRaisesRegex(ValueError, "transport"):
            BxCanPolicy().validate_soc(self.facts(), ("BUS_A", "BUS_B"))

    def test_malformed_empty_domain_is_not_an_absent_can_domain(self):
        from tools.configure.policies.bxcan import BxCanPolicy
        selected = SimpleNamespace(CAN_INTEGRATION=BxCanPolicy(), CAN_CONTROLLERS=())
        can.validate_soc({"controllers": {}}, selected)
        for value in ({}, [], False, ""):
            with self.subTest(value=value), self.assertRaises(ValueError):
                can.validate_soc({"controllers": {}, "can_domain": value}, selected)

    def test_combined_vector_covers_each_reason_once(self):
        facts = self.facts()
        controller = facts["controllers"]["BUS_A"]
        controller["irq"] = ["COMBINED_A"]
        controller["irq_sources"] = dict.fromkeys(("tx", "rx0", "rx1", "error"),
                                                 "COMBINED_A")
        can.validate_soc(facts, self.policy(facts))

    def test_missing_unknown_and_unbound_interrupt_sources_reject(self):
        for mutate in (
                lambda row: row["irq_sources"].pop("error"),
                lambda row: row["irq_sources"].update(wakeup="LINE_0_E"),
                lambda row: row["irq_sources"].update(error="ABSENT_LINE"),
                lambda row: row.update(clock_domain="")):
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                facts = copy.deepcopy(self.facts())
                mutate(facts["controllers"]["BUS_A"])
                can.validate_soc(facts, self.policy(facts))

    def test_f4_transport_rejects_unimplemented_clock_or_vector_topologies(self):
        selected = f4_provider("STM32F407VET6")
        for change in ("clock", "reordered", "combined"):
            with self.subTest(change=change), self.assertRaisesRegex(ValueError, "transport"):
                facts = thaw(selected.soc)
                row = facts["controllers"]["CAN1"]
                if change == "clock":
                    row["clock_domain"] = "apb2"
                elif change == "reordered":
                    row["irq_sources"]["tx"], row["irq_sources"]["error"] = (
                        row["irq_sources"]["error"], row["irq_sources"]["tx"])
                else:
                    row["irq"] = ["CAN1_TX"]
                    row["irq_sources"] = dict.fromkeys(selected.CAN_INTEGRATION.irq_roles,
                                                       "CAN1_TX")
                can.validate_soc(facts, selected)


if __name__ == "__main__":
    unittest.main()
