"""Relocate the real source SDK and compile/run independent public consumers."""

from functools import wraps
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch as mock_patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
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
    "Inc/stm32f4xx_hal_cryp.h",
    "Inc/stm32f4xx_hal_dma2d.h",
    "Inc/stm32f4xx_hal_dsi.h",
    "Inc/stm32f4xx_hal_hash.h",
    "Inc/stm32f4xx_hal_hash_ex.h",
    "Inc/stm32f4xx_hal_ltdc.h",
    "Inc/stm32f4xx_hal_ltdc_ex.h",
    "Inc/stm32f4xx_hal_pwr.h",
    "Inc/stm32f4xx_hal_pwr_ex.h",
    "Inc/stm32f4xx_hal_qspi.h",
    "Inc/stm32f4xx_hal_sai.h",
    "Inc/stm32f4xx_hal_sai_ex.h",
    "Inc/stm32f4xx_ll_fmc.h",
    "Src/stm32f4xx_hal_cryp.c",
    "Src/stm32f4xx_hal_dma2d.c",
    "Src/stm32f4xx_hal_gpio.c",
    "Src/stm32f4xx_hal_hash.c",
    "Src/stm32f4xx_hal_ltdc.c",
    "Src/stm32f4xx_hal_qspi.c",
    "Src/stm32f4xx_hal_rcc_ex.c",
    "Src/stm32f4xx_hal_sai.c",
    "Src/stm32f4xx_hal_sai_ex.c",
    "Src/stm32f4xx_ll_fmc.c",
    "Inc/Legacy/stm32_hal_legacy.h", "Inc/stm32f4xx_hal_def.h",
    "Inc/stm32f4xx_hal_eth.h", "Inc/stm32f4xx_ll_adc.h",
    "Inc/stm32f4xx_ll_bus.h", "Inc/stm32f4xx_ll_rcc.h",
    "Inc/stm32f4xx_ll_rng.h", "Inc/stm32f4xx_ll_crc.h",
    "Inc/stm32f4xx_ll_dac.h", "Inc/stm32f4xx_ll_dma.h",
    "Inc/stm32f4xx_ll_system.h", "Inc/stm32f4xx_ll_utils.h",
    "Inc/stm32f4xx_ll_gpio.h", "Inc/stm32f4xx_ll_i2c.h",
    "Inc/stm32f4xx_ll_iwdg.h", "Inc/stm32f4xx_ll_wwdg.h",
    "Inc/stm32f4xx_ll_spi.h",
    "Inc/stm32f4xx_ll_tim.h", "Inc/stm32f4xx_ll_usart.h", "LICENSE.md",
    "Inc/stm32f4xx_hal.h", "Inc/stm32f4xx_hal_dma.h",
    "Inc/stm32f4xx_hal_dma_ex.h", "Inc/stm32f4xx_hal_uart.h",
    "Inc/stm32f4xx_hal_rcc.h", "Inc/stm32f4xx_hal_rcc_ex.h",
    "Inc/stm32f4xx_hal_flash.h", "Inc/stm32f4xx_hal_flash_ex.h",
    "Inc/stm32f4xx_hal_flash_ramfunc.h", "Inc/stm32f4xx_hal_gpio.h",
    "Inc/stm32f4xx_hal_gpio_ex.h", "Src/stm32f4xx_hal_uart.c",
    "Src/stm32f4xx_hal_dma.c", "Src/stm32f4xx_hal_rcc.c",
    "Inc/stm32f4xx_ll_fsmc.h", "Inc/stm32f4xx_ll_pwr.h",
    "Inc/stm32f4xx_ll_rtc.h", "Inc/stm32f4xx_ll_sdmmc.h",
    "Src/stm32f4xx_ll_fsmc.c", "Src/stm32f4xx_ll_sdmmc.c",
    "Inc/stm32f4xx_hal_dcmi.h", "Inc/stm32f4xx_hal_dcmi_ex.h",
    "Inc/stm32f4xx_hal_hcd.h", "Inc/stm32f4xx_hal_pcd.h",
    "Inc/stm32f4xx_hal_pcd_ex.h", "Inc/stm32f4xx_ll_usb.h",
    "Src/stm32f4xx_hal_dcmi.c", "Src/stm32f4xx_hal_hcd.c",
    "Src/stm32f4xx_ll_usb.c",
})


def _isolated_sdk_prefix(test):
    """Keep the reusable SDK seed immutable while a case alters its own copy."""
    @wraps(test)
    def isolated(self):
        original = self.directory, self.prefix, self.sdk, self.identity
        with tempfile.TemporaryDirectory(
                prefix="isolated SDK case ", dir=original[0]) as name:
            try:
                self.directory = Path(name)
                self.prefix = self.directory / "relocated prefix with spaces"
                # Copy the exports too; hardlinks would let byte writes alter
                # the seed, and copying src alone invalidates its package proof.
                shutil.copytree(original[1], self.prefix)
                self.sdk = self.prefix / "share/nexus/src"
                self.identity = json.loads(
                    (self.sdk / source_sdk.MANIFEST).read_text())
                baseline = self.verify()
                self.assertEqual(baseline.returncode, 0, baseline.stderr)
                return test(self)
            finally:
                (self.directory, self.prefix, self.sdk,
                 self.identity) = original
    return isolated


