"""SDK device components retain exactly their reviewed upstream closure."""
from pathlib import Path
import json
import re
import sys
import shutil
import tempfile
import tomllib
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "cmake/package"))
import package_source_sdk as sdk
import verify_consumers as consumers
from tools.maintenance import cmsis_device_lock


class DeviceSDKPayloadTests(unittest.TestCase):
    def test_modern_consumer_inputs_are_explicit_and_use_only_relocated_sdk(self):
        with tempfile.TemporaryDirectory(prefix="modern consumer inputs ") as value:
            directory = Path(value)
            prefix = directory / "prefix"
            policy = prefix / "share/nexus/src/dependencies/platform-compatibility.json"
            policy.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / "dependencies/platform-compatibility.json", policy)
            for family, clock in (("h743", "hsi64-fixed-cache-off"),
                                  ("u575", "msis4"),
                                  ("f103", "hsi8-fixed"),
                                  ("gd303", "irc8"),
                                  ("g474", "hse24-pll170")):
                for backend in ("baremetal", "freertos"):
                    with self.subTest(family=family, backend=backend):
                        source, _, command = consumers.prepare_modern_consumer(
                            prefix, directory, family, backend, lambda argv: None,
                            expected_revision="a" * 40)
                        text = (source / "assembly.toml").read_text()
                        self.assertIn(f'backend = "{backend}"', text)
                        self.assertIn(f'clock = "{clock}"', text)
                        self.assertIn(str(prefix / "share/nexus/src/boards"), text)
                        self.assertNotIn(str(ROOT) + "/", text)
                        code = (source / "main.c").read_text()
                        self.assertIn("nx_factory_uart", code)
                        self.assertIn("nx_gpio_port_write", code)
                        authored = tomllib.loads(text)
                        board_name = Path(authored["board_package"]).name
                        board = json.loads((ROOT / "boards" / board_name /
                                            "board.json").read_text())
                        binding_id = authored["gpio"]["led"]["binding"]
                        binding = next(item for item in board["bindings"]
                                       if item["id"] == binding_id)
                        self.assertIn(authored["uart"]["console"]["binding"],
                                      {item["id"] for item in board["bindings"]})
                        match = re.search(r"nx_gpio_port_write\(led, 1U << (\d+)U", code)
                        self.assertIsNotNone(match)
                        self.assertEqual((1 << int(match[1])) & ~binding["mask"], 0,
                                         "Consumer write exceeds its Board pin lease")
                        self.assertEqual("vTaskStartScheduler" in code,
                                         backend == "freertos")
                        self.assertIn("-DNEXUS_EXPECTED_SOURCE_REVISION=" + "a" * 40,
                                      command)
                        self.assertNotIn("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON", command)
            for family, backend in (("cortex-m7", "baremetal"),
                                    ("h743", "threadx")):
                with self.subTest(family=family, backend=backend):
                    with self.assertRaises(ValueError):
                        consumers.prepare_modern_consumer(
                            prefix, directory, family, backend, lambda argv: None)

    def test_new_soc_payload_requires_its_actual_owned_firmware_sources(self):
        required = set(sdk.REQUIRED_FILES)
        for family, board in (("stm32h743", "stm32h743_nucleo_zi2"),
                              ("stm32u575", "stm32u575_nucleo_zi_q"),
                              ("stm32f103", "stm32f103_reference"),
                              ("gd32f303", "gd32f303_reference"),
                              ("stm32g474", "stm32g474re_hse24_reference")):
            names = (f"soc/{family}/CMakeLists.txt", f"soc/{family}/system.c",
                     f"soc/{family}/drivers/gpio.c", f"soc/{family}/drivers/uart.c",
                     f"soc/{family}/private/{family}_system.h",
                     f"soc/{family}/private/{family}_provider.h",
                     f"boards/{board}/board.json")
            for name in names:
                with self.subTest(name=name):
                    self.assertTrue(name in required,
                                    "SDK accepted missing owned firmware source: " + name)

    def test_h743_pre_ram_power_boot_assembly_and_header_are_required(self):
        required = set(sdk.REQUIRED_FILES)
        for name in ("soc/stm32h743/power_boot.S",
                     "soc/stm32h743/private/boot_registers.h"):
            with self.subTest(name=name):
                self.assertTrue(name in required,
                                "SDK omitted the actual H743 pre-RAM startup input")

    def test_full_upstream_checkout_exports_only_the_reviewed_five_files(self):
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            with self.subTest(part=part):
                entries = sdk.dependency_entries(ROOT, review["directory"])
                self.assertEqual(len(entries), 5)
                self.assertEqual(set(entries), review["files"])
                self.assertTrue(all(mode in {"100644", "100755"}
                                    and kind == "blob"
                                    for mode, kind, _ in entries.values()))

    def test_original_notice_lock_and_verifier_cannot_be_omitted_from_payload(self):
        required = set(sdk.REQUIRED_FILES)
        self.assertTrue("tools/maintenance/cmsis_device_lock.py" in required,
                        "Missing generic device source verifier payload")
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            with self.subTest(part=part):
                self.assertTrue(review["lock_name"] in required,
                                "Missing exact device source lock payload")
                for name in review["files"]:
                    self.assertTrue(review["directory"] + "/" + name in required,
                                    "Missing original reviewed device source: " + name)


if __name__ == "__main__":
    unittest.main()
