"""Strict opt-in port selection without changing Tick or security policy."""
import unittest

import lowpower


class LowpowerPortTests(unittest.TestCase):
    def policy(self, **changes):
        return {"lowpower_port": "timebase", "tickless": True,
                "tick_source": "systick", "memory_protection": False,
                "security_model": "single", **changes}

    def test_default_keeps_custom_port(self):
        self.assertEqual(lowpower.resolve_port({}), "custom")

    def test_explicit_custom_keeps_existing_external_hook_contract(self):
        self.assertEqual(lowpower.resolve_port(self.policy(
            lowpower_port="custom", tick_source="external")), "custom")

    def test_timebase_is_a_distinct_explicit_selection(self):
        self.assertEqual(lowpower.resolve_port(self.policy()), "timebase")

    def test_unknown_port_or_nonstring_is_rejected(self):
        for value in ("auto", "stop", True, None, 0):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "port"):
                lowpower.resolve_port(self.policy(lowpower_port=value))

    def test_timebase_requires_tickless(self):
        with self.assertRaisesRegex(ValueError, "tickless"):
            lowpower.resolve_port(self.policy(tickless=False))

    def test_timebase_never_silently_replaces_an_external_tick(self):
        with self.assertRaisesRegex(ValueError, "SysTick"):
            lowpower.resolve_port(self.policy(tick_source="external"))

    def test_timebase_rejects_mpu_domain(self):
        with self.assertRaisesRegex(ValueError, "privileged"):
            lowpower.resolve_port(self.policy(memory_protection=True))

    def test_timebase_rejects_split_security(self):
        with self.assertRaisesRegex(ValueError, "single"):
            lowpower.resolve_port(self.policy(security_model="split"))


if __name__ == "__main__":
    unittest.main()
