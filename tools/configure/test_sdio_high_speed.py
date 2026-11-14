"""SDIO speed is explicit typed policy, separate from its transport engine."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure
from tools.configure.stm32f4 import clocks, contract
from tools.configure import test_f407_block_configuration as fixtures
from tools.configure.tests.native_fixture import native_fixture


class SdioHighSpeedConfigurationTests(unittest.TestCase):
    def resolve(self, *, speed="high-speed", profile="hse8-pll150-sdio375",
                protocol="sd-memory", width=4):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        path = fixtures.F407BlockConfigurationTests().fixture(Path(temporary.name), width=width)
        text = path.read_text().replace('clock="hse8-pll168"', f'clock="{profile}"')
        text += f'protocol="{protocol}"\nspeed="{speed}"\n'
        path.write_text(text)
        return configure.resolve_ir(path, test_support=native_fixture(path))

    def test_closed_375_profile_is_typed_and_emits_static_speed(self):
        result = self.resolve()
        item, = result.controllers
        self.assertEqual("high-speed", item.options.speed)
        self.assertEqual(75000000, item.integration.clock_hz)
        self.assertIn(".high_speed = true", bindings.emit(result)[1])

    def test_closed_profile_is_not_granted_to_other_f4_device_policies(self):
        summary = dict(hse_hz=8000000, core_hz=150000000,
                       apb1_hz=37500000, apb2_hz=75000000)
        selected = clocks.plan("hse8-pll150-sdio375", summary)
        contract.validate_clock_policy("rm0090-f407-run", selected)
        for policy in ("rm0090-run", "rm0383-run", "rm0390-run", "rm0386-run"):
            with self.subTest(policy=policy), self.assertRaises(ValueError):
                contract.validate_clock_policy(policy, selected)

    def test_legacy_profile_retains_default_speed(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = fixtures.F407BlockConfigurationTests().fixture(Path(temporary))
            result = configure.resolve_ir(path, test_support=native_fixture(path))
        self.assertEqual("default", result.controllers[0].options.speed)
        self.assertIn(".high_speed = false", bindings.emit(result)[1])

    def test_closed_profile_rejects_legacy_sdio_functions_and_narrow_bus(self):
        for values in ({"speed": "default"}, {"protocol": "sdio"}, {"width": 1},
                       {"speed": "automatic"}):
            with self.subTest(values=values), self.assertRaises(ValueError):
                self.resolve(**values)


if __name__ == "__main__":
    unittest.main()
