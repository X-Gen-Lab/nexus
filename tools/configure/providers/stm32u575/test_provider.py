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
        self.assertEqual(self.provider.CPU_FEATURES["mpu_regions"], 8)
        self.assertEqual(self.provider.CPU_FEATURES["icache_line_bytes"], 0)
        self.assertEqual(self.provider.CPU_FEATURES["security"], "single")
        self.assertFalse(self.provider.DWT_CYCCNT)
        self.assertEqual(dict(self.provider.CLOCK_PROFILES["msis4"]),
            {"source": "msis", "input_hz": 4000000, "core_hz": 4000000,
             "apb1_hz": 4000000, "apb2_hz": 4000000})

    def test_only_reviewed_uart_gpio_routes_and_modes_are_accepted(self):
        self.provider.validate_selection(uart(), uart())
        self.provider.validate_selection(gpio(), gpio())
        for item in (uart(controller="USART2"), uart(mode="dma-duplex"),
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


class U575GpdmaSelectionTests(unittest.TestCase):
    def setUp(self):
        self.provider = importlib.import_module(
            "tools.configure.providers.stm32u575")

    def resolve(self, filename="usart1-dma-tx.toml"):
        from tools.configure import configure
        return configure.resolve_ir(
            ROOT / "tests/contracts/u575_assembly" / filename, ROOT)

    def test_exact_dma_mode_uses_selected_extension_without_public_new_face(self):
        item = uart(mode="dma-tx")
        self.provider.validate_selection(item, item)
        self.assertEqual(self.provider.mode_implementation("uart", "dma-tx"),
                         "uart_gpdma")
        result = Result(item)
        port = result.controllers[0]
        construction = self.provider.constructor(port, result, {}, {}, ())
        self.assertEqual(construction.state_type,
                         "nx_stm32u575_uart_gpdma_state_t")
        self.assertEqual(self.provider.operations(port),
                         "nx_stm32u575_uart_gpdma_ops")
        self.assertTrue(construction.retain_on_failed_init)

    def test_strict_gpdma_route_keeps_engine_channel_and_request_separate(self):
        self.assertEqual(self.provider.validate_dma_resource(
            "dma:GPDMA1:channel0:request25"), ("GPDMA1", 0, 25))
        self.assertEqual(self.provider.validate_dma_resource(
            "dma:GPDMA1:channel0"), ("GPDMA1", 0, None))
        self.assertEqual(self.provider.dma_irq("GPDMA1:channel0:request25"),
                         "irq:GPDMA1_Channel0")
        self.assertEqual(self.provider.dma_physical_key(
            "dma:GPDMA1:channel0:request25"), "dma:GPDMA1:channel0")
        for value in ("dma:DMA1:stream0:channel25",
                      "dma:LPDMA1:channel0:request25",
                      "dma:GPDMA2:channel0:request25",
                      "dma:GPDMA1:channel1:request25",
                      "dma:GPDMA1:channel0:request26",
                      "dma:GPDMA1:channel00:request25",
                      "dma:GPDMA1:channel0:request025"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.provider.validate_dma_resource(value)

    def test_normalizer_discards_request_but_preserves_real_channel_identity(self):
        from tools.configure import configure
        self.assertEqual(configure.normalize_resource(
            "dma:GPDMA1:channel0:request25"), "dma:GPDMA1:channel0")
        self.assertEqual(configure.normalize_resource(
            "dma:DMA2:stream7:channel4"), "dma:DMA2:stream7")

    def test_physical_projection_and_collision_use_gpdma_without_fake_dma1(self):
        from tools.configure import dma_topology
        selected = [{"id": "console", "controller": "USART1",
                     "dma": ["GPDMA1:channel0:request25"]}]
        plan = dma_topology.resolve_selected(selected, self.provider)
        lane = plan.lanes[0]
        self.assertEqual((lane.engine, lane.lane, lane.request),
                         ("GPDMA1", 0, 25))
        self.assertEqual(lane.physical_key, "dma:GPDMA1:channel0")
        self.assertEqual(dma_topology.DmaTopologyIR.from_validated(
            plan.to_dict()), plan)
        with self.assertRaises(AttributeError):
            lane.request = 26
        with self.assertRaisesRegex(ValueError, "conflict"):
            dma_topology.resolve_selected(selected + [{
                "id": "other", "controller": "USART2",
                "dma": ["GPDMA1:channel0:request25"]}], self.provider)

    def test_foreign_controller_profile_and_nondefault_frame_stay_rejected(self):
        items = [uart(mode="dma-tx", controller="USART2", pins=[
            {"pin": "PA2", "function": "tx", "af": 7},
            {"pin": "PA3", "function": "rx", "af": 7}]),
            uart(mode="dma-tx", rx_profile="blocks"),
            uart(mode="dma-tx", irq_priority=6),
            uart(mode="dma-tx", frame={"data_bits": 8, "parity": "even",
                                       "stop_bits": 1, "flow": "none"}),
            uart(mode="rx-dma-idle")]
        for item in items:
            with self.subTest(item=item), self.assertRaises(ValueError):
                self.provider.validate_selection(item, item)

    def test_authored_toml_projects_exact_two_vectors_and_pinned_ll_source(self):
        result = self.resolve()
        port = next(item for item in result.controllers if item.kind == "uart")
        self.assertEqual((port.controller, port.mode), ("USART1", "dma-tx"))
        claims = {item["key"] for item in result["claims"]}
        self.assertTrue({"dma:GPDMA1:channel0", "irq:GPDMA1_Channel0",
                         "irq:USART1"}.issubset(claims))
        self.assertEqual(result.dma_topology.lanes[0].engine, "GPDMA1")
        dependencies = [entry for entry in result["source_dependencies"]
                        if entry["path"] == "vendors/st/stm32u5_hal"]
        self.assertEqual(len(dependencies), 1)
        self.assertEqual(dependencies[0]["commit"],
                         "2552682ee5a0d6f826a6435750fc9267be65868b")

    def test_generated_vectors_tables_and_cold_retention_select_real_provider(self):
        from tools.configure import bindings
        result = self.resolve()
        _, source = bindings.emit(result)
        self.assertIn('#include "stm32u575_uart_gpdma.h"', source)
        self.assertIn("nx_stm32u575_uart_gpdma_state_t s_nx_port_console", source)
        self.assertIn("&nx_stm32u575_uart_gpdma_ops", source)
        self.assertIn("GPDMA1_Channel0_IRQHandler", source)
        self.assertIn("nx_stm32u575_uart_gpdma_irq(&s_nx_port_console)", source)
        self.assertIn("nx_stm32u575_uart_gpdma_uart_irq(&s_nx_port_console)", source)
        self.assertIn(".dma_irq_policy", source)
        self.assertLess(source.index(".uart.retained"),
                        source.index(".uart.registers = USART1"))
        self.assertIn("nx_stm32u575_uart_gpdma_initialize", source)
        self.assertNotIn("nx_stm32u575_uart_irq(&s_nx_port_console)", source)

    def test_preflight_claims_vector_without_reading_unclocked_gpdma(self):
        from tools.configure import preflight
        source = "\n".join(preflight.emit(self.resolve()))
        self.assertIn("NX_U575_IRQ_AVAILABLE(GPDMA1_Channel0_IRQn)", source)
        self.assertNotIn("GPDMA1_Channel0->", source)

    def test_ordinary_irq_constructor_keeps_original_state_ops_and_vector(self):
        result = Result(uart())
        item = result.controllers[0]
        construction = self.provider.constructor(item, result, {}, {}, ())
        source = "\n".join(construction.definitions)
        self.assertEqual(construction.state_type, "nx_stm32u575_uart_state_t")
        self.assertEqual(self.provider.operations(item), "nx_stm32u575_uart_ops")
        self.assertIn("nx_stm32u575_uart_irq", source)
        self.assertNotIn("gpdma", source.lower())
        self.assertNotIn("GPDMA1", source)


if __name__ == "__main__":
    unittest.main()
