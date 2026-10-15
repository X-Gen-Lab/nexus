"""Exercise target policy and incremental artifacts through real CMake builds."""

from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class BuildSystemTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="Nexus build contract ")
        self.addCleanup(self.temporary.cleanup)
        self.source = Path(self.temporary.name) / "source with spaces"
        self.source.mkdir()
        self.build = Path(self.temporary.name) / "build with spaces"
        self.cmake = shutil.which("cmake")
        self.environment = os.environ.copy()
        self.assertIsNotNone(self.cmake, "CMake is required for build contracts")

    def run_command(self, *arguments, success=True):
        completed = subprocess.run(arguments, capture_output=True, text=True,
                                   env=self.environment,
                                   timeout=120, check=False)
        if success:
            self.assertEqual(completed.returncode, 0,
                             completed.stdout + completed.stderr)
        else:
            self.assertNotEqual(completed.returncode, 0,
                                completed.stdout + completed.stderr)
        return completed

    def fixture(self, *, optimization="Os", extra="", source=None):
        """Separate the Nexus directory from the public helper's call site."""
        platform = self.source / "platform"
        platform.mkdir()
        config = platform / "generated"
        config.mkdir()
        (config / "nexus_config.h").write_text("/* Contract configuration. */\n")
        (config / "resolved.json").write_text('{"fixture": "target policy"}\n')
        (platform / "owned.c").write_text(
            "int nexus_contract_value(void);\n"
            "int nexus_contract_value(void) { return 7; }\n")
        (platform / "CMakeLists.txt").write_text(
            f'set(NEXUS_SOURCE_DIR "{ROOT.as_posix()}")\n'
            'set(NEXUS_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")\n'
            'set(NEXUS_CPU_ARCH native)\nset(NEXUS_ENUM_ABI native-int)\n'
            'set(NEXUS_CPU_COMPILE_OPTIONS "")\n'
            f'set(NEXUS_OPTIMIZATION "{optimization}")\n'
            f'include("{ROOT.as_posix()}/cmake/platform/Options.cmake")\n'
            f'include("{ROOT.as_posix()}/cmake/platform/Firmware.cmake")\n'
            'add_library(nexus_arch INTERFACE)\n'
            'add_library(Nexus::Arch ALIAS nexus_arch)\n'
            'add_library(nexus_os_baremetal INTERFACE)\n'
            'add_library(nexus_platform STATIC owned.c)\n'
            'target_link_libraries(nexus_platform PRIVATE nexus_build_options '
            'nexus_owned_warnings)\n'
            'set_target_properties(nexus_platform PROPERTIES\n'
            ' NEXUS_SOURCE_DIR "${NEXUS_SOURCE_DIR}"\n'
            ' NEXUS_CONFIG_DIR "${NEXUS_CONFIG_DIR}"\n'
            ' NEXUS_SOC_FAMILY native NEXUS_BACKEND baremetal\n'
            ' NEXUS_COMPONENT_TARGETS "")\n'
            'add_library(Nexus::Platform ALIAS nexus_platform)\n')
        (self.source / "main.cpp").write_text(source or (
            'extern "C" int nexus_contract_value(void);\n'
            'int main() { return nexus_contract_value() == 7 ? 0 : 1; }\n'))
        (self.source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(consumer LANGUAGES C CXX)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            'add_subdirectory(platform)\n'
            'nexus_add_firmware(app SOURCES main.cpp)\n' + extra + '\n'
            'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/policy.txt" CONTENT '
            '"$<TARGET_PROPERTY:app,INTERPROCEDURAL_OPTIMIZATION>\\n")\n')

    def configure(self, *, generator="Ninja", arguments=(), success=True):
        return self.run_command(self.cmake, "-S", str(self.source), "-B",
                                str(self.build), "-G", generator, *arguments,
                                success=success)

    def build_application(self, *, success=True):
        return self.run_command(self.cmake, "--build", str(self.build),
                                "--target", "app", "--parallel", "2",
                                success=success)

    def application_compile_command(self):
        commands = json.loads((self.build / "compile_commands.json").read_text())
        return next(entry["command"] for entry in commands
                    if entry["file"].endswith("main.cpp"))

    def test_lto_policy_crosses_public_helper_directory_and_runs_cpp(self):
        self.fixture(optimization="Os-lto")
        self.configure()
        self.assertEqual((self.build / "policy.txt").read_text().strip(), "ON")
        self.assertIn("-flto", self.application_compile_command())
        self.build_application()
        self.run_command(str(self.build / "bin/app"))

    def test_default_product_warnings_are_consumer_owned(self):
        self.fixture(source="int main() { int unused = 3; return 0; }\n")
        self.configure()
        self.assertNotIn("-Werror", self.application_compile_command())
        self.build_application()

    def test_owned_platform_keeps_strict_warnings(self):
        self.fixture()
        (self.source / "platform/owned.c").write_text(
            "int nexus_contract_value(void);\n"
            "int nexus_contract_value(void) { int unused = 2; return 7; }\n")
        self.configure()
        completed = self.build_application(success=False)
        self.assertIn("-Werror", completed.stdout + completed.stderr)
        self.assertIn("unused", completed.stdout + completed.stderr)

    def test_repository_workload_keeps_strict_owned_warning_policy(self):
        self.run_command(self.cmake, "-S", str(ROOT), "-B", str(self.build),
                         "-G", "Ninja", "-DNEXUS_BUILD_TESTS=OFF",
                         "-DNEXUS_BUILD_WORKLOAD=ON",
                         f"-DNEXUS_ASSEMBLY_FILE={ROOT}/tools/configure/"
                         "assemblies/native-empty.toml")
        commands = json.loads((self.build / "compile_commands.json").read_text())
        command = next(entry["command"] for entry in commands
                       if entry["file"] == str(ROOT / "tools/measurement/workload.c"))
        self.assertIn("-Werror", command)
        self.assertIn("-Wconversion", command)

    def test_product_can_explicitly_enable_owned_strict_warnings(self):
        self.fixture(extra="nexus_target_warnings(app STRICT)",
                     source="int main() { int unused = 3; return 0; }\n")
        self.configure()
        self.assertIn("-Werror", self.application_compile_command())
        completed = self.build_application(success=False)
        self.assertIn("unused", completed.stdout + completed.stderr)

    def test_og_profile_removes_conflicting_build_type_optimization(self):
        self.fixture(optimization="Og")
        self.configure()
        command = self.application_compile_command()
        self.assertIn("-Og", command)
        self.assertNotIn("-Os", command)
        self.build_application()

    def test_unknown_firmware_argument_is_reported(self):
        self.fixture(extra="nexus_add_firmware(other TYPO value SOURCES main.cpp)")
        result = self.configure(success=False)
        self.assertIn("unknown arguments", result.stdout + result.stderr)

    def test_missing_firmware_keyword_value_is_reported(self):
        self.fixture(extra="nexus_add_firmware(other SOURCES main.cpp LIBRARIES)")
        result = self.configure(success=False)
        self.assertIn("missing values", result.stdout + result.stderr)

    def test_repeated_sources_keyword_is_rejected(self):
        self.fixture(extra=(
            "nexus_add_firmware(other SOURCES main.cpp SOURCES main.cpp)"))
        result = self.configure(success=False)
        self.assertIn("repeated SOURCES", result.stdout + result.stderr)

    def test_repeated_libraries_keyword_is_rejected(self):
        self.fixture(extra=("nexus_add_firmware(other SOURCES main.cpp "
                            "LIBRARIES Nexus::Arch LIBRARIES Nexus::Arch)"))
        result = self.configure(success=False)
        self.assertIn("repeated LIBRARIES", result.stdout + result.stderr)

    def test_duplicate_firmware_target_is_reported(self):
        self.fixture(extra="nexus_add_firmware(app SOURCES main.cpp)")
        result = self.configure(success=False)
        self.assertIn("already exists", result.stdout + result.stderr)

    def test_missing_platform_metadata_is_reported_before_linking(self):
        self.fixture(extra=(
            "set_property(TARGET nexus_platform PROPERTY NEXUS_BACKEND \"\")\n"
            "nexus_add_firmware(other SOURCES main.cpp)"))
        result = self.configure(success=False)
        self.assertIn("Nexus::Platform is missing NEXUS_BACKEND",
                      result.stdout + result.stderr)

    def test_no_lto_profile_overrides_consumer_global_ipo_default(self):
        self.fixture(extra="set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)\n"
                     "nexus_add_firmware(other SOURCES main.cpp)\n"
                     "get_target_property(policy other INTERPROCEDURAL_OPTIMIZATION)\n"
                     "if(policy)\nmessage(FATAL_ERROR \"Leaked consumer IPO\")\nendif()")
        self.configure()
        self.assertEqual((self.build / "policy.txt").read_text().strip(), "OFF")

    def test_optional_cache_launcher_crosses_directories_with_strict_environment(self):
        self.fixture()
        tools = self.source / "mock tools"
        tools.mkdir()
        launcher = tools / "ccache"
        log = self.source / "launcher.jsonl"
        launcher.write_text(
            '#!/usr/bin/env python3\n'
            'import json, os, subprocess, sys\n'
            'keys = ("CCACHE_NAMESPACE", "CCACHE_COMPILERCHECK", '
            '"CCACHE_SLOPPINESS", "CCACHE_HASHDIR")\n'
            'values = {key: os.environ.get(key) for key in keys}\n'
            'assert len(values["CCACHE_NAMESPACE"]) == 64\n'
            'assert values["CCACHE_COMPILERCHECK"] == "content"\n'
            'assert values["CCACHE_SLOPPINESS"] == ""\n'
            'assert values["CCACHE_HASHDIR"] == "true"\n'
            f'with open({str(log)!r}, "a") as stream:\n'
            '    stream.write(json.dumps({"argv": sys.argv[1:], '
            '"environment": values}) + "\\n")\n'
            'raise SystemExit(subprocess.run(sys.argv[1:]).returncode)\n')
        launcher.chmod(0o755)
        self.environment["PATH"] = str(tools) + os.pathsep + self.environment["PATH"]
        self.configure(arguments=("-DNEXUS_COMPILER_CACHE=ON",))
        self.build_application()
        invocations = [json.loads(line) for line in log.read_text().splitlines()]
        self.assertTrue(any("main.cpp" in " ".join(item["argv"])
                            for item in invocations))
        self.assertTrue(any("owned.c" in " ".join(item["argv"])
                            for item in invocations))

    def test_firmware_requires_platform_before_reading_properties(self):
        (self.source / "main.c").write_text("int main(void) { return 0; }\n")
        (self.source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\nproject(consumer C)\n'
            f'include("{ROOT.as_posix()}/cmake/platform/Firmware.cmake")\n'
            'nexus_add_firmware(app SOURCES main.c)\n')
        result = self.configure(success=False)
        self.assertIn("requires a configured Nexus::Platform",
                      result.stdout + result.stderr)

    def test_second_platform_is_rejected_before_duplicate_targets(self):
        self.fixture(extra="add_subdirectory(platform another-platform)")
        result = self.configure(success=False)
        self.assertIn("One build may contain only one resolved Nexus assembly",
                      result.stdout + result.stderr)

    def test_runtime_cannot_be_mixed_into_platform_graph(self):
        self.fixture(extra=(
            f'include("{ROOT.as_posix()}/cmake/platform/Runtime.cmake")\n'
            'nexus_add_runtime(ASSEMBLY "missing-input.toml")'))
        result = self.configure(success=False)
        self.assertIn("One build may contain only one resolved Nexus assembly",
                      result.stdout + result.stderr)

    def invalid_sdk_fixture(self, override):
        sdk = self.source / "invalid sdk"
        (sdk / "cmake/package").mkdir(parents=True)
        (sdk / ".nexus-source-sdk.json").write_text("{}\n")
        shutil.copyfile(ROOT / "cmake/package/package_source_sdk.py",
                        sdk / "cmake/package/package_source_sdk.py")
        (self.source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\nproject(consumer C)\n'
            'find_package(Python3 REQUIRED COMPONENTS Interpreter)\n'
            f'set(NEXUS_SOURCE_DIR "{sdk.as_posix()}")\n' + override + '\n'
            f'include("{ROOT.as_posix()}/cmake/platform/SDK.cmake")\n')

    def test_normal_variable_cannot_skip_source_sdk_verification(self):
        self.invalid_sdk_fixture(
            'set(_nexus_verified_source_for_current_call "${NEXUS_SOURCE_DIR}")')
        result = self.configure(success=False)
        self.assertIn("Source SDK identity failed", result.stdout + result.stderr)

    def test_cache_variable_cannot_skip_source_sdk_verification(self):
        self.invalid_sdk_fixture(
            'set(_nexus_verified_source_for_current_call '
            '"${NEXUS_SOURCE_DIR}" CACHE STRING "")')
        result = self.configure(success=False)
        self.assertIn("Source SDK", result.stdout + result.stderr)

    def test_direct_sdk_entry_verifies_bytes_before_configuration_modules(self):
        sdk = self.source / "invalid sdk"
        (sdk / "cmake/package").mkdir(parents=True)
        (sdk / "cmake/platform").mkdir(parents=True)
        (sdk / ".nexus-source-sdk.json").write_text("{}\n")
        for name in ("CMakeLists.txt", "cmake/platform/SDK.cmake",
                     "cmake/platform/TargetPolicy.cmake",
                     "cmake/package/package_source_sdk.py"):
            shutil.copyfile(ROOT / name, sdk / name)
        (self.source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\nproject(consumer C)\n'
            f'set(NEXUS_ASSEMBLY_FILE "{self.source.as_posix()}/missing.toml")\n'
            f'add_subdirectory("{sdk.as_posix()}" nexus-sdk)\n')
        result = self.configure(success=False)
        self.assertIn("Source SDK identity failed", result.stdout + result.stderr)

    def test_unqualified_multiconfiguration_generator_is_rejected(self):
        self.fixture()
        result = self.configure(generator="Ninja Multi-Config", success=False)
        self.assertIn("single-configuration Ninja", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
