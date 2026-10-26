"""Execute the host-only CMSIS register model boundary with real compilers."""

import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "tests/cmake/CMSISHostModel.cmake"
SHIM = ROOT / "tests/cmsis_host"
CORE_MODULE = ROOT / "cmake/platform/CMSISCore.cmake"
CORE = ROOT / "vendors/arm/CMSIS_6/CMSIS/Core/Include"
DEVICE = ROOT / "vendors/st/cmsis_device_f4/Include"


class CMSISHostModelTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="CMSIS host model ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.case = 0

    def compile(self, source, *, definitions=(), arm=False, cxx=False):
        self.case += 1
        path = self.root / f"probe{self.case}.{'cpp' if cxx else 'c'}"
        path.write_text(source)
        name = ("arm-none-eabi-gcc" if arm else ("c++" if cxx else "cc"))
        compiler = shutil.which(name)
        self.assertIsNotNone(compiler, f"Required compiler is unavailable: {name}")
        command = [compiler, "-std=c++17" if cxx else "-std=c11", "-Werror",
                   "-I", str(SHIM), "-isystem", str(CORE), "-isystem",
                   str(DEVICE), "-DSTM32F407xx", "-DCMSIS_DISABLE_DEPRECATED=1"]
        if arm:
            command.extend(["-mcpu=cortex-m4", "-mthumb"])
        command.extend(f"-D{definition}" for definition in definitions)
        command.extend(["-c", str(path), "-o", str(path.with_suffix(".o"))])
        return subprocess.run(command, capture_output=True, text=True, timeout=30)

    def test_real_core_register_layout_compiles_for_host_c_and_cpp(self):
        for cxx in (False, True):
            with self.subTest(cxx=cxx):
                assertion = "static_assert" if cxx else "_Static_assert"
                result = self.compile(
                    '#include <stddef.h>\n#include <stm32f4xx.h>\n'
                    f'{assertion}(offsetof(NVIC_Type, IPR) == 0x300, "NVIC");\n'
                    f'{assertion}(offsetof(DCB_Type, DEMCR) == 12, "DCB");\n'
                    'int register_model(void) { return sizeof(NVIC_Type); }\n',
                    definitions=("NEXUS_CMSIS_HOST_MODEL=1",), cxx=cxx)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)

    def test_implicit_or_invalid_host_marker_rejects(self):
        for definitions in ((), ("NEXUS_CMSIS_HOST_MODEL=0",)):
            with self.subTest(definitions=definitions):
                result = self.compile('#include <arm_acle.h>\n',
                                      definitions=definitions)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("explicit host register model", result.stderr)

    def test_arm_compiler_rejects_host_shim_even_with_marker(self):
        result = self.compile('#include <arm_acle.h>\n', arm=True,
                              definitions=("NEXUS_CMSIS_HOST_MODEL=1",))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must not enter an Arm build", result.stderr)

    def test_host_cpu_instruction_remains_unimplemented(self):
        result = self.compile(
            '#include <stm32f4xx.h>\n'
            'void instruction(void) { __disable_irq(); }\n',
            definitions=("NEXUS_CMSIS_HOST_MODEL=1",))
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("No such file or directory", result.stderr)
        self.assertIn("cpsid", result.stderr)

    def configure(self, body):
        self.case += 1
        directory = self.root / f"cmake{self.case}"
        directory.mkdir()
        (directory / "model.c").write_text(
            '#include <stm32f4xx.h>\n'
            'int model(void) { return sizeof(NVIC_Type); }\n')
        (directory / "native.c").write_text(
            '#ifdef NEXUS_CMSIS_HOST_MODEL\n'
            '#error "Host model policy escaped its selected target"\n#endif\n'
            '#if __has_include(<arm_acle.h>)\n'
            '#error "Host model shim escaped its selected target"\n#endif\n'
            'int model(void);\nint main(void) { return model() == 0; }\n')
        (directory / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(cmsis_host_model C)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            f'include("{MODULE}")\n'
            f'include("{CORE_MODULE}")\n' + body)
        result = subprocess.run([
            shutil.which("cmake") or "cmake", "-S", str(directory), "-B",
            str(directory / "build"), "-G", "Ninja"], capture_output=True,
            text=True, timeout=30)
        return directory, result

    def test_private_policy_selects_only_final_core_model_targets(self):
        directory, result = self.configure(
            'add_library(model STATIC model.c)\n'
            'nexus_use_cmsis_host_model(model)\n'
            'nexus_use_cmsis_core(model)\n'
            f'target_include_directories(model PRIVATE "{DEVICE}")\n'
            'target_compile_definitions(model PRIVATE STM32F407xx)\n'
            'add_executable(native native.c)\n'
            'nexus_use_cmsis_host_model(native)\n'
            'target_link_libraries(native PRIVATE model)\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run([
            shutil.which("cmake") or "cmake", "--build",
            str(directory / "build"), "--parallel", "1"],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        commands = json.loads(
            (directory / "build/compile_commands.json").read_text())
        for row in commands:
            selected = Path(row["file"]).name == "model.c"
            self.assertEqual(str(SHIM) in row["command"], selected)
            self.assertEqual("-DNEXUS_CMSIS_HOST_MODEL=1" in row["command"],
                             selected)

    def test_cross_compilation_cannot_enroll_in_host_policy(self):
        _, result = self.configure(
            'add_library(model STATIC model.c)\n'
            'set(CMAKE_CROSSCOMPILING TRUE)\n'
            'nexus_use_cmsis_host_model(model)\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("host-only concrete target", result.stdout + result.stderr)

    def test_actual_native_fixture_contexts_keep_core_and_shim_private(self):
        build = self.root / "actual Native models"
        result = subprocess.run([
            shutil.which("cmake") or "cmake", "--preset", "native-debug",
            "-B", str(build)], cwd=ROOT, capture_output=True, text=True,
            timeout=90)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        commands = json.loads((build / "compile_commands.json").read_text())
        selected = 0
        native = 0
        for row in commands:
            arguments = shlex.split(row["command"])
            includes = set()
            for index, argument in enumerate(arguments):
                if argument in ("-I", "-isystem"):
                    includes.add(arguments[index + 1])
                elif argument.startswith("-I"):
                    includes.add(argument[2:])
                elif argument.startswith("-isystem"):
                    includes.add(argument[len("-isystem"):])
            with self.subTest(source=row["file"], command=row["command"]):
                if str(CORE) in includes:
                    selected += 1
                    self.assertIn("-DCMSIS_DISABLE_DEPRECATED=1", arguments)
                    self.assertIn("-DNEXUS_CMSIS_HOST_MODEL=1", arguments)
                    self.assertIn(str(SHIM), includes)
                else:
                    native += 1
                    self.assertNotIn("-DNEXUS_CMSIS_HOST_MODEL=1", arguments)
                    self.assertNotIn(str(SHIM), includes)
        self.assertGreater(selected, 0)
        self.assertGreater(native, 0)


if __name__ == "__main__":
    unittest.main()