class CMSISCoreSelectionTests(unittest.TestCase):
    """Export Core include files without importing unrelated CMSIS components."""

    dependency = "vendors/arm/CMSIS_6"
    headers = ("core_cm0.h", "core_cm0plus.h", "core_cm3.h", "core_cm4.h",
               "core_cm7.h", "core_cm23.h", "core_cm33.h", "core_cm55.h",
               "core_cm85.h", "cmsis_compiler.h", "cmsis_gcc.h",
               "cmsis_version.h", "m-profile/armv7m_mpu.h",
               "m-profile/armv8m_mpu.h", "m-profile/armv7m_cachel1.h",
               "m-profile/cmsis_gcc_m.h", "tz_context.h")

    def setUp(self):
        self.entries = {"CMSIS/Core/Include/" + name:
                        ("100644", "blob", "1" * 40)
                        for name in self.headers}
        self.entries.update({
            "LICENSE": ("100644", "blob", "2" * 40),
            "CMSIS/Core/Include/m-profile/internal/detail.h":
                ("100644", "blob", "3" * 40),
            "CMSIS/Core/Template/ARMv7-M/ARMv7-M.sct":
                ("100644", "blob", "4" * 40),
            "CMSIS/RTOS2/Include/cmsis_os2.h":
                ("100644", "blob", "5" * 40),
            "CMSIS/Core_A/Include/core_ca.h":
                ("100644", "blob", "6" * 40),
            "README.md": ("100644", "blob", "7" * 40),
        })

    def selected(self):
        with mock_patch.object(source_sdk, "tree_entries",
                               return_value=self.entries):
            return source_sdk.dependency_entries(ROOT, self.dependency)

    def test_export_is_recursive_core_headers_and_license_only(self):
        selected = self.selected()
        expected = {name for name in self.entries
                    if name.startswith("CMSIS/Core/Include/")} | {"LICENSE"}
        self.assertEqual(set(selected), expected)
        self.assertEqual(selected["CMSIS/Core/Include/core_cm4.h"],
                         self.entries["CMSIS/Core/Include/core_cm4.h"])

    def test_missing_maintained_cpu_or_compiler_header_rejects(self):
        for name in self.headers:
            with self.subTest(header=name):
                entry = self.entries.pop("CMSIS/Core/Include/" + name)
                with self.assertRaises(source_sdk.SDKError):
                    self.selected()
                self.entries["CMSIS/Core/Include/" + name] = entry

    def test_missing_license_rejects_instead_of_exporting_partial_notice(self):
        del self.entries["LICENSE"]
        with self.assertRaises(source_sdk.SDKError):
            self.selected()

    def test_symlink_or_gitlink_inside_include_tree_is_not_redistributed(self):
        for mode, kind in (("120000", "blob"), ("160000", "commit")):
            with self.subTest(mode=mode):
                self.entries["CMSIS/Core/Include/nested/foreign.h"] = (
                    mode, kind, "8" * 40)
                with self.assertRaises(source_sdk.SDKError):
                    self.selected()

    def test_missing_nested_transitive_header_is_not_hidden_by_top_level_list(self):
        name = "CMSIS/Core/Include/m-profile/internal/detail.h"
        selected = self.selected()
        self.assertIn(name, selected)
        self.assertEqual(selected[name], self.entries[name])


