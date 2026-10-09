"""Behavioral rejection/identity checks for Board resources and Flash layouts."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from board_package import block_boundaries, emit, validate_layout, validate_manifest

ROOT = Path(__file__).resolve().parents[2]


class BoardPackageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='nexus board ')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.manifest = json.loads((ROOT / 'boards/stm32f4discovery/board.json').read_text())
        for name in self.manifest['inputs']:
            (self.root / name).write_bytes((ROOT / 'boards/stm32f4discovery' / name).read_bytes())
        self.config = {'CONFIG_PLATFORM_NAME': 'stm32', 'CONFIG_STM32F407VG': True,
                       'CONFIG_STM32_HSE_VALUE': '8000000', 'CONFIG_LINKER_FLASH_SIZE': '0x100000', 'CONFIG_LINKER_RAM_SIZE': '0x20000', 'CONFIG_STM32_GPIO_ENABLE': True,
                       'CONFIG_STM32_UART_ENABLE': True, 'CONFIG_STM32_SPI_ENABLE': True,
                       'CONFIG_INSTANCE_STM32_GPIOD_PIN12': False, 'CONFIG_OSAL_FREERTOS': True,
                       'CONFIG_STM32_SPI1_ENABLE': True, 'CONFIG_INSTANCE_STM32_UART_1': True}
        self.write_manifest()

    def write_manifest(self):
        (self.root / 'board.json').write_text(json.dumps(self.manifest))

    def validate(self):
        self.write_manifest()
        return validate_manifest(self.root, self.config)

    def test_valid_resources_and_changes_bind_actual_input_identity(self):
        first = self.validate()[2]['sha256']
        path = self.root / 'uart.c'
        path.write_text(path.read_text() + '\n/* board wiring revision */\n')
        self.assertNotEqual(first, self.validate()[2]['sha256'])

    def test_clock_and_density_mismatch_are_rejected(self):
        self.manifest['hse_hz'] = 25000000
        with self.assertRaisesRegex(ValueError, 'clock conflict'):
            self.validate()
        self.manifest['hse_hz'] = 8000000
        self.manifest['soc'] = 'stm32f407ve'
        with self.assertRaisesRegex(ValueError, 'density conflict'):
            self.validate()

    def test_gpio_and_bus_pin_conflict_is_rejected(self):
        self.manifest['resources'][0]['pins'][0].update(port='A', pin=2)
        with self.assertRaisesRegex(ValueError, 'pin ownership conflict'):
            self.validate()

    def test_af_and_signal_route_are_checked(self):
        for changed in ({'af': 5}, {'port': 'B'}, {'signal': 'unknown'}):
            saved = copy.deepcopy(self.manifest)
            self.manifest['resources'][1]['pins'][0].update(changed)
            with self.assertRaises(ValueError):
                self.validate()
            self.manifest = saved

    def test_irq_priority_and_dma_route_are_checked(self):
        for key, value, expected in [('irq', 81, 'IRQ mismatch'), ('priority', 4, 'syscall mask')]:
            saved = copy.deepcopy(self.manifest)
            self.manifest['resources'][1][key] = value
            with self.assertRaisesRegex(ValueError, expected):
                self.validate()
            self.manifest = saved
        self.manifest['resources'][2]['dma'][0]['stream'] = 2
        with self.assertRaisesRegex(ValueError, 'DMA route mismatch'):
            self.validate()

    def test_missing_controller_binding_does_not_succeed(self):
        self.manifest['resources'].pop(1)
        with self.assertRaisesRegex(ValueError, 'missing Board resource'):
            self.validate()

    def test_outside_missing_or_symlink_inputs_are_rejected(self):
        for input_name in ('../CMakeLists.txt', 'missing.c'):
            self.manifest['inputs'].append(input_name)
            with self.assertRaises(ValueError):
                self.validate()
            self.manifest['inputs'].pop()
        path = self.root / 'uart.c'
        path.unlink()
        path.symlink_to(ROOT / 'boards/stm32f4discovery/uart.c')
        with self.assertRaisesRegex(ValueError, 'nonregular'):
            self.validate()

    def test_gpio_binding_and_physical_memory_must_match_effective_config(self):
        self.config['CONFIG_INSTANCE_STM32_GPIOA_PIN2'] = True
        with self.assertRaisesRegex(ValueError, 'GPIO/controller pin ownership'):
            self.validate()
        self.config['CONFIG_INSTANCE_STM32_GPIOA_PIN2'] = False
        self.config['CONFIG_INSTANCE_STM32_GPIOB_PIN5'] = True
        with self.assertRaisesRegex(ValueError, 'GPIO missing Board resource'):
            self.validate()
        self.config['CONFIG_INSTANCE_STM32_GPIOB_PIN5'] = False
        self.config['CONFIG_LINKER_RAM_SIZE'] = '0x30000'
        with self.assertRaisesRegex(ValueError, 'physical RAM'):
            self.validate()

    def test_unbound_controller_and_incomplete_selected_dma_are_rejected(self):
        self.config['CONFIG_STM32_SPI2_ENABLE'] = True
        with self.assertRaisesRegex(ValueError, 'missing Board resource'):
            self.validate()
        self.config['CONFIG_STM32_SPI2_ENABLE'] = False
        self.config['CONFIG_STM32_SPI_USE_DMA'] = True
        self.manifest['resources'][2]['dma'].pop()
        with self.assertRaisesRegex(ValueError, 'complete Board bindings'):
            self.validate()

    def test_disabled_controller_does_not_claim_active_pins(self):
        self.config['CONFIG_STM32_UART_ENABLE'] = False
        self.config['CONFIG_INSTANCE_STM32_UART_1'] = False
        self.manifest['resources'][0]['pins'][0].update(port='A', pin=2)
        self.validate()


class FlashLayoutTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'layout.json'
        self.layout = {'schema': 1, 'soc': 'stm32f407vg',
                       'image': {'offset': 0, 'size': 0xc0000},
                       'regions': [{'name': 'params_a', 'offset': 0xc0000, 'size': 0x20000},
                                   {'name': 'params_b', 'offset': 0xe0000, 'size': 0x20000}]}

    def validate(self, soc='stm32f407vg'):
        self.path.write_text(json.dumps(self.layout))
        return validate_layout(self.path, soc)

    def test_full_flash_default_reserves_no_product_region(self):
        for soc, capacity in [('stm32f407ve', 0x80000), ('stm32f407vg', 0x100000), ('gd32f470zg', 0x100000)]:
            resolved = validate_layout(None, soc)
            self.assertEqual(resolved['image']['size'], capacity)
            self.assertEqual(resolved['regions'], [])

    def test_nonuniform_stm32_and_gd32_block_geometry(self):
        self.assertEqual(len(block_boundaries('stm32f407ve')), 9)
        self.assertEqual(len(block_boundaries('stm32f407vg')), 13)
        self.assertEqual(len(block_boundaries('gd32f470zg')), 257)
        self.validate()
        self.layout = {'schema': 1, 'soc': 'gd32f470zg', 'image': {'offset': 0, 'size': 0xfc000},
                       'regions': [{'name': 'parameters', 'offset': 0xfc000, 'size': 0x4000}]}
        self.validate('gd32f470zg')

    def test_overlapping_image_outside_block_and_wrong_soc_are_rejected(self):
        for changed, expected in [({'offset': 0xa0000}, 'overlap'), ({'offset': 0xc0001}, 'erase block'),
                                  ({'size': 0x80000}, 'outside physical')]:
            saved = copy.deepcopy(self.layout)
            self.layout['regions'][0].update(changed)
            with self.assertRaisesRegex(ValueError, expected):
                self.validate()
            self.layout = saved
        with self.assertRaisesRegex(ValueError, 'SoC conflict'):
            self.validate('stm32f407ve')

    def test_duplicate_name_overflow_and_relocated_image_are_rejected(self):
        self.layout['regions'][1]['name'] = 'params_a'
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            self.validate()
        self.layout['regions'][1]['name'] = 'params_b'
        self.layout['regions'][0]['offset'] = 0xffffffff
        with self.assertRaisesRegex(ValueError, 'outside physical'):
            self.validate()
        self.layout = {'schema': 1, 'soc': 'stm32f407vg', 'image': {'offset': 0x4000, 'size': 0x4000}, 'regions': []}
        with self.assertRaisesRegex(ValueError, 'nonzero image origin'):
            self.validate()

    def test_one_input_emits_matching_linker_header_and_identity(self):
        layout = self.validate()
        out = Path(self.tmp.name) / 'generated'
        manifest = {'id': 'fixture-board', 'soc': 'stm32f407vg', 'interface_target': 'fixture_board', 'object_targets': []}
        emit(out, manifest, {'platform': 'stm32', 'ram_size': 0x20000}, {'sha256': 'a' * 64}, [], layout, ROOT)
        self.assertIn(layout['sha256'], (out / 'nx_flash_layout.h').read_text())
        self.assertEqual(json.loads((out / 'layout.json').read_text()), layout)
        script = (out / 'firmware.ld').read_text()
        self.assertIn('LENGTH = 786432', script)
        self.assertIn('__nexus_board_sha256_0 = 0xaaaaaaaa;', script)
        self.assertIn('__nexus_region_params_a_start = 0x080c0000', script)
        self.assertIn('params_a, UINT32_C(786432), UINT32_C(131072)', (out / 'nx_flash_layout.h').read_text())


if __name__ == '__main__':
    unittest.main()
