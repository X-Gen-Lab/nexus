"""SDK device components retain exactly their reviewed upstream closure."""
from pathlib import Path
import hashlib
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
from tools.configure.providers import descriptor


class DeviceSDKPayloadTests(unittest.TestCase):
    def test_rtscts_canonical_inputs_and_boards_are_owned_sdk_requirements(self):
        for family, stem in (("stm32h743", "h743"),
                             ("stm32u575", "u575")):
            board = f"boards/{family}zi_rtscts_reference/board.json"
            assembly = ("cmake/package/verification/io_evolution/" +
                        stem + "-uart-rtscts.toml")
            with self.subTest(family=family):
                self.assertIn(board, descriptor.maintained(family).owned_payload)
                for name in (assembly, board):
                    self.assertTrue((ROOT / name).is_file())
                    self.assertIn(name, sdk.REQUIRED_FILES)
                    self.assertTrue(sdk.selected(name))
                self.assertIn((stem + " rtscts", stem + "-uart-rtscts",
                               "uart-fifo"), consumers.SELECTED_CONSUMERS)
        self.assertEqual(len(consumers.SELECTED_CONSUMERS), 15)

    def test_selected_adc_and_rtscts_sources_are_owned_sdk_requirements(self):
        additions = {
            "stm32g474": ("drivers/adc_group.c",
                          "private/stm32g474_adc_group.h"),
            "stm32h743": ("drivers/uart_rtscts.c",
                          "private/stm32h743_uart_rtscts.h"),
            "stm32u575": ("drivers/uart_rtscts.c",
                          "private/stm32u575_uart_rtscts.h"),
        }
        for family, relative in additions.items():
            payload = set(descriptor.maintained(family).owned_payload)
            for name in relative:
                source = f"soc/{family}/{name}"
                with self.subTest(source=source):
                    self.assertTrue((ROOT / source).is_file())
                    self.assertIn(source, payload)
                    self.assertIn(source, sdk.REQUIRED_FILES)
                    self.assertTrue(sdk.selected(source))

    def test_expanded_soc_ownership_is_complete_and_required_by_sdk(self):
        required = set(sdk.REQUIRED_FILES)
        for family in ("stm32f103", "gd32f303", "stm32g474",
                       "stm32h743", "stm32u575"):
            directory = ROOT / "soc" / family
            # This independent boundary inventories owned firmware files; it
            # does not discover or select production translation units.
            expected = {path.relative_to(ROOT).as_posix()
                        for path in directory.rglob("*")
                        if path.is_file() and path.suffix in {".c", ".h", ".S"}}
            expected.add(f"soc/{family}/CMakeLists.txt")
            payload = set(descriptor.maintained(family).owned_payload)
            with self.subTest(family=family):
                self.assertFalse(expected - payload,
                                 "Incomplete owned payload: " +
                                 repr(sorted(expected - payload)))
                self.assertFalse(expected - required,
                                 "SDK accepts truncated firmware: " +
                                 repr(sorted(expected - required)))

    def test_rehashed_omission_cannot_remove_new_provider_inputs(self):
        names = {
            "soc/common/legacy_adc.c", "soc/common/legacy_pwm.c",
            "soc/common/legacy_flash.c", "soc/common/uart_v1_channel_dma.c",
            "soc/common/i2c_v1_async.h", "soc/common/i2c_v2.c",
            "soc/stm32g474/drivers/adc_stream.c",
            "soc/stm32g474/private/stm32g474_acquisition.h",
            "soc/stm32h743/memory_init.c",
            "soc/stm32h743/private/memory_init.h",
            "soc/stm32h743/drivers/adc.c", "soc/stm32h743/drivers/flash.c",
            "soc/stm32u575/secure_batch.c",
            "soc/stm32u575/private/stm32u575_secure_batch.h",
            "soc/stm32g474/drivers/adc_group.c",
            "soc/stm32g474/private/stm32g474_adc_group.h",
            "soc/stm32h743/drivers/uart_rtscts.c",
            "soc/stm32h743/private/stm32h743_uart_rtscts.h",
            "soc/stm32u575/drivers/uart_rtscts.c",
            "soc/stm32u575/private/stm32u575_uart_rtscts.h",
            "boards/stm32h743zi_rtscts_reference/board.json",
            "boards/stm32u575zi_rtscts_reference/board.json",
            "cmake/package/verification/io_evolution/h743-uart-rtscts.toml",
            "cmake/package/verification/io_evolution/u575-uart-rtscts.toml",
        }
        names.update(
            "vendors/gigadevice/gd32f30x/Firmware/"
            "GD32F30x_standard_peripheral/Include/gd32f30x_" + kind + ".h"
            for kind in ("adc", "dma", "exti", "i2c"))
        self.assertFalse(names - set(sdk.REQUIRED_FILES),
                         "Missing immutable omission guard: " +
                         repr(sorted(names - set(sdk.REQUIRED_FILES))))
        # Exercise the real early completeness verifier with a byte-consistent
        # rehashed map. Dependency contents are intentionally outside this
        # early metadata boundary; no downstream provenance check is mocked.
        with tempfile.TemporaryDirectory(prefix="required SDK omission ") as value:
            root = Path(value) / "prefix/share/nexus/src"
            files = {}
            for name in sdk.REQUIRED_FILES:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"owned input\n")
                files[name] = hashlib.sha256(path.read_bytes()).hexdigest()
            identity = {
                "schema": 1, "kind": "nexus-installed-source-sdk",
                "source_revision": "a" * 40, "source_tree": "b" * 40,
                "publishable": False, "source_dirty": True,
                "files_sha256": files, "dependencies": [],
                "reviewed_imports": [],
                "toolchains_lock_sha256": files["dependencies/toolchains.lock.json"],
                "snapshot_sha256": "", "package_files_sha256": {},
            }
            for name in sorted(names):
                with self.subTest(omitted=name):
                    target = root / name
                    original = target.read_bytes()
                    expected = files.pop(name)
                    target.unlink()
                    identity["snapshot_sha256"] = hashlib.sha256(json.dumps(
                        files, sort_keys=True,
                        separators=(",", ":")).encode()).hexdigest()
                    (root / sdk.MANIFEST).write_text(json.dumps(identity) + "\n")
                    try:
                        with self.assertRaisesRegex(
                                sdk.SDKError, "missing required sources/licenses"):
                            sdk.verify(root)
                    finally:
                        target.write_bytes(original)
                        files[name] = expected

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

    def test_upstream_checkout_exports_only_the_exact_reviewed_component(self):
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            with self.subTest(part=part):
                entries = sdk.dependency_entries(ROOT, review["directory"])
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