class SourceSDKSelectionTests(unittest.TestCase):
    def assert_rehashed_required_omissions(self, required):
        """An otherwise identical outer map cannot omit a mandatory member."""
        import hashlib

        self.assertIn(required[0], source_sdk.REQUIRED_FILES)
        with tempfile.TemporaryDirectory(prefix="SDK required plan ") as value:
            root = Path(value) / "prefix/share/nexus/src"
            root.mkdir(parents=True)
            # Filesystem/manifest admission only, not a usable SDK or build.
            payload = b"required-source fixture\n"
            files = {}
            contents = {}
            # Explicit names keep new omissions executable against the old
            # production list; deriving the fixture only from REQUIRED_FILES
            # would otherwise never create a newly missing source.
            for name in sorted(set(source_sdk.required_files(ROOT)) | set(required)):
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                contents[name] = (ROOT / name).read_bytes()
                target.write_bytes(contents[name])
                files[name] = hashlib.sha256(contents[name]).hexdigest()
            prefix = root.parents[2]
            exports = {}
            for name in ("lib/cmake/Nexus/NexusConfig.cmake",
                         "lib/cmake/Nexus/NexusConfigVersion.cmake"):
                target = prefix / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(payload)
                exports[name] = hashlib.sha256(payload).hexdigest()
            identity = {"schema": 2, "profile": source_sdk.source_profile(), "kind": "nexus-installed-source-sdk",
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
                        (root / name).write_bytes(contents[name])
                        files[name] = original
                        write_identity()


    def test_reviewed_kernel_overlays_are_required_relocatable_sdk_payload(self):
        required = {"os/reviewed_patch.py",
                    "os/freertos/patches/m7-integer-port.c.patch",
                    "os/freertos/patches/m7-integer-portmacro.h.patch",
                    "os/threadx/patches/linux-lowlevel.patch",
                    "os/threadx/patches/linux-scheduler.patch"}
        required.update("os/freertos/mpu/patches/" + name + ".patch"
                        for name in (
                            "arm_cm3_mpu-port", "arm_cm4_mpu-port", "v8-port",
                            "v8-baseline-assembly", "v8-mainline-assembly",
                            "v7-veneers", "v8-baseline-veneers",
                            "v8-mainline-veneers"))
        self.assertFalse(required - set(source_sdk.REQUIRED_FILES),
                         sorted(required - set(source_sdk.REQUIRED_FILES)))
        self.assertTrue(all(source_sdk.selected(name) for name in required))


    def test_required_configuration_payload_follows_the_generator_closure(self):
        import importlib.util
        from tools.configure import dependencies
        specification = importlib.util.spec_from_file_location(
            "sdk_configuration_closure", ROOT / "cmake/package/package_source_sdk.py")
        updated = importlib.util.module_from_spec(specification)
        with mock_patch.object(dependencies, "COMMON",
                               (*dependencies.COMMON, "new_shared_contract")):
            specification.loader.exec_module(updated)
            self.assertIn("tools/configure/new_shared_contract.py",
                          updated.REQUIRED_FILES)
        for mode in ("platform", "runtime"):
            self.assertLessEqual(set(dependencies.source_names(mode, None, sdk=True)),
                                 set(source_sdk.REQUIRED_FILES))

    def test_component_construction_helpers_are_required_sdk_payload(self):
        required = {"cmake/platform/Components.cmake",
                    "components/soc/CMakeLists.txt",
                    "components/soc/stm32/CMakeLists.txt"}
        self.assertTrue(required.issubset(set(source_sdk.required_files(ROOT))),
                        sorted(required - set(source_sdk.required_files(ROOT))))
        self.assertTrue(all(source_sdk.selected(name) for name in required))

    def test_development_preset_catalogue_is_not_a_partial_sdk_payload(self):
        self.assertFalse(source_sdk.selected("CMakePresets.json"))
        for name in ("base", "native", "stm32", "gd32", "verification"):
            with self.subTest(preset=name):
                self.assertFalse(source_sdk.selected(
                    "cmake/presets/" + name + ".json"))

    def test_required_public_verification_sources_survive_snapshot_selection(self):
        omitted = sorted(name for name in source_sdk.REQUIRED_FILES
                         if name.startswith("cmake/package/verification/io_evolution/")
                         and not source_sdk.selected(name))
        self.assertEqual(omitted, [],
                         "Required public SDK programs cannot be excluded by selection")





    def test_selected_f407_audio_public_closure_is_required(self):
        required = {
            "tools/configure/stm32f4/capture_audio.py",
            "soc/stm32/stm32f4/drivers/audio.c",
            "soc/stm32/stm32f4/drivers/av.c",
            "soc/stm32/stm32f4/private/stm32f407_audio.h",
            "soc/stm32/stm32f4/private/stm32f407_av.h",
            "tools/configure/assemblies/sky-i2s2-slave-tx.toml",
            "tools/configure/assemblies/qiming-i2s2-slave-tx.toml",
            "tools/configure/assemblies/f407-audio-reference/ve/board.json",
            "tools/configure/assemblies/f407-audio-reference/zg/board.json",
            "cmake/package/verification/io_evolution/consumers/"
            "f407-i2s2-slave-tx.c",
        }
        self.assertTrue(required.issubset(set(source_sdk.required_files(ROOT))),
                        sorted(required - set(source_sdk.required_files(ROOT))))
        for name in sorted(required):
            with self.subTest(path=name):
                self.assertTrue(source_sdk.selected(name),
                                "Required audio closure omitted: " + name)
        payload = source_sdk.provider_descriptors.maintained("stm32f4").owned_payload
        hardware = required - {"tools/configure/stm32f4/capture_audio.py"}
        self.assertTrue(hardware.issubset(set(payload)),
                        sorted(hardware - set(payload)))
        retired = {
            "tools/configure/i2s2.py",
            "tools/configure/stm32f4/i2s2.py",
            "soc/stm32/stm32f4/drivers/i2s2_slave_tx.c",
            "soc/stm32/stm32f4/private/stm32f407_i2s2_slave_tx.h",
        }
        self.assertFalse(retired.intersection(source_sdk.required_files(ROOT)))

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
            "io/include/nexus/io/usb_activation.h",
            "io/src/mechanisms/usb_profile.c",
            "tools/configure/usb_profiles.py",
        }
        self.assertTrue(required.issubset(set(source_sdk.required_files(ROOT))),
                        sorted(required - set(source_sdk.required_files(ROOT))))
        for name in required:
            with self.subTest(name=name):
                self.assertTrue((ROOT / name).is_file())
                self.assertTrue(source_sdk.selected(name))









    def test_selected_contract_helper_is_an_explicit_sdk_requirement(self):
        self.assertIn("tools/configure/selected_contracts.py",
                      source_sdk.REQUIRED_FILES)

    def test_consumer_plan_is_required_even_after_a_rehashed_omission(self):
        required = (
            "tools/maintenance/consumer_profiles.py",
            "tools/configure/experience.py",
            "tools/configure/selection.py",
            "tools/configure/onboarding.py",
            "tools/dev/projects.py",
            "tools/dev/device.py",
            "tools/dev/timings.py",
            "io/include/nexus/io/diagnostic.h",
            "io/src/mechanisms/diagnostic.c",
            "components/usb_control/include/nexus/components/usb_acm.h",
            "components/usb_control/src/usb_acm.c",
        )
        self.assert_rehashed_required_omissions(required)

    def test_public_host_entry_closure_rejects_rehashed_omissions(self):
        # Independent build-facing oracle: deleting/re-hashing a required
        # source cannot reduce SDK integrity or break public developer tools.
        required = (
            "tools/dev/projects.py", "tools/dev/provenance.py",
            "tools/maintenance/capabilities.py",
            "tools/maintenance/consumer_profiles.py",
            "tools/evidence/common.py", "tools/evidence/identity.py",
            "tools/configure/documentation.py",
            "scripts/validation/junit.py",
        )
        self.assert_rehashed_required_omissions(required)

    def test_build_policy_and_tool_module_closure_is_required(self):
        required = {"cmake/platform/TargetPolicy.cmake",
                    "tools/configure/bundle.py", "tools/configure/dependencies.py",
                    "tools/configure/contracts.py", "tools/configure/matrix.py",
                    "tools/dev/dev.py", "tools/dev/presets.py", "tools/dev/process.py",
                    "tools/dev/change_paths.py", "tools/dev/cache.py",
                    "tools/dev/dependencies.py", "tools/dev/provenance.py",
                    "tools/evidence/common.py", "tools/evidence/identity.py"}
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
        self.assertTrue(source_sdk.selected("tests/support/native/assemblies/native.toml",
            profile=source_sdk.source_profile(host_support=True)))


    def test_consumer_uses_authored_toml_for_each_maintained_family(self):
        with tempfile.TemporaryDirectory(prefix="SDK consumer inputs ") as value:
            directory = Path(value)
            policy = directory / "prefix/share/nexus/src/dependencies/platform-compatibility.json"
            policy.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / "dependencies/platform-compatibility.json", policy)
            for name, arm, chip in (("native", False, "native"),
                                    ("stm32", True, "stm32"),
                                    ("stm32 board", True, "stm32")):
                _, _, command = consumer_gate.prepare_consumer(
                    directory / "prefix", directory, name,
                    lambda arguments: None, arm=arm, chip=chip)
                assembly = next(argument for argument in command
                                if argument.startswith("-DNEXUS_ASSEMBLY_FILE="))
                self.assertTrue(assembly.endswith(".toml"), assembly)

    def test_optional_handoff_is_consumed_only_by_the_exact_f407_arm_project(self):
        for arm, chip in ((False, "native"), (True, "stm32")):
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
            "soc/stm32/stm32f4/drivers/uart_hal.c",
            "soc/stm32/stm32f4/private/stm32f407_uart_hal.h",
            "soc/stm32/stm32f4/private/stm32f407_hal_atomic.h",
            "soc/stm32/stm32f4/private/stm32f407_hal_profile.h",
            "soc/stm32/stm32f4/private/hal/stm32f4xx_hal_conf.h",
        }
        self.assertTrue(required.issubset(set(source_sdk.required_files(ROOT))),
                        sorted(required - set(source_sdk.required_files(ROOT))))
        self.assertIn("vendors/st/stm32f4_hal", source_sdk.REQUIRED_DEPENDENCIES)

    def test_f4_sdk_redistributes_only_selected_graph_and_notice(self):
        selected = source_sdk.dependency_entries(ROOT, "vendors/st/stm32f4_hal")
        self.assertEqual(set(selected), OFFICIAL_F4_VERIFIED_FILES)


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
        self.assertTrue((official | owned).issubset(set(source_sdk.required_files(
            ROOT, source_sdk.source_profile(backend="threadx")))))
        self.assertIn("ext/threadx", source_sdk.runtime_dependency_closure(
            source_sdk.source_profile(backend="threadx")))




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
            f'[submodule "{item.path}"]\npath = {item.path}\nurl = {item.fetch_url}\n'
            for item in ALL_SOURCE_DEPENDENCIES))
        (self.root / "baseline").write_text("committed source baseline\n")
        self.git(self.root, "add", ".")
        self.path = "vendors/st/stm32f4_hal"
        owning_entries = source_sdk.dependency_snapshot(ROOT, development_fixture=True)[0]
        # Validate the complete owning graph while selecting only the real F4
        # checkout below. Other dependencies remain uninitialized Gitlinks.
        for item in ALL_SOURCE_DEPENDENCIES:
            if item.path == self.path:
                continue
            row = owning_entries[item.path]
            self.assertEqual(row[:2], ("160000", "commit"))
            (self.root / item.path).mkdir(parents=True, exist_ok=True)
            self.git(self.root, "update-index", "--add", "--cacheinfo",
                     "160000", row[2], item.path)
        self.commit(self.root, "committed baseline without new dependency")
        self.baseline = self.git(self.root, "rev-parse", "HEAD").strip()
        self.checkout = self.root / self.path
        self.checkout.parent.mkdir(parents=True, exist_ok=True)
        self.git(self.root, "clone", "--quiet", "--shared",
                 str(ROOT / self.path), str(self.checkout))
        self.git(self.checkout, "remote", "set-url", "origin",
                 source_sdk.dependency(self.path).fetch_url)
        self.git(self.root, "add", self.path)
        self.git(self.root, "submodule", "init", "--", self.path)
        # This test exercises the source-snapshot boundary using one real,
        # clean official dependency. Full SDK tests below retain every source.
        patch = mock_patch.object(source_sdk, "runtime_dependency_closure",
            return_value={self.path: source_sdk.dependency(self.path)})
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
        with self.assertRaisesRegex(ValueError, "missing top-level dependency Gitlink"):
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

    def test_parent_staged_index_does_not_replace_a_dependency_index(self):
        alternate = self.root / ".git/selected-index"
        shutil.copy2(self.root / ".git/index", alternate)
        self.git(self.root, "update-index", "--force-remove", self.path)
        with mock_patch.dict(os.environ, {"GIT_INDEX_FILE": str(alternate)}):
            entries, index = source_sdk.dependency_snapshot(self.root, True)
            self.assertIn(self.path, entries)
            records = source_sdk.dependency_records(self.root, entries)
            self.assertEqual(records[0]["commit"], entries[self.path][2])
            source_sdk.verify_preparation_snapshot(self.root, self.baseline,
                                                   index)

    def test_dependency_tree_ignores_local_commit_replacement(self):
        original = self.git(self.checkout, "rev-parse", "HEAD").strip()
        expected = source_sdk.tree_entries(self.checkout)
        (self.checkout / "unreviewed-payload.h").write_text("unreviewed\n")
        self.git(self.checkout, "add", "unreviewed-payload.h")
        self.commit(self.checkout, "unreviewed alternate source")
        changed = self.git(self.checkout, "rev-parse", "HEAD").strip()
        self.git(self.checkout, "checkout", "--quiet", "--detach", original)
        self.git(self.checkout, "replace", original, changed)
        actual = source_sdk.tree_entries(self.checkout)
        self.assertEqual(set(actual) - set(expected), set())
        self.assertEqual(actual, expected)

    def test_committed_blob_reader_ignores_local_blob_replacement(self):
        entries = source_sdk.tree_entries(self.checkout)
        selected = {"LICENSE.md": entries["LICENSE.md"]}
        expected = source_sdk.committed_blobs(self.checkout, selected)
        changed = self.git(self.checkout, "hash-object", "-w", "--stdin",
                           input=b"unreviewed replacement notice\n").strip()
        self.git(self.checkout, "replace", selected["LICENSE.md"][2], changed)
        self.assertEqual(source_sdk.committed_blobs(self.checkout, selected),
                         expected)

    def test_selected_source_cannot_use_an_unapproved_fetch_origin(self):
        entries, _ = source_sdk.dependency_snapshot(self.root, True)
        self.git(self.checkout, "remote", "set-url", "origin",
                 "https://github.com/unreviewed/stm32f4xx-hal-driver.git")
        with self.assertRaisesRegex(ValueError, "fetch origin"):
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
        self.write("arch/arm/cortex_m/staged.c", "int staged_source;\n")
        self.write("docs/staged.txt", "nonselected staged source\n")
        self.git("add", "arch/arm/cortex_m/staged.c", "docs/staged.txt")
        self.write("tools/configure/configure.py", "untracked_source = True\n")
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
            "core/committed.c", "arch/arm/cortex_m/staged.c",
            "tools/configure/configure.py"})
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
                     "tools/evidence/identity.py")
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
                self.assertNotIn(committed[1], names)
                self.assertNotIn(committed[2], names)
                self.assertNotIn(staged[1], names)
                self.assertEqual(untracked[1] in names, development)
        self.assertEqual(self.git("write-tree"), index_before)


