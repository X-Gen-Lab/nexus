"""Relocate the real source SDK and compile/run independent public consumers."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class SourceSDKTests(unittest.TestCase):
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
        return subprocess.run(arguments, text=True, capture_output=True, timeout=180, **kwargs)

    @classmethod
    def run_ok(cls, arguments, **kwargs):
        result = cls.run_process(arguments, **kwargs)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        return result

    def verify(self):
        return self.run_process([sys.executable, "-B", str(self.sdk / "cmake/package/package_source_sdk.py"),
                         "--verify", str(self.sdk)])

    def consumer(self, name, *, fixture=True, arm=False, expected_revision=None, version="0.1.0", contracts=False):
        source = self.directory / (name + " source")
        build = self.directory / (name + " build")
        source.mkdir()
        # A parent Git identity must never become the SDK source identity.
        self.run_ok(["git", "init", "--quiet", str(source)])
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.21)\n'
            'project(installed_source_consumer LANGUAGES C CXX ASM)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            f'find_package(Nexus {version} EXACT CONFIG REQUIRED)\n'
            'find_package(Nexus CONFIG REQUIRED)\n'
            'add_executable(parent_plain parent.c)\n'
            'nexus_add_application(TARGET installed_c SOURCES main.c)\n'
            'nexus_add_application(TARGET installed_cpp SOURCES main.cpp)\n'
            'get_target_property(sdk_root Nexus::Config NEXUS_SOURCE_DIR)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/resolved-sdk.txt" "${sdk_root}")\n')
        (source / "parent.c").write_text(
            '#ifdef NEXUS_EFFECTIVE_CONFIG\n#error SDK leaked into unrelated parent target\n#endif\n'
            'int main(void) { return 0; }\n')
        if arm:
            code = '#include "runtime/nx_runtime.h"\nint main(void) { return nx_runtime_bootstrap(0) != NX_OK; }\n'
        else:
            code = ('#include "runtime/nx_runtime.h"\n#include <string.h>\n'
                    '#ifndef NX_CONFIG_PLATFORM_NATIVE\n#error Wrong resolved platform\n#endif\n'
                    'int main(void) {\n'
                    f' if (strncmp(NEXUS_SOURCE_REVISION, "{self.identity["source_revision"]}", 40)) return 1;\n'
                    ' if (strcmp(nx_platform_get_info()->board, "native-reference")) return 2;\n'
                    ' if (nx_runtime_bootstrap(0) != NX_OK) return 3;\n'
                    ' return nx_runtime_shutdown(0) != NX_OK;\n}\n')
        (source / "main.c").write_text(code)
        (source / "main.cpp").write_text(code)
        arguments = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                     "-DCMAKE_BUILD_TYPE=Release", f"-DNexus_DIR={self.prefix / 'lib/cmake/Nexus'}",
                     "-DNEXUS_BUILD_TESTS=OFF"]
        if fixture:
            arguments.append("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON")
        if contracts:
            arguments.append("-DNEXUS_BUILD_CONTRACTS=ON")
        if expected_revision:
            arguments.append("-DNEXUS_EXPECTED_SOURCE_REVISION=" + expected_revision)
        if arm:
            arguments.extend(["-DNEXUS_PLATFORM=stm32",
                              f"-DCMAKE_TOOLCHAIN_FILE={self.sdk / 'cmake/toolchains/arm-gcc.cmake'}",
                              f"-DNEXUS_CONFIG_FILE={self.sdk / 'configs/stm32f407_baremetal_defconfig'}"])
        else:
            arguments.append(f"-DNEXUS_CONFIG_FILE={self.sdk / 'configs/native_minimal_defconfig'}")
        return source, build, self.run_process(arguments)

    def test_relocated_c_and_cpp_consumer_build_and_run_without_original_source(self):
        source, build, result = self.consumer("native")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((build / "resolved-sdk.txt").read_text(), str(self.sdk))
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4"])
        for application in ("installed_c", "installed_cpp"):
            self.run_ok([str(build / "nexus-sdk/bin" / application)])
        self.run_ok([str(build / "parent_plain")])
        compile_commands = json.loads((build / "compile_commands.json").read_text())
        self.assertGreater(len(compile_commands), 0)
        for entry in compile_commands:
            self.assertNotIn(str(ROOT) + "/", entry.get("command", ""))

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_arm_consumer_compiles_real_startup_and_linker(self):
        _, build, result = self.consumer("arm", arm=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4", "--target", "installed_c", "installed_cpp"])
        self.run_ok([sys.executable, "-B", str(self.sdk / "scripts/ci/validate_firmware_elf.py"),
                     "--build-dir", str(build / "nexus-sdk"),
                     "--report", str(build / "firmware-static-contract.json")])
        for application in ("installed_c", "installed_cpp"):
            self.assertGreater((build / "nexus-sdk/bin" / (application + ".elf")).stat().st_size, 0)

    def test_fixture_requires_explicit_consumer_opt_in(self):
        self.assertFalse(self.identity["publishable"])
        _, _, result = self.consumer("fixture rejection", fixture=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("nonpublishable development fixture", result.stdout + result.stderr)

    def test_expected_revision_conflict_is_rejected(self):
        _, _, result = self.consumer("revision rejection", expected_revision="0" * 40)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("expected revision", " ".join((result.stdout + result.stderr).split()))

    def test_different_source_package_version_is_rejected(self):
        _, _, result = self.consumer("version rejection", version="99.0.0")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("version", result.stdout + result.stderr)

    def test_development_contracts_are_explicitly_rejected(self):
        _, _, result = self.consumer("contract rejection", contracts=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("excludes development tests/contracts", " ".join((result.stdout + result.stderr).split()))

    def test_modified_required_file_is_rejected(self):
        target = self.sdk / "runtime/src/nx_runtime.c"
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
        self.run_ok(["git", "-C", str(repo), "add", "README.md"])
        self.run_ok(["git", "-C", str(repo), "-c", "user.name=SDK test", "-c", "user.email=sdk-test@local.invalid",
                     "commit", "--quiet", "-m", "fixture baseline"])
        result = self.run_process([sys.executable, "-B", str(ROOT / "cmake/package/package_source_sdk.py"),
                                  "--source", str(repo), "--output", str(self.directory / "must not export empty SDK")])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("required dependency Gitlink", result.stderr)
        self.assertFalse((self.directory / "must not export empty SDK").exists())


if __name__ == "__main__":
    unittest.main()
