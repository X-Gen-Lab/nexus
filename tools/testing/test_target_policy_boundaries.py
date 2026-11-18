"""Compile real consumers with separate ABI, target and firmware policies."""

import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class TargetPolicyBoundaryTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="nexus policy boundary ")
        self.addCleanup(temporary.cleanup)
        self.source = Path(temporary.name)
        self.build = self.source / "build"
        self.environment = dict(os.environ)
        self.cmake = shutil.which("cmake")
        self.assertIsNotNone(self.cmake, "Prepared CMake is required")
        (self.source / "owned").mkdir()
        (self.source / "owned/value.c").write_text(
            "int policy_value(void);\n"
            "int policy_value(void) { return 7; }\n", encoding="utf-8")
        (self.source / "main.c").write_text(
            "int policy_value(void);\n"
            "int main(void) { return policy_value() == 7 ? 0 : 1; }\n",
            encoding="utf-8")

    def command(self, *arguments):
        completed = subprocess.run(
            arguments, text=True, capture_output=True, check=False,
            timeout=90, env=self.environment)
        self.assertEqual(completed.returncode, 0,
                         completed.stdout + completed.stderr)
        return completed.stdout

    def fixture(self, *, arm=False, optimization="Os", parent="", owned=""):
        cpu = ("-mcpu=cortex-m4;-mthumb;-mfpu=fpv4-sp-d16;"
               "-mfloat-abi=hard;-fshort-enums") if arm else ""
        (self.source / "owned/CMakeLists.txt").write_text(
            f'set(NEXUS_SOURCE_DIR "{ROOT.as_posix()}")\n'
            'set(NEXUS_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")\n'
            f'set(NEXUS_CPU_ARCH {"cortex-m4" if arm else "native"})\n'
            f'set(NEXUS_ENUM_ABI {"short-enums" if arm else "native-int"})\n'
            f'set(NEXUS_CPU_COMPILE_OPTIONS "{cpu}")\n'
            f'set(NEXUS_OPTIMIZATION "{optimization}")\n'
            f'include("{ROOT.as_posix()}/cmake/platform/Options.cmake")\n'
            'add_library(owned STATIC value.c)\n'
            'target_link_libraries(owned PRIVATE nexus_build_options)\n'
            'nexus_apply_target_policy(owned)\n' + owned, encoding="utf-8")
        (self.source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(policy_boundary C)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            'add_subdirectory(owned)\n' + parent, encoding="utf-8")
        arguments = []
        if arm:
            self.assertIsNotNone(shutil.which("arm-none-eabi-gcc"),
                                 "Prepared ARM compiler is required")
            arguments.append(
                f"-DCMAKE_TOOLCHAIN_FILE={ROOT}/cmake/toolchains/arm-gcc.cmake")
        self.command(self.cmake, "-S", str(self.source), "-B", str(self.build),
                     "-G", "Ninja", *arguments)

    def ninja_commands(self, target):
        ninja = shutil.which("ninja")
        self.assertIsNotNone(ninja, "Prepared Ninja is required")
        return self.command(ninja, "-C", str(self.build), "-t", "commands", target)

    def test_abi_only_library_has_cpu_contract_without_configuration_policy(self):
        self.fixture(arm=True, parent=(
            'add_library(abi_only STATIC owned/value.c)\n'
            'target_link_libraries(abi_only PRIVATE Nexus::ABI)\n'))
        self.command(self.cmake, "--build", str(self.build), "--target",
                     "abi_only", "--parallel", "1")
        database = json.loads((self.build / "compile_commands.json").read_text())
        command = next(entry["command"] for entry in database
                       if "abi_only.dir" in entry["command"])
        arguments = shlex.split(command)
        for flag in ("-mcpu=cortex-m4", "-mfloat-abi=hard", "-fshort-enums"):
            self.assertIn(flag, arguments)
        self.assertNotIn("-Os", arguments)
        self.assertNotIn("-Werror", arguments)
        self.assertFalse(any("generated" in argument for argument in arguments))

    def test_runtime_specs_belong_only_to_explicit_firmware(self):
        self.fixture(arm=True, parent=(
            'add_executable(ordinary main.c)\n'
            'target_link_libraries(ordinary PRIVATE owned nexus_build_options)\n'
            'add_executable(image main.c)\n'
            'target_link_libraries(image PRIVATE owned)\n'
            'nexus_apply_firmware_policy(image)\n'))
        ordinary = self.ninja_commands("ordinary")
        firmware = self.ninja_commands("image")
        for flag in ("--specs=nano.specs", "--specs=nosys.specs",
                     "-nostartfiles", "-Wl,--gc-sections"):
            self.assertNotIn(flag, ordinary)
            self.assertIn(flag, firmware)
        self.assertIn("-mfloat-abi=hard", firmware)
        self.assertIn("-std=c11", firmware)

    def test_owned_ipo_and_launcher_do_not_change_sibling_targets(self):
        launcher = self.source / "launcher.py"
        log = self.source / "launcher.jsonl"
        launcher.write_text(
            'import json, subprocess, sys\n'
            f'with open({str(log)!r}, "a", encoding="utf-8") as stream:\n'
            '    stream.write(json.dumps(sys.argv[1:]) + "\\n")\n'
            'raise SystemExit(subprocess.run(sys.argv[1:]).returncode)\n',
            encoding="utf-8")
        (self.source / "plain.c").write_text(
            "int main(void) { return 0; }\n", encoding="utf-8")
        self.fixture(optimization="Os-lto", owned=(
            f'set_property(TARGET nexus_build_options PROPERTY '
            f'NEXUS_COMPILER_LAUNCHER "{sys.executable};{launcher}")\n'
            'nexus_apply_target_policy(owned)\n'), parent=(
            'add_executable(image main.c)\n'
            'target_link_libraries(image PRIVATE owned)\n'
            'nexus_apply_firmware_policy(image)\n'
            'add_executable(sibling plain.c)\n'))
        built = self.command(self.cmake, "--build", str(self.build), "--target",
                             "image", "sibling", "--parallel", "1", "--verbose")
        database = json.loads((self.build / "compile_commands.json").read_text())
        for name in ("main.c", "value.c"):
            command = next(entry["command"] for entry in database
                           if Path(entry["file"]).name == name)
            self.assertIn("-flto", command)
            self.assertIn("-std=c11", command)
        sibling = next(entry["command"] for entry in database
                       if Path(entry["file"]).name == "plain.c")
        self.assertNotIn("-flto", sibling)
        self.assertNotIn("-Os", sibling)
        invocations = [json.loads(line) for line in log.read_text().splitlines()]
        self.assertEqual(len(invocations), 2)
        self.assertFalse(any("plain.c" in argument
                             for call in invocations for argument in call))
        image_link = next(line for line in built.splitlines()
                          if " -o image " in line)
        self.assertIn("-flto", image_link)
        for name in ("image", "sibling"):
            self.command(str(self.build / name))


if __name__ == "__main__":
    unittest.main()
