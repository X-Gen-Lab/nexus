"""Reviewed C binding leaves are typed before finite recipe projection."""
import copy
import unittest

from tools.configure import configure
from tools.configure.dma_topology import DmaLaneIR
from tools.configure.ir import freeze
from tools.configure.package_policy import BindingPolicy


class StaticBindingFactsTests(unittest.TestCase):
    def document(self):
        return configure.load(configure.ROOT / "soc/stm32/stm32f4/bindings.json")

    def test_selected_pads_and_dma_retain_reviewed_c_leaf_identity(self):
        policy = BindingPolicy.from_validated(self.document())
        pad, = policy.pad_bindings((freeze({"pin": "PA9", "af": 7,
                                           "function": "tx"}),))
        self.assertEqual((pad.identity, pad.registers, pad.port_index,
                          pad.number, pad.alternate), ("PA9", "GPIOA", 0, 9, 7))
        dma, = policy.dma_bindings((DmaLaneIR("uart1", "USART1", "DMA2", 7, 4,
                                            "dma:DMA2:stream7"),))
        self.assertEqual((dma.stream, dma.irq, dma.handler, dma.lane, dma.request),
                         ("DMA2_Stream7", "DMA2_Stream7_IRQn",
                          "DMA2_Stream7_IRQHandler", 7, 4))
        self.assertEqual((dma.status_member, dma.clear_member, dma.flag_shift),
                         ("HISR", "HIFCR", 22))

    def test_binding_rows_reject_untyped_or_unbounded_leaf_values(self):
        original = self.document()
        for group, row, field, invalid in (
                ("pads", "PA9", "registers", "GPIOA; call()"),
                ("pads", "PA9", "port_index", True),
                ("pads", "PA9", "number", 32),
                ("pads", "PA9", "unknown", 0),
                ("dma_lanes", "dma:DMA2:stream7", "stream", "DMA2[7]"),
                ("dma_lanes", "dma:DMA2:stream7", "flag_bank", "X"),
                ("dma_lanes", "dma:DMA2:stream7", "flag_shift", True),
                ("dma_lanes", "dma:DMA2:stream7", "status_member", "HISR->bad")):
            with self.subTest(group=group, field=field):
                changed = copy.deepcopy(original)
                owner = changed if group == "pads" else changed["hardware"]
                owner[group][row][field] = invalid
                with self.assertRaises(ValueError):
                    BindingPolicy.from_validated(changed)
        policy = BindingPolicy.from_validated(original)
        with self.assertRaises(ValueError):
            policy.pad_bindings((freeze({"pin": "PL0", "af": 0,
                                        "function": "input"}),))

    def test_actual_controller_integration_owns_typed_static_connections(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/uart_rx_compact_assembly/f407-dma-duplex.toml")
        item, = result.controllers
        self.assertEqual(tuple(pad.identity for pad in item.integration.pads),
                         ("PA9", "PA10"))
        self.assertEqual(item.integration.vector_bindings[0].handler,
                         "USART1_IRQHandler")
        self.assertFalse(item.integration.kernel_bound)
        self.assertEqual(tuple(lane.lane for lane in item.integration.dma_bindings),
                         tuple(lane.lane for lane in item.integration.dma))

    def test_mode_owned_publishers_keep_catalog_identity_without_a_second_vector_table(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/spi_frames_assembly/three-frames-ve.toml")
        for item in result.controllers:
            with self.subTest(controller=item.controller):
                self.assertEqual(tuple(value.handler for value in item.integration.vector_bindings),
                                 (item.controller + "_IRQHandler",))
                self.assertIn("irq:" + item.controller,
                    {claim.key for claim in item.integration.resources})


if __name__ == "__main__":
    unittest.main()
