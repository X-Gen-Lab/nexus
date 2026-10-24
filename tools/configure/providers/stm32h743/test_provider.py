"""Exact H743 DMA route admission and complete generated ownership."""
import copy
import json
from pathlib import Path
import unittest

from tools.configure.providers import stm32h743 as provider
from tools.configure.providers.common import ConfigurationError

ROOT = Path(__file__).resolve().parents[4]


class H743DmaConfiguration(unittest.TestCase):
    def test_receive_route_is_exact_and_distinct_from_transmit(self):
        self.assertEqual(provider.mode_implementation("uart", "rx-dma-idle"),
                         "uart_receive")
        route = {"kind": "uart", "controller": "USART3",
                 "mode_dma": {"rx-dma-idle": ["DMA1:stream1:channel45"]}}
        provider.validate_dma(route, "rx-dma-idle", lambda value: value)
        for path in ("DMA1:stream0:channel45", "DMA2:stream1:channel45",
                     "DMA1:stream1:channel46"):
            invalid = copy.deepcopy(route)
            invalid["mode_dma"]["rx-dma-idle"] = [path]
            with self.subTest(path=path), self.assertRaises(ConfigurationError):
                provider.validate_dma(invalid, "rx-dma-idle", lambda value: value)
        self.assertEqual(provider.dma_irq("DMA1:stream1:channel45"),
                         "irq:DMA1_Stream1")

    def test_dma_route_has_one_exact_engine_stream_and_request(self):
        self.assertEqual(provider.mode_implementation("uart", "dma-tx"),
                         "uart_dma")
        route = {"kind": "uart", "controller": "USART3",
                 "mode_dma": {"dma-tx": ["DMA1:stream0:channel46"]}}
        provider.validate_dma(route, "dma-tx", lambda value: value)
        for path in ("DMA2:stream0:channel46", "DMA1:stream1:channel46",
                     "DMA1:stream0:channel45"):
            invalid = copy.deepcopy(route)
            invalid["mode_dma"]["dma-tx"] = [path]
            with self.subTest(path=path), self.assertRaises(ConfigurationError):
                provider.validate_dma(invalid, "dma-tx", lambda value: value)
        self.assertEqual(provider.dma_irq("DMA1:stream0:channel46"),
                         "irq:DMA1_Stream0")

    def test_soc_facts_declare_dma_for_the_existing_pin_route_only(self):
        soc = json.loads((ROOT / "soc/stm32/stm32h743/soc.json").read_text())
        routes = json.loads((ROOT / "soc/stm32/stm32h743/routes.json").read_text())
        self.assertIn("dma-tx", soc["controllers"]["USART3"]["modes"])
        route = next(item for item in routes["routes"]
                     if item["controller"] == "USART3")
        self.assertEqual(route["mode_dma"]["dma-tx"],
                         ["DMA1:stream0:channel46"])
        self.assertEqual([pin["pin"] for pin in route["pins"]], ["PD8", "PD9"])


if __name__ == "__main__":
    unittest.main()
