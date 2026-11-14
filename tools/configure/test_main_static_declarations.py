"""Core F4 modes retain typed static data until the common C emitter."""
import subprocess
import sys
import unittest

from tools.configure import bindings, configure
from tools.configure.data_provider import DataProvider
from tools.configure.providers import for_selection
from tools.configure.static_ir import Declaration, IrqConnection


class MainStaticDeclarations(unittest.TestCase):
    def test_real_package_fixture_has_no_family_python_fallback(self):
        script = (
            "import sys; "
            "from tools.configure.tests.f4_fixture import load_f4_fixture; "
            "from tools.configure.data_provider import DataProvider; "
            "selected = load_f4_fixture('STM32F407ZGT6'); "
            "assert isinstance(selected.provider, DataProvider); "
            "assert selected.provider.package is selected.package; "
            "assert selected.part == selected.device.part == 'STM32F407ZGT6'; "
            "assert 'CAN1' in selected.provider.soc['controllers']; "
            "other = load_f4_fixture('STM32F411RET6'); "
            "assert 'CAN1' not in other.provider.soc['controllers']; "
            "assert selected.package.fact('soc') in selected.snapshots; "
            "assert selected.package.fact('bindings') in selected.snapshots; "
            "assert all(selected.package.fact(name) in selected.snapshots "
            "for name in selected.package.facts); "
            "assert all(path in selected.snapshots for path in selected.package.binding_files); "
            "assert not any(name == 'tools.configure.stm32f4' or "
            "name.startswith('tools.configure.stm32f4.') for name in sys.modules)"
        )
        completed = subprocess.run([sys.executable, "-c", script], cwd=configure.ROOT,
                                   capture_output=True, text=True)
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_real_selected_modes_have_no_c_initializer_fragments(self):
        cases = (
            ("uart_rx_compact_assembly/f407-irq.toml", "uart"),
            ("uart_rx_compact_assembly/f407-dma-tx.toml", "uart"),
            ("uart_rx_compact_assembly/f407-dma-duplex.toml", "uart"),
            ("io_evolution_assembly/f407-uart7e2.toml", "uart"),
            ("stm32_assembly/zg-spi.toml", "spi"),
            ("stm32_assembly/zg-i2c-dma.toml", "i2c"),
            ("stm32_assembly/ve-i2c.toml", "i2c"),
            ("stm32_assembly/ve-exti.toml", "exti"),
            ("stm32_assembly/zg-pwm.toml", "pwm"),
            ("stm32_assembly/ve-adc.toml", "adc"),
            ("stm32_assembly/zg-flash.toml", "flash"),
            ("adc_group_assembly/zg-triple-timer-injected-blocks.toml", "adc_group"),
            ("fsmc_assembly/zg-all.toml", "external_bus"),
            ("i2c_target/ve-smbus.toml", "i2c_target"),
            ("timer_waveform_assembly/zg-sequence.toml", "timer_sequence"),
            ("usb_device_hs_assembly/stm32-zg-ulpi-dma.toml", "usb_device"),
            ("usb_host_assembly/stm32-zg-ulpi-dma.toml", "usb_host"),
            ("ethernet_assembly/stm32-zg.toml", "eth"),
            ("stm32f4_qspi/f479ni-qspi-map.toml", "external_bus"),
            ("stm32f4_sdram/f479ni-sdram.toml", "external_bus"),
            ("multi_controller_assembly/stm32-ve-encoders-baremetal.toml", "encoder"),
            ("spi_frames_assembly/three-frames-ve.toml", "spi"),
            ("rtc_bypass_assembly/provision-empty.toml", "rtc"),
        )
        for relative, kind in cases:
            with self.subTest(assembly=relative):
                result = configure.resolve_ir(
                    configure.ROOT / "tests/contracts" / relative)
                selected = [item for item in result.controllers if item.kind == kind]
                self.assertTrue(selected)
                provider = for_selection(result)
                self.assertIsInstance(provider, DataProvider)
                for item in selected:
                    construction = provider.constructor(
                        item, result, result.board_bindings, result.routes,
                        result.devices)
                    self.assertEqual(construction.definitions, ())
                    self.assertTrue(construction.static_declarations)
                    self.assertTrue(all(isinstance(node, Declaration)
                                        for node in construction.static_declarations))
                header, source = bindings.emit(result)
                self.assertTrue(header)
                self.assertTrue(source)

    def test_shared_exti_connections_retain_static_array_nodes(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/stm32_assembly/ve-exti.toml")
        provider = for_selection(result)
        self.assertIsInstance(provider, DataProvider)
        nodes = provider.shared_irqs(result)
        self.assertTrue(any(isinstance(node, Declaration) for node in nodes))
        self.assertTrue(any(isinstance(node, IrqConnection) for node in nodes))
        self.assertTrue(all(isinstance(node, (Declaration, IrqConnection))
                            for node in nodes))


if __name__ == "__main__":
    unittest.main()
