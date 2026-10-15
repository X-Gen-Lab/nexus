"""Fail-closed kernel profile matrix and optional-hook audit regressions."""
import importlib
import json
from pathlib import Path
import tempfile
import unittest


class KernelProfileGateTests(unittest.TestCase):
    def gate(self):
        return importlib.import_module("os_profiles")

    def test_matrix_contains_positive_profiles_and_separate_expected_rejections(self):
        matrix = self.gate().matrix()
        self.assertEqual(len(matrix), 14)
        self.assertEqual(len({item["name"] for item in matrix}), 14)
        self.assertEqual(sum(item["expected_rejection"] for item in matrix), 2)
        self.assertTrue({"cortex-m0", "cortex-m23", "cortex-m55", "cortex-m85"}
                        <= {item["facts"]["cpu"]["arch"] for item in matrix})
        self.assertEqual({item["choices"]["profile"] for item in matrix},
                         {"standard", "minimal", "diagnostic", "lowpower"})

    def test_inputs_select_os_policy_once_through_toml(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            variant = self.gate().matrix()[0]
            assembly = self.gate().write_inputs(directory, variant)
            self.assertIn("[os]", assembly.read_text())
            self.assertEqual(json.loads((directory / "cpu.json").read_text()),
                             variant["facts"])
            self.assertNotIn("configMAX_PRIORITIES", assembly.read_text())

    def test_negative_build_requires_precise_unresolved_hook_and_no_elf(self):
        gate = self.gate()
        variant = next(item for item in gate.matrix()
                       if item["name"] == "external-tick-missing")
        diagnostic = "undefined reference to `nx_freertos_external_tick_setup'"
        gate.check_rejection(variant, 1, diagnostic, False)
        for returncode, text, elf in ((0, diagnostic, False),
                                       (2, diagnostic, False),
                                       (1, "missing include file", False),
                                       (1, diagnostic, True)):
            with self.subTest(returncode=returncode, text=text, elf=elf):
                with self.assertRaisesRegex(ValueError, "expected hook rejection"):
                    gate.check_rejection(variant, returncode, text, elf)

    def test_counter_rejection_requires_both_actual_provider_symbols(self):
        gate = self.gate()
        variant = next(item for item in gate.matrix()
                       if item["name"] == "runtime-counter-missing")
        diagnostic = ("undefined reference to nx_freertos_runtime_counter_start\n"
                      "undefined reference to nx_freertos_runtime_counter_now")
        gate.check_rejection(variant, 1, diagnostic, False)
        with self.assertRaises(ValueError):
            gate.check_rejection(variant, 1, diagnostic.splitlines()[0], False)

    def test_external_tick_must_retain_a_strong_hook(self):
        gate = self.gate()
        variant = next(item for item in gate.matrix()
                       if item["name"] == "external-tick")
        symbols = "08000000 T vPortSetupTimerInterrupt\n08000004 T nx_freertos_external_tick_setup\n"
        gate.check_optional_symbols(variant, symbols)
        with self.assertRaisesRegex(ValueError, "strong external Tick"):
            gate.check_optional_symbols(variant, symbols.replace("T vPort", "W vPort"))

    def test_disabled_features_do_not_pull_in_notification_counter_or_tick_provider(self):
        gate = self.gate()
        variant = next(item for item in gate.matrix() if item["name"] == "minimal")
        gate.check_optional_symbols(variant, "08000000 T vPortSetupTimerInterrupt\n")
        for name in ("xTaskGenericNotify", "ulTaskGenericNotifyTake",
                     "nx_freertos_runtime_counter_now", "nx_freertos_external_tick_setup"):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "disabled feature"):
                gate.check_optional_symbols(variant, "08000000 T " + name + "\n")

    def test_costs_require_real_absolute_size_symbols_and_positive_ram(self):
        gate = self.gate()
        text = "00000060 A __nexus_os_size_tcb\n00000010 A __nexus_os_size_wait_port\n"
        self.assertEqual(gate.object_sizes(text)["tcb"], 96)
        with self.assertRaises(ValueError):
            gate.object_sizes(text.replace(" A ", " T "))
        self.assertEqual(gate.image_size("text data bss dec hex filename\n100 4 80 184 b8 image.elf\n"),
                         {"text": 100, "data": 4, "bss": 80, "static_ram": 84,
                          "flash_load": 104})
        with self.assertRaises(ValueError):
            gate.image_size("text data bss dec hex filename\n0 0 0 0 0 image.elf\n")

    def test_minimal_profile_savings_must_be_measured(self):
        gate = self.gate()
        variants = [{"name": name, "costs": {"object_sizes": {"tcb": size},
                                            "image": {"static_ram": ram}}}
                    for name, size, ram in (("standard", 104, 1200), ("minimal", 88, 1000))]
        self.assertEqual(gate.compare_costs(variants)["static_ram_saved"], 200)
        variants[1]["costs"]["image"]["static_ram"] = 1200
        with self.assertRaisesRegex(ValueError, "minimal profile"):
            gate.compare_costs(variants)


if __name__ == "__main__":
    unittest.main()
