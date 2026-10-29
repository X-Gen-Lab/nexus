"""Exact H743 DMA route admission and complete generated ownership."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from soc.stm32.stm32h743 import configure as provider
from tools.configure import configure
from tools.configure.providers.common import ConfigurationError

ROOT = Path(__file__).resolve().parents[5]


class H743DmaConfiguration(unittest.TestCase):
    def test_dma_modes_start_the_selected_instance_prepare_function(self):
        fixtures = (
            ("tests/contracts/h743_assembly/dma-cache.toml", "uart_dma"),
            ("tests/contracts/h743_assembly/rx-cache.toml", "uart_receive"),
            ("tools/configure/assemblies/h743-uart-continuous.toml",
             "uart_continuous"),
            ("tools/configure/assemblies/h743-uart-duplex-continuous.toml",
             "uart_duplex"),
        )
        for relative, driver in fixtures:
            with self.subTest(driver=driver), tempfile.TemporaryDirectory() as directory:
                original = ROOT / relative
                text = original.read_text()
                board = text.split('board_package = "', 1)[1].split('"', 1)[0]
                text = text.replace(board, str((original.parent / board).resolve()))
                text = text.replace("[uart.console", "[uart.telemetry")
                source = Path(directory) / "assembly.toml"
                source.write_text(text)
                output = Path(directory) / "generated"
                configure.configure(source, output)
                body = (output / "bindings.c").read_text()
                self.assertIn(
                    f"result.primary = nx_stm32h743_{driver}_prepare("
                    "&s_nx_port_telemetry, &s_nx_config_telemetry,",
                    body)
                self.assertIn(
                    "static const nx_stm32h743_uart_config_t "
                    "s_nx_config_telemetry", body)
                self.assertNotIn("prepare_telemetry(void)", body)

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
