"""Exact legacy transport modes retain immutable static construction facts."""
from pathlib import Path
import json
import tempfile
import unittest

from tools.configure import configure

ROOT = Path(__file__).resolve().parents[2]


class LegacyTransportTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def assembly(self, family, mode):
        f1 = family == "stm32f103"
        board = "stm32f103_reference" if f1 else "gd32f303_reference"
        binding = "uart1" if f1 else "uart0"
        clock = "hsi8-fixed" if f1 else "irc8"
        rx = ('format="bytes"\ncapacity=16\n' if mode == "dma-tx" else
              'format="blocks"\n')
        path = self.root / (family + ".toml")
        path.write_text(f'schema=2\nboard="{board}"\nbackend="baremetal"\n'
                        f'clock="{clock}"\n[factory]\ncapabilities=true\n'
                        '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                        f'[uart.serial]\nbinding="{binding}"\nmode="{mode}"\n'
                        f'baud=115200\n[uart.serial.rx]\n{rx}'
                        '[uart.serial.irq]\npriority=5\ncalls_os=false\n')
        return path

    def test_both_precise_families_generate_direct_dma_faces(self):
        for family in ("stm32f103", "gd32f303"):
            for mode in ("dma-tx", "dma-duplex"):
                with self.subTest(family=family, mode=mode):
                    path = self.assembly(family, mode)
                    ir = configure.resolve_ir(path)
                    self.assertEqual(ir.controllers[0].mode, mode)
                    keys = {claim.key for claim in ir.claims}
                    tx = ("dma:DMA1:stream7" if family == "stm32f103" else
                          "dma:DMA0:stream3")
                    rx = ("dma:DMA1:stream6" if family == "stm32f103" else
                          "dma:DMA0:stream4")
                    self.assertIn(tx, keys)
                    self.assertEqual(rx in keys, mode == "dma-duplex")
                    output = self.root / (family + mode)
                    configure.configure(path, output)
                    source = (output / "bindings.c").read_text()
                    method = "uart_duplex" if mode == "dma-duplex" else "uart_dma"
                    operations = ("nx_uart_v1_channel_dma_ops" if mode == "dma-duplex"
                                  else "nx_uart_v1_channel_dma_tx_ops")
                    self.assertIn(f"nx_{family}_{method}_prepare", source)
                    self.assertIn(
                        "static const nx_uart_v1_dma_config_t s_nx_config_serial = {",
                        source)
                    state_kind = "duplex" if mode == "dma-duplex" else "dma"
                    self.assertIn(
                        f"static nx_{family}_uart_{state_kind}_state_t s_nx_port_serial;",
                        source)
                    self.assertIn(
                        f"nx_{family}_{method}_prepare(&s_nx_port_serial, &s_nx_config_serial)",
                        source)
                    self.assertIn(
                        f".stop_irqs = nx_{family}_uart_dma_stop_irqs", source)
                    self.assertNotIn("static nx_result_t prepare_serial(void)", source)
                    self.assertIn("&" + operations, source)
                    self.assertIn("DMA", source)
                    self.assertNotIn("HAL_Init", source)
                    closure = json.loads((output / "configuration_code_paths.json").read_text())
                    self.assertIn("tools/configure/providers/legacy_transport.py", closure)
                    if mode == "dma-duplex":
                        self.assertIn("NX_UART_RX_BLOCKS", source)
                        self.assertNotIn("static uint8_t s_nx_rx_serial", source)

    def test_continuous_and_receive_only_fallback_are_rejected(self):
        for family in ("stm32f103", "gd32f303"):
            for mode in ("rx-dma-idle", "dma-duplex-continuous"):
                with self.subTest(family=family, mode=mode):
                    with self.assertRaises(ValueError):
                        configure.resolve_ir(self.assembly(family, mode))

    def test_i2c_has_independent_pb_lease_and_explicit_endpoint(self):
        for family, clock in (("stm32f103", "hsi8-fixed"),
                              ("gd32f303", "irc8")):
            with self.subTest(family=family):
                path = self.root / (family + "-i2c.toml")
                path.write_text(f'schema=2\nboard="{family}_reference"\n'
                    f'backend="baremetal"\nclock="{clock}"\n'
                    '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                    '[i2c.bus]\nbinding="i2c0"\nmode="irq-messages"\n'
                    'max_hz=80000\n[i2c.bus.irq]\npriority=0\ncalls_os=false\n'
                    '[i2c_device.sensor]\ncontroller="bus"\n'
                    'driver="i2c-endpoint"\naddress=80\n')
                output = self.root / (family + "-i2c")
                configure.configure(path, output)
                source = (output / "bindings.c").read_text()
                self.assertIn(f"nx_{family}_i2c_async_prepare", source)
                self.assertIn(f"static const nx_{family}_i2c_async_config_t", source)
                self.assertNotIn(".core.registers =", source)
                self.assertIn(".port = &s_nx_port_bus.core", source)
                self.assertNotIn("pins_acquire", source)
                self.assertNotIn("GPIOA->CRL", source)


if __name__ == "__main__":
    unittest.main()
