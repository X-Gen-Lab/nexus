"""F407 SDIO owns a socket, while media discovery remains explicit at runtime."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure as gate, selected_contracts


class F407BlockConfigurationTests(unittest.TestCase):
    def fixture(self, directory, *, width=4, maximum=8, extra=""):
        board = gate.load(gate.ROOT /
                          "boards/stm32f407zg_qiming_v41/board.json")
        board["id"] = "f407-sdio-configuration-software-fixture"
        board["bindings"] = [
            {"id": "sd", "controller": "SDIO", "route": "sdio-pc8-pd2"},
            {"id": "serial5", "controller": "UART5",
             "route": "uart5-pc12-pd2"}]
        board["provenance"] = []
        board["unknowns"] = ["Software socket fixture; no physical result"]
        (directory / "board.json").write_text(json.dumps(board))
        assembly = directory / "assembly.toml"
        assembly.write_text(
            'schema=2\nboard_package="."\nbackend="baremetal"\n'
            'clock="hse8-pll168"\noptimization="Os"\n'
            '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
            'flash_load_limit_bytes=65536\nstatic_ram_limit_bytes=32768\n'
            '[block.sd0]\nbinding="sd"\nmode="sdio-poll"\n'
            f'bus_width={width}\nmax_blocks={maximum}\npoll_budget=1000\n'
            'stop_timeout_us=25000\n' + extra)
        return assembly

    def test_exact_socket_produces_static_factory_without_media_activation(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = gate.resolve_ir(self.fixture(Path(temporary)))
            selected, = result.controllers
            self.assertEqual((selected.kind, selected.controller, selected.mode),
                             ("block", "SDIO", "sdio-poll"))
            self.assertEqual(selected.options.bus_width, 4)
            self.assertEqual(selected.options.max_blocks, 8)
            with self.assertRaises(FrozenInstanceError):
                selected.options.max_blocks = 32
            source = bindings.emit(result)[1]
            self.assertIn('#include "stm32f407_sdio.h"', source)
            self.assertIn("nx_stm32_sdio_prepare", source)
            self.assertIn("nx_stm32_sdio_close", source)
            self.assertIn("nx_stm32_sdio_ops", source)
            self.assertNotIn("nx_block_port_activate(", source)
            self.assertNotIn("SDIO_IRQHandler", source)
            self.assertNotIn("HAL_Init", source)
            self.assertIn("RCC_APB2ENR_SDIOEN", source)
            described = selected_contracts.describe(selected)
            self.assertEqual((described.unit, described.unit_bytes,
                              described.buffer_alignment,
                              described.max_transfer_units),
                             ("blocks", 512, 4, 8))
            self.assertEqual(described.features, ("POLLING",))
            self.assertFalse(selected.dma)

    def test_one_bit_mode_still_reserves_the_complete_six_pad_socket(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = gate.resolve_ir(self.fixture(Path(temporary), width=1))
            selected, = result.controllers
            self.assertEqual([pin["pin"] for pin in selected.pins],
                             ["PC8", "PC9", "PC10", "PC11", "PC12", "PD2"])
            claims = {claim.key for claim in selected.integration.resources}
            self.assertTrue({"pin:PC8", "pin:PC9", "pin:PC10", "pin:PC11",
                             "pin:PC12", "pin:PD2"}.issubset(claims))
            self.assertFalse(any(key.startswith(("dma:", "irq:"))
                                 for key in claims))

    def test_uart5_conflict_is_rejected_even_in_one_bit_mode(self):
        extra = ('[uart.debug]\nbinding="serial5"\nmode="irq-byte-event"\n'
                 'baud=115200\n[uart.debug.rx]\nformat="bytes"\n'
                 'capacity=16\n[uart.debug.irq]\npriority=5\n')
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "(?i)(conflict|owned)"):
                gate.resolve_ir(self.fixture(Path(temporary), width=1,
                                             extra=extra))

    def test_nonphysical_width_and_boolean_or_unbounded_limits_are_rejected(self):
        for width, maximum, extra in ((2, 8, ""), (4, 33, ""),
                                      (4, 8, "irq_priority=5\n")):
            with self.subTest(width=width, maximum=maximum, extra=extra):
                with tempfile.TemporaryDirectory() as temporary:
                    with self.assertRaises(ValueError):
                        gate.resolve_ir(self.fixture(Path(temporary),
                                                     width=width,
                                                     maximum=maximum,
                                                     extra=extra))


if __name__ == "__main__":
    unittest.main()
