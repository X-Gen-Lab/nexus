"""Official STM32 scopes stay private and reject implicit HAL selection."""
from __future__ import annotations
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class STM32OfficialCMakeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="official LL CMake ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "empty.c").write_text("int official_fixture(void) { return 0; }\n")

    def configure(self, statement, module_root=None, *, arm=False):
        module_root = ROOT if module_root is None else module_root
        (self.root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(official_ll_fixture C)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            f'set(Python3_EXECUTABLE "{sys.executable}")\n'
            f'include("{module_root}/cmake/platform/STM32Official.cmake")\n'
            'add_library(provider STATIC empty.c)\n'
            'target_compile_definitions(provider PRIVATE STM32F407xx)\n' + statement)
        return subprocess.run([
            shutil.which("cmake"), "-S", str(self.root), "-B",
            str(self.root / "build"), "-G", "Ninja",
            *(["-DCMAKE_TOOLCHAIN_FILE=" + str(
                ROOT / "cmake/toolchains/arm-gcc.cmake")] if arm else [])],
            capture_output=True, text=True, timeout=60)

    def test_selected_target_has_private_official_headers_without_hal_source(self):
        result = self.configure(
            'nexus_stm32f4_official(provider)\n'
            'get_target_property(private provider INCLUDE_DIRECTORIES)\n'
            'get_target_property(public provider INTERFACE_INCLUDE_DIRECTORIES)\n'
            'get_target_property(sources provider SOURCES)\n'
            'get_target_property(links provider LINK_LIBRARIES)\n'
            'get_target_property(pin provider NEXUS_STM32F4_OFFICIAL_REVISION)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/properties" '
            '"${private}\n${public}\n${sources}\n${links}\n${pin}\n")\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = (self.root / "build/properties").read_text().splitlines()
        self.assertEqual(lines[0], str(ROOT / "vendors/st/stm32f4_hal/Inc"))
        self.assertEqual(lines[1], "public-NOTFOUND")
        self.assertEqual(lines[2], "empty.c")
        self.assertEqual(lines[3], "links-NOTFOUND")
        self.assertEqual(lines[4], "b6f0ed3829f3829eb358a2e7417d80bba1a42db7")
        built = subprocess.run([
            shutil.which("cmake"), "--build", str(self.root / "build")],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)

    def test_unknown_interface_and_extra_arguments_are_rejected(self):
        for statement in ('nexus_stm32f4_official(missing)\n',
                          'add_library(face INTERFACE)\n'
                          'nexus_stm32f4_official(face)\n',
                          'nexus_stm32f4_official(provider HAL)\n'):
            with self.subTest(statement=statement):
                result = self.configure(statement)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("official", result.stdout + result.stderr)



    def test_explicit_uart_hal_closes_only_private_vendor_transport(self):
        result = self.configure(
            'nexus_stm32f407_uart_hal(provider)\n'
            'get_target_property(private provider INCLUDE_DIRECTORIES)\n'
            'get_target_property(public provider INTERFACE_INCLUDE_DIRECTORIES)\n'
            'get_target_property(sources provider SOURCES)\n'
            'get_target_property(vendor provider_stm32f407_hal SOURCES)\n'
            'get_target_property(options provider_stm32f407_hal COMPILE_OPTIONS)\n'
            'get_target_property(kind provider_stm32f407_hal TYPE)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/hal-properties" '
            '"${private}\\n${public}\\n${sources}\\n${vendor}\\n${options}\\n${kind}\\n")\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = (self.root / "build/hal-properties").read_text().splitlines()
        self.assertIn(str(ROOT / "soc/stm32/stm32f4/private/hal"),
                      lines[0].split(";"))
        self.assertEqual(lines[1], "public-NOTFOUND")
        self.assertEqual(lines[3].split(";"), [
            str(ROOT / "vendors/st/stm32f4_hal/Src" / name)
            for name in ("stm32f4xx_hal_uart.c",)])
        self.assertIn("$<TARGET_OBJECTS:provider_stm32f407_hal>", lines[2])
        self.assertIn("-include" + str(
            ROOT / "soc/stm32/stm32f4/private/stm32f407_hal_profile.h"),
            lines[4].split(";"))
        self.assertEqual(lines[5], "OBJECT_LIBRARY")

    def test_real_f407_assembly_selects_one_hal_transport_only_for_dma_tx(self):
        self.assertIsNotNone(shutil.which("arm-none-eabi-gcc"),
                             "Prepared ARM compiler required")
        fixture = ROOT / "tests/contracts/uart_rx_compact_assembly"
        for mode in ("dma-tx", "irq"):
            with self.subTest(mode=mode):
                build = self.root / mode
                result = subprocess.run([
                    shutil.which("cmake"), "-S", str(ROOT), "-B", str(build),
                    "-G", "Ninja",
                    "-DCMAKE_TOOLCHAIN_FILE=" + str(
                        ROOT / "cmake/toolchains/arm-gcc.cmake"),
                    "-DNEXUS_ASSEMBLY_FILE=" + str(
                        fixture / ("f407-" + mode + ".toml")),
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                    "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF"],
                    capture_output=True, text=True, timeout=90)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)
                commands = json.loads((build / "compile_commands.json")
                                      .read_text())
                vendor = [entry for entry in commands
                          if "/vendors/st/stm32f4_hal/Src/" in entry["file"]]
                self.assertEqual(
                    {Path(entry["file"]).name for entry in vendor},
                    {"stm32f4xx_hal_uart.c", "stm32f4xx_hal_dma.c",
                     "stm32f4xx_hal_rcc.c"} if mode == "dma-tx" else set())
                drivers = {Path(entry["file"]).name for entry in commands
                           if "/soc/stm32/stm32f4/drivers/" in entry["file"]}
                self.assertEqual("uart_hal.c" in drivers, mode == "dma-tx")
                self.assertNotIn("uart_dma.c", drivers)
                for entry in vendor:
                    if Path(entry["file"]).name == "stm32f4xx_hal_dma.c":
                        self.assertIn("/private/dma_hal", entry["command"])
                        self.assertNotIn("stm32f407_hal_profile.h", entry["command"])
                    elif Path(entry["file"]).name == "stm32f4xx_hal_rcc.c":
                        self.assertIn("/private/rcc_hal", entry["command"])
                        self.assertNotIn("stm32f407_hal_profile.h", entry["command"])
                        self.assertNotIn("/private/hal ", entry["command"])
                    else:
                        self.assertIn("stm32f407_hal_profile.h", entry["command"])
                        self.assertIn("/private/hal", entry["command"])

    def test_real_usb_products_select_remote_wakeup_only_inside_soc_target(self):
        fixtures = (
            ("usb_assembly/stm32-ve.toml", False, "controller_main.c"),
            ("usb_assembly/stm32-ve-remote.toml", True, "controller_main.c"),
            ("usb_otg_assembly/ve-hs-fs-dma-remote.toml", True, "otg_main.c"),
            ("usb_otg_assembly/zg-ulpi-dma-remote.toml", True, "otg_main.c"))
        macro = "-DNEXUS_STM32F4_USB_REMOTE_WAKEUP=1"
        for index, (relative, selected, main) in enumerate(fixtures):
            with self.subTest(assembly=relative):
                fixture = ROOT / "tests/contracts" / relative
                build = self.root / ("usb-" + str(index))
                result = subprocess.run([
                    shutil.which("cmake"), "-S", str(fixture.parent),
                    "-B", str(build), "-G", "Ninja",
                    "-DCMAKE_TOOLCHAIN_FILE=" + str(
                        ROOT / "cmake/toolchains/arm-gcc.cmake"),
                    "-DNEXUS_SOURCE_ROOT=" + str(ROOT),
                    "-DNEXUS_ASSEMBLY_FILE=" + str(fixture)],
                    capture_output=True, text=True, timeout=90)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)
                commands = json.loads((build / "compile_commands.json").read_text())
                device = [entry for entry in commands
                          if Path(entry["file"]).name == "usb_device_controller.c"]
                consumer = [entry for entry in commands
                            if Path(entry["file"]).name == main]
                self.assertTrue(device, "actual DCD compilation is required")
                self.assertEqual(len(consumer), 1)
                for entry in device:
                    argv = entry.get("arguments") or shlex.split(entry["command"])
                    self.assertEqual(argv.count(macro), int(selected),
                                     entry["command"])
                self.assertNotIn(macro, shlex.split(consumer[0]["command"]))

    def test_serial_qspi_and_parallel_nor_select_their_own_real_target_sources(self):
        fixtures = (("qspi", "stm32f4_qspi/f479ni-qspi.toml", True),
                    ("parallel-nor", "fsmc_assembly/zg-nor.toml", False))
        for name, relative, serial in fixtures:
            with self.subTest(mode=name):
                build = self.root / name
                result = subprocess.run([
                    shutil.which("cmake"), "-S", str(ROOT), "-B", str(build),
                    "-G", "Ninja",
                    "-DCMAKE_TOOLCHAIN_FILE=" + str(
                        ROOT / "cmake/toolchains/arm-gcc.cmake"),
                    "-DNEXUS_ASSEMBLY_FILE=" + str(
                        ROOT / "tests/contracts" / relative),
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                    "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF"],
                    capture_output=True, text=True, timeout=90)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)
                commands = json.loads((build / "compile_commands.json").read_text())
                drivers = {Path(entry["file"]).name for entry in commands
                           if "/soc/stm32/stm32f4/drivers/" in entry["file"]}
                self.assertEqual("fsmc_nor.c" in drivers, not serial)
                self.assertEqual("fsmc.c" in drivers, not serial)
                self.assertEqual("qspi.c" in drivers, serial)
                qspi = [entry for entry in commands
                        if Path(entry["file"]).name == "stm32f4xx_hal_qspi.c"]
                self.assertEqual(len(qspi), int(serial))
                dma = [entry for entry in commands
                       if Path(entry["file"]).name == "stm32f4xx_hal_dma.c"]
                self.assertEqual(len(dma), int(serial))
                if serial:
                    self.assertIn("/private/qspi_hal", qspi[0]["command"])
                    self.assertIn("USE_HAL_QSPI_REGISTER_CALLBACKS=1U",
                                  qspi[0]["command"])
                    self.assertIn("/private/dma_hal", dma[0]["command"])

    def test_uart_and_repeated_dma_users_share_one_real_arm_object(self):
        result = self.configure(
            'nexus_stm32f407_uart_hal(provider)\n'
            'nexus_stm32f4_dma_hal(provider)\n'
            'nexus_stm32f4_dma_hal(provider)\n'
            'target_compile_options(provider_stm32_dma_hal PRIVATE '
            '-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)\n',
            arm=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        commands = json.loads((self.root / "build/compile_commands.json").read_text())
        dma = [entry for entry in commands
               if Path(entry["file"]).name == "stm32f4xx_hal_dma.c"]
        self.assertEqual(len(dma), 1)
        self.assertIn("/private/dma_hal", dma[0]["command"])
        self.assertNotIn("/private/hal ", dma[0]["command"])
        built = subprocess.run([
            shutil.which("cmake"), "--build", str(self.root / "build"),
            "--target", "provider_stm32_dma_hal", "--parallel", "1"],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)

    def test_uart_and_extended_users_share_one_private_real_arm_rcc_object(self):
        (self.root / "nexus_config.h").write_text(
            "#define NEXUS_CLOCK_SOURCE_HSE 0\n")
        result = self.configure(
            'nexus_stm32f407_uart_hal(provider)\n'
            'nexus_stm32f4_rcc_hal(provider)\n'
            'nexus_stm32f4_rcc_hal(provider EXTENDED)\n'
            'nexus_stm32f4_rcc_hal(provider EXTENDED)\n'
            'target_include_directories(provider_stm32_rcc_hal PRIVATE '
            '"${CMAKE_SOURCE_DIR}")\n'
            'target_compile_options(provider_stm32_rcc_hal PRIVATE '
            '-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)\n'
            'get_target_property(public provider INTERFACE_INCLUDE_DIRECTORIES)\n'
            'get_target_property(sources provider SOURCES)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/rcc-properties" '
            '"${public}\n${sources}\n")\n', arm=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        commands = json.loads((self.root / "build/compile_commands.json").read_text())
        selected = [entry for entry in commands if Path(entry["file"]).name in
                    {"stm32f4xx_hal_rcc.c", "stm32f4xx_hal_rcc_ex.c"}]
        self.assertCountEqual([Path(entry["file"]).name for entry in selected],
                              ["stm32f4xx_hal_rcc.c", "stm32f4xx_hal_rcc_ex.c"])
        for entry in selected:
            self.assertIn("/private/rcc_hal", entry["command"])
            self.assertNotIn("stm32f407_hal_profile.h", entry["command"])
            self.assertNotIn("/private/hal ", entry["command"])
        public, sources = (self.root / "build/rcc-properties").read_text().splitlines()
        self.assertEqual("public-NOTFOUND", public)
        self.assertEqual(1, sources.count("TARGET_OBJECTS:provider_stm32_rcc_hal"))
        built = subprocess.run([shutil.which("cmake"), "--build",
            str(self.root / "build"), "--target", "provider_stm32_rcc_hal",
            "--parallel", "1"], capture_output=True, text=True, timeout=60)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)

    def test_shared_rcc_rejects_unknown_options_and_a_changed_device(self):
        for statement, reason in (
                ('nexus_stm32f4_rcc_hal(missing)\n', "RCC HAL requires"),
                ('nexus_stm32f4_rcc_hal(provider UNKNOWN)\n', "RCC HAL requires"),
                ('nexus_stm32f4_rcc_hal(provider)\n'
                 'set_target_properties(provider PROPERTIES '
                 'COMPILE_DEFINITIONS STM32F446xx)\n'
                 'nexus_stm32f4_rcc_hal(provider EXTENDED)\n', "RCC HAL device changed")):
            with self.subTest(statement=statement):
                result = self.configure(statement)
                self.assertNotEqual(0, result.returncode)
                self.assertIn(reason, result.stdout + result.stderr)

    def test_repeated_entry_rechecks_current_bytes_without_cached_success(self):
        fixture = self.root / "source"
        (fixture / "cmake/platform").mkdir(parents=True)
        shutil.copyfile(ROOT / "cmake/platform/STM32Official.cmake",
                        fixture / "cmake/platform/STM32Official.cmake")
        (fixture / "dependencies").mkdir()
        (fixture / "dependencies/stm32f4-hal.lock.json").write_text(
            '{"files":{"LICENSE.md":{"bytes":1,"sha256":"fixture"}}}\n')
        (fixture / "tools/maintenance").mkdir(parents=True)
        # Verification is an external subprocess port here. The real locked
        # sources are exercised above and by the source-verifier test suite.
        (fixture / "tools/maintenance/stm32f4_hal_lock.py").write_text(
            'from pathlib import Path\nimport sys\n'
            'root=Path(__file__).resolve().parents[2]\n'
            'count=root/"calls"\n'
            'count.write_text(str(int(count.read_text())+1) '
            'if count.exists() else "1")\n'
            'if (root/"payload").read_text() != "approved":\n'
            '    sys.exit("current source bytes changed")\n')
        (fixture / "payload").write_text("approved")
        result = self.configure(
            'nexus_stm32f4_official(provider)\n'
            f'file(WRITE "{fixture}/payload" "changed")\n'
            'nexus_stm32f4_official(provider)\n', module_root=fixture)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("current source bytes changed", result.stdout + result.stderr)
        self.assertEqual((fixture / "calls").read_text(), "2")


if __name__ == "__main__":
    unittest.main()
