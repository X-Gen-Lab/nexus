"""Shared IRQ claims require one reviewed dispatcher and matching constraints."""
import unittest
from types import SimpleNamespace

from tools.configure import shared_irq


class IrqClaimTests(unittest.TestCase):
    def planner(self):
        records = {}
        def claim(key, owner, mode, source):
            if key in records:
                raise ValueError("Resource conflict " + key)
            records[key] = dict(key=key, owner=owner, mode=mode, source=source)
        return shared_irq.IrqClaims(records, claim), records

    def contract(self, dispatch="dispatch_lines"):
        return shared_irq.SharedIrqGroup("line_state_t", dispatch, "nx_arch_dsb",
            "selected.vector_bindings.0.handler", "selected.irq.priority",
            "selected.constants.mask")

    def claim(self, planner, owner, **changes):
        arguments = dict(kind="exti", priority=5, calls_os=False,
                         shared=True, contract=self.contract())
        arguments.update(changes)
        planner.claim("irq:GROUP", owner, "edge", "reviewed-route", **arguments)

    def test_one_physical_claim_tracks_compatible_publishers(self):
        planner, records = self.planner()
        self.claim(planner, "line_a")
        self.claim(planner, "line_b")
        self.assertEqual(records["irq:GROUP"]["owner"], "line_a;line_b")
        self.assertEqual(records["irq:GROUP"]["mode"], "static-irq-dispatch")

    def test_no_sharing_without_declared_dispatcher_and_matching_constraints(self):
        for changes in ({"shared": False}, {"contract": None}, {"priority": 6},
                        {"calls_os": True}, {"kind": "can"},
                        {"contract": self.contract("other_dispatch")}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                planner, _ = self.planner()
                self.claim(planner, "line_a")
                self.claim(planner, "line_b", **changes)

    def test_board_reserved_vector_still_conflicts(self):
        planner, records = self.planner()
        records["irq:GROUP"] = dict(key="irq:GROUP", owner="board",
            mode="reserved", source="board.json")
        with self.assertRaisesRegex(ValueError, "Resource conflict"):
            self.claim(planner, "line_a")

    def test_dispatcher_must_cover_the_exact_canonical_claim_and_owners(self):
        from tools.configure.tests.resources.peripherals.test_shared_irq import SharedIrqTests
        fixture = SharedIrqTests()
        specification = fixture.spec()
        selected = ((specification, fixture.item("a", 32)),
                    (specification, fixture.item("b", 64)))
        claims = (SimpleNamespace(key="irq:PHYSICAL", owner="a;b",
                                  mode="static-irq-dispatch"),)
        nodes = shared_irq.connections(selected, claims=claims,
            vector_handlers={"irq:PHYSICAL": "LINE_GROUP_IRQHandler"})
        self.assertEqual(nodes[-1].name, "LINE_GROUP_IRQHandler")
        for handlers, owner in (({"irq:PHYSICAL": "OTHER_IRQHandler"}, "a;b"),
                                ({"irq:PHYSICAL": "LINE_GROUP_IRQHandler"}, "a;c")):
            with self.subTest(owner=owner, handlers=handlers), self.assertRaises(ValueError):
                shared_irq.connections(selected,
                    claims=(SimpleNamespace(key="irq:PHYSICAL", owner=owner,
                                            mode="static-irq-dispatch"),),
                    vector_handlers=handlers)


if __name__ == "__main__":
    unittest.main()
