"""Exact U575 reference metadata and allocation-free emitter contracts."""
import importlib
import json
from pathlib import Path
import unittest

from tools.configure.ir import ControllerIR
from tools.configure.providers.common import ConfigurationError

ROOT = Path(__file__).resolve().parents[4]


def uart(**changes):
    return {**{"id": "console", "binding": "uart1", "kind": "uart",
        "controller": "USART1", "mode": "irq-bytes", "baud": 115200,
        "rx_profile": "bytes", "rx_capacity": 64, "irq_priority": 5,
        "calls_os": False, "pins": [{"pin": "PA9", "function": "tx", "af": 7},
            {"pin": "PA10", "function": "rx", "af": 7}]}, **changes}


def gpio(**changes):
    return {**{"id": "led", "binding": "led0", "kind": "gpio",
        "controller": "GPIOC", "mode": "output", "mask": 128, "initial": 0,
        "pins": [{"pin": "PC7", "function": "output", "af": 0}]}, **changes}


class Result(dict):
    def __init__(self, *items):
        super().__init__(irq={"external_count": 126, "priority_bits": 4,
                              "syscall_priority": None}, devices=[])
        self.controllers = tuple(ControllerIR.create(item, "stm32u575")
                                 for item in items)


class U575ProviderTests(unittest.TestCase):
    def setUp(self):
        self.provider = importlib.import_module(
            "tools.configure.providers.stm32u575")

    def test_exact_m33_fpu_single_state_cpu_and_internal_clock(self):
        self.assertEqual(self.provider.CPU_ABI, {"arch": "cortex-m33",
            "fpu": "fpv5-sp-d16", "float_abi": "hard"})
        self.assertEqual(self.provider.EXTERNAL_IRQ_COUNT, 126)
        self.assertEqual(self.provider.CPU_FEATURES["mpu_regions"], 16)
        self.assertEqual(self.provider.CPU_FEATURES["icache_line_bytes"], 0)
        self.assertEqual(self.provider.CPU_FEATURES["security"], "single")
        self.assertFalse(self.provider.DWT_CYCCNT)
        self.assertEqual(dict(self.provider.CLOCK_PROFILES["msis4"]),
            {"source": "msis", "input_hz": 4000000, "core_hz": 4000000,
             "apb1_hz": 4000000, "apb2_hz": 4000000})

    def test_only_reviewed_uart_gpio_routes_and_modes_are_accepted(self):
        self.provider.validate_selection(uart(), uart())
        self.provider.validate_selection(gpio(), gpio())
        for item in (uart(controller="USART2"), uart(mode="dma-tx"),
                     uart(rx_profile="blocks"), gpio(mask=64)):
            with self.subTest(item=item):
                with self.assertRaises(ConfigurationError):
                    self.provider.validate_selection(item, item)

    def test_usart_divider_bounds_reject_bool_and_unrepresentable_baud(self):
        for baud in (62, 9600, 115200, 250000):
            self.provider.validate_selection(uart(baud=baud), uart())
        for baud in (True, 0, 61, 250001):
            with self.subTest(baud=baud):
                with self.assertRaises(ConfigurationError):
                    self.provider.validate_selection(uart(baud=baud), uart())

    def test_generated_factory_uses_static_real_face_and_context(self):
        result = Result(gpio(), uart())
        for item in result.controllers:
            construction = self.provider.constructor(item, result, {}, {}, ())
            source = "\n".join(construction["definitions"])
            self.assertLess(source.index(".retained"),
                            source.index(".registers ="))
            self.assertEqual(self.provider.operations(item),
                             "nx_stm32u575_" + item.kind + "_ops")
            self.assertNotIn("malloc", source)
            self.assertNotIn("xTask", source)
        self.assertIn("USART1_IRQHandler", source)
        self.assertIn("4000000u, 1000000u", source)

    def test_pin_leases_cover_ports_and_af_words_without_unused_tables(self):
        result = Result(gpio(), uart())
        source = "\n".join(self.provider.state_declarations(result))
        self.assertIn("s_nx_gpioa_lease", source)
        self.assertIn("s_nx_gpioc_lease", source)
        self.assertIn("AFR[1]", source)
        self.assertEqual(self.provider.state_declarations(Result()), [])
        self.assertEqual(self.provider.start_resources(Result()), [])

    def test_package_inventory_and_sram1_default_remain_exact(self):
        facts = json.loads((ROOT / "soc/stm32u575/soc.json").read_text())
        variant = facts["variants"]["STM32U575ZIT6Q"]
        self.assertEqual(len(variant["pins"]), 111)
        self.assertNotIn("PB12", variant["pins"])
        self.assertNotIn("PC4", variant["pins"])
        self.assertEqual(variant["memory"][0]["size"], 196608)
        self.assertEqual(variant["flash"]["erase_blocks"], [8192] * 256)

    def test_reference_board_does_not_invent_hse_or_physical_qualification(self):
        board = json.loads((ROOT / "boards/stm32u575_nucleo_zi_q/"
                            "board.json").read_text())
        self.assertEqual(board["clocks"], {})
        self.assertIsNone(board["physical_pcb_revision"])
        self.assertTrue(any("TZEN" in text for text in board["unknowns"]))


if __name__ == "__main__":
    unittest.main()
