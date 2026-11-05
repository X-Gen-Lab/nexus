"""Real ARM component links retain helpers without physical RTC selection."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class U575ComponentDomainTests(unittest.TestCase):
    def prepare(self, root):
        root.mkdir(parents=True, exist_ok=True)
        smoke = (ROOT / "cmake/package/verification/io_evolution/consumers/"
                 "u575-retained-time.c")
        (root / "CMakeLists.txt").write_text(f'''
cmake_minimum_required(VERSION 3.31)
project(retained_clock_consumer LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "Firmware has no host framework")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit public consumer")
add_subdirectory("{ROOT}" nexus-sdk)
nexus_add_firmware(consumer SOURCES "{smoke}" clock_reader.c
    LIBRARIES Nexus::U575RetainedTime)
target_link_libraries(consumer PRIVATE -Wl,--no-gc-sections)
''')
        (root / "clock_reader.c").write_text('''
#include "nexus/core/time.h"
nx_time_us_t nx_installed_clock_read(const nx_clock_t* clock);
nx_time_us_t nx_installed_clock_read(const nx_clock_t* clock) {
    return clock->read(clock->context);
}
''')

    def build(self, root):
        environment = dict(os.environ)
        environment["PATH"] = (str(ROOT / ".venv/bin") + os.pathsep +
                               environment.get("PATH", ""))
        cmake = shutil.which("cmake", path=environment["PATH"])
        self.assertIsNotNone(cmake, "Prepared CMake is required")
        self.assertIsNotNone(shutil.which("arm-none-eabi-gcc",
                                         path=environment["PATH"]),
                             "Prepared ARM toolchain is required")
        build = root / "build"
        commands = [
            ("configure", [cmake, "-S", str(root), "-B", str(build),
                           "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                           "-DCMAKE_TOOLCHAIN_FILE=" + str(
                               ROOT / "cmake/toolchains/arm-gcc.cmake"),
                           "-DNEXUS_ASSEMBLY_FILE=" + str(
                               ROOT / "tools/configure/assemblies/"
                               "nucleo-u575-baremetal-empty.toml")]),
            ("build", [cmake, "--build", str(build), "--target", "consumer",
                       "--parallel", "1"]),
        ]
        for stage, command in commands:
            process = subprocess.run(command, env=environment,
                                     capture_output=True, text=True,
                                     timeout=180)
            output = process.stdout + process.stderr
            (root / (stage + ".log")).write_text(output)
            self.assertEqual(process.returncode, 0, output)
        self.assertTrue((build / "bin/consumer.elf").is_file())
        # Read the selected target, not every optional compile database entry.
        ninja = shutil.which("ninja", path=environment["PATH"])
        self.assertIsNotNone(ninja, "Prepared Ninja is required")
        commands = subprocess.run(
            [ninja, "-C", str(build), "-t", "commands", "consumer"],
            capture_output=True, text=True, timeout=30, check=True).stdout
        self.assertIn(str(ROOT / "io/src/api/rtc.c"), commands)
        self.assertNotIn(str(ROOT / "soc/stm32/stm32u575/drivers/rtc.c"),
                         commands)

    def test_retained_clock_links_without_a_selected_rtc_provider(self):
        with tempfile.TemporaryDirectory(prefix="nexus retained clock ") as path:
            root = Path(path)
            self.prepare(root)
            self.build(root)


if __name__ == "__main__":
    unittest.main()
