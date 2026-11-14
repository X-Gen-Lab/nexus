"""Selected publishers share one finite dispatcher with exact source ownership."""
from dataclasses import dataclass
import json
from types import MappingProxyType
import unittest


@dataclass(frozen=True)
class Vector:
    handler: str = "LINE_GROUP_IRQHandler"


@dataclass(frozen=True)
class Irq:
    priority: int


@dataclass(frozen=True)
class Integration:
    irq: Irq
    constants: object
    vector_bindings: tuple = (Vector(),)


@dataclass(frozen=True)
class Item:
    id: str
    integration: Integration


class SharedIrqTests(unittest.TestCase):
    def spec(self):
        from tools.configure.shared_irq import SharedIrqGroup
        return SharedIrqGroup.from_validated({"state_type": "line_state_t",
            "dispatch": "dispatch_lines", "barrier": "nx_arch_dsb",
            "handler": "selected.vector_bindings.0.handler",
            "priority": "selected.irq.priority", "mask": "selected.constants.mask"})

    def item(self, name, mask, priority=5):
        return Item(name, Integration(Irq(priority), MappingProxyType({"mask": mask})))

    def test_one_dispatcher_preserves_exact_ports_mask_priority_and_barrier(self):
        from tools.configure.shared_irq import connections
        spec = self.spec()
        declarations, irq = connections(((spec, self.item("a", 32)),
                                         (spec, self.item("b", 64))))
        self.assertEqual(declarations.count, 2)
        self.assertIn("&s_nx_port_a, &s_nx_port_b", declarations.render()[0])
        self.assertEqual(irq.name, "LINE_GROUP_IRQHandler")
        self.assertIn("2u, 96u)", irq.calls[0].render())
        self.assertEqual(irq.exit_barrier.name, "nx_arch_dsb")

    def test_overlap_priority_and_unknown_publisher_fail_before_rendering(self):
        from tools.configure.shared_irq import connections
        spec = self.spec()
        for other in (self.item("b", 32), self.item("b", 64, priority=6)):
            with self.subTest(item=other), self.assertRaises(ValueError):
                connections(((spec, self.item("a", 32)), (spec, other)))
        with self.assertRaises(ValueError):
            connections(((spec, self.item("a", True)),))
        with self.assertRaises(ValueError):
            self.spec().member(Item("a", Integration(Irq(5), MappingProxyType({"mask": 1}), ())))

    def test_actual_f4_edge_owners_share_one_catalog_bound_dispatcher(self):
        from tools.configure import configure
        from tools.configure.data_provider import DataProvider
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/stm32_assembly/ve-exti.toml")
        document = configure.load(configure.ROOT /
            "soc/stm32/stm32f4/bindings/exti.json")
        provider = DataProvider(result.provider_context.package, {}, {
            "enum_abi": "short-enums", "backends": ["baremetal"], "headers": [],
            "bindings": document["bindings"]}, result.provider_context.domain_policies)
        declarations, irq = provider.shared_irqs(result)
        self.assertEqual(declarations.count, 2)
        self.assertEqual(irq.name, "EXTI9_5_IRQHandler")
        self.assertEqual(irq.calls[0].function, "nx_stm32_exti_dispatch")
        self.assertEqual(irq.calls[0].arguments[1:], (2, (1 << 5) | (1 << 6)))
        self.assertEqual(irq.exit_barrier.name, "nx_arch_dsb")


if __name__ == "__main__":
    unittest.main()
