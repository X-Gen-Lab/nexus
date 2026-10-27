"""Exact legacy device admission and generated static factory contracts."""
from pathlib import Path
import tempfile
import unittest
import unittest.mock as mock

from tools.configure import bindings, configure
from tools.configure.providers.common import ConfigurationError
from tools.configure.providers import maintained

ROOT = Path(__file__).resolve().parents[2]


class LegacyDevicesTests(unittest.TestCase):
    def fixture(self, family):
        return ROOT / f"tests/contracts/mainstream_assembly/{family}-devices.toml"

    def test_constructor_rejects_unresolved_mapping(self):
        for family in ("stm32f103", "gd32f303"):
            with self.subTest(family=family):
                provider = maintained(family)
                with self.assertRaisesRegex(ConfigurationError, "typed"):
                    provider.constructor({}, None, {}, {}, ())

    def test_exact_devices_generate_private_static_states(self):
        for family in ("f103", "gd303"):
            with self.subTest(family=family):
                result = configure.resolve_ir(self.fixture(family), ROOT)
                _, source = bindings.emit(result)
                prefix = "stm32f103" if family == "f103" else "gd32f303"
                self.assertIn(f"nx_{prefix}_adc_prepare", source)
                self.assertIn(f"nx_{prefix}_pwm_prepare", source)
                self.assertIn(f"nx_{prefix}_flash_prepare", source)
                self.assertNotIn(".startup_delay_us = ", source)
                self.assertNotIn("malloc(", source)
                physical = next(item for item in result.controllers
                                if item.kind == "flash")
                provider = maintained(result.soc_family)
                construction = provider.constructor(
                    physical, result, result.board_bindings,
                    result.routes, result.devices)
                self.assertFalse(construction.retain_on_failed_init)
                self.assertEqual(provider.FLASH_PROGRAM_UNIT, 2)
                self.assertEqual(provider.FLASH_INPUT_ALIGNMENT, 1)
                if family == "gd303":
                    self.assertIn("NX_GD303_IRQ_AVAILABLE(EXTI1_IRQn)", source)
                    self.assertNotIn("EXTI1__IRQn", source)

    def test_adc_rejects_unreviewed_channel_sample_and_reference(self):
        for family in ("f103", "gd303"):
            text = self.fixture(family).read_text()
            for old, new in (("channels = [0]", "channels = [1]"),
                             ("sample_times = [7]", "sample_times = [0]"),
                             ("reference_mv = 3300", "reference_mv = 1800")):
                with self.subTest(family=family, new=new):
                    with tempfile.TemporaryDirectory() as directory:
                        path = Path(directory) / "invalid.toml"
                        path.write_text(text.replace(old, new))
                        with self.assertRaises(ConfigurationError):
                            configure.resolve_ir(path, ROOT)

    def test_non_exact_timer_divider_is_rejected(self):
        for family in ("f103", "gd303"):
            text = self.fixture(family).read_text().replace(
                "tick_hz = 1000000", "tick_hz = 3000000")
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "invalid.toml"
                path.write_text(text)
                with self.assertRaises(ConfigurationError):
                    configure.resolve_ir(path, ROOT)

    def test_source_sdk_requires_each_new_gd303_device_header(self):
        from tools.configure.providers import descriptor
        payload = descriptor.maintained("gd32f303").owned_payload
        prefix = ("vendors/gigadevice/gd32f30x/Firmware/"
                  "GD32F30x_standard_peripheral/Include/")
        for unit in ("adc", "exti"):
            self.assertIn(prefix + f"gd32f30x_{unit}.h", payload)

    def test_physical_flash_factory_rejects_missing_exact_provider_contract(self):
        from tools.configure import factory
        result = configure.resolve_ir(self.fixture("f103"), ROOT)
        provider = maintained("stm32f103")
        for attribute in ("FLASH_PROGRAM_UNIT", "FLASH_INPUT_ALIGNMENT"):
            with self.subTest(attribute=attribute):
                with mock.patch.object(provider, attribute, None):
                    delattr(provider, attribute)
                    with self.assertRaises(ConfigurationError):
                        factory.emit(result)
