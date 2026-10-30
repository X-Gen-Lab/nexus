"""Official STM32 scopes stay private and reject implicit HAL selection."""
from __future__ import annotations
import json
from pathlib import Path
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

    def configure(self, statement, module_root=ROOT):
        (self.root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(official_ll_fixture C)\n'
            f'set(Python3_EXECUTABLE "{sys.executable}")\n'
            f'include("{module_root}/cmake/platform/STM32Official.cmake")\n'
            'add_library(provider STATIC empty.c)\n' + statement)
        return subprocess.run([
            shutil.which("cmake"), "-S", str(self.root), "-B",
            str(self.root / "build"), "-G", "Ninja"],
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

    def test_f103_all_locked_driver_inputs_retrigger_source_verification(self):
        from tools.maintenance import stm32f1_hal_lock as f1
        generated = self.root / "generated"
        generated.mkdir()
        (generated / "resolved.json").write_text(json.dumps({
            "controllers": [{"kind": "i2c", "mode": "irq-messages"}],
        }))
        source = ROOT / "soc/stm32/stm32f103"
        (self.root / "CMakeLists.txt").write_text(f'''\
cmake_minimum_required(VERSION 3.31)
project(f103_locked_inputs LANGUAGES C ASM)
set(Python3_EXECUTABLE "{sys.executable}")
set(NEXUS_SOURCE_DIR "{ROOT.as_posix()}")
set(NEXUS_CONFIG_DIR "{generated.as_posix()}")
add_library(nexus_io INTERFACE)
add_library(nexus_arch INTERFACE)
add_library(Nexus::Arch ALIAS nexus_arch)
add_library(nexus_build_options INTERFACE)
add_library(nexus_owned_warnings INTERFACE)
add_subdirectory("{source.as_posix()}" soc)
get_property(inputs DIRECTORY "{source.as_posix()}"
    PROPERTY CMAKE_CONFIGURE_DEPENDS)
list(JOIN inputs "\\n" lines)
file(WRITE "${{CMAKE_BINARY_DIR}}/source-inputs.txt" "${{lines}}\\n")
''')
        result = subprocess.run([
            shutil.which("cmake"), "-S", str(self.root), "-B",
            str(self.root / "build"), "-G", "Ninja"],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        observed = set((self.root / "build/source-inputs.txt")
                       .read_text().splitlines())
        expected = {str(ROOT / f1.DIRECTORY / name)
                    for name in f1.REVIEWED_FILES}
        self.assertFalse(expected - observed,
                         "Locked driver inputs do not retrigger verification: " +
                         repr(sorted(expected - observed)))

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
        self.assertIn(str(ROOT / "soc/stm32/stm32f407/private/hal"),
                      lines[0].split(";"))
        self.assertEqual(lines[1], "public-NOTFOUND")
        self.assertEqual(lines[3].split(";"), [
            str(ROOT / "vendors/st/stm32f4_hal/Src" / name)
            for name in ("stm32f4xx_hal_uart.c", "stm32f4xx_hal_dma.c",
                         "stm32f4xx_hal_rcc.c")])
        self.assertIn("$<TARGET_OBJECTS:provider_stm32f407_hal>", lines[2])
        self.assertIn("-include" + str(
            ROOT / "soc/stm32/stm32f407/private/stm32f407_hal_profile.h"),
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
                           if "/soc/stm32/stm32f407/drivers/" in entry["file"]}
                self.assertEqual("uart_hal.c" in drivers, mode == "dma-tx")
                self.assertNotIn("uart_dma.c", drivers)
                for entry in vendor:
                    self.assertIn("stm32f407_hal_profile.h", entry["command"])
                    self.assertIn("/private/hal", entry["command"])

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
