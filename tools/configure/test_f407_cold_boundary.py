"""Complete selected F407 paths keep executable hardware behavior in C."""
import unittest

from tools.configure import configure as gate
from tools.configure.providers import maintained


class F407ColdBoundaryTests(unittest.TestCase):
    def test_optional_domains_emit_only_policy_storage_and_typed_connections(self):
        inputs = (
            "can_assembly/stm32-first.toml",
            "can_assembly/stm32-both.toml",
            "timer_assembly/stm32-zg.toml",
            "timer_assembly/stm32-ve-freertos.toml",
            "usb_assembly/stm32-zg.toml",
            "usb_assembly/stm32-ve-interrupt.toml",
            "ethernet_assembly/stm32-zg.toml",
            "ethernet_assembly/stm32-zg-freertos.toml",
            "../../cmake/package/verification/io_evolution/f407zg-eth-frames.toml",
        )
        kinds = set()
        for relative in inputs:
            with self.subTest(assembly=relative):
                result = gate.resolve_ir(gate.ROOT / "tests/contracts" / relative)
                provider = maintained(result.soc_family)
                self.assertEqual(result.soc_family, "stm32f407")
                for item in result.controllers:
                    if item.kind not in {"can", "usb_device", "eth", "eth_frames",
                                         "timer_capture", "advanced_pwm"}:
                        continue
                    kinds.add(item.kind)
                    construction = provider.constructor(
                        item, result, result.board_bindings, result.routes,
                        result.devices)
                    self.assertTrue(construction.initialize.function.startswith(
                        "nx_stm32_"))
                    self.assertTrue(construction.retain_on_failed_init)
                    self.assertEqual(set(construction.pin_snapshots),
                                     {pin["pin"] for pin in item.pins})
                    definitions = "\n".join(construction.definitions)
                    for behavior in ("NX_PLATFORM_REGISTER_WRITE(",
                                     "NX_PLATFORM_PRIORITY_WRITE(", "NVIC->",
                                     "static nx_result_t prepare_",
                                     "static nx_result_t stop_", "memset(",
                                     "for (", "while ("):
                        self.assertNotIn(behavior, definitions)
        self.assertEqual(kinds, {"can", "usb_device", "eth", "eth_frames",
                                 "timer_capture", "advanced_pwm"})


if __name__ == "__main__":
    unittest.main()
