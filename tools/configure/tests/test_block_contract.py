"""A polling block controller has explicit units and immutable selected limits."""
from dataclasses import FrozenInstanceError
import unittest

from tools.configure import authored, contracts, factory, ir
from tools.configure.providers.common import mode_traits


class BlockContractTests(unittest.TestCase):
    def test_controller_contract_has_no_flash_or_async_alias(self):
        self.assertIn("block", contracts.CONTROLLER_KINDS)
        selected = contracts.CONTROLLER_CONTRACTS["block"]
        self.assertEqual(selected.header, "block")
        self.assertEqual(selected.authored_fields,
                         frozenset(("binding", "mode", "bus_width", "max_blocks",
                                    "poll_budget", "stop_timeout_us", "irq",
                                    "protocol", "speed")))
        self.assertEqual(factory.interface_type("block", "port"),
                         "nx_block_port_t")

    def test_selected_limits_are_named_and_immutable(self):
        selected = ir.mode_options({"kind": "block", "bus_width": 4,
                                    "max_blocks": 8, "poll_budget": 1000,
                                    "stop_timeout_us": 25000})
        self.assertEqual(type(selected).__name__, "BlockOptions")
        self.assertEqual(selected.bus_width, 4)
        self.assertEqual(selected.max_blocks, 8)
        self.assertEqual(selected.poll_budget, 1000)
        self.assertEqual(selected.stop_timeout_us, 25000)
        self.assertEqual(ir.option_projection(selected), {
            "bus_width": 4, "max_blocks": 8, "poll_budget": 1000,
            "stop_timeout_us": 25000, "protocol": "sd-memory",
            "speed": "default"})
        with self.assertRaises(FrozenInstanceError):
            selected.max_blocks = 64

    def test_polling_does_not_advertise_irq_or_dma(self):
        traits = mode_traits("block", "sdio-poll")
        self.assertTrue(traits.polling)
        self.assertFalse(traits.irq)
        self.assertFalse(traits.dma)
        self.assertFalse(traits.block_stream)

    def test_editor_uses_exact_positive_block_bounds(self):
        definition = authored.editor_schema()["properties"]["block"]
        selected = definition["additionalProperties"]
        self.assertFalse(selected["additionalProperties"])
        properties = selected["properties"]
        self.assertEqual(properties["bus_width"]["enum"], [1, 4, 8])
        self.assertEqual(properties["max_blocks"]["minimum"], 1)
        self.assertEqual(properties["max_blocks"]["maximum"], 32)
        self.assertEqual(properties["poll_budget"]["minimum"], 1)
        self.assertEqual(properties["poll_budget"]["maximum"], 1000000)
        self.assertEqual(properties["stop_timeout_us"]["minimum"], 1)
        self.assertNotIn("startup_timeout_us", properties)
        self.assertNotIn("address", properties)


if __name__ == "__main__":
    unittest.main()
