"""Closed F4 mapped-read and high-speed SDIO profiles use the public SDK plan."""
import importlib.util
from pathlib import Path
import unittest

from tools.maintenance.consumer_profiles import SERVICE_CONSUMERS, consumer_plan

ROOT = Path(__file__).resolve().parents[2]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "cmake/package" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class ClosedF4SDKModesTests(unittest.TestCase):
    def test_coupled_pwm_adc_profiles_keep_relocatable_inputs_and_provider(self):
        package, consumers = load("package_source_sdk"), load("verify_consumers")
        profiles = {item.name: item for item in SERVICE_CONSUMERS}
        for name, scenario, symbol in (
                ("f407 pwm adc finite", "f407_pwm_adc", "nx_stm32_pwm_adc_prepare"),
                ("f407 pwm adc loop", "f407_pwm_adc_loop",
                 "nx_stm32_pwm_adc_loop_prepare")):
            with self.subTest(name=name):
                self.assertIn(name, profiles)
                profile = profiles[name]
                self.assertTrue(profile.arm)
                directory = ROOT / "tests/contracts" / scenario
                self.assertEqual(consumers.service_directory(ROOT, profile), directory)
                sources = consumers.service_sources(ROOT, profile)
                self.assertEqual(set(sources), {"main.c", "board/board.json"})
                for path in (*sources.values(), directory / profile.assembly):
                    self.assertTrue(path.is_file())
                    self.assertIn(path.relative_to(ROOT).as_posix(),
                                  package.REQUIRED_FILES)
                self.assertIn(symbol, consumers.service_symbols(profile))

    def test_exact_profiles_are_unique_in_the_existing_plan(self):
        expected = {"f479ni qspi mapped cpu", "f407 sd37 dma", "f407 mmc37 irq"}
        selected = [item for item in SERVICE_CONSUMERS if item.name in expected]
        self.assertEqual(expected, {item.name for item in selected})
        self.assertTrue(all(item.arm for item in selected))
        plan = consumer_plan()
        self.assertEqual(len(plan), len({(kind, str(value)) for kind, value in plan}))

    def test_selected_profiles_own_payload_and_real_retained_methods(self):
        package, consumers = load("package_source_sdk"), load("verify_consumers")
        required = set(package.REQUIRED_FILES)
        for path in ("tests/contracts/stm32f4_qspi/mapped_consumer.c",
                     "tests/contracts/stm32f4_qspi/f479ni-qspi-map.toml",
                     "tests/contracts/stm32f4_sdio_hs/sd37-dma.toml",
                     "tests/contracts/stm32f4_sdio_hs/mmc37-irq.toml"):
            self.assertIn(path, required)
        profiles = {item.name: item for item in SERVICE_CONSUMERS}
        self.assertIn("nx_factory_mapped_storage", consumers.service_symbols(
            profiles["f479ni qspi mapped cpu"]))
        self.assertIn("nx_stm32_sdio_prepare", consumers.service_symbols(
            profiles["f407 sd37 dma"]))


if __name__ == "__main__":
    unittest.main()
