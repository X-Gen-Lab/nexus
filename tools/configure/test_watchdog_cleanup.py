"""Generated teardown retains every disabled watchdog preparation owner."""
import unittest

from tools.configure import bindings, configure


class WatchdogCleanupGenerationTests(unittest.TestCase):
    def test_f407_stop_reaches_the_provider_cleanup_without_state_query(self):
        result = configure.resolve_ir(configure.ROOT /
                                      "tests/contracts/stm32_assembly/ve-watchdog.toml")
        self.assertTrue(any(item.kind == "watchdog" for item in result.controllers))
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32_watchdog_stop(&s_nx_port_watchdog0)", source)
        self.assertNotIn("nx_watchdog_port_state(", source)


if __name__ == "__main__":
    unittest.main()
