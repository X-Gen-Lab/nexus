"""Provider startup requirements stay private and compile once per image."""

import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]


class ProviderStartupScopeTests(unittest.TestCase):
    def test_images_compile_independent_startup_with_private_usage(self):
        with tempfile.TemporaryDirectory(prefix="Nexus startup scope ") as value:
            source = Path(value)
            build = source / "build"
            private = source / "provider private"
            private.mkdir()
            (private / "vendor.h").write_text("#define VENDOR_VALUE 7\n")
            (source / "startup.c").write_text(
                '#include "vendor.h"\n#include "image.h"\n'
                '#if STARTUP_PRIVATE != 19 || IMAGE_MARKER != IMAGE_VALUE\n'
                '#error startup usage or image scope was lost\n#endif\n'
                'int startup_value(void) { return VENDOR_VALUE + IMAGE_VALUE; }\n')
            (source / "provider.c").write_text("int provider_value(void) { return 0; }\n")
            (source / "bindings.c").write_text("int binding_value(void) { return 0; }\n")
            config = source / "generated"
            config.mkdir()
            (config / "configuration_code_paths.json").write_text(
                json.dumps({"startup.c": str(source / "startup.c")}))
            (config / "memory.ld").write_text("/* Not linked by this compile contract. */\n")
            for name, marker in (("first", 1), ("second", 2)):
                image = source / name
                image.mkdir()
                (image / "image.h").write_text(f"#define IMAGE_VALUE {marker}\n")
                (image / "main.c").write_text(
                    '#ifdef STARTUP_PRIVATE\n#error private startup define leaked\n#endif\n'
                    'int main(void) { return 0; }\n')
                (image / "CMakeLists.txt").write_text(
                    f"add_compile_definitions(IMAGE_MARKER={marker})\n"
                    'include_directories("${CMAKE_CURRENT_SOURCE_DIR}")\n'
                    f"nexus_add_firmware({name} SOURCES main.c)\n")
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.31)\n'
                'project(startup_scope LANGUAGES C)\n'
                'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
                f'set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")\n'
                'add_library(nexus_build_options INTERFACE)\n'
                'add_library(Nexus::Config ALIAS nexus_build_options)\n'
                'set_target_properties(nexus_build_options PROPERTIES '
                'NEXUS_LTO_REQUIRED OFF NEXUS_COMPILER_LAUNCHER "")\n'
                'add_library(nexus_owned_warnings INTERFACE)\n'
                'add_library(nexus_firmware_options INTERFACE)\n'
                'add_library(nexus_arch INTERFACE)\n'
                'add_library(Nexus::Arch ALIAS nexus_arch)\n'
                'add_library(nexus_io INTERFACE)\n'
                'add_library(Nexus::IO ALIAS nexus_io)\n'
                'add_library(selected_os INTERFACE)\n'
                'add_library(Nexus::OS ALIAS selected_os)\n'
                'add_library(startup_usage INTERFACE)\n'
                'target_include_directories(startup_usage INTERFACE '
                '"${CMAKE_CURRENT_SOURCE_DIR}/provider private")\n'
                'target_compile_definitions(startup_usage INTERFACE STARTUP_PRIVATE=19)\n'
                'add_library(provider STATIC provider.c)\n'
                'set_target_properties(provider PROPERTIES '
                'NEXUS_STARTUP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/startup.c" '
                'NEXUS_STARTUP_USAGE_TARGET startup_usage)\n'
                'add_library(nexus_platform STATIC bindings.c)\n'
                'add_library(Nexus::Platform ALIAS nexus_platform)\n'
                'set_target_properties(nexus_platform PROPERTIES '
                'NEXUS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" '
                'NEXUS_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated" '
                'NEXUS_SOC_FAMILY fixture NEXUS_CPU_ARCH cortex-m4 '
                'NEXUS_BACKEND baremetal NEXUS_PROVIDER_TARGET provider)\n'
                f'include("{ROOT.as_posix()}/cmake/platform/Firmware.cmake")\n'
                'add_subdirectory(first)\nadd_subdirectory(second)\n')
            environment = dict(os.environ)
            for key in tuple(environment):
                if key.startswith("GIT_"):
                    environment.pop(key)
            for arguments in (
                    [shutil.which("cmake"), "-S", str(source), "-B", str(build),
                     "-G", "Ninja"],
                    [shutil.which("cmake"), "--build", str(build), "--parallel", "2",
                     "--target", "first_nexus_startup", "second_nexus_startup"]):
                process = subprocess.run(arguments, capture_output=True, text=True,
                                         timeout=90, env=environment)
                self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
            commands = json.loads((build / "compile_commands.json").read_text())
            startups = [entry for entry in commands
                        if Path(entry["file"]).name == "startup.c"]
            applications = [entry for entry in commands
                            if Path(entry["file"]).name == "main.c"]
            self.assertEqual(len(startups), 2)
            self.assertEqual(len(applications), 2)
            self.assertNotEqual(startups[0]["command"], startups[1]["command"])
            # Compile application objects with their actual generated commands;
            # this focused test does not invent an MCU linker or resource model.
            for entry in applications:
                process = subprocess.run(shlex.split(entry["command"]),
                                         cwd=entry["directory"], env=environment,
                                         capture_output=True, text=True, timeout=30)
                self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
            for entry in startups:
                self.assertIn("STARTUP_PRIVATE=19", entry["command"])
                self.assertIn("provider private", entry["command"])
            for entry in applications:
                self.assertNotIn("STARTUP_PRIVATE", entry["command"])
                self.assertNotIn("provider private", entry["command"])


if __name__ == "__main__":
    unittest.main()
