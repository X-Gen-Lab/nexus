"""RM0008 DMA1 channels are exclusive resources with wired requests."""
from types import SimpleNamespace
import unittest

from tools.configure import dma_topology
from tools.configure.domain_bridge import DomainBridge
from tools.configure.domain_policies import DomainPolicies
from tools.configure.package_policy import BindingPolicy
from tools.configure.providers.common import VendorIrqBinding
from tools.configure.resource_graph import resource_claims


FACTS = {
    "schema_version": 1,
    "custom_clock_profile": "custom",
    "pinmux_policy": {"encoding": "route-fixed"},
    "mode_exclusions": [],
    "system": {"oscillator_resources": {}, "timebase": {
        "timer": "TIM2", "irq": "TIM2_IRQn", "handler": "TIM2_IRQHandler",
        "gate": "RCC_APB1ENR_TIM2EN", "reset": "RCC_APB1RSTR_TIM2RST",
        "clock_domain": "apb1_timer", "counter_bits": 16,
    }},
    "domains": {"schema_version": 1, "dma": {
        "encoding": "fixed-channel",
        "lanes": {
            "DMA1:channel6": {"channel": "DMA1_Channel6", "irq": "DMA1_Channel6_IRQn"},
            "DMA1:channel7": {"channel": "DMA1_Channel7", "irq": "DMA1_Channel7_IRQn"},
        },
        "transfers": {
            "USART2": {"tx": "DMA1:channel7", "rx": "DMA1:channel6"},
            "I2C1": {"tx": "DMA1:channel6", "rx": "DMA1:channel7"},
        },
    }},
}


class FixedChannelDmaTests(unittest.TestCase):
    def setUp(self):
        self.provider = DomainBridge()
        self.provider.domain_policies = DomainPolicies.from_validated(None, {}, FACTS)
        self.provider.package = SimpleNamespace(family="sample")
        self.tx = {"id": "console", "kind": "uart", "controller": "USART2",
                   "dma": ["DMA1:channel7"]}
        self.i2c = {"id": "sensors", "kind": "i2c", "controller": "I2C1",
                    "dma": ["DMA1:channel6", "DMA1:channel7"]}

    def test_exact_configured_channel_projects_wired_request_and_catalog_vector(self):
        self.provider.validate_dma(self.tx, "dma-tx", None)
        topology = dma_topology.resolve_selected([self.tx], self.provider)
        lane, = topology.lanes
        self.assertEqual((lane.engine, lane.lane, lane.request), ("DMA1", 7, None))
        self.assertEqual(lane.physical_key, "dma:DMA1:channel7")
        self.assertEqual(self.provider.dma_irq(self.tx["dma"][0]), "irq:DMA1_Channel7")
        policy = BindingPolicy.from_validated(FACTS,
            fixed_dma=self.provider.domain_policies.FIXED_CHANNEL_DMA)
        with self.assertRaisesRegex(ValueError, "reviewed startup binding"):
            policy.dma_bindings(topology.lanes)
        policy = policy.with_irq_bindings({
            "DMA1_Channel7_IRQn": VendorIrqBinding(17, "DMA1_Channel7_IRQHandler"),
        }, {})
        wire, = policy.dma_bindings(topology.lanes)
        self.assertEqual((wire.registers, wire.channel, wire.irq, wire.handler),
                         ("DMA1", "DMA1_Channel7", "DMA1_Channel7_IRQn",
                          "DMA1_Channel7_IRQHandler"))

    def test_shared_request_lane_conflicts_and_adjacent_lane_cannot_replace_wire(self):
        self.provider.validate_dma(self.i2c, "dma-messages", None)
        claims, _, claim = resource_claims(self.provider)
        claim("dma:" + self.tx["dma"][0], "console", "dma-tx", "usart2-default")
        self.assertIn("dma:DMA1:channel7", claims)
        with self.assertRaisesRegex(ValueError, "conflict dma:DMA1:channel7"):
            claim("dma:" + self.i2c["dma"][1], "sensors", "dma-messages", "i2c1-default")
        with self.assertRaisesRegex(ValueError, "DMA physical lane conflict"):
            dma_topology.resolve_selected([self.tx, self.i2c], self.provider)
        with self.assertRaisesRegex(ValueError, "wired DMA requests"):
            self.provider.validate_dma({**self.tx, "dma": ["DMA1:channel6"]},
                                       "dma-tx", None)
        for path in ("DMA1:channel07", "DMA1:stream7:channel0",
                     "DMA1:channel7:request0", "DMA2:channel7"):
            with self.subTest(path=path), self.assertRaisesRegex(ValueError, "topology"):
                self.provider.dma_irq(path)
