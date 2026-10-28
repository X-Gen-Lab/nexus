"""Exact finite hardware-chain UART selection and immutable DMA plan contracts."""
from dataclasses import FrozenInstanceError
import unittest

from tools.configure import dma_topology
from soc.stm32.stm32u575 import configure as stm32u575
from tools.configure.providers.common import ConfigurationError
from .test_provider import Result, uart


class U575UartChainTests(unittest.TestCase):
    def item(self, **changes):
        return uart(mode="dma-chain-tx", dma=["GPDMA1:channel0:request25"],
                    **changes)

    def test_real_chain_mode_keeps_one_public_uart_face(self):
        item = self.item()
        stm32u575.validate_selection(item, item)
        result = Result(item)
        construction = stm32u575.constructor(
            result.controllers[0], result, {}, {}, ())
        self.assertEqual(construction.state_type,
                         "nx_stm32u575_uart_gpdma_state_t")
        self.assertEqual(stm32u575.operations(result.controllers[0]),
                         "nx_stm32u575_uart_chain_ops")
        self.assertTrue(construction.retain_on_failed_init)
        source = "\n".join(construction.definitions)
        self.assertIn("_Alignas(32)", source)
        self.assertIn("nx_stm32u575_uart_chain_node_t", source)
        self.assertIn("nx_stm32u575_uart_gpdma_config_t", source)
        self.assertIn("nx_stm32u575_uart_chain_prepare", construction.initialize.render())
        self.assertNotIn(".uart.registers =", source)

    def test_selected_plan_names_symbolic_placement_not_guessed_addresses(self):
        item = self.item()
        plan = dma_topology.resolve_selected([item], stm32u575)
        self.assertEqual(len(plan.chains), 1)
        chain = plan.chains[0]
        self.assertEqual(chain.owner, "console")
        self.assertEqual((chain.engine, chain.lane), ("GPDMA1", 0))
        self.assertEqual((chain.node_capacity, chain.node_bytes), (2, 12))
        self.assertEqual((chain.alignment, chain.window_bytes), (32, 65536))
        self.assertEqual((chain.region, chain.block_bytes, chain.maximum_bytes),
                         ("SRAM1", 65535, 196605))
        self.assertTrue(chain.finite)
        self.assertNotIn("address", plan.to_dict()["chains"][0])
        self.assertEqual(dma_topology.DmaTopologyIR.from_validated(
            plan.to_dict()), plan)
        with self.assertRaises(FrozenInstanceError):
            chain.node_capacity = 3

    def test_other_uart_modes_do_not_create_descriptor_storage(self):
        for mode in ("irq-bytes", "dma-tx"):
            item = uart(mode=mode, dma=(
                ["GPDMA1:channel0:request25"] if mode == "dma-tx" else []))
            self.assertEqual(dma_topology.resolve_selected(
                [item], stm32u575).chains, ())
            result = Result(item)
            source = "\n".join(stm32u575.constructor(
                result.controllers[0], result, {}, {}, ()).definitions)
            self.assertNotIn("uart_chain_node", source)

    def test_chain_mode_rejects_other_controller_and_word_or_flow_format(self):
        for item in (self.item(controller="USART2"),
                     self.item(frame={"data_bits": 9, "parity": "none",
                                      "stop_bits": 1, "flow": "none"}),
                     self.item(frame={"data_bits": 8, "parity": "none",
                                      "stop_bits": 1, "flow": "rts-cts"})):
            with self.subTest(item=item), self.assertRaises(ConfigurationError):
                stm32u575.validate_selection(item, item)


if __name__ == "__main__":
    unittest.main()
