"""Reviewed DMA receive modes resolve and emit actual immutable provider facts."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


class AdcContinuousConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def authored(self, family, mode="trigger-dma-continuous", samples="[0, 7]",
                 irq=True):
        if family == "stm32f407":
            board = gate.ROOT / "tests/contracts/stm32_assembly/zg_board"
            clock = "hse8-pll168"
        else:
            board = gate.ROOT / "tests/contracts/gd32_assembly/software_board"
            clock = "hxtal25-pll200"
        content = (f'schema=2\nboard_package="{board}"\n'
                   f'backend="baremetal"\nclock="{clock}"\n'
                   '[factory]\ncapabilities=true\n'
                   '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                   '[adc.samples]\nbinding="adc_scan"\n'
                   f'mode="{mode}"\nchannels=[0,1]\n'
                   f'sample_times={samples}\nreference_mv=3300\n')
        if family == "gd32f470":
            content += 'timeout_ms=100\n'
        if irq:
            content += '[adc.samples.irq]\npriority=5\ncalls_os=false\n'
        path = self.root / "assembly.toml"
        path.write_text(content)
        return path

    def test_continuous_keeps_reviewed_dma_timer_and_immutable_sample_codes(self):
        for family, prefix, timer, dma in (
                ("stm32f407", "stm32", "TIM3", "DMA2:stream4"),
                ("gd32f470", "gd32", "TIMER2", "DMA1:stream0")):
            with self.subTest(family=family):
                assembly = self.authored(family)
                ir = gate.resolve_ir(assembly)
                adc = ir.controllers[0]
                self.assertEqual(adc.mode, "trigger-dma-continuous")
                self.assertEqual(adc.options.sample_times, (0, 7))
                with self.assertRaises(FrozenInstanceError):
                    adc.options.sample_times = (1, 1)
                claims = {item.key for item in ir.claims}
                self.assertIn("controller:" + timer, claims)
                self.assertIn("dma:" + dma, claims)
                output = self.root / family
                gate.configure(assembly, output)
                source = (output / "bindings.c").read_text()
                if family == "stm32f407":
                    self.assertIn("nx_stm32_adc_continuous_prepare", source)
                    self.assertIn("nx_stm32_adc_stream_config_t", source)
                    self.assertIn("s_nx_adc_samples_samples[] = { 0u, 7u }", source)
                else:
                    self.assertIn("nx_gd32_adc_continuous_cold_prepare", source)
                    self.assertIn(".samples = s_nx_adc_samples_samples", source)
                self.assertIn("NX_FACTORY_CAP_RX_DMA", source)
                self.assertIn("NX_FACTORY_CAP_BLOCK_STREAM", source)

    def test_continuous_requires_explicit_bounded_irq_policy(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                with self.assertRaisesRegex(ValueError, "IRQ priority"):
                    gate.resolve_ir(self.authored(family, irq=False))

    def test_sample_codes_remain_integer_encodings_not_arbitrary_cycles(self):
        for family in ("stm32f407", "gd32f470"):
            for samples in ("[0,8]", "[0,true]", "[0]", "[0,7,0]"):
                with self.subTest(family=family, samples=samples):
                    with self.assertRaisesRegex(ValueError, "sample"):
                        gate.resolve_ir(self.authored(family, samples=samples))

    def test_gd_finite_mode_retains_its_implemented_fixed_sample_contract(self):
        with self.assertRaisesRegex(ValueError, "fifteen cycles"):
            gate.resolve_ir(self.authored("gd32f470", mode="trigger-dma"))


class UartReceiveConfigurationTests(unittest.TestCase):
    MODES = ("rx-dma-idle", "dma-duplex", "rx-dma-continuous",
             "dma-duplex-continuous")

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def authored(self, family, mode, capacity=False, reserved_tx=False,
                 binding=None):
        if family == "stm32f407":
            board = gate.ROOT / "tests/contracts/stm32_assembly/zg_board"
            clock, binding_default, dma = "hse8-pll168", "uart1", "DMA2"
        else:
            board = gate.ROOT / "tests/contracts/gd32_assembly/software_board"
            clock, binding_default, dma = "hxtal25-pll200", "uart0", "DMA1"
        if reserved_tx:
            data = json.loads((board / "board.json").read_text())
            data["reserved_resources"].append(f"dma:{dma}:stream7:channel4")
            board = self.root / "board"
            board.mkdir(exist_ok=True)
            (board / "board.json").write_text(json.dumps(data))
        content = (f'schema=2\nboard_package="{board}"\n'
                   f'backend="baremetal"\nclock="{clock}"\n'
                   '[factory]\ncapabilities=true\n'
                   '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                   '[uart.link]\n'
                   f'binding="{binding or binding_default}"\n'
                   f'mode="{mode}"\nbaud=115200\n'
                   '[uart.link.rx]\nformat="blocks"\n')
        if capacity:
            content += 'capacity=4\n'
        content += '[uart.link.irq]\npriority=5\ncalls_os=false\n'
        path = self.root / "assembly.toml"
        path.write_text(content)
        return path

    def test_each_receive_mode_claims_only_its_reviewed_directional_engines(self):
        for family, prefix, dma in (("stm32f407", "stm32", "DMA2"),
                                    ("gd32f470", "gd32", "DMA1")):
            for mode in self.MODES:
                with self.subTest(family=family, mode=mode):
                    path = self.authored(family, mode)
                    ir = gate.resolve_ir(path)
                    uart = ir.controllers[0]
                    duplex = mode.startswith("dma-duplex")
                    expected = ((f"{dma}:stream7:channel4",) if duplex else ())
                    expected += (f"{dma}:stream2:channel4",)
                    self.assertEqual(uart.dma, expected)
                    self.assertIsNone(uart.options.rx_capacity)
                    claims = {item.key for item in ir.claims}
                    self.assertIn(f"dma:{dma}:stream2", claims)
                    self.assertEqual(f"dma:{dma}:stream7" in claims, duplex)
                    output = self.root / (family + "-" + mode)
                    gate.configure(path, output)
                    source = (output / "bindings.c").read_text()
                    implementation = "uart_duplex" if duplex else "uart_receive"
                    self.assertIn(
                        f"nx_{prefix}_{implementation}_cold_prepare", source)
                    self.assertNotIn(".dma_transmit", source)
                    self.assertIn("NX_FACTORY_CAP_RX_DMA", source)
                    self.assertNotIn("static uint8_t s_nx_rx_link[", source)
                    self.assertNotIn("static nx_uart_rx_event_t s_nx_rx_link[", source)
                    self.assertIn(f"void {dma}_", source)
                    self.assertIn("s_nx_platform_dma_clocks_added", source)

    def test_receive_blocks_reject_a_hidden_generated_payload_ring(self):
        for family in ("stm32f407", "gd32f470"):
            for mode in self.MODES:
                with self.subTest(family=family, mode=mode):
                    with self.assertRaisesRegex(ValueError, "caller-owned"):
                        gate.resolve_ir(self.authored(family, mode, capacity=True))

    def test_rx_only_does_not_acquire_a_reserved_tx_dma_engine(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                gate.resolve_ir(self.authored(family, "rx-dma-idle",
                                             reserved_tx=True))
                with self.assertRaisesRegex(ValueError, "Resource conflict dma"):
                    gate.resolve_ir(self.authored(family, "dma-duplex",
                                                 reserved_tx=True))

    def test_gd_second_uart_cannot_inherit_first_controller_dma_routes(self):
        with self.assertRaisesRegex(ValueError, "Unsupported mode"):
            gate.resolve_ir(self.authored("gd32f470", "rx-dma-idle",
                                         binding="uart1"))


if __name__ == "__main__":
    unittest.main()
