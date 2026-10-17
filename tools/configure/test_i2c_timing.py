"""I2C V2 timing is derived from explicit clock and electrical bounds."""

from dataclasses import FrozenInstanceError
import unittest

from tools.configure import i2c_timing as timing


def inputs(**changes):
    return {"kernel_hz_min": 16000000, "kernel_hz_max": 16000000,
            "pclk_hz_min": 16000000, "bus_hz": 100000,
            "bus_hz_min": 60000,
            "rise_ns_max": 1000, "fall_ns_max": 300,
            "analog_filter_ns_min": 50, "analog_filter_ns_max": 260,
            "digital_filter_cycles": 0, **changes}


class I2cTimingTests(unittest.TestCase):
    def test_standard_mode_counters_and_bounded_actual_frequency(self):
        result = timing.resolve(inputs())
        self.assertLessEqual(result.bus_hz_max, 100000)
        self.assertGreater(result.bus_hz_min, 0)
        self.assertLessEqual(result.bus_hz_min, result.bus_hz_max)
        self.assertEqual(result.timingr & 0x0f000000, 0)
        self.assertTrue(timing.validate(result.timingr, inputs()))
        with self.assertRaises(FrozenInstanceError):
            result.timingr = 0

    def test_fast_mode_uses_explicit_faster_electrical_envelope(self):
        facts = inputs(bus_hz=400000, rise_ns_max=300, fall_ns_max=300)
        result = timing.resolve(facts)
        self.assertLessEqual(result.bus_hz_max, 400000)
        self.assertGreater(result.bus_hz_max, 200000)
        self.assertTrue(timing.validate(result.timingr, facts))

    def test_kernel_tolerance_is_applied_in_both_directions(self):
        facts = inputs(kernel_hz_min=15650000, kernel_hz_max=16250000)
        result = timing.resolve(facts)
        self.assertLessEqual(result.bus_hz_max, 100000)
        self.assertLess(result.bus_hz_min, result.bus_hz_max)
        self.assertTrue(timing.validate(result.timingr, facts))

    def test_observed_fixed_u575_value_remains_valid_at_documented_bounds(self):
        facts = inputs(kernel_hz_min=15650000, kernel_hz_max=16250000)
        self.assertTrue(timing.validate(0x10f42f2f, facts))
        self.assertTrue(timing.validate(0x20f41f1f, facts))

    def test_computed_rate_bounds_include_three_cycle_synchronizer_delay(self):
        values = inputs()
        result = timing.resolve(values)
        encoded = result.timingr
        presc = (encoded >> 28) + 1
        count = ((encoded >> 8) & 255) + (encoded & 255) + 2
        # Exact16MHz gives62.5ns. Slow rate includes two3-cycle sync delays,
        # two260ns analog delays and declared1300ns total edge transition.
        slow_period = count * presc * 62.5 + 2 * (260 + 3 * 62.5) + 1300
        self.assertLessEqual(result.bus_hz_min, 1000000000 / slow_period)

    def test_filter_disable_is_a_distinct_explicit_register_contract(self):
        values = inputs(analog_filter_ns_min=0, analog_filter_ns_max=0)
        result = timing.resolve(values)
        self.assertEqual(result.cr1_filter_bits, 1 << 12)

    def test_unknown_clock_and_electrical_bounds_never_get_defaults(self):
        for field in inputs():
            facts = inputs()
            del facts[field]
            with self.subTest(field=field), self.assertRaises(ValueError):
                timing.resolve(facts)

    def test_invalid_envelopes_and_unknown_fields_reject(self):
        for changes in ({"kernel_hz_min": 0},
                        {"kernel_hz_min": 17000000},
                        {"kernel_hz_max": True}, {"bus_hz": 1000000},
                        {"bus_hz_min": 100001}, {"pclk_hz_min": 1},
                        {"bus_hz": 400000, "rise_ns_max": 1000},
                        {"rise_ns_max": -1}, {"digital_filter_cycles": 16},
                        {"analog_filter_ns_min": 300}, {"typo": 1}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                timing.resolve(inputs(**changes))

    def test_rejects_no_encodable_timing_instead_of_slow_fallback(self):
        with self.assertRaises(ValueError):
            timing.resolve(inputs(kernel_hz_min=100000,
                                  kernel_hz_max=100000))

    def test_zero_and_reserved_bit_candidates_fail(self):
        self.assertFalse(timing.validate(0, inputs()))
        self.assertFalse(timing.validate(0xffffffff, inputs()))

    def test_solver_is_deterministic_and_returns_json_units(self):
        first = timing.resolve(inputs())
        self.assertEqual(first, timing.resolve(inputs()))
        self.assertEqual(first.to_dict()["timingr"], first.timingr)
        self.assertEqual(first.to_dict()["time_unit"], "ns")


if __name__ == "__main__":
    unittest.main()
