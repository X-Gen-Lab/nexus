"""Exact U575 finite peripheral selection and generated ownership contracts."""
import unittest
from pathlib import Path
from tools.configure import configure
from soc.stm32.stm32u575 import configure as stm32u575
from tools.configure.providers.common import ConfigurationError
from .test_provider import Result


def device(kind, controller, mode, pins, **options):
    return dict(id="port", binding="binding", kind=kind,
                controller=controller, mode=mode, pins=[
                    dict(pin=pin, function=function, af=af)
                    for pin, function, af in pins], **options)


class U575DeviceSelectionTests(unittest.TestCase):
    def test_polling_completion_irq_and_independent_clock_are_real_claims(self):
        root = Path(__file__).resolve().parents[5]
        result = configure.resolve_ir(
            root / "tests/contracts/u575_assembly/peripherals.toml", root)
        claims = {item.key: item.owner for item in result.claims}
        self.assertEqual("storage", claims.get("irq:FLASH"))
        self.assertEqual("control", claims.get("clock:HSI16"))
        self.assertEqual("control", claims.get("clock-mux:I2C1"))

    def test_shared_i2c_operations_keep_their_actual_linked_identity(self):
        item = device("i2c", "I2C1", "poll-messages", [
            ("PB8", "scl", 4), ("PB9", "sda", 4)], max_hz=100000)
        result = Result(item)
        controller = result.controllers[0]
        self.assertEqual(stm32u575.operations(controller), "nx_i2c_v2_ops")
        construction = stm32u575.constructor(controller, result, {}, {}, ())
        self.assertEqual(construction.state_type, "nx_i2c_v2_state_t")
        source = "\n".join(construction.definitions)
        self.assertIn("static const nx_i2c_v2_config_t", source)
        self.assertIn("nx_stm32u575_i2c_prepare", construction.initialize.render())
        self.assertIn("nx_stm32u575_i2c_close", construction.stop[0].render())

    def test_exact_selected_modes_and_pin_routes(self):
        items = [
            device("spi", "SPI1", "short-poll", [
                ("PA5", "sck", 5), ("PA6", "miso", 5),
                ("PA7", "mosi", 5)], max_hz=1000000),
            device("i2c", "I2C1", "poll-messages", [
                ("PB8", "scl", 4), ("PB9", "sda", 4)], max_hz=100000),
            device("pwm", "TIM3", "fixed-pwm", [("PA6", "ch1", 2)],
                   tick_hz=1000000, period_ticks=1000, duty_ticks=250),
            device("adc", "ADC4", "single-shot", [("PC0", "analog", 0)],
                   channels=[1], sample_times=[7], reference_mv=3300),
            device("exti", "EXTI1", "edge-event", [("PA1", "input", 0)],
                   edge="both", event_capacity=3, irq_priority=5),
            device("flash", "FLASH", "poll-word", [])]
        for item in items:
            with self.subTest(kind=item["kind"]):
                stm32u575.validate_selection(item, item)
                self.assertEqual(item["kind"], stm32u575.mode_implementation(
                    item["kind"], item["mode"]))
                bad = {**item, "controller": "FOREIGN"}
                with self.assertRaises(ConfigurationError):
                    stm32u575.validate_selection(bad, bad)

    def test_adc_uses_reviewed_channel_and_no_guessed_supply_enable(self):
        item = device("adc", "ADC4", "single-shot", [("PC0", "analog", 0)],
                      channels=[1], sample_times=[7], reference_mv=3300)
        result = Result(item)
        construction = stm32u575.constructor(
            result.controllers[0], result, {}, {}, ())
        source = "\n".join(construction.definitions)
        self.assertEqual(construction.initialize.function,
                         "nx_stm32u575_adc_prepare")
        self.assertIn("static const nx_stm32u575_adc_config_t", source)
        self.assertNotIn("NX_U575_WRITE", source)
        self.assertNotIn("static nx_result_t prepare_", source)
        self.assertNotIn("SVMCR", source)
        minimum = {**item, "reference_mv": 1000}
        stm32u575.validate_selection(minimum, minimum)
        for change in ({"channels": [0]}, {"sample_times": [0]},
                       {"reference_mv": 999}):
            bad = {**item, **change}
            with self.assertRaises(ConfigurationError):
                stm32u575.validate_selection(bad, bad)

    def test_flash_constructor_never_unlocks_or_reserves_a_partition(self):
        item = device("flash", "FLASH", "poll-word", [])
        result = Result(item)
        construction = stm32u575.constructor(
            result.controllers[0], result, {}, {}, ())
        source = "\n".join(construction.definitions)
        self.assertIn("0x08000000", source)
        self.assertNotIn("NSKEYR", source)
        self.assertNotIn("region", source)


if __name__ == "__main__":
    unittest.main()