class SourceSDKTests(unittest.TestCase):
    @_isolated_sdk_prefix
    def test_recursive_dependency_membership_matches_reviewed_catalogue(self):
        identity = self.sdk / source_sdk.MANIFEST
        original = identity.read_bytes()
        child = "ext/mbedtls/tf-psa-crypto"
        try:
            for change in ("missing-child", "unregistered-child"):
                with self.subTest(change=change):
                    manifest = json.loads(original)
                    if change == "missing-child":
                        manifest["dependencies"] = [
                            row for row in manifest["dependencies"]
                            if row["path"] != child]
                    else:
                        manifest["dependencies"].append({
                            "path": "ext/mbedtls/library",
                            "commit": "a" * 40, "tree": "b" * 40})
                    identity.write_text(json.dumps(manifest) + "\n")
                    rejected = self.verify()
                    self.assertNotEqual(rejected.returncode, 0)
                    self.assertIn("dependency set", rejected.stderr)
        finally:
            identity.write_bytes(original)

    @_isolated_sdk_prefix
    def test_rehashed_nested_fetch_declaration_is_not_an_origin_override(self):
        identity = self.sdk / source_sdk.MANIFEST
        original_identity = identity.read_bytes()
        target = self.sdk / "ext/mbedtls/.gitmodules"
        original = target.read_bytes()
        try:
            changed = original.replace(b"https://github.com/",
                                       b"https://unreviewed.invalid/")
            self.assertNotEqual(changed, original)
            self.rehash_manifest_file(json.loads(original_identity), target,
                                      changed)
            rejected = self.verify()
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn("dependency", rejected.stderr)
        finally:
            target.write_bytes(original)
            identity.write_bytes(original_identity)




    @_isolated_sdk_prefix
    def test_mutable_prefix_is_a_complete_independent_copy(self):
        import hashlib

        seed_prefix = type(self).prefix
        seed_sdk = type(self).sdk
        self.assertNotEqual(self.prefix, seed_prefix)
        self.assertNotEqual(self.sdk, seed_sdk)

        def snapshot(prefix):
            return {path.relative_to(prefix).as_posix(): (
                path.stat().st_dev, path.stat().st_ino,
                path.stat().st_mode,
                hashlib.sha256(path.read_bytes()).hexdigest())
                for path in prefix.rglob("*") if path.is_file()}

        before = snapshot(seed_prefix)
        copied = snapshot(self.prefix)
        self.assertEqual(set(copied), set(before))
        for name, original in before.items():
            with self.subTest(file=name):
                self.assertNotEqual(copied[name][:2], original[:2])
                self.assertEqual(copied[name][2:], original[2:])
        self.assertTrue((self.prefix /
                         "lib/cmake/Nexus/NexusConfig.cmake").is_file())
        self.assertEqual(self.verify().returncode, 0)
        seed_command = [sys.executable, "-B",
                        str(seed_sdk / "cmake/package/package_source_sdk.py"),
                        "--verify", str(seed_sdk)]
        for name, diagnostic in (
                ("share/nexus/src/core/src/request.c", "file identity changed"),
                ("lib/cmake/Nexus/NexusConfig.cmake",
                 "Exported CMake package identity changed")):
            with self.subTest(mutated=name):
                target = self.prefix / name
                original = target.read_bytes()
                target.write_bytes(
                    original + b"\n/* private fixture mutation */\n")
                rejected = self.verify()
                self.assertNotEqual(rejected.returncode, 0)
                self.assertIn(diagnostic, rejected.stderr)
                self.assertEqual(snapshot(seed_prefix), before)
                self.assertEqual(self.run_process(seed_command).returncode, 0)
                target.write_bytes(original)

    def test_read_only_case_reuses_the_class_seed(self):
        self.assertEqual(self.prefix, type(self).prefix)
        self.assertEqual(self.sdk, type(self).sdk)

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
                    "--source", str(ROOT), "--output", str(original), "--development-fixture",
                    "--backend", "threadx", "--host-support",
                    "--component", "network", "--component", "tls",
                    "--component", "mqtt", "--component", "mcuboot"])
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
        soc = json.loads((self.sdk / "tests/support/native/soc.json").read_text())
        (directory / "cpu.json").write_text(json.dumps({
            "schema_version": 1, "cpu": soc["cpu"], "irq": soc["irq"],
            "clock_hz": 1000000}) + "\n")
        runtime = directory / "runtime.toml"
        runtime.write_text('schema_version=1\ncpu_facts="cpu.json"\n'
                           'backend="baremetal"\noptimization="Os"\n')
        return {
            "platform": [sys.executable, "-B", str(self.sdk / "tools/configure/configure.py"),
                "--assembly", str(self.sdk / "tests/support/native/assemblies/native-empty.toml"),
                "--output", str(directory / "platform-generated"), "--test-support"],
            "runtime": [sys.executable, "-B", str(self.sdk / "tools/configure/runtime.py"),
                "--assembly", str(runtime), "--output", str(directory / "runtime-generated")],
            "cpu": [sys.executable, "-B", str(self.sdk / "tools/configure/cpu.py"),
                "--facts", str(directory / "cpu.json"), "--backend", "baremetal",
                "--output", str(directory / "cpu-generated")],
        }

    @_isolated_sdk_prefix
    def test_standalone_resolvers_reject_source_byte_tamper(self):
        commands = self.standalone_resolver_commands()
        for command in commands.values():
            self.run_ok(command)
        target = self.sdk / "io/src/api/gpio.c"
        original = target.read_bytes()
        try:
            target.write_bytes(original + b"\n/* Source SDK payload tamper. */\n")
            for name, command in commands.items():
                with self.subTest(boundary=name):
                    rejected = self.run_process(command)
                    self.assertNotEqual(rejected.returncode, 0)
                    self.assertIn("Source SDK file identity changed: io/src/api/gpio.c",
                                  rejected.stderr)
                    self.assertFalse(Path(command[-1]).exists())
        finally:
            target.write_bytes(original)

    @_isolated_sdk_prefix
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
        self.run_ok(["cmake", "--build", str(build), "--parallel", "1"])
        for application in ("installed_c", "installed_cpp"):
            self.run_ok([str(build / "bin" / application)])
        self.run_ok([str(build / "parent_plain")])
        compile_commands = json.loads((build / "compile_commands.json").read_text())
        self.assertGreater(len(compile_commands), 0)
        for entry in compile_commands:
            self.assertNotIn(str(ROOT) + "/", entry.get("command", ""))

    def test_nominal_ids_reject_cross_class_endpoint_and_raw_ids_in_consumers(self):
        source, build, command = consumer_gate.prepare_consumer(
            self.prefix, self.directory, "nominal identity", self.run_ok,
            fixture=True)
        assembly = source / "assembly.toml"
        assembly.write_text(
            'schema=2\nbackend="native"\nclock="model"\n'
            f'board_package="{self.sdk}/tests/support/native/board"\n'
            '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
            '[gpio.led0]\nbinding="led0"\nmode="output"\n'
            '[spi.bus0]\nbinding="spi0"\nmode="short-poll"\n'
            'max_hz=1000000\n[spi_device.sensor0]\ncontroller="bus0"\n'
            'driver="spi-endpoint"\nmode=0\nmax_hz=1000000\n')
        command = [argument for argument in command
                   if not argument.startswith("-DNEXUS_ASSEMBLY_FILE=")]
        command.append("-DNEXUS_ASSEMBLY_FILE=" + str(assembly))
        with (source / "CMakeLists.txt").open("a") as stream:
            stream.write(
                'set_target_properties(installed_cpp PROPERTIES '
                'CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)\n'
                'nexus_target_warnings(installed_c STRICT)\n'
                'nexus_target_warnings(installed_cpp STRICT)\n')
        code = (
            '#include "nexus_bindings.h"\n'
            '#include "nexus_factory_strong.h"\n'
            'int main(void) {\n'
            '    if (nx_factory_gpio_ref(NX_GPIO_REF_LED0) == 0 ||\n'
            '        nx_factory_spi_ref(NX_SPI_REF_BUS0) == 0 ||\n'
            '        nx_factory_spi_device_ref(NX_SPI_DEVICE_REF_SENSOR0) == 0)\n'
            '        return 3;\n'
            '    nx_platform_start_result_t start = nx_platform_start();\n'
            '    if (start.primary != NX_SUCCESS) return 1;\n'
            '    return nx_platform_stop() == NX_SUCCESS ? 0 : 2;\n}\n')
        for filename in ("main.c", "main.cpp"):
            (source / filename).write_text(code)
        self.run_ok(command)
        self.run_ok(["cmake", "--build", str(build), "--target", "installed_c",
                     "installed_cpp", "--parallel", "1"])
        for application in ("installed_c", "installed_cpp"):
            self.run_ok([str(build / "bin" / application)])
        entries = json.loads((build / "compile_commands.json").read_text())
        expressions = {
            "valid": "nx_factory_gpio_ref(NX_GPIO_REF_LED0)",
            "cross-class": "nx_factory_gpio_ref(NX_SPI_REF_BUS0)",
            "controller-as-endpoint":
                "nx_factory_spi_device_ref(NX_SPI_REF_BUS0)",
            "endpoint-as-controller":
                "nx_factory_spi_ref(NX_SPI_DEVICE_REF_SENSOR0)",
            "raw-id": "nx_factory_gpio_ref(NX_GPIO_ID_LED0)",
        }
        for filename, standard in (("main.c", "-std=c11"),
                                   ("main.cpp", "-std=c++17")):
            entry, = [item for item in entries
                      if Path(item["file"]) == source / filename]
            context = entry.get("arguments") or shlex.split(entry["command"])
            self.assertIn(standard, context)
            self.assertFalse(consumer_gate.private_compile_inputs(entry, self.sdk))
            for label, expression in expressions.items():
                with self.subTest(language=filename, expression=label):
                    probe = source / (label + Path(filename).suffix)
                    probe.write_text('#include "nexus_factory_strong.h"\n'
                                     'const void* nominal_lookup(void);\n'
                                     'const void* nominal_lookup(void) {\n'
                                     '    return ' + expression + ';\n}\n')
                    arguments = list(context)
                    arguments[arguments.index("-o") + 1] = str(
                        source / (probe.name + ".o"))
                    arguments[arguments.index(str(source / filename))] = str(probe)
                    result = self.run_process(arguments, cwd=entry["directory"])
                    if label == "valid":
                        self.assertEqual(result.returncode, 0, result.stderr)
                    else:
                        self.assertNotEqual(result.returncode, 0, expression)
                        self.assertNotIn("No such file", result.stderr)
                        self.assertIn("nx_", result.stderr)

    def test_relocated_sdk_exports_policy_version_and_rejects_other_versions(self):
        version = load_policy(self.sdk)["semantic_version"]
        project_build = self.directory / "policy source project"
        self.run_ok(["cmake", "-S", str(self.sdk), "-B", str(project_build),
                     "-G", "Ninja", "-DNEXUS_BUILD_TESTS=OFF",
                     "-DNEXUS_BUILD_WORKLOAD=OFF", "-DNEXUS_TEST_SUPPORT=ON",
                     "-DNEXUS_ASSEMBLY_FILE=" + str(self.sdk /
                     "tests/support/native/assemblies/native.toml")])
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
        self.run_ok(["cmake", "--build", str(build), "--parallel", "1", "--target", "installed_c", "installed_cpp"])
        for application in ("installed_c", "installed_cpp"):
            self.assertGreater((build / "bin" / (application + ".elf")).stat().st_size, 0)
            resources = json.loads((build / "bin" / (application + ".resources.json")).read_text())
            self.assertEqual(resources["status"], "passed")
        self.check_private_includes(build)



    def check_private_includes(self, build):
        commands = json.loads((build / "compile_commands.json").read_text())
        product = [entry for entry in commands if Path(entry["file"]).name in {"main.c", "main.cpp"}]
        self.assertEqual(len(product), 2)
        for entry in product:
            command = entry["command"]
            self.assertFalse(consumer_gate.private_compile_inputs(entry, self.sdk),
                             json.dumps(entry, sort_keys=True))
            self.assertNotIn(str(ROOT) + "/", command)





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
        self.run_ok(["cmake", "--build", str(build), "--parallel", "1",
                     "--target", "installed_threadx"])
        commands = json.loads((build / "compile_commands.json").read_text())
        product = [entry for entry in commands
                   if Path(entry["file"]).resolve() == source / "main.c"]
        self.assertEqual(len(product), 1)
        for entry in commands:
            self.assertNotIn(str(ROOT) + "/", entry["command"])
        self.assertFalse(consumer_gate.private_compile_inputs(product[0], self.sdk))
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
                ("sky", "stm32f407ve_sky_qingchun", "hse8-pll168")):
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

    @_isolated_sdk_prefix
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

    @_isolated_sdk_prefix
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

    @_isolated_sdk_prefix
    def test_missing_license_is_rejected(self):
        target = self.sdk / "ext/threadx/LICENSE.txt"
        original = target.read_bytes()
        try:
            target.unlink()
            result = self.verify()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Missing source SDK file", result.stderr)
        finally:
            target.write_bytes(original)

    @_isolated_sdk_prefix
    def test_rehashed_f407_ccm_initializer_omission_rejects_non_ccm_consumer(self):
        import hashlib

        assembly = self.sdk / "tools/configure/assemblies/qiming-baremetal.toml"
        output = self.directory / "non-ccm consumer"
        command = [sys.executable, "-B",
                   str(self.sdk / "tools/configure/configure.py"),
                   "--source-root", str(self.sdk), "--assembly", str(assembly),
                   "--output", str(output)]
        baseline = self.run_process(command)
        self.assertEqual(baseline.returncode, 0,
                         baseline.stdout + baseline.stderr)
        self.assertIn("set(NEXUS_MEMORY_CCM 0)",
                      (output / "selection.cmake").read_text())
        identity_path = self.sdk / source_sdk.MANIFEST
        identity = json.loads(identity_path.read_text())
        name = "soc/stm32/stm32f4/memory_init.c"
        (self.sdk / name).unlink()
        identity["files_sha256"].pop(name)
        identity["snapshot_sha256"] = hashlib.sha256(json.dumps(
            identity["files_sha256"], sort_keys=True,
            separators=(",", ":")).encode()).hexdigest()
        identity_path.write_text(json.dumps(identity) + "\n")
        # Completeness is an installed-SDK contract even when this consumer
        # never selects CCM. The actual resolver must reject the rehashed SDK
        # before attempting selected generation; no verifier is replaced.
        rejected = self.run_process(command)
        self.assertNotEqual(rejected.returncode, 0,
                            rejected.stdout + rejected.stderr)
        self.assertIn("missing required sources/licenses", rejected.stderr)

    @_isolated_sdk_prefix
    def test_rehashed_missing_arch_or_core_source_is_rejected(self):
        identity = self.sdk / source_sdk.MANIFEST
        original_identity = identity.read_bytes()
        for name in ("cmake/platform/CMSISCore.cmake",
                     "core/include/nexus/core/lifecycle.h",
                     "core/src/lifecycle.c",
                     "arch/include/nexus/arch/arch.h",
                     "arch/arm/cortex_m/nx_arch_cortex_m.c",
                     "arch/arm/cortex_m/private/compiler.h",
                     "arch/arm/cortex_m/private/registers.h",
                     "arch/native/nx_arch_native.c",
                     "vendors/arm/CMSIS_6/LICENSE",
                     "vendors/arm/CMSIS_6/CMSIS/Core/Include/cmsis_gcc.h",
                     "vendors/arm/CMSIS_6/CMSIS/Core/Include/"
                     "m-profile/cmsis_gcc_m.h",
                     "vendors/arm/CMSIS_6/CMSIS/Core/Include/"
                     "m-profile/armv8m_mpu.h"):
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

    @_isolated_sdk_prefix
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
                     "tools/dev/provenance.py", "tools/evidence/identity.py",
                     "tools/maintenance/consumer_profiles.py"):
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


    @_isolated_sdk_prefix
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

    @_isolated_sdk_prefix
    def test_rehashed_exposed_io_clock_and_cold_source_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        names = (
            'core/src/fault.c',
            'core/include/nexus/core/fault.h',
            'io/include/nexus/io/usb_device.h',
            'soc/stm32/stm32f4/drivers/ip/fs_device_pio/usb_device_controller.c',
            'soc/stm32/stm32f4/drivers/ip/fs_device_pio/usb_device_controller.h',
            'tests/support/native/drivers/usb_device_access.h',
            'io/include/nexus/io/ethernet.h',
            'soc/stm32/stm32f4/drivers/ip/legacy_mac/eth_mac.c',
            'soc/stm32/stm32f4/drivers/ip/legacy_mac/eth_mac.h',
            'io/include/nexus/io/mdio.h',
            'soc/stm32/stm32f4/drivers/usb_device.c',
            'soc/stm32/stm32f4/private/stm32f407_can.h',
            'soc/stm32/stm32f4/drivers/ip/classic_can/can_controller.c',
            'soc/stm32/stm32f4/drivers/ip/classic_can/can_controller.h',
            'io/include/nexus/io/advanced_pwm.h',
            'soc/stm32/stm32f4/drivers/ip/advanced_timer/timer_advanced_controller.h',
            'os/freertos/timebase_lowpower.c',
            'os/freertos/private/timebase_lowpower.h',
            'soc/stm32/stm32f4/timebase_lowpower.c',
            'tools/configure/usb_device.py',
            'tools/configure/ethernet.py',
            'tools/configure/timer_advanced.py',
            'tools/configure/bindings.py',
            'cmake/platform/ControllerModes.cmake',
            'components/clock_domain/private/clock_domain_impl.h',
            'components/amp_ipc/CMakeLists.txt',
            'components/amp_ipc/src/amp_ipc.c',
            'components/amp_ipc/include/nexus/components/amp_ipc.h',
            'components/amp_ipc/include/nexus/components/amp_ipc_storage.h',
            'components/soc/stm32/stm32f407_handoff/CMakeLists.txt',
            'components/soc/stm32/stm32f407_handoff/include/nexus/components/stm32f407_handoff.h',
            'soc/stm32/stm32f4/handoff.c',
            'soc/stm32/stm32f4/private/stm32f407_handoff_hw.h',
            'io/include/nexus/io/frame_stream.h',
            'io/src/mechanisms/frame_stream.c',
            'components/mapped_storage/CMakeLists.txt',
            'components/mapped_storage/src/mapped_storage_owner.c',
            'components/mapped_storage/src/native_mapped_storage.c',
            'components/mapped_storage/include/nexus/components/mapped_storage.h',
            'components/user_io_broker/include/nexus/components/user_io_broker.h',
            'components/dp83848/include/nexus/dp83848/dp83848.h',
            'soc/stm32/stm32f4/system.c',
            'soc/stm32/stm32f4/drivers/platform_lifecycle.c',
            'soc/stm32/stm32f4/drivers/system_lifecycle.c',
            'soc/stm32/stm32f4/devices.json',
        )
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

    @_isolated_sdk_prefix
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

    @_isolated_sdk_prefix
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

    @_isolated_sdk_prefix
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

    @_isolated_sdk_prefix
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

    @_isolated_sdk_prefix
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

    @_isolated_sdk_prefix
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
        self.assertEqual(json.loads(result.stdout)["verified_files"],
                         len(OFFICIAL_F4_VERIFIED_FILES))
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




    def rehash_manifest_file(self, manifest, file, data):
        file.write_bytes(data)
        manifest["files_sha256"][file.relative_to(self.sdk).as_posix()] = (
            source_sdk.digest(data))
        manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
            manifest["files_sha256"], sort_keys=True,
            separators=(",", ":")).encode())
        (self.sdk / source_sdk.MANIFEST).write_text(json.dumps(manifest) + "\n")






    @_isolated_sdk_prefix
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
                        with mock_patch.object(source_sdk.threadx_lock, "verify",
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
            with mock_patch.object(source_sdk.threadx_lock, "verify",
                                   side_effect=add_file):
                with self.assertRaisesRegex(ValueError, "changed during"):
                    source_sdk.verify(self.sdk)
        finally:
            extra.unlink(missing_ok=True)

    @_isolated_sdk_prefix
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
                    with mock_patch.object(Path, "read_bytes", new=read_file):
                        with self.assertRaisesRegex(ValueError, "changed during"):
                            source_sdk.verify(self.sdk)
                    self.assertEqual(calls, 2,
                                     "Filesystem injection did not reach final read")
                finally:
                    target.write_bytes(old)

    @_isolated_sdk_prefix
    def test_git_checkout_inside_sdk_cannot_bypass_whole_manifest(self):
        for family, dependency, lock_name, extra in (
                ("F4", "vendors/st/stm32f4_hal", "stm32f4-hal",
                 ["--cmsis-source", str(self.sdk / "vendors/st/cmsis_device_f4")]),
                ("ThreadX", "ext/threadx", "threadx", ["--port", "linux"])):
            with self.subTest(dependency=dependency):
                source = self.sdk / dependency
                private = self.directory / ("private " + family + " Git metadata")
                self.run_ok(["git", "clone", "--shared", "--no-checkout", "--quiet",
                             str(ROOT / dependency), str(private)])
                lock_path = self.sdk / ("dependencies/" + lock_name + ".lock.json")
                lock = json.loads(lock_path.read_text())
                self.run_ok(["git", "-C", str(private), "remote", "set-url", "origin",
                             source_sdk.dependency(dependency).fetch_url])
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
                        sys.executable, "-B", str(self.sdk / ("tools/maintenance/" + lock_name.replace("-", "_") + "_lock.py")),
                        "--source", str(source), "--lock", str(lock_path), *extra])
                    self.assertNotEqual(result.returncode, 0,
                                        "Valid source Git must not bypass SDK manifest verification")
                finally:
                    other.write_bytes(original)
                    shutil.rmtree(source / ".git")

    @_isolated_sdk_prefix
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
        self.assertIn("missing top-level dependency Gitlink", result.stderr)
        self.assertFalse((self.directory / "must not export empty SDK").exists())





if __name__ == "__main__":
    unittest.main()
