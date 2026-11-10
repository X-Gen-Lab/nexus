"""Exact SPI role, pads and frame options reach the real static C owner."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure, factory, io_traits
from tools.configure import selected_contracts


class SpiFramesModes(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def resolve(self, controller="SPI1", role="master", lines="two-line",
                fields="", *, part="STM32F407VET6", child="", maximum=500000,
                word_bits=16, latency=2):
        pads = {"SPI1": ("pa5", "pa6", "pa7", "pa4"),
                "SPI2": ("pb13", "pb14", "pb15", "pb12"),
                "SPI3": ("pc10", "pc11", "pc12", "pa15")}[controller]
        sck, miso, mosi, nss = pads
        data = ((miso, mosi) if lines == "two-line" else
                ((miso if role == "master" else mosi),)
                if lines == "receive-only" else
                ((mosi if role == "master" else miso),))
        route = (controller.lower() + "-frames-" + lines + "-" +
                 "-".join((sck, *data, nss)))
        board = configure.load(configure.ROOT /
            "tests/contracts/multi_controller_assembly/stm32-ve-board/board.json")
        board.update(id="spi-frames-software-fixture", soc=part,
                     bindings=[dict(id="wire", controller=controller, route=route)])
        (self.root / "board.json").write_text(json.dumps(board))
        contents = ('schema=2\nboard_package="."\nbackend="baremetal"\n'
                    'clock="hse8-pll168"\noptimization="Os"\n'
                    '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                    'flash_load_limit_bytes=65536\nstatic_ram_limit_bytes=16384\n'
                    '[spi.serial]\nbinding="wire"\nmode="irq-frames"\n'
                    f'max_hz={maximum}\n[spi.serial.irq]\npriority=5\ncalls_os=false\n'
                    '[spi.serial.frames]\nrole="' + role + '"\nlines="' +
                    lines + f'"\nword_bits={word_bits}\nirq_latency_us={latency}\n' + fields +
                    '\n' + child)
        assembly = self.root / "frames.toml"
        assembly.write_text(contents)
        return configure.resolve_ir(assembly)

    def test_all_three_instances_both_roles_and_line_sets_have_exact_factories(self):
        for controller in ("SPI1", "SPI2", "SPI3"):
            for part in ("STM32F407VET6", "STM32F407ZGT6"):
                for role in ("master", "slave"):
                    for lines in ("two-line", "receive-only", "one-wire"):
                        with self.subTest(controller=controller, part=part,
                                          role=role, lines=lines):
                            result = self.resolve(controller, role, lines, part=part)
                            item, = result.controllers
                            self.assertEqual(item.options.frames.role, role)
                            self.assertEqual(len(item.pins), 4 if lines == "two-line" else 3)
                            with self.assertRaises(FrozenInstanceError):
                                item.options.frames.role = "slave"
                            generated = bindings.emit(result)[1]
                            self.assertIn("nx_stm32_spi_frames_prepare", generated)
                            self.assertIn("nx_stm32_spi_frames_admit", generated)
                            self.assertIn(controller + "_IRQHandler", generated)
                            self.assertIn("NX_SPI_FRAMES_" + role.upper(), generated)
                            self.assertIn("nx_factory_spi_frames", factory.emit(result)[0])
                            self.assertNotIn("NX_SPI_ID_SERIAL", factory.emit(result)[0])
                            self.assertIn("NX_SPI_FRAMES_ID_SERIAL", factory.emit(result)[0])
                            traits = selected_contracts.describe(item)
                            self.assertEqual(traits.max_transfer_units, "UINT16_MAX")
                            self.assertEqual(traits.unit, "words")
                            self.assertEqual(traits.unit_bytes, 2)
                            self.assertEqual(io_traits.describe(item).fields["word_bits"], 16)

    def test_ti_crc_options_reach_the_actual_driver(self):
        result = self.resolve(fields='format="ti"\ncrc_polynomial=4129')
        generated = bindings.emit(result)[1]
        self.assertIn("NX_SPI_FRAMES_TI", generated)
        self.assertIn(".crc_polynomial = 4129u", generated)
        self.assertIn(".irq_latency_us = 2u", generated)

    def test_traits_use_the_nominal_frame_factory_id(self):
        result = self.resolve()
        header, source = io_traits.emit_controllers(result.controllers)
        self.assertIn("nx_factory_spi_frames_traits", "\n".join(header))
        self.assertIn("NX_SPI_FRAMES_ID_SERIAL", "\n".join(source))
        self.assertNotIn("NX_SPI_ID_SERIAL", "\n".join(source))

    def test_slow_slave_clocks_have_no_master_divide_by_256_floor(self):
        for controller in ("SPI1", "SPI2", "SPI3"):
            result = self.resolve(controller, "slave", maximum=1000)
            self.assertEqual(result.controllers[0].options.max_hz, 1000)
        with self.assertRaises(ValueError):
            self.resolve(maximum=1000)

    def test_receive_tail_budget_includes_timer_rounding(self):
        with self.assertRaises(ValueError):
            self.resolve(lines="receive-only", maximum=1000000,
                         word_bits=8, latency=6)

    def test_invalid_polynomial_and_unknown_options_reject(self):
        for fields in ('crc_polynomial=2', 'irq_latency_ticks=10',
                       'format="ti"\nlsb_first=true',
                       'format="ti"\nmode=3'):
            with self.subTest(fields=fields), self.assertRaises(ValueError):
                self.resolve(fields=fields)

    def test_an_endpoint_cannot_bypass_the_frame_owner(self):
        with self.assertRaises(ValueError):
            self.resolve(child='[spi_device.sensor]\ncontroller="serial"\n'
                         'driver="spi-endpoint"\nmode=0\nmax_hz=500000\n')


if __name__ == "__main__":
    unittest.main()
