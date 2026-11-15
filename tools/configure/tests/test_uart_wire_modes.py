"""Exact F407 wire modes resolve into immutable cold facts and real factories."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure, factory, io_traits
from tools.configure import selected_contracts


class UartWireModes(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def resolve(self, controller, route, wire, *, part="STM32F407VET6"):
        board = configure.load(configure.ROOT /
            "tests/contracts/multi_controller_assembly/stm32-ve-board/board.json")
        board.update(id="serial-wire-software-fixture", soc=part,
                     bindings=[dict(id="wire", controller=controller, route=route)])
        (self.root / "board.json").write_text(json.dumps(board))
        contents = ('schema=2\nboard_package="."\nbackend="baremetal"\n'
                    'clock="hse8-pll168"\noptimization="Os"\n'
                    '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                    'flash_load_limit_bytes=65536\nstatic_ram_limit_bytes=16384\n'
                    '[uart.serial]\nbinding="wire"\nmode="irq-wire"\nbaud=9600\n'
                    '[uart.serial.rx]\nformat="events"\ncapacity=8\n'
                    '[uart.serial.irq]\npriority=5\ncalls_os=false\n'
                    '[uart.serial.wire]\n' + wire + '\n')
        assembly = self.root / "wire.toml"
        assembly.write_text(contents)
        return configure.resolve_ir(assembly)

    def test_generated_wire_tuple_uses_the_private_f4_configuration_type(self):
        result = self.resolve("USART1", "usart1-pa9-pa10",
                              'protocol="irda"\nprescaler=42')
        source = bindings.emit(result)[1]
        self.assertIn("static const nx_stm32_uart_wire_config_t", source)
        self.assertNotIn("static const nx_uart_wire_config_t", source)

    def test_six_half_duplex_modes_bind_only_the_open_drain_transmit_pad(self):
        for controller, pad in (("USART1", "pa9"), ("USART2", "pa2"),
                                ("USART3", "pb10"), ("UART4", "pa0"),
                                ("UART5", "pc12"), ("USART6", "pc6")):
            for part in ("STM32F407VET6", "STM32F407ZGT6"):
                with self.subTest(controller=controller, part=part):
                    result = self.resolve(controller,
                        f"{controller.lower()}-half-duplex-{pad}",
                        'protocol="half-duplex"', part=part)
                    item, = result.controllers
                    self.assertEqual(len(item.pins), 1)
                    self.assertEqual(item.options.wire.protocol, "half-duplex")
                    with self.assertRaises(FrozenInstanceError):
                        item.options.wire.protocol = "lin"
                    generated = bindings.emit(result)[1]
                    self.assertIn("NX_UART_WIRE_HALF_DUPLEX", generated)
                    self.assertIn(".wire = &s_nx_uart_wire_serial", generated)
                    self.assertIn(".pin_count = 1u", generated)
                    self.assertIn("nx_stm32_uart_wire_cold_prepare", generated)
                    self.assertIn("nx_factory_uart", factory.emit(result)[0])

    def test_four_usarts_bind_sync_clock_and_smartcard_with_exact_pads(self):
        for controller, tx, rx, ck in (
                ("USART1", "pa9", "pa10", "pa8"),
                ("USART2", "pa2", "pa3", "pa4"),
                ("USART3", "pb10", "pb11", "pb12"),
                ("USART6", "pc6", "pc7", "pc8")):
            with self.subTest(controller=controller):
                result = self.resolve(controller,
                    f"{controller.lower()}-synchronous-{tx}-{rx}-{ck}",
                    'protocol="synchronous"\nclock_polarity=true\n'
                    'clock_phase=true\nlast_bit_clock=true')
                self.assertEqual(len(result.controllers[0].pins), 3)
                generated = bindings.emit(result)[1]
                self.assertIn("NX_UART_WIRE_SYNCHRONOUS", generated)
                self.assertIn(".clock_phase = true", generated)
                result = self.resolve(controller,
                    f"{controller.lower()}-smartcard-{tx}-{ck}",
                    'protocol="smartcard"\nprescaler=21\nguard_time=16\n'
                    'smartcard_nack=true')
                self.assertEqual(len(result.controllers[0].pins), 2)
                self.assertIn("NX_UART_WIRE_SMARTCARD", bindings.emit(result)[1])
                item, = result.controllers
                self.assertEqual(io_traits.describe(item).fields["parity"], 1)
                self.assertEqual(io_traits.describe(item).fields["stop_half_bits"], 3)
                self.assertEqual(selected_contracts.describe(item).max_transfer_units, 1)

    def test_all_six_lin_and_irda_modes_keep_both_data_directions(self):
        for controller, tx, rx in (
                ("USART1", "pa9", "pa10"), ("USART2", "pa2", "pa3"),
                ("USART3", "pb10", "pb11"), ("UART4", "pa0", "pa1"),
                ("UART5", "pc12", "pd2"), ("USART6", "pc6", "pc7")):
            for protocol, fields in (("lin", 'lin_break_bits=11'),
                                     ("irda", 'prescaler=42')):
                with self.subTest(controller=controller, protocol=protocol):
                    result = self.resolve(controller,
                        f"{controller.lower()}-{tx}-{rx}",
                        f'protocol="{protocol}"\n{fields}')
                    self.assertEqual(len(result.controllers[0].pins), 2)
                    self.assertIn("NX_UART_WIRE_" + protocol.upper(),
                                  bindings.emit(result)[1])

    def test_invalid_wire_options_are_never_silently_ignored(self):
        for wire in ('protocol="lin"\nlin_break_bits=9',
                     'protocol="half-duplex"\nprescaler=42',
                     'protocol="irda"\nprescaler=0',
                     'protocol="smartcard"\nprescaler=32',
                     'protocol="synchronous"\nclock_phase=1',
                     'protocol="lin"\nlin_break_bits=11\nunknown=true'):
            with self.subTest(wire=wire), self.assertRaises(ValueError):
                self.resolve("USART1", "usart1-pa9-pa10", wire)

    def test_uart_four_and_five_cannot_claim_sync_smartcard_or_modem_control(self):
        for controller, tx, rx in (("UART4", "pa0", "pa1"),
                                   ("UART5", "pc12", "pd2")):
            for protocol, fields in (("synchronous", ""),
                                     ("smartcard", "prescaler=21"),
                                     ("asynchronous", 'flow="rts-cts"')):
                with self.subTest(controller=controller, protocol=protocol):
                    with self.assertRaises(ValueError):
                        self.resolve(controller, f"{controller.lower()}-{tx}-{rx}",
                                     f'protocol="{protocol}"\n{fields}')


if __name__ == "__main__":
    unittest.main()
