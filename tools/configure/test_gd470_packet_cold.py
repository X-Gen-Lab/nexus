"""GD470 packet and advanced timer hardware lives in concrete C adapters."""
import unittest

from tools.configure import configure, dependencies
from tools.configure.providers import maintained
from tools.configure.providers.descriptor import maintained as descriptor


class GD470PacketColdTests(unittest.TestCase):
    def test_selected_packet_and_timer_construction_is_static(self):
        expected = {
            "can": "nx_gd32_can_prepare",
            "usb_device": "nx_gd32_usb_cold_prepare",
            "eth": "nx_gd32_eth_cold_prepare",
            "eth_frames": "nx_gd32_eth_frames_cold_prepare",
            "timer_capture": "nx_gd32_capture_cold_prepare",
            "advanced_pwm": "nx_gd32_advanced_cold_prepare",
        }
        for fixture in ("tests/contracts/can_assembly/gd32-both.toml",
                        "tests/contracts/usb_assembly/gd32.toml",
                        "tests/contracts/ethernet_assembly/gd32.toml",
                        "tests/contracts/timer_assembly/gd32.toml",
                        "cmake/package/verification/io_evolution/gd470-eth-frames.toml"):
            result = configure.resolve_ir(
                configure.ROOT / fixture)
            provider = maintained("gd32f470")
            for item in result.controllers:
                if item.kind not in expected:
                    continue
                with self.subTest(fixture=fixture, kind=item.kind):
                    construction = provider.constructor(
                        item, result, result.board_bindings, result.routes,
                        result.devices)
                    self.assertEqual(construction.initialize.function,
                                     expected[item.kind])
                    declarations = "\n".join(construction.definitions)
                    self.assertNotIn("static nx_result_t", declarations)
                    self.assertNotIn("NX_PLATFORM_REGISTER_WRITE", declarations)
                    self.assertNotIn("NX_PLATFORM_PRIORITY_WRITE", declarations)
                    self.assertEqual(len(construction.pin_snapshots),
                                     len(item.pins))

    def test_family_helpers_and_private_headers_are_in_declared_closure(self):
        family = "soc/gd32/gd32f470/"
        names = set(dependencies.source_names("platform", "gd32f470"))
        for name in ("can", "timer", "packet"):
            self.assertIn(family + "configure/" + name + ".py", names)
        payload = descriptor("gd32f470").owned_payload
        for name in ("can", "timer", "packet", "usb", "eth", "eth_frames"):
            self.assertIn(family + "private/gd32f470_" + name + "_cold.h",
                          payload)


if __name__ == "__main__":
    unittest.main()
