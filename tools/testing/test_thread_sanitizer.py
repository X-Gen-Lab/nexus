"""Native race instrumentation must be explicit and incompatible with ASan."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class ThreadSanitizerConfigurationTests(unittest.TestCase):
    def configure(self, directory, extra):
        source = Path(directory) / "source"
        build = Path(directory) / "build"
        source.mkdir()
        (source / "main.c").write_text("int main(void) { return 0; }\n")
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(instrumentation_contract C)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            'set(NEXUS_CPU_ARCH native)\n'
            'set(NEXUS_ENUM_ABI native-int)\n'
            'set(NEXUS_CPU_COMPILE_OPTIONS "")\n'
            'set(NEXUS_OPTIMIZATION O2)\n'
            f'include("{ROOT / "cmake/platform/Options.cmake"}")\n'
            'add_executable(probe main.c)\n'
            'target_link_libraries(probe PRIVATE nexus_build_options)\n')
        result = subprocess.run(
            [shutil.which("cmake") or "cmake", "-S", str(source), "-B",
             str(build), "-G", "Ninja", *extra], text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
        return result, build

    def test_native_thread_instrumentation_is_in_actual_compile_command(self):
        with tempfile.TemporaryDirectory() as directory:
            result, build = self.configure(
                directory, ["-DNEXUS_ENABLE_THREAD_SANITIZER=ON"])
            self.assertEqual(result.returncode, 0, result.stdout)
            commands = json.loads((build / "compile_commands.json").read_text())
            self.assertIn("-fsanitize=thread", commands[0]["command"])
            self.assertIn("-fno-omit-frame-pointer", commands[0]["command"])
            self.assertNotIn("-fsanitize=address", commands[0]["command"])

    def test_incompatible_sanitizers_fail_before_build(self):
        with tempfile.TemporaryDirectory() as directory:
            result, _ = self.configure(
                directory, ["-DNEXUS_ENABLE_THREAD_SANITIZER=ON",
                            "-DNEXUS_ENABLE_SANITIZERS=ON"])
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("mutually exclusive", result.stdout)

    def test_native_tsan_preset_uses_separate_build_root(self):
        presets = json.loads((ROOT / "CMakePresets.json").read_text())
        configured = {row["name"]: row
                      for row in presets["configurePresets"]}
        self.assertIn("native-tsan", configured)
        tsan = configured["native-tsan"]
        self.assertEqual(tsan["binaryDir"], "${sourceDir}/build/native-tsan")
        self.assertEqual(tsan["cacheVariables"][
            "NEXUS_ENABLE_THREAD_SANITIZER"], "ON")
        self.assertNotIn("NEXUS_ENABLE_SANITIZERS", tsan["cacheVariables"])
        for kind in ("buildPresets", "testPresets"):
            self.assertEqual(next(row["configurePreset"]
                                  for row in presets[kind]
                                  if row["name"] == "native-tsan"),
                             "native-tsan")


if __name__ == "__main__":
    unittest.main()
