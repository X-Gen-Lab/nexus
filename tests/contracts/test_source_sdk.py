"""Relocate the real source SDK and compile/run independent public consumers."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "cmake/package"))
import verify_consumers as consumer_gate
import package_source_sdk as source_sdk
from tools.configure.metadata import load_policy

# Fixture repositories are disposable; automatic maintenance must neither
# detach nor outlive the Git command that owns their temporary directory.
GIT_FIXTURE_OPTIONS = ("-c", "maintenance.auto=false", "-c", "gc.auto=0",
                       "-c", "maintenance.autoDetach=false",
                       "-c", "gc.autoDetach=false")
OFFICIAL_F4_VERIFIED_FILES = frozenset({
    "Inc/Legacy/stm32_hal_legacy.h", "Inc/stm32f4xx_hal_def.h",
    "Inc/stm32f4xx_hal_eth.h", "Inc/stm32f4xx_ll_adc.h",
    "Inc/stm32f4xx_ll_gpio.h", "Inc/stm32f4xx_ll_i2c.h",
    "Inc/stm32f4xx_ll_iwdg.h", "Inc/stm32f4xx_ll_spi.h",
    "Inc/stm32f4xx_ll_tim.h", "Inc/stm32f4xx_ll_usart.h", "LICENSE.md",
})


class SourceSDKSelectionTests(unittest.TestCase):
    def test_required_public_verification_sources_survive_snapshot_selection(self):
        omitted = sorted(name for name in source_sdk.REQUIRED_FILES
                         if name.startswith("cmake/package/verification/io_evolution/")
                         and not source_sdk.selected(name))
        self.assertEqual(omitted, [],
                         "Required public SDK programs cannot be excluded by selection")

    def test_async_and_retained_owner_public_closure_is_required(self):
        required = {
            "components/user_io_broker/CMakeLists.txt",
            "components/user_io_broker/include/nexus/components/user_io_async.h",
            "components/user_io_broker/include/nexus/components/"
            "user_io_async_storage.h",
            "components/user_io_broker/src/user_io_async.c",
            "components/stm32u575_retained_time/CMakeLists.txt",
            "components/stm32u575_retained_time/include/nexus/components/"
            "stm32u575_retained_time.h",
            "components/stm32u575_retained_time/private/retained_time.h",
            "components/stm32u575_retained_time/src/stm32u575_retained_time.c",
            "soc/stm32u575/private/stm32u575_rtc_calendar.h",
            "components/stm32u575_stop2/CMakeLists.txt",
            "components/stm32u575_stop2/include/nexus/components/"
            "stm32u575_stop2.h",
            "soc/stm32u575/stop2.c",
            "soc/stm32u575/private/stm32u575_stop2_hw.h",
        }
        required.update("cmake/package/verification/io_evolution/consumers/"
                        + name + ".c" for name in (
                            "user-io-async", "u575-retained-time", "u575-stop2"))
        self.assertFalse(required - set(source_sdk.REQUIRED_FILES),
                         sorted(required - set(source_sdk.REQUIRED_FILES)))
        for name in sorted(required):
            with self.subTest(name=name):
                self.assertTrue((ROOT / name).is_file())
                self.assertTrue(source_sdk.selected(name),
                                "Required public closure omitted: " + name)

    def test_selected_u575_mpu_gpio_assembly_is_explicitly_catalogued(self):
        catalogue = json.loads((source_sdk.SOURCE_ROOT /
            "tools/configure/assemblies/catalogue.json").read_text())
        self.assertEqual(catalogue["assemblies"].count("u575-mpu-gpio-write.toml"), 1)

    def test_selected_u575_mpu_gpio_public_closure_is_required(self):
        required = {
            "os/freertos/include/nexus/os/freertos_mpu_gpio.h",
            "os/freertos/mpu/gpio.c",
            "soc/stm32u575/private/stm32u575_gpio_mpu.h",
            "soc/stm32u575/mpu_user_init.c",
            "soc/stm32u575/private/stm32u575_mpu_user_init.h",
            "tools/configure/assemblies/u575-mpu-gpio-write.toml",
            "cmake/package/verification/io_evolution/consumers/u575-mpu-gpio.c",
        }
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))
        for name in required:
            with self.subTest(path=name):
                self.assertTrue(source_sdk.selected(name))
        payload = source_sdk.provider_descriptors.maintained("stm32u575").owned_payload
        self.assertTrue((required - {"os/freertos/include/nexus/os/freertos_mpu_gpio.h",
                                    "os/freertos/mpu/gpio.c"}).issubset(set(payload)))

    def test_selected_f407_i2s2_public_closure_is_required(self):
        role_module = "tools/configure/i2s2.py"
        required = {
            role_module,
            "components/f407_i2s2_slave_tx/CMakeLists.txt",
            "components/f407_i2s2_slave_tx/include/nexus/components/"
            "f407_i2s2_slave_tx.h",
            "soc/stm32f407/drivers/i2s2_slave_tx.c",
            "soc/stm32f407/private/stm32f407_i2s2_slave_tx.h",
            "tools/configure/assemblies/sky-i2s2-slave-tx.toml",
            "tools/configure/assemblies/qiming-i2s2-slave-tx.toml",
            "cmake/package/verification/io_evolution/consumers/"
            "f407-i2s2-slave-tx.c",
        }
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))
        for name in sorted(required):
            with self.subTest(path=name):
                self.assertTrue(source_sdk.selected(name),
                                "Required I2S2 closure omitted: " + name)
        payload = source_sdk.provider_descriptors.maintained("stm32f407").owned_payload
        hardware = required - {role_module}
        self.assertTrue(hardware.issubset(set(payload)),
                        sorted(hardware - set(payload)))

    def test_optional_usb_control_and_profile_sources_cannot_be_omitted(self):
        required = {
            "components/usb_control/CMakeLists.txt",
            "components/usb_control/include/nexus/components/usb_control.h",
            "components/usb_control/include/nexus/components/usb_descriptors.h",
            "components/usb_control/include/nexus/components/usb_acm.h",
            "components/usb_control/src/usb_control.c",
            "components/usb_control/src/usb_descriptors.c",
            "components/usb_control/src/usb_acm.c",
            "io/include/nexus/io/usb_profile.h",
            "io/src/usb_profile.c",
            "tools/configure/usb_profiles.py",
        }
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))
        for name in required:
            with self.subTest(name=name):
                self.assertTrue((ROOT / name).is_file())
                self.assertTrue(source_sdk.selected(name))

    def test_provider_canonical_smokes_are_not_omitted_from_sdk_selection(self):
        prefix = "cmake/package/verification/io_evolution/"
        smokes = {name for descriptor in source_sdk.provider_descriptors.families()
                  for name in descriptor.owned_payload
                  if name.startswith(prefix)}
        self.assertTrue(smokes, "Reviewed provider consumer closure is empty")
        for name in sorted(smokes):
            with self.subTest(name=name):
                self.assertIn(name, source_sdk.REQUIRED_FILES)
                self.assertTrue(source_sdk.selected(name),
                                "Required consumer omitted from SDK: " + name)

    def test_official_f4_required_payload_has_the_reviewed_eleven_files(self):
        lock_path = ROOT / "dependencies/stm32f4-hal.lock.json"
        lock = json.loads(lock_path.read_text())
        self.assertEqual(len(OFFICIAL_F4_VERIFIED_FILES), 11)
        self.assertEqual(set(lock["files"]), OFFICIAL_F4_VERIFIED_FILES)
        for name in OFFICIAL_F4_VERIFIED_FILES:
            with self.subTest(name=name):
                self.assertIn("vendors/st/stm32f4_hal/" + name,
                              source_sdk.REQUIRED_FILES)

    def test_enhanced_ethernet_canonical_inputs_and_public_smoke_are_required(self):
        directory = "cmake/package/verification/io_evolution/"
        required = {directory + name for name in (
            "f407zg-eth-frames.toml", "gd470-eth-frames.toml",
            "boards/f407zg-eth-frames/board.json",
            "boards/gd470-eth-frames/board.json",
            "consumers/ethernet-frames.c")}
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))
        for name in required:
            with self.subTest(name=name):
                self.assertTrue((ROOT / name).is_file())
                self.assertTrue(source_sdk.selected(name))

    def test_enhanced_frames_common_and_native_sources_are_required(self):
        required = {
            "io/include/nexus/io/ethernet_frames.h",
            "io/private/eth_frames_mac.h",
            "io/private/eth_frames_validation.h",
            "io/src/ethernet_frames.c", "io/src/eth_frames_mac.c",
            "io/native/ethernet_frames.c", "io/native/ethernet_frames.h",
            "io/native/ethernet_frames_access.h"}
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))
        for name in required:
            with self.subTest(name=name):
                self.assertTrue((ROOT / name).is_file())
                self.assertTrue(source_sdk.selected(name))

    def test_selected_family_extensions_belong_to_provider_payloads(self):
        additions = {
            "stm32h743": ("drivers/flash_execution.c",
                          "private/stm32h743_flash_execution.h",
                          "drivers/flash_ram.c", "drivers/flash_ram_owner.c",
                          "drivers/flash_ram_transaction.c",
                          "drivers/flash_ram_enter.S", "drivers/flash_mpu.c",
                          "private/stm32h743_flash_ram.h",
                          "private/stm32h743_flash_ram_layout.h",
                          "private/stm32h743_flash_mpu.h",
                          "drivers/uart_fifo.c",
                          "private/stm32h743_uart_fifo.h",
                          "drivers/uart_duplex.c",
                          "private/stm32h743_uart_duplex.h",
                          "private/stm32h743_uart_continuous_impl.h",
                          "private/stm32h743_uart_dma_parts.h"),
            "stm32u575": ("drivers/uart_fifo.c",
                          "private/stm32u575_uart_fifo.h"),
            "stm32f407": ("drivers/ethernet_frames.c",
                          "private/stm32f407_eth_frames.h"),
            "gd32f470": ("drivers/ethernet_frames.c",
                         "private/gd32f470_eth_frames.h")}
        for family, relative in additions.items():
            with self.subTest(family=family):
                descriptor = source_sdk.provider_descriptors.maintained(family)
                required = {f"soc/{family}/" + name for name in relative}
                self.assertTrue(required.issubset(set(descriptor.owned_payload)),
                                sorted(required - set(descriptor.owned_payload)))
                self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                                sorted(required - set(source_sdk.REQUIRED_FILES)))

    def test_fifo_verification_inputs_and_public_smoke_are_required(self):
        directory = "cmake/package/verification/io_evolution/"
        required = {directory + name for name in (
            "h743-uart-fifo.toml", "u575-uart-fifo.toml",
            "consumers/uart-fifo.c")}
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))
        for name in required:
            with self.subTest(name=name):
                self.assertTrue((ROOT / name).is_file())
                self.assertTrue(source_sdk.selected(name))

    def test_finite_adc_group_inputs_public_smoke_and_real_sources_are_required(self):
        directory = "cmake/package/verification/io_evolution/"
        required = {directory + name for name in (
            "g474-adc-group-regular-software.toml",
            "g474-adc-group-regular-timer.toml",
            "g474-adc-group-injected-software.toml",
            "g474-adc-group-injected-timer.toml",
            "boards/g474-adc-group/board.json", "consumers/g474-adc-group.c")}
        required.update({
            "tools/configure/providers/stm32g474/adc_group.py",
            "soc/stm32g474/reviews/adc-group.json",
            "io/include/nexus/io/adc_group.h", "io/src/adc_group.c",
            "soc/stm32g474/private/stm32g474_adc_group.h",
            "soc/stm32g474/drivers/adc_group.c",
            "soc/common/stm32_identity.h", "core/include/nexus/core/silicon.h",
            "core/src/silicon.c"})
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))
        for name in required:
            with self.subTest(name=name):
                self.assertTrue(source_sdk.selected(name))

    def test_fifo_template_and_both_selected_family_leaves_are_required(self):
        required = {"io/private/uart_fifo_impl.h"}
        for family in ("stm32h743", "stm32u575"):
            required.update({f"soc/{family}/drivers/uart_fifo.c",
                             f"soc/{family}/private/{family}_uart_fifo.h"})
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))

    def test_selected_contract_helper_is_an_explicit_sdk_requirement(self):
        self.assertIn("tools/configure/selected_contracts.py",
                      source_sdk.REQUIRED_FILES)

    def test_consumer_plan_is_required_even_after_a_rehashed_omission(self):
        import hashlib
        required = (
            "tools/release/transport.py",
            "components/usb_control/include/nexus/components/usb_acm.h",
            "components/usb_control/src/usb_acm.c",
            "os/freertos/include/nexus/os/freertos_mpu_gpio.h",
            "os/freertos/mpu/gpio.c",
            "soc/stm32u575/private/stm32u575_gpio_mpu.h",
            "soc/stm32u575/mpu_user_init.c",
            "soc/stm32u575/private/stm32u575_mpu_user_init.h",
            "tools/configure/assemblies/u575-mpu-gpio-write.toml",
            "cmake/package/verification/io_evolution/consumers/u575-mpu-gpio.c",
        )
        self.assertIn(required[0], source_sdk.REQUIRED_FILES)
        with tempfile.TemporaryDirectory(prefix="SDK required plan ") as value:
            root = Path(value) / "prefix/share/nexus/src"
            root.mkdir(parents=True)
            # Filesystem/manifest admission only, not a usable SDK or build.
            payload = b"required-source fixture\n"
            files = {}
            # Explicit names keep new omissions executable against the old
            # production list; deriving the fixture only from REQUIRED_FILES
            # would otherwise never create a newly missing source.
            for name in sorted(set(source_sdk.REQUIRED_FILES) | set(required)):
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(payload)
                files[name] = hashlib.sha256(payload).hexdigest()
            prefix = root.parents[2]
            exports = {}
            for name in ("lib/cmake/Nexus/NexusConfig.cmake",
                         "lib/cmake/Nexus/NexusConfigVersion.cmake"):
                target = prefix / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(payload)
                exports[name] = hashlib.sha256(payload).hexdigest()
            identity = {"schema": 1, "kind": "nexus-installed-source-sdk",
                "source_revision": "a" * 40, "source_tree": "b" * 40,
                "publishable": True, "source_dirty": False,
                "files_sha256": files, "dependencies": [],
                "reviewed_imports": [], "package_files_sha256": exports,
                "toolchains_lock_sha256": files["dependencies/toolchains.lock.json"]}

            def write_identity():
                identity["snapshot_sha256"] = source_sdk.digest(json.dumps(
                    files, sort_keys=True, separators=(",", ":")).encode())
                (root / source_sdk.MANIFEST).write_text(json.dumps(identity))

            write_identity()
            with self.assertRaisesRegex(source_sdk.SDKError,
                                        "dependency commit identities"):
                source_sdk.verify(root)
            for name in required:
                with self.subTest(name=name):
                    original = files.pop(name)
                    (root / name).unlink()
                    try:
                        write_identity()
                        with self.assertRaisesRegex(
                                source_sdk.SDKError,
                                "missing required sources/licenses"):
                            source_sdk.verify(root)
                    finally:
                        (root / name).write_bytes(payload)
                        files[name] = original
                        write_identity()

    def test_portable_io_verification_inputs_and_public_smokes_are_required(self):
        directory = "cmake/package/verification/io_evolution/"
        names = {
            "h743-high-empty.toml", "h743-high-uart.toml",
            "h743-high-peripherals.toml", "u575-uart-gpdma.toml",
            "g474-can-fd-loopback.toml", "g474-can-fd-normal.toml",
            "g474-pwm-adc.toml", "boards/g474-fdcan/board.json",
            "boards/g474-pwm-adc/board.json", "consumers/h743-high.c",
            "consumers/u575-uart-gpdma.c", "consumers/g474-can-fd.c",
            "consumers/g474-pwm-adc.c"}
        required = {directory + name for name in names}
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))
        for name in required:
            with self.subTest(name=name):
                self.assertTrue(source_sdk.selected(name))

    def test_portable_verification_namespace_rejects_unreviewed_payloads(self):
        for name in ("model.c", "tests/consumer_test.cpp", "trace.raw",
                     "boards/unreviewed/board.json", "consumers/product.c",
                     "consumers/GoogleTest.cpp", "capture.xml"):
            with self.subTest(name=name):
                self.assertFalse(source_sdk.selected(
                    "cmake/package/verification/io_evolution/" + name))
        self.assertNotIn("tests", source_sdk.ROOT_DIRS)

    def test_coupled_io_public_private_and_generator_payloads_are_required(self):
        required = {
            "io/include/nexus/io/can_fd.h", "io/include/nexus/io/pwm_adc.h",
            "io/src/can_fd.c", "io/src/pwm_adc.c", "io/private/can_fd_impl.h",
            "tools/configure/coupled_io.py",
            "tools/configure/providers/stm32g474/coupled.py",
            "soc/stm32g474/drivers/fdcan.c",
            "soc/stm32g474/drivers/pwm_adc.c",
            "soc/stm32g474/private/stm32g474_fdcan.h",
            "soc/stm32g474/private/stm32g474_pwm_adc.h"}
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))

    def test_coupled_g474_units_and_generator_are_explicit_family_payload(self):
        descriptor = source_sdk.provider_descriptors.maintained("stm32g474")
        required = {"soc/stm32g474/drivers/fdcan.c",
                    "soc/stm32g474/drivers/pwm_adc.c",
                    "soc/stm32g474/private/stm32g474_fdcan.h",
                    "soc/stm32g474/private/stm32g474_pwm_adc.h"}
        self.assertTrue(required.issubset(set(descriptor.owned_payload)),
                        sorted(required - set(descriptor.owned_payload)))
        self.assertIn("tools/configure/providers/stm32g474/coupled.py",
                      descriptor.configuration_modules)

    def test_build_policy_and_tool_module_closure_is_required(self):
        required = {"cmake/platform/TargetPolicy.cmake",
                    "tools/configure/bundle.py", "tools/configure/dependencies.py",
                    "tools/configure/contracts.py", "tools/configure/matrix.py",
                    "tools/dev/dev.py", "tools/dev/presets.py", "tools/dev/process.py",
                    "tools/dev/change_paths.py", "tools/dev/cache.py",
                    "tools/dev/dependencies.py", "tools/dev/qualification.py",
                    "tools/evidence/store.py", "tools/evidence/environment.py",
                    "tools/evidence/recipe.py"}
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)))

    def test_metadata_and_memory_generation_are_required_sdk_payload(self):
        required = {"dependencies/platform-compatibility.json",
                    "tools/configure/metadata.py", "tools/configure/memory.py"}
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)))

    def test_test_framework_payload_is_not_a_firmware_sdk_dependency(self):
        for name in ("ext/googletest/googletest/src/gtest-all.cc",
                     "ext/googletest/googlemock/src/gmock-all.cc",
                     "ext/googletest/LICENSE",
                     "cmake/testing/GoogleTest.cmake",
                     "dependencies/source/googletest-1.16.0.tar.gz",
                     "dependencies/source/googletest-LICENSE.txt",
                     "tests/contracts/factory_test.cpp"):
            with self.subTest(name=name):
                self.assertFalse(source_sdk.selected(name))
        self.assertTrue(source_sdk.selected("dependencies/googletest.lock.json"))
        self.assertTrue(source_sdk.selected("io/include/nexus/io/factory.h"))
        self.assertTrue(source_sdk.selected("tools/configure/assemblies/native.toml"))

    def test_audit_archives_stay_outside_sdk_but_operational_tools_remain(self):
        for name in ("tools/testing/evidence/report.json",
                     "tools/testing/evidence/nested/results.xml",
                     "tools/testing/evidence/sources/provider.c.raw",
                     "tools/testing/evidence/firmware.elf"):
            with self.subTest(name=name):
                self.assertFalse(source_sdk.selected(name))
        for name in ("tools/testing/evidence.py",
                     "tools/testing/evidence_extra/report.json",
                     "tools/testing/test_source_identity.py",
                     "tools/evidence/verify.py",
                     "tools/maintenance/stm32g4_lock.py",
                     "dependencies/googletest.lock.json"):
            with self.subTest(name=name):
                self.assertTrue(source_sdk.selected(name))

    def test_consumer_uses_authored_toml_for_each_maintained_family(self):
        with tempfile.TemporaryDirectory(prefix="SDK consumer inputs ") as value:
            directory = Path(value)
            policy = directory / "prefix/share/nexus/src/dependencies/platform-compatibility.json"
            policy.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / "dependencies/platform-compatibility.json", policy)
            for name, arm, chip in (("native", False, "native"),
                                    ("stm32", True, "stm32"),
                                    ("gd32", True, "gd32")):
                _, _, command = consumer_gate.prepare_consumer(
                    directory / "prefix", directory, name,
                    lambda arguments: None, arm=arm, chip=chip)
                assembly = next(argument for argument in command
                                if argument.startswith("-DNEXUS_ASSEMBLY_FILE="))
                self.assertTrue(assembly.endswith(".toml"), assembly)

    def test_optional_handoff_is_consumed_only_by_the_exact_f407_arm_project(self):
        for arm, chip in ((False, "stm32"), (True, "stm32"), (True, "gd32")):
            with self.subTest(arm=arm, chip=chip), tempfile.TemporaryDirectory() as value:
                directory = Path(value)
                policy = directory / "prefix/share/nexus/src/dependencies/platform-compatibility.json"
                policy.parent.mkdir(parents=True)
                shutil.copyfile(ROOT / "dependencies/platform-compatibility.json", policy)
                source, _, _ = consumer_gate.prepare_consumer(
                    directory / "prefix", directory, "optional",
                    lambda arguments: None, arm=arm, chip=chip)
                cmake = (source / "CMakeLists.txt").read_text()
                code = (source / "main.cpp").read_text()
                expected = arm and chip == "stm32"
                self.assertEqual("Nexus::STM32F407Handoff" in cmake, expected)
                self.assertEqual("nx_stm32f407_handoff_initialize(0)" in code,
                                 expected)
                self.assertIn("Nexus::MappedStorage", cmake)
                self.assertIn("Nexus::AMPIPC", cmake)
                self.assertIn("Nexus::UserIoBroker", cmake)
                self.assertIn('"nexus/components/user_io_async_storage.h"',
                              code)
                self.assertIn(
                    "nx_user_io_async_construct(nullptr, nullptr, 0U, nullptr) "
                    "!= NX_ERROR_INVALID", code)
                self.assertIn("nx_amp_ipc_construct(nullptr, nullptr) != NX_ERROR_INVALID", code)

    def test_optional_frame_blitter_is_consumed_only_by_h743_projects(self):
        for family in ("h743", "u575", "f103", "gd303", "g474"):
            for backend in ("baremetal", "freertos"):
                with self.subTest(family=family, backend=backend), tempfile.TemporaryDirectory() as value:
                    directory = Path(value)
                    policy = directory / "prefix/share/nexus/src/dependencies/platform-compatibility.json"
                    policy.parent.mkdir(parents=True)
                    shutil.copyfile(ROOT / "dependencies/platform-compatibility.json", policy)
                    source, _, _ = consumer_gate.prepare_modern_consumer(
                        directory / "prefix", directory, family, backend,
                        lambda arguments: None)
                    cmake = (source / "CMakeLists.txt").read_text()
                    code = (source / "main.c").read_text()
                    expected = family == "h743"
                    self.assertEqual("Nexus::H743FrameDMA2D" in cmake, expected)
                    for public_contract in (
                            '#include "nexus/components/frame_dma2d.h"',
                            "nx_h743_frame_dma2d_initialize(",
                            "NX_FRAME_BLIT_ARGB8888_TO_RGB565"):
                        self.assertEqual(public_contract in code, expected,
                                         public_contract)

    def test_exposed_io_core_clock_and_provider_closures_are_required(self):
        required = {
            "arch/CMakeLists.txt", "cmake/toolchains/arm-gcc.cmake",
            "os/wait.c", "os/include/nexus/os/wait.h",
            "os/baremetal/notify.c", "os/native/native.c",
            "components/CMakeLists.txt",
            "core/include/nexus/core/fault.h", "core/src/fault.c",
            "core/include/nexus/core/stack_observation.h",
            "core/src/stack_observation.c", "io/CMakeLists.txt",
            "io/include/nexus/io/usb_device.h", "io/src/usb_device.c",
            "io/src/usb_device_controller.c", "io/private/usb_device_controller.h",
            "io/native/usb_device_access.h", "io/native/usb_device.c",
            "io/include/nexus/io/ethernet.h", "io/include/nexus/io/mdio.h",
            "io/src/eth_mac.c", "io/private/eth_mac.h",
            "io/native/ethernet_access.h", "io/native/ethernet.c",
            "io/src/can_controller.c", "io/private/can_controller.h",
            "io/include/nexus/io/timer_capture.h",
            "io/include/nexus/io/advanced_pwm.h",
            "io/private/timer_advanced_controller.h",
            "tools/configure/bindings.py", "tools/configure/emission.py",
            "tools/configure/usb_device.py", "tools/configure/ethernet.py",
            "tools/configure/timer_advanced.py",
            "cmake/platform/Configure.cmake", "cmake/platform/ControllerModes.cmake",
            "os/freertos/timebase_lowpower.c",
            "os/freertos/private/timebase_lowpower.h",
            "os/freertos/stack_snapshot.c",
            "os/freertos/include/nexus/os/freertos_stack.h",
            "components/clock_domain/src/clock_domain.c",
            "components/amp_ipc/CMakeLists.txt",
            "components/amp_ipc/src/amp_ipc.c",
            "components/amp_ipc/include/nexus/components/amp_ipc.h",
            "components/amp_ipc/include/nexus/components/amp_ipc_storage.h",
            "components/stm32f407_handoff/CMakeLists.txt",
            "components/stm32f407_handoff/include/nexus/components/stm32f407_handoff.h",
            "soc/stm32f407/handoff.c",
            "soc/stm32f407/private/stm32f407_handoff_hw.h",
            "io/include/nexus/io/frame_stream.h", "io/src/frame_stream.c",
            "components/frame_dma2d/CMakeLists.txt",
            "components/frame_dma2d/include/nexus/components/frame_dma2d.h",
            "components/frame_dma2d/src/h743_frame_dma2d.c",
            "components/mapped_storage/CMakeLists.txt",
            "components/mapped_storage/src/mapped_storage.c",
            "components/mapped_storage/src/mapped_storage_owner.c",
            "components/mapped_storage/src/native_mapped_storage.c",
            "components/mapped_storage/include/nexus/components/mapped_storage.h",
            "components/mapped_storage/include/nexus/components/mapped_storage_owner.h",
            "components/mapped_storage/include/nexus/components/native_mapped_storage.h",
            "components/user_io_broker/src/user_io_broker.c",
            "components/dp83848/src/dp83848.c",
            "scripts/ci/vendor_import_identity.py", "scripts/ci/verify_vendor_source.py",
        }
        for family in ("stm32f407", "gd32f470"):
            required.update(f"soc/{family}/" + name for name in (
                "system.c", "timebase_lowpower.c", "usb_clock.c",
                "drivers/usb_device.c", "drivers/ethernet.c", "drivers/can.c",
                "drivers/timer_capture.c", "drivers/advanced_pwm.c",
                "CMakeLists.txt", "soc.json", "routes.json"))
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))


class SourceSDKPortableVerificationTests(unittest.TestCase):
    """One authored input is shared by models, ARM presets and installed SDKs."""

    def test_canonical_inputs_resolve_and_old_authored_copies_are_removed(self):
        from tools.configure import configure
        directory = ROOT / "cmake/package/verification/io_evolution"
        moves = {
            "h743-high-empty.toml": "h743_high_assembly/empty.toml",
            "h743-high-uart.toml": "h743_high_assembly/io.toml",
            "h743-high-peripherals.toml": "h743_high_assembly/peripherals.toml",
            "u575-uart-gpdma.toml": "u575_assembly/usart1-dma-tx.toml",
            "g474-can-fd-loopback.toml": "g474_control_assembly/can-fd.toml",
            "g474-can-fd-normal.toml": "g474_control_assembly/can-fd-normal.toml",
            "g474-pwm-adc.toml": "g474_control_assembly/pwm-adc.toml"}
        for current, previous in moves.items():
            with self.subTest(assembly=current):
                self.assertTrue((directory / current).is_file())
                result = configure.resolve(directory / current, ROOT)
                self.assertEqual(result["backend"], "baremetal")
                self.assertFalse((ROOT / "tests/contracts" / previous).exists())
        for current, previous in (
                ("g474-fdcan", "fdcan_board"),
                ("g474-pwm-adc", "pwm_adc_board")):
            with self.subTest(board=current):
                board = json.loads((directory / "boards" / current /
                                    "board.json").read_text())
                self.assertIsNone(board["physical_pcb_revision"])
                self.assertFalse((ROOT / "tests/contracts/g474_control_assembly" /
                                  previous / "board.json").exists())

    def test_all_seven_normal_presets_select_the_installed_canonical_inputs(self):
        presets = json.loads((ROOT / "CMakePresets.json").read_text())
        expected = {
            "h743-high-empty", "h743-high-uart", "h743-high-peripherals",
            "u575-uart-gpdma", "g474-can-fd", "g474-can-fd-normal",
            "g474-pwm-adc"}
        selected = {}
        for preset in presets["configurePresets"]:
            name = preset["name"].removeprefix("fixture-io-evolution-")
            if name not in expected:
                continue
            value = preset["cacheVariables"]["NEXUS_ASSEMBLY_FILE"]
            self.assertTrue(value.startswith(
                "${sourceDir}/cmake/package/verification/io_evolution/"), value)
            selected[name] = Path(value).name
        self.assertEqual(set(selected), expected)
        self.assertEqual(len(set(selected.values())), 7)

    def test_public_smokes_do_not_import_concrete_providers_or_vendor_headers(self):
        directory = ROOT / "cmake/package/verification/io_evolution/consumers"
        for name in ("h743-high", "u575-uart-gpdma", "g474-can-fd",
                     "g474-pwm-adc", "g474-adc-group", "uart-fifo",
                     "ethernet-frames"):
            with self.subTest(smoke=name):
                source = (directory / (name + ".c")).read_text()
                self.assertIn('#include "nexus_factory.h"', source)
                self.assertIn("nx_platform_start()", source)
                self.assertIn("nx_platform_stop()", source)
                self.assertNotIn("#include \"stm32", source)
                self.assertNotIn("/private/", source)
                self.assertNotIn("GoogleTest", source)

    def test_four_adc_group_normal_presets_share_one_canonical_software_board(self):
        import tomllib
        from tools.configure import configure
        directory = ROOT / "cmake/package/verification/io_evolution"
        expected = {"g474-adc-group-" + name + ".toml" for name in (
            "regular-software", "regular-timer", "injected-software",
            "injected-timer")}
        presets = json.loads((ROOT / "CMakePresets.json").read_text())
        selected = [preset.get("cacheVariables", {}).get("NEXUS_ASSEMBLY_FILE", "")
                    for preset in presets["configurePresets"]]
        for name in expected:
            with self.subTest(assembly=name):
                file = directory / name
                result = configure.resolve_ir(file, ROOT)
                self.assertEqual(result.controllers[0].kind, "adc_group")
                package = tomllib.loads(file.read_text())["board_package"]
                self.assertEqual((file.parent / package).resolve(),
                                 directory / "boards/g474-adc-group")
                canonical = "${sourceDir}/" + file.relative_to(ROOT).as_posix()
                self.assertEqual(selected.count(canonical), 1)

    def test_enhanced_ethernet_inputs_keep_software_only_board_boundaries(self):
        import tomllib
        from tools.configure import configure
        directory = ROOT / "cmake/package/verification/io_evolution"
        presets = json.loads((ROOT / "CMakePresets.json").read_text())
        selected = [preset.get("cacheVariables", {}).get("NEXUS_ASSEMBLY_FILE", "")
                    for preset in presets["configurePresets"]]
        for part in ("f407zg", "gd470"):
            with self.subTest(part=part):
                file = directory / (part + "-eth-frames.toml")
                result = configure.resolve_ir(file, ROOT)
                self.assertEqual(result.controllers[0].kind, "eth_frames")
                self.assertEqual(result.controllers[0].mode, "rmii-vlan-sg-ptp")
                package = tomllib.loads(file.read_text())["board_package"]
                board = json.loads((file.parent / package / "board.json").read_text())
                self.assertIsNone(board["physical_pcb_revision"])
                self.assertTrue(board["unknowns"])
                canonical = "${sourceDir}/" + file.relative_to(ROOT).as_posix()
                self.assertEqual(selected.count(canonical), 1)

    def test_both_fifo_normal_presets_reuse_the_installed_canonical_inputs(self):
        from tools.configure import configure
        directory = ROOT / "cmake/package/verification/io_evolution"
        presets = json.loads((ROOT / "CMakePresets.json").read_text())
        selected = [preset.get("cacheVariables", {}).get("NEXUS_ASSEMBLY_FILE", "")
                    for preset in presets["configurePresets"]]
        for family in ("h743", "u575"):
            with self.subTest(family=family):
                file = directory / (family + "-uart-fifo.toml")
                result = configure.resolve_ir(file, ROOT)
                self.assertEqual(result.controllers[0].mode, "irq-fifo-bytes")
                canonical = "${sourceDir}/" + file.relative_to(ROOT).as_posix()
                self.assertEqual(selected.count(canonical), 1)


class SourceSDKSTM32OfficialSelectionTests(unittest.TestCase):
    def test_official_f4_ll_dependency_and_verifier_closure_are_required(self):
        required = {
            "dependencies/stm32f4-hal.lock.json",
            "tools/maintenance/stm32f4_hal_lock.py",
            "cmake/platform/STM32Official.cmake",
            "vendors/st/stm32f4_hal/LICENSE.md",
            "vendors/st/stm32f4_hal/Inc/stm32f4xx_ll_gpio.h",
            "vendors/st/stm32f4_hal/Inc/stm32f4xx_ll_usart.h",
            "vendors/st/stm32f4_hal/Inc/stm32f4xx_ll_spi.h",
            "vendors/st/stm32f4_hal/Inc/stm32f4xx_ll_adc.h",
            "vendors/st/stm32f4_hal/Inc/stm32f4xx_ll_tim.h",
            "vendors/st/stm32f4_hal/Inc/stm32f4xx_ll_iwdg.h",
            "vendors/st/stm32f4_hal/Inc/stm32f4xx_ll_i2c.h",
        }
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))
        self.assertIn("vendors/st/stm32f4_hal", source_sdk.REQUIRED_DEPENDENCIES)

    def test_f4_sdk_redistributes_only_selected_headers_and_notice(self):
        selected = source_sdk.dependency_entries(ROOT, "vendors/st/stm32f4_hal")
        self.assertEqual(set(selected), {
            "Inc/stm32f4xx_ll_gpio.h", "Inc/stm32f4xx_ll_usart.h",
            "LICENSE.md",
            "Inc/stm32f4xx_ll_spi.h", "Inc/stm32f4xx_ll_adc.h",
            "Inc/stm32f4xx_ll_tim.h", "Inc/stm32f4xx_ll_iwdg.h",
            "Inc/stm32f4xx_ll_i2c.h",
            "Inc/stm32f4xx_hal_eth.h", "Inc/stm32f4xx_hal_def.h",
            "Inc/Legacy/stm32_hal_legacy.h",
        })


class SourceSDKThreadXSelectionTests(unittest.TestCase):
    def test_threadx_dependency_snapshot_is_exact_reviewed_source_closure(self):
        lock = json.loads((ROOT / "dependencies/threadx.lock.json").read_text())
        selected = source_sdk.dependency_entries(ROOT, "ext/threadx")
        self.assertEqual(set(selected), set(lock["files"]) | {lock["license"]["path"]})
        self.assertEqual(len(selected), 215)
        self.assertNotIn("ports/cortex_m7/gnu/src/tx_thread_schedule.S", selected)

    def test_approved_origin_declaration_is_required_and_packaged(self):
        self.assertTrue(source_sdk.selected(".gitmodules"))
        self.assertIn(".gitmodules", source_sdk.REQUIRED_FILES)

    def test_all_reviewed_threadx_sources_headers_license_and_owned_composition_are_required(self):
        lock = json.loads((ROOT / "dependencies/threadx.lock.json").read_text())
        official = {"ext/threadx/" + name for name in lock["files"]}
        official.add("ext/threadx/" + lock["license"]["path"])
        owned = {"dependencies/threadx.lock.json", "tools/maintenance/threadx_lock.py",
                 "tools/maintenance/source_catalogue.py", "tools/configure/threadx.py",
                 "tools/configure/lowpower.py", "os/threadx/CMakeLists.txt",
                 "os/threadx/Kernel.cmake", "os/threadx/threadx.c", "os/threadx/m4.c",
                 "os/threadx/m4_vectors.S", "os/threadx/prepare_linux.py",
                 "os/threadx/config/tx_user.h", "os/threadx/include/nexus/os/threadx.h"}
        self.assertTrue((official | owned).issubset(set(source_sdk.REQUIRED_FILES)))
        self.assertIn("ext/threadx", source_sdk.REQUIRED_DEPENDENCIES)


class SourceSDKG4SelectionTests(unittest.TestCase):
    def test_g4_snapshot_contains_only_five_reviewed_sources_and_original_notice(self):
        lock = json.loads((ROOT / "dependencies/stm32g4.lock.json").read_text())
        entries = source_sdk.dependency_entries(ROOT, "vendors/st/cmsis_device_g4")
        self.assertEqual(set(entries), set(lock["files"]))
        self.assertEqual(len(entries), 5)
        self.assertNotIn("Include/stm32g491xx.h", entries)

    def test_g4_lock_verifier_and_all_five_official_files_are_required(self):
        lock = json.loads((ROOT / "dependencies/stm32g4.lock.json").read_text())
        required = {"dependencies/stm32g4.lock.json",
                    "tools/maintenance/stm32g4_lock.py"}
        required.update("vendors/st/cmsis_device_g4/" + name
                        for name in lock["files"])
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)))
        self.assertIn("vendors/st/cmsis_device_g4", source_sdk.REQUIRED_DEPENDENCIES)

    def test_closed_g4_owned_platform_and_reference_sources_are_required(self):
        required = {
            "soc/stm32g474/CMakeLists.txt", "soc/stm32g474/system.c",
            "soc/stm32g474/drivers/gpio.c", "soc/stm32g474/drivers/uart.c",
            "soc/stm32g474/private/stm32g474_system.h",
            "soc/stm32g474/private/stm32g474_provider.h",
            "soc/stm32g474/soc.json", "soc/stm32g474/routes.json",
            "boards/stm32g474re_hse24_reference/board.json",
            "tools/configure/providers/stm32g474/__init__.py",
            "tools/configure/providers/stm32g474/bindings.py",
            "tools/configure/providers/stm32g474/emission.py"}
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))


class SourceSDKDependencySnapshotTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="SDK staged dependency ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.environment = {key: value for key, value in os.environ.items()
                            if not key.startswith("GIT_")}
        self.git(self.root, "init", "--quiet")
        from tools.maintenance.source_catalogue import ALL_SOURCE_DEPENDENCIES
        (self.root / ".gitmodules").write_text("".join(
            f'[submodule "{item.path}"]\npath = {item.path}\nurl = {item.origin}\n'
            for item in ALL_SOURCE_DEPENDENCIES))
        (self.root / "baseline").write_text("committed source baseline\n")
        self.git(self.root, "add", ".")
        self.commit(self.root, "committed baseline without new dependency")
        self.baseline = self.git(self.root, "rev-parse", "HEAD").strip()
        self.path = "vendors/st/stm32f4_hal"
        self.checkout = self.root / self.path
        self.checkout.parent.mkdir(parents=True)
        self.git(self.root, "clone", "--quiet", "--shared",
                 str(ROOT / self.path), str(self.checkout))
        self.git(self.checkout, "remote", "set-url", "origin",
                 "https://github.com/STMicroelectronics/stm32f4xx-hal-driver.git")
        self.git(self.root, "add", self.path)
        self.git(self.root, "submodule", "init", "--", self.path)
        # This test exercises the source-snapshot boundary using one real,
        # clean official dependency. Full SDK tests below retain every source.
        patch = mock.patch.object(source_sdk, "REQUIRED_DEPENDENCIES", (self.path,))
        patch.start()
        self.addCleanup(patch.stop)

    def git(self, root, *arguments, input=None):
        result = subprocess.run(["git", *GIT_FIXTURE_OPTIONS, "-C", str(root), *arguments],
                                input=input, capture_output=True,
                                env=self.environment, timeout=30)
        self.assertEqual(result.returncode, 0,
                         result.stdout.decode() + result.stderr.decode())
        return result.stdout.decode()

    def commit(self, root, message):
        self.git(root, "-c", "user.name=SDK snapshot test", "-c",
                 "user.email=sdk-snapshot@local.invalid", "commit", "--quiet",
                 "--allow-empty", "-m", message)

    def test_parent_and_cloned_dependency_do_not_start_auto_maintenance(self):
        # A host may enable maintenance even for a tiny temporary repository.
        # Each Git child must finish before TemporaryDirectory removes it.
        for name, repository in (("owner", self.root),
                                 ("dependency", self.checkout)):
            with self.subTest(repository=name):
                for key, value in (("maintenance.auto", "true"),
                                   ("maintenance.autoDetach", "true"),
                                   ("maintenance.gc.enabled", "false"),
                                   ("maintenance.loose-objects.enabled", "true"),
                                   ("maintenance.loose-objects.auto", "1"),
                                   ("gc.auto", "1"),
                                   ("gc.autoDetach", "true")):
                    self.git(repository, "config", key, value)
                trace = self.root / (name + "-git-trace.json")
                self.environment["GIT_TRACE2_EVENT"] = str(trace)
                try:
                    self.commit(repository, "fixture lifetime boundary")
                finally:
                    self.environment.pop("GIT_TRACE2_EVENT", None)
                events = [json.loads(line) for line in trace.read_text().splitlines()]
                self.assertTrue(any(event.get("event") == "exit" and
                                    event.get("code") == 0 for event in events),
                                events)
                started = [event.get("argv", []) for event in events
                           if event.get("event") == "child_start"]
                self.assertFalse(
                    [command for command in started
                     if "maintenance" in command or "gc" in command], started)

    def test_explicit_development_fixture_uses_staged_clean_gitlink(self):
        before = self.git(self.root, "write-tree").strip()
        entries, index = source_sdk.dependency_snapshot(self.root, True)
        self.assertEqual(index, before)
        records = source_sdk.dependency_records(self.root, entries)
        self.assertEqual([record["path"] for record in records], [self.path])
        self.assertEqual(records[0]["commit"],
                         self.git(self.checkout, "rev-parse", "HEAD").strip())
        source_sdk.verify_preparation_snapshot(self.root, self.baseline, index)
        self.assertEqual(self.git(self.root, "write-tree").strip(), before)
        self.assertEqual(self.git(self.root, "rev-parse", "HEAD").strip(), self.baseline)

    def test_publishable_selection_stays_on_head_and_rejects_staged_addition(self):
        entries, index = source_sdk.dependency_snapshot(self.root, False)
        self.assertIsNone(index)
        self.assertNotIn(self.path, entries)
        with self.assertRaisesRegex(ValueError, "required dependency Gitlink"):
            source_sdk.dependency_records(self.root, entries)
        with self.assertRaisesRegex(ValueError, "dirty source checkout"):
            source_sdk.prepare(self.root, self.root.parent / (self.root.name + " output"))

    def test_unmerged_dependency_index_rejects_without_changing_index(self):
        commit = self.git(self.checkout, "rev-parse", "HEAD").strip()
        self.git(self.root, "update-index", "--index-info", input=(
            f'0 {"0" * 40}\t{self.path}\n'
            f'160000 {commit} 1\t{self.path}\n'
            f'160000 {commit} 2\t{self.path}\n').encode())
        before = self.git(self.root, "ls-files", "--stage")
        with self.assertRaises(ValueError):
            source_sdk.dependency_snapshot(self.root, True)
        self.assertEqual(self.git(self.root, "ls-files", "--stage"), before)

    def test_clean_checkout_different_from_staged_pin_and_dirty_source_reject(self):
        entries, _ = source_sdk.dependency_snapshot(self.root, True)
        extra = self.checkout / "untracked-source.c"
        extra.write_text("int untracked_dependency;\n")
        with self.assertRaisesRegex(ValueError, "dirty"):
            source_sdk.dependency_records(self.root, entries)
        extra.unlink()
        self.commit(self.checkout, "different local dependency revision")
        with self.assertRaisesRegex(ValueError, "pinned dependency"):
            source_sdk.dependency_records(self.root, entries)

    def test_staged_index_change_during_preparation_rejects(self):
        _, index = source_sdk.dependency_snapshot(self.root, True)
        (self.root / "later").write_text("index changed during preparation\n")
        self.git(self.root, "add", "later")
        with self.assertRaisesRegex(ValueError, "index changed"):
            source_sdk.verify_preparation_snapshot(self.root, self.baseline, index)

    def test_head_change_during_preparation_rejects(self):
        _, index = source_sdk.dependency_snapshot(self.root, True)
        self.commit(self.root, "HEAD changed during preparation")
        with self.assertRaisesRegex(ValueError, "commit changed"):
            source_sdk.verify_preparation_snapshot(self.root, self.baseline, index)


class SourceSDKDevelopmentSnapshotTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="SDK staged source ")
        self.addCleanup(self.temporary.cleanup)
        self.repository = Path(self.temporary.name)
        self.environment = {
            key: value for key, value in os.environ.items()
            if not key.startswith("GIT_")}
        self.git("init", "--quiet")
        self.write("core/committed.c", "int committed_source;\n")
        self.write("docs/committed.txt", "external documentation\n")
        self.git("add", ".")
        self.git("-c", "user.name=SDK test", "-c",
                 "user.email=sdk-test@local.invalid", "commit", "--quiet",
                 "-m", "committed snapshot baseline")
        self.entries = source_sdk.tree_entries(self.repository)
        self.write("core/committed.c", "int working_source;\n")
        self.write("arch/cortex_m/staged.c", "int staged_source;\n")
        self.write("docs/staged.txt", "nonselected staged source\n")
        self.git("add", "arch/cortex_m/staged.c", "docs/staged.txt")
        self.write("tools/untracked.py", "untracked_source = True\n")
        self.write("docs/untracked.txt", "nonselected untracked source\n")
        self.write("dependencies/source/host-framework.tar.gz", "host only\n")

    def git(self, *arguments):
        result = subprocess.run(
            ["git", *GIT_FIXTURE_OPTIONS, "-C", str(self.repository), *arguments],
            env=self.environment, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0,
                         result.stdout.decode() + result.stderr.decode())
        return result.stdout

    def write(self, name, data):
        file = self.repository / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(data)

    def test_development_snapshot_includes_staged_and_untracked_sources(self):
        before = self.git("write-tree")
        names = source_sdk.snapshot_files(
            self.repository, self.entries, development_fixture=True)
        self.assertEqual(names, {
            "core/committed.c", "arch/cortex_m/staged.c",
            "tools/untracked.py"})
        self.assertEqual(self.git("write-tree"), before)

    def test_publishable_snapshot_uses_only_original_committed_blobs(self):
        names = source_sdk.snapshot_files(
            self.repository, self.entries, development_fixture=False)
        self.assertEqual(names, {"core/committed.c"})
        blobs = source_sdk.committed_blobs(
            self.repository, {name: self.entries[name] for name in names})
        self.assertEqual(blobs["core/committed.c"], b"int committed_source;\n")
        self.assertNotEqual(blobs["core/committed.c"],
                            (self.repository / "core/committed.c").read_bytes())

    def test_real_git_snapshots_exclude_archives_in_both_package_modes(self):
        committed = ("tools/testing/evidence/committed/report.json",
                     "tools/testing/evidence.py",
                     "tools/testing/evidence_extra/committed.json")
        for name in committed:
            self.write(name, "committed SDK selection fixture\n")
        self.git("add", *committed)
        self.git("-c", "user.name=SDK test", "-c",
                 "user.email=sdk-test@local.invalid", "commit", "--quiet",
                 "-m", "committed archive and operational tool boundaries")
        entries = source_sdk.tree_entries(self.repository)
        staged = ("tools/testing/evidence/staged/results.xml",
                  "tools/testing/evidence_extra/staged.py")
        for name in staged:
            self.write(name, "staged SDK selection fixture\n")
        self.git("add", *staged)
        untracked = ("tools/testing/evidence/untracked/provider.c.raw",
                     "tools/evidence/verify.py")
        for name in untracked:
            self.write(name, "untracked SDK selection fixture\n")
        index_before = self.git("write-tree")
        for development in (False, True):
            with self.subTest(development=development):
                names = source_sdk.snapshot_files(
                    self.repository, entries, development_fixture=development)
                self.assertNotIn(committed[0], names)
                self.assertNotIn(staged[0], names)
                self.assertNotIn(untracked[0], names)
                self.assertIn(committed[1], names)
                self.assertIn(committed[2], names)
                self.assertEqual(staged[1] in names, development)
                self.assertEqual(untracked[1] in names, development)
        self.assertEqual(self.git("write-tree"), index_before)


class SourceSDKTests(unittest.TestCase):
    def test_rehashed_import_cannot_add_an_unreviewed_firmware_file(self):
        import hashlib
        manifest = self.sdk / ".nexus-source-sdk.json"
        original_manifest = manifest.read_bytes()
        for family in ("gd32f30x", "gd32f4xx"):
            with self.subTest(family=family):
                name = "vendors/gigadevice/" + family + "/Firmware/unreviewed.h"
                target = self.sdk / name
                self.assertFalse(target.exists())
                try:
                    target.write_bytes(b"/* outside the original vendor lock */\n")
                    identity = json.loads(original_manifest)
                    identity["files_sha256"][name] = hashlib.sha256(target.read_bytes()).hexdigest()
                    identity["snapshot_sha256"] = hashlib.sha256(json.dumps(
                        identity["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode()).hexdigest()
                    manifest.write_text(json.dumps(identity, indent=2, sort_keys=True) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("outside the reviewed import", result.stderr)
                finally:
                    target.unlink(missing_ok=True)
                    manifest.write_bytes(original_manifest)

    def test_rehashed_import_cannot_add_an_unreviewed_auxiliary_file(self):
        import hashlib
        manifest = self.sdk / ".nexus-source-sdk.json"
        original_manifest = manifest.read_bytes()
        for family in ("gd32f30x", "gd32f4xx"):
            with self.subTest(family=family):
                name = "vendors/gigadevice/" + family + "/unreviewed.txt"
                target = self.sdk / name
                self.assertFalse(target.exists())
                try:
                    target.write_bytes(b"outside the reviewed import notice set\n")
                    identity = json.loads(original_manifest)
                    identity["files_sha256"][name] = hashlib.sha256(target.read_bytes()).hexdigest()
                    identity["snapshot_sha256"] = hashlib.sha256(json.dumps(
                        identity["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode()).hexdigest()
                    manifest.write_text(json.dumps(identity, indent=2, sort_keys=True) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("outside the reviewed import", result.stderr)
                finally:
                    target.unlink(missing_ok=True)
                    manifest.write_bytes(original_manifest)

    def test_rehashed_vendor_import_bytes_still_must_match_original_lock(self):
        import hashlib
        manifest = self.sdk / ".nexus-source-sdk.json"
        original_manifest = manifest.read_bytes()
        for family in ("gd32f30x", "gd32f4xx"):
            with self.subTest(family=family):
                directory = "vendors/gigadevice/" + family
                lock = json.loads((self.sdk / directory / "source.lock.json").read_text())
                name = directory + "/" + lock["files"][0]["path"]
                target = self.sdk / name
                original = target.read_bytes()
                try:
                    target.write_bytes(bytes([original[0] ^ 1]) + original[1:])
                    identity = json.loads(original_manifest)
                    identity["files_sha256"][name] = hashlib.sha256(target.read_bytes()).hexdigest()
                    identity["snapshot_sha256"] = hashlib.sha256(json.dumps(
                        identity["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode()).hexdigest()
                    manifest.write_text(json.dumps(identity, indent=2, sort_keys=True) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("Imported source changed", result.stderr)
                finally:
                    target.write_bytes(original)
                    manifest.write_bytes(original_manifest)

    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="nexus installed source sdk ")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.directory = Path(cls.temporary.name)
        cls.run_ok(["git", "init", "--quiet", str(cls.directory)])
        (cls.directory / "parent-identity.txt").write_text("consumer owner\n")
        cls.run_ok(["git", "-C", str(cls.directory), "add", "parent-identity.txt"])
        cls.run_ok(["git", "-C", str(cls.directory), "-c", "user.name=SDK test", "-c", "user.email=sdk-test@local.invalid",
                    "commit", "--quiet", "-m", "unrelated consumer owner"])
        original = cls.directory / "initial package"
        cls.prefix = cls.directory / "relocated prefix with spaces"
        cls.run_ok([sys.executable, "-B", str(ROOT / "cmake/package/package_source_sdk.py"),
                    "--source", str(ROOT), "--output", str(original), "--development-fixture"])
        original.rename(cls.prefix)
        cls.sdk = cls.prefix / "share/nexus/src"
        cls.identity = json.loads((cls.sdk / ".nexus-source-sdk.json").read_text())

    @staticmethod
    def run_process(arguments, **kwargs):
        environment = os.environ.copy()
        for key in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                    "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH", "LIBRARY_PATH"):
            environment.pop(key, None)
        kwargs.setdefault("env", environment)
        if arguments[0] == "git":
            arguments = ["git", *GIT_FIXTURE_OPTIONS, *arguments[1:]]
        return subprocess.run(arguments, text=True, capture_output=True, timeout=180, **kwargs)

    @classmethod
    def run_ok(cls, arguments, **kwargs):
        result = cls.run_process(arguments, **kwargs)
        if result.returncode:
            raise AssertionError(
                f"Command failed (exit {result.returncode}): {arguments!r}\n"
                + result.stdout + result.stderr)
        return result

    def verify(self):
        return self.run_process([sys.executable, "-B", str(self.sdk / "cmake/package/package_source_sdk.py"),
                         "--verify", str(self.sdk)])

    def standalone_resolver_commands(self):
        directory = self.directory / "standalone resolver boundary"
        directory.mkdir(exist_ok=True)
        soc = json.loads((self.sdk / "soc/native/soc.json").read_text())
        (directory / "cpu.json").write_text(json.dumps({
            "schema_version": 1, "cpu": soc["cpu"], "irq": soc["irq"],
            "clock_hz": 1000000}) + "\n")
        runtime = directory / "runtime.toml"
        runtime.write_text('schema_version=1\ncpu_facts="cpu.json"\n'
                           'backend="baremetal"\noptimization="Os"\n')
        return {
            "platform": [sys.executable, "-B", str(self.sdk / "tools/configure/configure.py"),
                "--assembly", str(self.sdk / "tools/configure/assemblies/native-empty.toml"),
                "--output", str(directory / "platform-generated")],
            "runtime": [sys.executable, "-B", str(self.sdk / "tools/configure/runtime.py"),
                "--assembly", str(runtime), "--output", str(directory / "runtime-generated")],
            "cpu": [sys.executable, "-B", str(self.sdk / "tools/configure/cpu.py"),
                "--facts", str(directory / "cpu.json"), "--backend", "baremetal",
                "--output", str(directory / "cpu-generated")],
        }

    def test_standalone_resolvers_reject_source_byte_tamper(self):
        commands = self.standalone_resolver_commands()
        for command in commands.values():
            self.run_ok(command)
        target = self.sdk / "io/src/gpio.c"
        original = target.read_bytes()
        try:
            target.write_bytes(original + b"\n/* Source SDK payload tamper. */\n")
            for name, command in commands.items():
                with self.subTest(boundary=name):
                    rejected = self.run_process(command)
                    self.assertNotEqual(rejected.returncode, 0)
                    self.assertIn("Source SDK file identity changed: io/src/gpio.c",
                                  rejected.stderr)
                    self.assertFalse(Path(command[-1]).exists())
        finally:
            target.write_bytes(original)

    def test_standalone_resolvers_reject_rehashed_required_module_omission(self):
        import hashlib
        commands = self.standalone_resolver_commands()
        target = self.sdk / "tools/dev/cache.py"
        manifest = self.sdk / ".nexus-source-sdk.json"
        original, original_manifest = target.read_bytes(), manifest.read_bytes()
        try:
            target.unlink()
            value = json.loads(original_manifest)
            del value["files_sha256"]["tools/dev/cache.py"]
            value["snapshot_sha256"] = hashlib.sha256(json.dumps(
                value["files_sha256"], sort_keys=True, separators=(",", ":")).encode()).hexdigest()
            manifest.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
            for name, command in commands.items():
                with self.subTest(boundary=name):
                    rejected = self.run_process(command)
                    self.assertNotEqual(rejected.returncode, 0)
                    self.assertIn("missing required sources/licenses", rejected.stderr)
        finally:
            target.write_bytes(original)
            manifest.write_bytes(original_manifest)

    def test_cli_summary_cannot_be_confused_with_complete_manifest(self):
        result = self.verify()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        summary = json.loads(result.stdout)
        self.assertEqual(summary["kind"], "nexus_source_sdk_summary")
        self.assertEqual(summary["sdk_kind"], self.identity["kind"])
        self.assertNotEqual(summary["kind"], self.identity["kind"])
        self.assertEqual(summary["snapshot_sha256"],
                         self.identity["snapshot_sha256"])
        self.assertEqual(summary["files"], len(self.identity["files_sha256"]))
        self.assertNotIn("files_sha256", summary)

    def consumer(self, name, **options):
        source, build, command = consumer_gate.prepare_consumer(
            self.prefix, self.directory, name, self.run_ok, **options)
        return source, build, self.run_process(command)

    def test_relocated_c_and_cpp_consumer_build_and_run_without_original_source(self):
        source, build, result = self.consumer("native", fixture=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((build / "resolved-sdk.txt").read_text(), str(self.sdk))
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4"])
        for application in ("installed_c", "installed_cpp"):
            self.run_ok([str(build / "bin" / application)])
        self.run_ok([str(build / "parent_plain")])
        compile_commands = json.loads((build / "compile_commands.json").read_text())
        self.assertGreater(len(compile_commands), 0)
        for entry in compile_commands:
            self.assertNotIn(str(ROOT) + "/", entry.get("command", ""))

    def test_relocated_sdk_exports_policy_version_and_rejects_other_versions(self):
        version = load_policy(self.sdk)["semantic_version"]
        project_build = self.directory / "policy source project"
        self.run_ok(["cmake", "-S", str(self.sdk), "-B", str(project_build),
                     "-G", "Ninja", "-DNEXUS_BUILD_TESTS=OFF",
                     "-DNEXUS_BUILD_WORKLOAD=OFF"])
        cache = (project_build / "CMakeCache.txt").read_text().splitlines()
        observed = next(line.split("=", 1)[1] for line in cache
                        if line.startswith("CMAKE_PROJECT_VERSION:STATIC="))
        self.assertEqual(observed, version)
        source, build, result = self.consumer("policy version", fixture=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        with (source / "CMakeLists.txt").open("a") as stream:
            stream.write('file(WRITE "${CMAKE_BINARY_DIR}/package-version.txt" '
                         '"${Nexus_VERSION}")\n')
        self.run_ok(["cmake", "-S", str(source), "-B", str(build)])
        self.assertEqual((build / "package-version.txt").read_text(), version)
        major, minor, patch = map(int, version.split("."))
        wrong = f"{major}.{minor}.{patch + 1}"
        _, _, rejected = self.consumer("wrong policy version", fixture=True,
                                        version=wrong)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("requested version", rejected.stderr)
        source, build, command = consumer_gate.prepare_consumer(
            self.prefix, self.directory, "ranged policy version", self.run_ok,
            fixture=True, version=version)
        cmake = source / "CMakeLists.txt"
        cmake.write_text(cmake.read_text().replace(
            f"Nexus {version} EXACT", f"Nexus {version}...{wrong}"))
        rejected = self.run_process(command)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("requested version range", rejected.stderr)

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_arm_consumer_compiles_real_startup_and_linker(self):
        _, build, result = self.consumer("arm", arm=True, fixture=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4", "--target", "installed_c", "installed_cpp"])
        for application in ("installed_c", "installed_cpp"):
            self.assertGreater((build / "bin" / (application + ".elf")).stat().st_size, 0)
            resources = json.loads((build / "bin" / (application + ".resources.json")).read_text())
            self.assertEqual(resources["status"], "passed")
        self.check_private_includes(build)

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_gd32_consumer_compiles_real_startup_and_linker(self):
        _, build, result = self.consumer("gd32 arm", arm=True, chip="gd32", fixture=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4", "--target", "installed_c", "installed_cpp"])
        for application in ("installed_c", "installed_cpp"):
            self.assertGreater((build / "bin" / (application + ".elf")).stat().st_size, 0)
            resources = json.loads((build / "bin" / (application + ".resources.json")).read_text())
            self.assertEqual(resources["status"], "passed")
        self.check_private_includes(build)

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_g474_selected_baremetal_and_freertos_consumers_link(self):
        for backend, filename in (("baremetal", "io.toml"),
                                  ("freertos", "io-freertos.toml")):
            with self.subTest(backend=backend):
                source, build, arguments = consumer_gate.prepare_consumer(
                    self.prefix, self.directory, "g474 " + backend,
                    self.run_ok, fixture=True, arm=True)
                kernel = " LIBRARIES Nexus::OSFreeRTOS" if backend == "freertos" else ""
                definitions = "NX_G474_IO_FIXTURE"
                if backend == "freertos":
                    definitions += " NX_G474_FREERTOS_FIXTURE"
                (source / "CMakeLists.txt").write_text(
                    'cmake_minimum_required(VERSION 3.31)\n'
                    'project(installed_g474_consumer LANGUAGES C ASM)\n'
                    'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
                    f'find_package(Nexus {load_policy(self.sdk)["semantic_version"]} EXACT CONFIG REQUIRED)\n'
                    f'nexus_add_firmware(installed_g474 SOURCES main.c{kernel})\n'
                    f'target_compile_definitions(installed_g474 PRIVATE {definitions})\n'
                    'get_target_property(sdk_root Nexus::Config NEXUS_SOURCE_DIR)\n'
                    'get_target_property(config_root Nexus::Platform NEXUS_CONFIG_DIR)\n'
                    'file(WRITE "${CMAKE_BINARY_DIR}/resolved-sdk.txt" "${sdk_root}")\n'
                    'file(WRITE "${CMAKE_BINARY_DIR}/resolved-config.txt" "${config_root}")\n')
                fixture = ROOT / "tests/contracts/g474_assembly"
                shutil.copyfile(fixture / "main.c", source / "main.c")
                assembly = source / "g474.toml"
                text = (fixture / filename).read_text()
                text = text.replace(
                    'board_package = "../../../boards/stm32g474re_hse24_reference"',
                    f'board_package = "{self.sdk}/boards/stm32g474re_hse24_reference"')
                assembly.write_text(text)
                arguments = [argument for argument in arguments
                             if not argument.startswith("-DNEXUS_ASSEMBLY_FILE=")]
                arguments.append(f"-DNEXUS_ASSEMBLY_FILE={assembly}")
                self.run_ok(arguments)
                self.assertEqual((build / "resolved-sdk.txt").read_text(), str(self.sdk))
                self.run_ok(["cmake", "--build", str(build), "--parallel", "4",
                             "--target", "installed_g474"])
                image = build / "bin/installed_g474.elf"
                resources = json.loads(
                    (build / "bin/installed_g474.resources.json").read_text())
                self.assertEqual(resources["status"], "passed")
                self.assertGreater(image.stat().st_size, 0)
                config = Path((build / "resolved-config.txt").read_text())
                self.run_ok([sys.executable, "-B",
                    str(self.sdk / "tools/measurement/retention.py"),
                    "--elf", str(image), "--bindings", str(config / "bindings.c"),
                    "--output", str(build / "provider-retention.json")])
                retention = json.loads((build / "provider-retention.json").read_text())
                self.assertEqual(retention["status"], "passed")
                commands = json.loads((build / "compile_commands.json").read_text())
                products = [entry for entry in commands
                            if Path(entry["file"]).resolve() == source / "main.c"]
                self.assertEqual(len(products), 1)
                for entry in commands:
                    self.assertNotIn(str(ROOT) + "/", entry["command"])
                for private in ("/vendors/", "/soc/", "-DSTM32G474"):
                    self.assertNotIn(private, products[0]["command"])

    def check_private_includes(self, build):
        commands = json.loads((build / "compile_commands.json").read_text())
        product = [entry for entry in commands if Path(entry["file"]).name in {"main.c", "main.cpp"}]
        self.assertEqual(len(product), 2)
        for entry in product:
            command = entry["command"]
            self.assertNotIn("/vendors/", command)
            self.assertNotIn("/soc/", command)
            self.assertNotIn(str(ROOT) + "/", command)
            self.assertNotIn("-DSTM32F407", command)
            self.assertNotIn("-DGD32F470", command)

    def modern_consumer(self, family, board, backend):
        consumers = self.directory / "typed platform consumers"
        consumers.mkdir(exist_ok=True)
        source, build, arguments = consumer_gate.prepare_modern_consumer(
            self.prefix, consumers, family, backend, self.run_ok,
            fixture=True)
        self.assertIn(str(self.sdk / "boards" / board),
                      (source / "assembly.toml").read_text())
        self.run_ok(arguments)
        self.assertEqual((build / "resolved-sdk.txt").read_text(), str(self.sdk))
        self.run_ok(["cmake", "--build", str(build), "--parallel", "2",
                     "--target", "installed_modern"])
        image = build / "bin/installed_modern.elf"
        resources = json.loads(
            (build / "bin/installed_modern.resources.json").read_text())
        self.assertEqual(resources["status"], "passed")
        self.assertGreater(image.stat().st_size, 0)
        config = Path((build / "resolved-config.txt").read_text())
        self.run_ok([sys.executable, "-B",
            str(self.sdk / "tools/measurement/retention.py"),
            "--elf", str(image), "--bindings", str(config / "bindings.c"),
            "--resolved", str(config / "resolved.json"),
            "--output", str(build / "provider-retention.json")])
        retention = json.loads((build / "provider-retention.json").read_text())
        self.assertEqual(retention["status"], "passed")
        commands = json.loads((build / "compile_commands.json").read_text())
        products = [entry for entry in commands
                    if Path(entry["file"]).resolve() == source / "main.c"]
        self.assertEqual(len(products), 1)
        for entry in commands:
            self.assertNotIn(str(ROOT) + "/", entry["command"])
        for private in ("/vendors/", "/soc/", "-DSTM32", "-DGD32"):
            self.assertNotIn(private, products[0]["command"])
        # The SDK must supply the selected original vector table, exactly once.
        startup_name = {
            "h743": "startup_stm32h743xx.s", "u575": "startup_stm32u575xx.s",
            "f103": "startup_stm32f103xb.s", "gd303": "startup_gd32f30x_hd.S",
            "g474": "startup_stm32g474xx.s"}[family]
        startup = [entry for entry in commands
                   if Path(entry["file"]).name == startup_name]
        self.assertEqual(len(startup), 1)
        self.assertTrue(Path(startup[0]["file"]).is_relative_to(self.sdk))

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_h743_selected_baremetal_and_freertos_consumers_link(self):
        for backend in ("baremetal", "freertos"):
            with self.subTest(backend=backend):
                self.modern_consumer("h743", "stm32h743_nucleo_zi2", backend)

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_u575_selected_baremetal_and_freertos_consumers_link(self):
        for backend in ("baremetal", "freertos"):
            with self.subTest(backend=backend):
                self.modern_consumer("u575", "stm32u575_nucleo_zi_q", backend)

    def test_relocated_mainstream_consumers_use_their_actual_startup_and_abi(self):
        self.assertIsNotNone(shutil.which("arm-none-eabi-gcc"),
                             "ARM GNU toolchain required for MCU consumers")
        for family, board in (("f103", "stm32f103_reference"),
                              ("gd303", "gd32f303_reference"),
                              ("g474", "stm32g474re_hse24_reference")):
            for backend in ("baremetal", "freertos"):
                with self.subTest(family=family, backend=backend):
                    self.modern_consumer(family, board, backend)

    def threadx_consumer(self, name, board, clock, *, arm):
        source, build, arguments = consumer_gate.prepare_consumer(
            self.prefix, self.directory, name, self.run_ok,
            fixture=True, arm=arm)
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(installed_threadx_consumer LANGUAGES C ASM)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            f'find_package(Nexus {load_policy(self.sdk)["semantic_version"]} EXACT CONFIG REQUIRED)\n'
            'nexus_add_firmware(installed_threadx SOURCES main.c '
            'LIBRARIES Nexus::OSThreadX)\n'
            'get_target_property(sdk_root Nexus::Config NEXUS_SOURCE_DIR)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/resolved-sdk.txt" "${sdk_root}")\n')
        # The fixture deliberately retains a permanent owner task: link only.
        shutil.copyfile(ROOT / "tests/contracts/os_threadx_runtime/main.c",
                        source / "main.c")
        assembly = source / "threadx.toml"
        assembly.write_text(
            f'schema = 2\nboard = "{board}"\nbackend = "threadx"\n'
            f'clock = "{clock}"\noptimization = "Os"\n\n'
            '[memory]\nmain_stack_bytes = 2048\n'
            'flash_load_limit_bytes = 65536\n'
            'static_ram_limit_bytes = 32768\nlibc_heap_bytes = 0\n')
        arguments = [argument for argument in arguments
                     if not argument.startswith("-DNEXUS_ASSEMBLY_FILE=")]
        arguments.append(f"-DNEXUS_ASSEMBLY_FILE={assembly}")
        self.run_ok(arguments)
        self.assertEqual((build / "resolved-sdk.txt").read_text(), str(self.sdk))
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4",
                     "--target", "installed_threadx"])
        commands = json.loads((build / "compile_commands.json").read_text())
        product = [entry for entry in commands
                   if Path(entry["file"]).resolve() == source / "main.c"]
        self.assertEqual(len(product), 1)
        for entry in commands:
            self.assertNotIn(str(ROOT) + "/", entry["command"])
        for private in ("/vendors/", "/soc/", "-DSTM32F407", "-DGD32F470"):
            self.assertNotIn(private, product[0]["command"])
        if arm:
            image = build / "bin/installed_threadx.elf"
            resources = json.loads(
                (build / "bin/installed_threadx.resources.json").read_text())
            self.assertEqual(resources["status"], "passed")
        else:
            image = build / "bin/installed_threadx"
        self.assertGreater(image.stat().st_size, 0)

    def test_relocated_threadx_native_consumer_links_actual_official_kernel(self):
        self.threadx_consumer("threadx native", "native_reference", "model",
                              arm=False)

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_threadx_three_board_consumers_link_actual_official_kernel(self):
        for name, board, clock in (
                ("qiming", "stm32f407_qiming_v31", "hse8-pll168"),
                ("sky", "stm32f407ve_sky_qingchun", "hse8-pll168"),
                ("liangshan", "gd32f470_liangshan", "hxtal25-pll200")):
            with self.subTest(board=board):
                self.threadx_consumer("threadx " + name, board, clock, arm=True)

    def test_fixture_requires_explicit_consumer_opt_in(self):
        self.assertFalse(self.identity["publishable"])
        _, _, result = self.consumer("fixture rejection", fixture=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("nonpublishable development fixture", result.stdout + result.stderr)

    def test_formal_consumer_cli_rejects_nonpublishable_fixture(self):
        output = self.directory / "formal consumer rejection"
        result = self.run_process([sys.executable, "-B",
            str(ROOT / "cmake/package/verify_consumers.py"),
            "--prefix", str(self.prefix), "--output", str(output)])
        self.assertNotEqual(result.returncode, 0)
        report = json.loads((output / "verification.json").read_text())
        self.assertEqual(report["status"], "failed")
        self.assertIn("publishable", report["error"])
        self.assertEqual(len(report["commands"]), 1)
        self.assertEqual(report["commands"][0]["exit_code"], 0)

    def test_expected_revision_conflict_is_rejected(self):
        _, _, result = self.consumer("revision rejection", fixture=True, expected_revision="0" * 40)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("expected revision", " ".join((result.stdout + result.stderr).split()))

    def test_different_source_package_version_is_rejected(self):
        _, _, result = self.consumer("version rejection", fixture=True, version="99.0.0")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("version", result.stdout + result.stderr)

    def test_development_contracts_are_explicitly_rejected(self):
        _, _, result = self.consumer("contract rejection", contracts=True, fixture=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("excludes development tests/contracts", " ".join((result.stdout + result.stderr).split()))

    def test_modified_required_file_is_rejected(self):
        target = self.sdk / "core/src/request.c"
        original = target.read_bytes()
        try:
            target.write_bytes(original + b"\n/* changed after packaging */\n")
            result = self.verify()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("file identity changed", result.stderr)
        finally:
            target.write_bytes(original)

    def test_modified_exported_config_is_rejected(self):
        target = self.prefix / "lib/cmake/Nexus/NexusConfig.cmake"
        original = target.read_bytes()
        try:
            target.write_bytes(original + b"\n# changed after export\n")
            result = self.verify()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Exported CMake package identity changed", result.stderr)
        finally:
            target.write_bytes(original)

    def test_missing_license_is_rejected(self):
        target = self.sdk / "ext/freertos/LICENSE.md"
        original = target.read_bytes()
        try:
            target.unlink()
            result = self.verify()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Missing source SDK file", result.stderr)
        finally:
            target.write_bytes(original)

    def test_rehashed_missing_arch_source_is_rejected(self):
        identity = self.sdk / source_sdk.MANIFEST
        original_identity = identity.read_bytes()
        for name in ("arch/include/nexus/arch/arch.h",
                     "arch/cortex_m/nx_arch_cortex_m.c",
                     "arch/cortex_m/private/compiler.h",
                     "arch/native/nx_arch_native.c"):
            with self.subTest(name=name):
                target = self.sdk / name
                original_source = target.read_bytes()
                try:
                    target.unlink()
                    manifest = json.loads(original_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("missing required sources/licenses",
                                  result.stderr)
                finally:
                    target.write_bytes(original_source)
                    identity.write_bytes(original_identity)

    def test_rehashed_default_target_source_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        for name in ("arch/CMakeLists.txt", "os/wait.c",
                     "components/CMakeLists.txt", "cmake/toolchains/arm-gcc.cmake",
                     "components/usb_control/include/nexus/components/usb_acm.h",
                     "components/usb_control/src/usb_acm.c",
                     "tools/measurement/resources.py", "tools/evidence/common.py",
                     "cmake/platform/TargetPolicy.cmake",
                     "tools/configure/bundle.py", "tools/configure/dependencies.py",
                     "tools/configure/contracts.py", "tools/dev/dev.py",
                     "tools/dev/presets.py", "tools/dev/process.py",
                     "tools/dev/cache.py", "tools/dev/dependencies.py",
                     "tools/dev/qualification.py", "tools/evidence/store.py",
                     "tools/evidence/environment.py", "tools/evidence/recipe.py",
                     "tools/release/transport.py"):
            with self.subTest(name=name):
                target = self.sdk / name
                old_source = target.read_bytes()
                try:
                    target.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0,
                                        "Rehashed default target omission accepted: " + name)
                    self.assertIn("missing required sources/licenses", result.stderr)
                finally:
                    target.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_soc_extensions_and_reference_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        names = ["soc/stm32g474/drivers/exti.c",
                 "soc/stm32h743/drivers/uart_dma.c",
                 "soc/stm32h743/private/stm32h743_dma.h",
                 "soc/stm32u575/security.c",
                 "soc/stm32u575/private/stm32u575_security.h",
                 "soc/stm32u575/private/stm32u575_security_hw.h"]
        for mechanism in ("cache_control", "nonsecure"):
            names += [f"arch/include/nexus/arch/{mechanism}.h",
                      f"arch/cortex_m/nx_arch_{mechanism}.c",
                      f"arch/cortex_m/private/{mechanism}.h"]
        names += [f"tools/configure/assemblies/nucleo-{chip}-{backend}-{mode}.toml"
                  for chip in ("h743", "u575")
                  for backend in ("baremetal", "freertos")
                  for mode in ("empty", "uart")]
        self.assertEqual(self.verify().returncode, 0)
        for name in names:
            with self.subTest(name=name):
                target = self.sdk / name
                old_source = target.read_bytes()
                try:
                    target.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0,
                                        "Rehashed extension omission accepted: " + name)
                    self.assertIn("missing required sources/licenses", result.stderr)
                finally:
                    target.write_bytes(old_source)
                    identity.write_bytes(old_identity)
        self.assertEqual(self.verify().returncode, 0)

    def test_rehashed_secondary_controller_generator_omission_rejects(self):
        name = "tools/configure/secondary_controllers.py"
        target = self.sdk / name
        identity = self.sdk / source_sdk.MANIFEST
        old_source = target.read_bytes()
        old_identity = identity.read_bytes()
        try:
            target.unlink()
            manifest = json.loads(old_identity)
            manifest["files_sha256"].pop(name)
            manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                manifest["files_sha256"], sort_keys=True,
                separators=(",", ":")).encode())
            identity.write_text(json.dumps(manifest) + "\n")
            result = self.verify()
            self.assertNotEqual(result.returncode, 0,
                                "Rehashed controller generator omission accepted")
            self.assertIn("missing required sources/licenses", result.stderr)
        finally:
            target.write_bytes(old_source)
            identity.write_bytes(old_identity)

    def test_rehashed_exposed_io_clock_and_cold_source_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        names = (
            "core/src/fault.c", "core/include/nexus/core/fault.h",
            "io/include/nexus/io/usb_device.h", "io/src/usb_device_controller.c",
            "io/private/usb_device_controller.h", "io/native/usb_device_access.h",
            "io/include/nexus/io/ethernet.h", "io/src/eth_mac.c",
            "io/private/eth_mac.h", "io/include/nexus/io/mdio.h",
            "soc/stm32f407/drivers/usb_device.c",
            "soc/gd32f470/drivers/ethernet.c",
            "soc/stm32f407/private/stm32f407_can.h",
            "io/src/can_controller.c", "io/private/can_controller.h",
            "io/include/nexus/io/advanced_pwm.h",
            "io/private/timer_advanced_controller.h",
            "soc/gd32f470/drivers/timer_capture.c",
            "os/freertos/timebase_lowpower.c",
            "os/freertos/private/timebase_lowpower.h",
            "soc/stm32f407/timebase_lowpower.c", "soc/gd32f470/usb_clock.c",
            "tools/configure/usb_device.py", "tools/configure/ethernet.py",
            "tools/configure/timer_advanced.py", "tools/configure/bindings.py",
            "cmake/platform/ControllerModes.cmake",
            "components/clock_domain/private/clock_domain_impl.h",
            "components/amp_ipc/CMakeLists.txt",
            "components/amp_ipc/src/amp_ipc.c",
            "components/amp_ipc/include/nexus/components/amp_ipc.h",
            "components/amp_ipc/include/nexus/components/amp_ipc_storage.h",
            "components/stm32f407_handoff/CMakeLists.txt",
            "components/stm32f407_handoff/include/nexus/components/stm32f407_handoff.h",
            "soc/stm32f407/handoff.c",
            "soc/stm32f407/private/stm32f407_handoff_hw.h",
            "io/include/nexus/io/frame_stream.h", "io/src/frame_stream.c",
            "components/frame_dma2d/CMakeLists.txt",
            "components/frame_dma2d/include/nexus/components/frame_dma2d.h",
            "components/frame_dma2d/src/h743_frame_dma2d.c",
            "components/mapped_storage/CMakeLists.txt",
            "components/mapped_storage/src/mapped_storage_owner.c",
            "components/mapped_storage/src/native_mapped_storage.c",
            "components/mapped_storage/include/nexus/components/mapped_storage.h",
            "components/user_io_broker/include/nexus/components/user_io_broker.h",
            "components/dp83848/include/nexus/dp83848/dp83848.h",
            "soc/stm32g474/system.c", "soc/stm32g474/drivers/uart.c",
            "soc/stm32g474/private/stm32g474_provider.h",
            "soc/stm32g474/soc.json", "soc/stm32g474/routes.json",
            "boards/stm32g474re_hse24_reference/board.json",
            "tools/configure/providers/stm32g474/__init__.py",
            "tools/configure/providers/stm32g474/bindings.py",
            "tools/configure/providers/stm32g474/emission.py")
        for name in names:
            with self.subTest(name=name):
                target = self.sdk / name
                old_source = target.read_bytes()
                try:
                    target.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0,
                                        "Rehashed exposed source omission accepted: " + name)
                    self.assertIn("missing required sources/licenses", result.stderr)
                finally:
                    target.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_threadx_kernel_header_license_and_owned_source_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        for name in ("ext/threadx/common/src/tx_thread_create.c",
                     "ext/threadx/ports/linux/gnu/src/tx_timer_interrupt.c",
                     "ext/threadx/ports/cortex_m4/gnu/src/tx_thread_schedule.S",
                     "ext/threadx/common/inc/tx_api.h", "ext/threadx/LICENSE.txt",
                     "os/threadx/Kernel.cmake", "tools/configure/threadx.py",
                     "tools/maintenance/threadx_lock.py"):
            with self.subTest(name=name):
                target = self.sdk / name
                old_source = target.read_bytes()
                try:
                    target.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("missing required sources/licenses", result.stderr)
                finally:
                    target.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_threadx_wrong_commit_or_source_identity_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        try:
            manifest = json.loads(old_identity)
            next(x for x in manifest["dependencies"] if x["path"] == "ext/threadx")["commit"] = "a" * 40
            identity.write_text(json.dumps(manifest) + "\n")
            self.assertNotEqual(self.verify().returncode, 0)
        finally:
            identity.write_bytes(old_identity)
        source = self.sdk / "ext/threadx/common/src/tx_thread_create.c"
        old_source = source.read_bytes()
        try:
            source.write_bytes(old_source + b"\n/* unreviewed source */\n")
            manifest = json.loads(old_identity)
            manifest["files_sha256"][source.relative_to(self.sdk).as_posix()] = source_sdk.digest(source.read_bytes())
            manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                manifest["files_sha256"], sort_keys=True, separators=(",", ":")).encode())
            identity.write_text(json.dumps(manifest) + "\n")
            self.assertNotEqual(self.verify().returncode, 0)
        finally:
            source.write_bytes(old_source)
            identity.write_bytes(old_identity)

    def test_installed_threadx_cli_does_not_write_into_readonly_sdk(self):
        before = {p.relative_to(self.sdk).as_posix() for p in self.sdk.rglob("*") if p.is_file()}
        command = [sys.executable, str(self.sdk / "tools/maintenance/threadx_lock.py"),
                   "--source", str(self.sdk / "ext/threadx"),
                   "--lock", str(self.sdk / "dependencies/threadx.lock.json"),
                   "--port", "linux"]
        result = self.run_process(command)
        self.assertEqual(result.returncode, 0, result.stderr)
        after = {p.relative_to(self.sdk).as_posix() for p in self.sdk.rglob("*") if p.is_file()}
        self.assertEqual(after, before)

    def test_installed_threadx_cli_requires_verified_manifest_not_missing_git_fallback(self):
        command = [sys.executable, str(self.sdk / "tools/maintenance/threadx_lock.py"),
                   "--source", str(self.sdk / "ext/threadx"),
                   "--lock", str(self.sdk / "dependencies/threadx.lock.json"),
                   "--port", "linux"]
        result = self.run_process(command)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["source_kind"], "installed_source_sdk")
        source = self.sdk / "ext/threadx/common/src/tx_thread_create.c"
        old_source = source.read_bytes()
        try:
            source.write_bytes(b"corrupted")
            result = self.run_process(command)
            self.assertNotEqual(result.returncode, 0)
        finally:
            source.write_bytes(old_source)

    def test_rehashed_official_f4_source_notice_and_verifier_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        lock = json.loads((self.sdk / "dependencies/stm32f4-hal.lock.json").read_text())
        names = ["vendors/st/stm32f4_hal/" + name for name in lock["files"]]
        names.extend(("dependencies/stm32f4-hal.lock.json",
                      "tools/maintenance/stm32f4_hal_lock.py",
                      "cmake/platform/STM32Official.cmake"))
        for name in names:
            with self.subTest(name=name):
                file = self.sdk / name
                old_source = file.read_bytes()
                try:
                    file.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    self.assertNotEqual(self.verify().returncode, 0)
                finally:
                    file.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_official_f4_wrong_pairing_source_or_lock_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        original_identity = identity.read_bytes()
        source = self.sdk / "vendors/st/stm32f4_hal/Inc/stm32f4xx_ll_usart.h"
        lock_path = self.sdk / "dependencies/stm32f4-hal.lock.json"
        original_source = source.read_bytes()
        original_lock = lock_path.read_bytes()
        try:
            for dependency in ("vendors/st/stm32f4_hal",
                               "vendors/st/cmsis_device_f4"):
                manifest = json.loads(original_identity)
                next(record for record in manifest["dependencies"]
                     if record["path"] == dependency)["commit"] = "a" * 40
                identity.write_text(json.dumps(manifest))
                self.assertNotEqual(self.verify().returncode, 0)
            for file, data in ((source, original_source + b"\n/* changed */\n"),
                               (lock_path, json.dumps({**json.loads(original_lock),
                                                       "files": {}}).encode())):
                self.rehash_manifest_file(json.loads(original_identity), file, data)
                self.assertNotEqual(self.verify().returncode, 0)
                source.write_bytes(original_source)
                lock_path.write_bytes(original_lock)
        finally:
            source.write_bytes(original_source)
            lock_path.write_bytes(original_lock)
            identity.write_bytes(original_identity)

    def test_installed_official_f4_cli_requires_complete_sdk_and_is_read_only(self):
        command = [sys.executable, "-B",
                   str(self.sdk / "tools/maintenance/stm32f4_hal_lock.py"),
                   "--source", str(self.sdk / "vendors/st/stm32f4_hal"),
                   "--lock", str(self.sdk / "dependencies/stm32f4-hal.lock.json"),
                   "--cmsis-source", str(self.sdk / "vendors/st/cmsis_device_f4")]
        before = {path.relative_to(self.sdk).as_posix()
                  for path in self.sdk.rglob("*") if path.is_file()}
        driver = self.sdk / "vendors/st/stm32f4_hal"
        lock = self.sdk / "dependencies/stm32f4-hal.lock.json"
        self.assertEqual(set(json.loads(lock.read_text())["files"]),
                         OFFICIAL_F4_VERIFIED_FILES)
        checked_paths = [driver / name
                         for name in sorted(OFFICIAL_F4_VERIFIED_FILES)]
        checked_paths.extend((lock, self.sdk / source_sdk.MANIFEST))

        def payload_identity():
            return {path.relative_to(self.sdk).as_posix(): (
                source_sdk.digest(path.read_bytes()), path.stat().st_mode,
                path.stat().st_uid, path.stat().st_gid, path.stat().st_mtime_ns)
                    for path in checked_paths}

        before_payload = payload_identity()
        result = self.run_process(command)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stdout)["verified_files"], 11)
        self.assertEqual(json.loads(result.stdout)["source_kind"],
                         "installed_source_sdk")
        self.assertEqual(payload_identity(), before_payload)
        self.assertEqual({path.relative_to(self.sdk).as_posix()
                          for path in self.sdk.rglob("*") if path.is_file()}, before)
        file = self.sdk / "core/src/time.c"
        original = file.read_bytes()
        try:
            file.write_bytes(original + b"\n/* unrelated payload tamper */\n")
            self.assertNotEqual(self.run_process(command).returncode, 0)
        finally:
            file.write_bytes(original)

    def test_rehashed_g4_source_notice_lock_and_verifier_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        lock = json.loads((self.sdk / "dependencies/stm32g4.lock.json").read_text())
        names = ["vendors/st/cmsis_device_g4/" + name for name in lock["files"]]
        names.extend(("dependencies/stm32g4.lock.json",
                      "tools/maintenance/stm32g4_lock.py"))
        for name in names:
            with self.subTest(name=name):
                file = self.sdk / name
                old_source = file.read_bytes()
                try:
                    file.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    self.assertNotEqual(self.verify().returncode, 0)
                finally:
                    file.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_g4_wrong_commit_changed_source_or_reduced_graph_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        source = self.sdk / "vendors/st/cmsis_device_g4/Include/stm32g474xx.h"
        lock_path = self.sdk / "dependencies/stm32g4.lock.json"
        original_source = source.read_bytes()
        original_lock = lock_path.read_bytes()
        try:
            manifest = json.loads(old_identity)
            next(x for x in manifest["dependencies"]
                 if x["path"] == "vendors/st/cmsis_device_g4")["commit"] = "a" * 40
            identity.write_text(json.dumps(manifest))
            self.assertNotEqual(self.verify().returncode, 0)
            for file, content in ((source, original_source + b"\n/* unreviewed */\n"),
                                  (lock_path, json.dumps({**json.loads(original_lock),
                                      "files": {}}).encode())):
                file.write_bytes(content)
                manifest = json.loads(old_identity)
                manifest["files_sha256"][file.relative_to(self.sdk).as_posix()] = source_sdk.digest(content)
                manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                    manifest["files_sha256"], sort_keys=True,
                    separators=(",", ":")).encode())
                identity.write_text(json.dumps(manifest))
                self.assertNotEqual(self.verify().returncode, 0)
                source.write_bytes(original_source)
                lock_path.write_bytes(original_lock)
        finally:
            source.write_bytes(original_source)
            lock_path.write_bytes(original_lock)
            identity.write_bytes(old_identity)

    def test_installed_g4_cli_verifies_manifest_readonly_without_git_fallback(self):
        before = {p.relative_to(self.sdk).as_posix()
                  for p in self.sdk.rglob("*") if p.is_file()}
        command = [sys.executable, str(self.sdk / "tools/maintenance/stm32g4_lock.py"),
                   "--source", str(self.sdk / "vendors/st/cmsis_device_g4"),
                   "--lock", str(self.sdk / "dependencies/stm32g4.lock.json")]
        result = self.run_process(command)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stdout)["verified_files"], 5)
        self.assertEqual(json.loads(result.stdout)["source_kind"], "installed_source_sdk")
        after = {p.relative_to(self.sdk).as_posix()
                 for p in self.sdk.rglob("*") if p.is_file()}
        self.assertEqual(after, before)
        source = self.sdk / "vendors/st/cmsis_device_g4/Include/stm32g474xx.h"
        old_source = source.read_bytes()
        try:
            source.write_bytes(b"corrupted")
            result = self.run_process(command)
            self.assertNotEqual(result.returncode, 0)
        finally:
            source.write_bytes(old_source)

    def rehash_manifest_file(self, manifest, file, data):
        file.write_bytes(data)
        manifest["files_sha256"][file.relative_to(self.sdk).as_posix()] = (
            source_sdk.digest(data))
        manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
            manifest["files_sha256"], sort_keys=True,
            separators=(",", ":")).encode())
        (self.sdk / source_sdk.MANIFEST).write_text(json.dumps(manifest) + "\n")

    def test_rehashed_modern_soc_owned_firmware_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        names = ["soc/stm32h743/power_boot.S",
                 "soc/stm32h743/private/boot_registers.h"]
        for family, board in (("stm32h743", "stm32h743_nucleo_zi2"),
                              ("stm32u575", "stm32u575_nucleo_zi_q")):
            names += [f"soc/{family}/" + name for name in (
                "CMakeLists.txt", "system.c", "drivers/gpio.c", "drivers/uart.c",
                f"private/{family}_system.h", f"private/{family}_provider.h",
                "soc.json", "routes.json")]
            names.append(f"boards/{board}/board.json")
        for name in names:
            with self.subTest(name=name):
                file = self.sdk / name
                old_source = file.read_bytes()
                try:
                    file.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0,
                                        "Rehashed actual firmware omission accepted")
                    self.assertIn("missing required sources/licenses", result.stderr)
                finally:
                    file.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_modern_device_source_notice_lock_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        names = ["tools/maintenance/cmsis_device_lock.py"]
        for review in source_sdk.cmsis_device_lock.REVIEWED_DEVICES.values():
            names += [review["directory"] + "/" + name
                      for name in sorted(review["files"])]
            names.append(review["lock_name"])
        for name in names:
            with self.subTest(name=name):
                file = self.sdk / name
                old_source = file.read_bytes()
                try:
                    file.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    self.assertNotEqual(self.verify().returncode, 0,
                                        "Rehashed original device input omission accepted")
                finally:
                    file.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_modern_device_wrong_commit_changed_source_and_reduced_lock_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        for review in source_sdk.cmsis_device_lock.REVIEWED_DEVICES.values():
            with self.subTest(part=review["directory"]):
                source = (self.sdk / review["directory"] /
                          review.get("license_path", "LICENSE.md"))
                lock = self.sdk / review["lock_name"]
                old_source, old_lock = source.read_bytes(), lock.read_bytes()
                try:
                    manifest = json.loads(old_identity)
                    next(record for record in manifest["dependencies"]
                         if record["path"] == review["directory"])["commit"] = "a" * 40
                    identity.write_text(json.dumps(manifest) + "\n")
                    self.assertNotEqual(self.verify().returncode, 0,
                                        "Wrong reviewed device commit accepted")
                    self.rehash_manifest_file(json.loads(old_identity), source,
                        old_source + b"\n/* unreviewed original notice */\n")
                    self.assertNotEqual(self.verify().returncode, 0,
                                        "Rehashed changed original notice accepted")
                    source.write_bytes(old_source)
                    reduced = {**json.loads(old_lock), "files": {}}
                    self.rehash_manifest_file(json.loads(old_identity), lock,
                                              json.dumps(reduced).encode())
                    self.assertNotEqual(self.verify().returncode, 0,
                                        "Rehashed reduced reviewed graph accepted")
                finally:
                    source.write_bytes(old_source)
                    lock.write_bytes(old_lock)
                    identity.write_bytes(old_identity)

    def test_installed_modern_device_cli_is_readonly_and_requires_complete_sdk(self):
        before = {p.relative_to(self.sdk).as_posix()
                  for p in self.sdk.rglob("*") if p.is_file()}
        for review in source_sdk.cmsis_device_lock.REVIEWED_DEVICES.values():
            with self.subTest(part=review["directory"]):
                command = [sys.executable,
                    str(self.sdk / "tools/maintenance/cmsis_device_lock.py"),
                    "--source", str(self.sdk / review["directory"]),
                    "--lock", str(self.sdk / review["lock_name"])]
                result = self.run_process(command)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                report = json.loads(result.stdout)
                self.assertEqual(report["verified_files"], len(review["files"]))
                self.assertEqual(report["source_kind"], "installed_source_sdk")
                source = self.sdk / "core/src/request.c"
                old_source = source.read_bytes()
                try:
                    source.write_bytes(b"corrupted unrelated SDK owner source")
                    self.assertNotEqual(self.run_process(command).returncode, 0,
                                        "Device CLI ignored complete SDK proof")
                finally:
                    source.write_bytes(old_source)
        after = {p.relative_to(self.sdk).as_posix()
                 for p in self.sdk.rglob("*") if p.is_file()}
        self.assertEqual(after, before)

    def test_sdk_manifest_sources_exports_and_file_set_stay_unchanged_during_verify(self):
        # Inject at the external vendor-verifier boundary after the SDK has
        # read all hashes. The actual SDK verification runs without a fake map.
        external = source_sdk.threadx_lock.verify
        paths = (self.sdk / source_sdk.MANIFEST,
                 self.sdk / "core/src/request.c",
                 self.prefix / "lib/cmake/Nexus/NexusConfig.cmake")
        for file in paths:
            old = file.read_bytes()
            old_mode = file.stat().st_mode
            for change in ("rewrite-identical", "changed-bytes", "changed-mode"):
                with self.subTest(file=str(file), change=change):
                    def mutate(*args, **kwargs):
                        result = external(*args, **kwargs)
                        if change == "rewrite-identical":
                            replacement = file.with_name(file.name + ".replacement")
                            replacement.write_bytes(old)
                            replacement.replace(file)
                        elif change == "changed-bytes":
                            file.write_bytes(old + b"\n")
                        else:
                            file.chmod(old_mode ^ 0o100)
                        return result
                    try:
                        with mock.patch.object(source_sdk.threadx_lock, "verify",
                                               side_effect=mutate):
                            with self.assertRaisesRegex(ValueError, "changed during"):
                                source_sdk.verify(self.sdk)
                    finally:
                        file.write_bytes(old)
                        file.chmod(old_mode)
        extra = self.sdk / "late-added.c"
        def add_file(*args, **kwargs):
            result = external(*args, **kwargs)
            extra.write_text("int late_added;\n")
            return result
        try:
            with mock.patch.object(source_sdk.threadx_lock, "verify",
                                   side_effect=add_file):
                with self.assertRaisesRegex(ValueError, "changed during"):
                    source_sdk.verify(self.sdk)
        finally:
            extra.unlink(missing_ok=True)

    def test_final_file_read_cannot_hide_a_late_change_to_an_earlier_checked_input(self):
        last_export = self.prefix / "lib/cmake/Nexus/NexusConfigVersion.cmake"
        real_read = Path.read_bytes
        for target in (self.sdk / source_sdk.MANIFEST,
                       self.sdk / "core/src/request.c",
                       self.prefix / "lib/cmake/Nexus/NexusConfig.cmake"):
            with self.subTest(target=str(target)):
                old = target.read_bytes()
                calls = 0

                def read_file(path):
                    nonlocal calls
                    data = real_read(path)
                    if path == last_export:
                        calls += 1
                        if calls == 2:
                            # The final content-read sweep has already passed
                            # this earlier file. Same bytes still change inode.
                            replacement = target.with_name(target.name + ".replacement")
                            replacement.write_bytes(old)
                            replacement.replace(target)
                    return data

                try:
                    with mock.patch.object(Path, "read_bytes", new=read_file):
                        with self.assertRaisesRegex(ValueError, "changed during"):
                            source_sdk.verify(self.sdk)
                    self.assertEqual(calls, 2,
                                     "Filesystem injection did not reach final read")
                finally:
                    target.write_bytes(old)

    def test_git_checkout_inside_sdk_cannot_bypass_whole_manifest(self):
        for family, dependency, lock_name, extra in (
                ("G4", "vendors/st/cmsis_device_g4", "stm32g4", []),
                ("ThreadX", "ext/threadx", "threadx", ["--port", "linux"])):
            with self.subTest(dependency=dependency):
                source = self.sdk / dependency
                private = self.directory / ("private " + family + " Git metadata")
                self.run_ok(["git", "clone", "--shared", "--no-checkout", "--quiet",
                             str(ROOT / dependency), str(private)])
                lock_path = self.sdk / ("dependencies/" + lock_name + ".lock.json")
                lock = json.loads(lock_path.read_text())
                self.run_ok(["git", "-C", str(private), "remote", "set-url", "origin",
                             lock["repository"]])
                self.run_ok(["git", "-C", str(private), "config", "core.sparseCheckout", "true"])
                names = set(lock["files"]) | {lock["license"]["path"]}
                (private / ".git/info/sparse-checkout").write_text(
                    "".join("/" + name + "\n" for name in sorted(names)))
                self.run_ok(["git", "-C", str(private), "checkout", "--quiet", "--detach",
                             lock["commit"]])
                (private / ".git").rename(source / ".git")
                other = self.sdk / "CMakeLists.txt"
                original = other.read_bytes()
                try:
                    self.assertEqual(self.run_ok(["git", "-C", str(source), "status",
                                                 "--porcelain"]).stdout, "")
                    other.write_bytes(original + b"\n# altered SDK composition\n")
                    result = self.run_process([
                        sys.executable, "-B", str(self.sdk / ("tools/maintenance/" + lock_name + "_lock.py")),
                        "--source", str(source), "--lock", str(lock_path), *extra])
                    self.assertNotEqual(result.returncode, 0,
                                        "Valid source Git must not bypass SDK manifest verification")
                finally:
                    other.write_bytes(original)
                    shutil.rmtree(source / ".git")

    def test_additional_file_and_symlink_are_rejected(self):
        extra = self.sdk / "unexpected.c"
        try:
            extra.write_text("int unexpected;\n")
            self.assertNotEqual(self.verify().returncode, 0)
            extra.unlink()
            extra.symlink_to(self.sdk / "CMakeLists.txt")
            self.assertNotEqual(self.verify().returncode, 0)
        finally:
            extra.unlink(missing_ok=True)

    def test_default_preparation_rejects_dirty_checkout(self):
        repo = self.directory / "dirty repository"
        repo.mkdir()
        self.run_ok(["git", "init", "--quiet", str(repo)])
        (repo / "README.md").write_text("source\n")
        self.run_ok(["git", "-C", str(repo), "add", "README.md"])
        self.run_ok(["git", "-C", str(repo), "-c", "user.name=SDK test", "-c", "user.email=sdk-test@local.invalid",
                     "commit", "--quiet", "-m", "fixture baseline"])
        (repo / "README.md").write_text("modified\n")
        result = self.run_process([sys.executable, "-B", str(ROOT / "cmake/package/package_source_sdk.py"),
                           "--source", str(repo), "--output", str(self.directory / "must not publish")])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("dirty source checkout", result.stderr)
        self.assertFalse((self.directory / "must not publish").exists())

    def test_output_parent_alias_cannot_bypass_source_boundary(self):
        output = self.directory / ".." / self.directory.name / "aliased package"
        result = self.run_process([sys.executable, "-B", str(ROOT / "cmake/package/package_source_sdk.py"),
                                  "--source", str(ROOT), "--output", str(output), "--development-fixture"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("canonical path", result.stderr)
        self.assertFalse(output.exists())

    def test_clean_checkout_with_missing_dependency_gitlinks_is_rejected(self):
        repo = self.directory / "missing dependencies repository"
        repo.mkdir()
        self.run_ok(["git", "init", "--quiet", str(repo)])
        (repo / "README.md").write_text("source\n")
        (repo / ".gitmodules").write_bytes((ROOT / ".gitmodules").read_bytes())
        self.run_ok(["git", "-C", str(repo), "add", "README.md", ".gitmodules"])
        self.run_ok(["git", "-C", str(repo), "-c", "user.name=SDK test", "-c", "user.email=sdk-test@local.invalid",
                     "commit", "--quiet", "-m", "fixture baseline"])
        result = self.run_process([sys.executable, "-B", str(ROOT / "cmake/package/package_source_sdk.py"),
                                  "--source", str(repo), "--output", str(self.directory / "must not export empty SDK")])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("required dependency Gitlink", result.stderr)
        self.assertFalse((self.directory / "must not export empty SDK").exists())


if __name__ == "__main__":
    unittest.main()
