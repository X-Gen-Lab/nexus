"""Classic CAN topology comes from admitted facts, not one MCU's IRQ slots."""
import copy
from types import SimpleNamespace
import unittest

from tools.configure import can, device_view
from tools.configure.domain_policies import DomainPolicies
from tools.configure.providers.common import VendorIrqBinding


class CanFactContracts(unittest.TestCase):
    def facts(self, owners=2, banks=14):
        names = ("BUS_A", "BUS_B", "BUS_C")[:owners]
        controllers = {}
        for index, name in enumerate(names):
            controllers[name] = {"kind": "can", "modes": ["classic-irq"],
                "irq": [f"LINE_{index}_{role}" for role in ("T", "A", "B", "E")],
                "irq_numbers": [5 + index * 31, 9 + index * 31,
                                12 + index * 31, 18 + index * 31],
                "clock_hz": 24000000, "pins": []}
        return {"id": "unrelated_mcu", "controllers": controllers,
                "can_domain": {"controllers": list(names), "primary": names[0],
                               "filter_banks": banks},
                "irq": {"external_count": 128}}

    def provider(self, soc):
        policy = {"schema_version": 1, "can": {
            "irq_roles": ["tx", "rx0", "rx1", "error"],
            "fifo_order_supported": True}}
        return DomainPolicies.from_validated(
            SimpleNamespace(family=soc["id"], vendor="unrelated"), soc,
            {"domains": policy})

    def catalog(self, soc):
        return {name: VendorIrqBinding(number, f"Publisher{number}")
                for controller in soc["controllers"].values()
                for name, number in zip(controller["irq"], controller["irq_numbers"])}

    def filter(self, owner, bank, fifo=0):
        return {"controller": owner, "bank": bank, "id": 291, "mask": 2047,
                "format": "standard", "fifo": fifo,
                "remote": False, "match_remote": True}

    def resolve(self, soc, split, filters):
        selected = [{"id": f"bus{index}", "kind": "can", "controller": name}
                    for index, name in enumerate(soc["can_domain"]["controllers"])]
        claims = []
        result = can.resolve_domain({"split": split, "filters": filters}, selected,
            soc, self.provider(soc), lambda *claim: claims.append(claim), {})
        return result, claims

    def test_nonvendor_names_and_catalog_irqs_require_no_tool_change(self):
        soc = self.facts()
        device_view.validate_controller_irqs(soc["controllers"], self.catalog(soc))
        policy = self.provider(soc)
        can.validate_soc(soc, policy)
        self.assertEqual(policy.CAN_CONTROLLERS, ("BUS_A", "BUS_B"))
        self.assertEqual(policy.CAN_CLOCK_HZ, 24000000)
        self.assertEqual(policy.CAN_INTEGRATION.irq_roles, ("tx", "rx0", "rx1", "error"))

    def test_single_controller_owns_the_authored_full_filter_domain(self):
        soc = self.facts(owners=1)
        can.validate_soc(soc, self.provider(soc))
        result, claims = self.resolve(soc, 14, [self.filter("bus0", 13)])
        self.assertEqual(result["filters"][0]["controller_index"], 0)
        self.assertEqual(result["filters"][0]["match_index"], 26)
        self.assertIn(("can:filter-bank:13", "bus0", "filter", soc["id"]), claims)
        with self.assertRaisesRegex(ValueError, "CAN.*split"):
            self.resolve(soc, 7, [])

    def test_dual_controller_capacity_and_order_follow_one_author(self):
        soc = self.facts(banks=32)
        soc["can_domain"]["controllers"].reverse()
        soc["can_domain"]["primary"] = "BUS_B"
        policy = self.provider(soc)
        can.validate_soc(soc, policy)
        result, _ = self.resolve(soc, 30, [self.filter("bus0", 29), self.filter("bus1", 31)])
        self.assertEqual([item["controller_index"] for item in result["filters"]], [0, 1])
        self.assertEqual([item["tag"] for item in result["filters"]], [1, 1])
        self.assertEqual([item["match_index"] for item in result["filters"]], [58, 2])
        self.assertEqual(policy.CAN_CONTROLLERS, ("BUS_B", "BUS_A"))

    def test_raw_group_clock_projection_reuses_existing_controller_facts(self):
        soc = self.facts()
        controllers = soc.pop("controllers")
        soc["controller_groups"] = {"reviewed_ip": {"controllers": controllers}}
        policy = self.provider(soc)
        self.assertEqual(policy.CAN_CLOCK_HZ, 24000000)
        self.assertEqual(policy.CAN_CONTROLLERS, ("BUS_A", "BUS_B"))

    def test_catalog_mismatch_remains_rejected_independently(self):
        soc = self.facts()
        catalog = self.catalog(soc)
        soc["controllers"]["BUS_A"]["irq_numbers"][0] = 6
        with self.assertRaisesRegex(ValueError, "selected device SDK"):
            device_view.validate_controller_irqs(soc["controllers"], catalog)

    def test_invalid_topology_clock_and_irq_role_contracts_are_rejected(self):
        original = self.facts()
        for mutate in (
                lambda value: value["can_domain"].update(primary="BUS_B"),
                lambda value: value["can_domain"].update(controllers=["BUS_A", []]),
                lambda value: value["can_domain"].update(filter_banks=0),
                lambda value: value["can_domain"].update(filter_banks=33),
                lambda value: value["can_domain"].update(filter_banks=True),
                lambda value: value["controllers"]["BUS_A"]["irq"].pop(),
                lambda value: value["controllers"]["BUS_A"].update(clock_hz=True),
                lambda value: value["controllers"]["BUS_B"].update(clock_hz=12000000)):
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                soc = copy.deepcopy(original)
                mutate(soc)
                can.validate_soc(soc, self.provider(soc))
        with self.assertRaisesRegex(ValueError, "CAN"):
            soc = self.facts(owners=3)
            can.validate_soc(soc, self.provider(soc))
        for roles in (("tx", "rx0", "rx1"), ("tx", "tx", "rx1", "error"),
                      ("rx0", "tx", "rx1", "error")):
            with self.subTest(roles=roles), self.assertRaises(ValueError):
                DomainPolicies.from_validated(SimpleNamespace(), original,
                    {"domains": {"schema_version": 1, "can": {
                        "irq_roles": list(roles), "fifo_order_supported": True}}})

    def test_filter_boundaries_use_selected_bank_count_and_partition(self):
        soc = self.facts(banks=14)
        for split, filters in (
                (15, []), (7, [self.filter("bus0", 14)]),
                (7, [self.filter("bus0", 7)]),
                (7, [self.filter("bus1", 6)]),
                (7, [self.filter("bus0", 0), self.filter("bus0", 0)])):
            with self.subTest(split=split, filters=filters), self.assertRaises(ValueError):
                self.resolve(soc, split, filters)


if __name__ == "__main__":
    unittest.main()
