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
sys.path.insert(0, str(ROOT / "cmake/package"))
import verify_consumers as consumer_gate
import package_source_sdk as source_sdk


class SourceSDKSelectionTests(unittest.TestCase):
    def test_test_framework_payload_is_not_a_firmware_sdk_dependency(self):
        for name in ("dependencies/source/googletest-1.16.0.tar.gz",
                     "dependencies/source/googletest-LICENSE.txt",
                     "tests/contracts/factory_test.cpp"):
            with self.subTest(name=name):
                self.assertFalse(source_sdk.selected(name))
        self.assertTrue(source_sdk.selected("dependencies/googletest.lock.json"))
        self.assertTrue(source_sdk.selected("io/include/nexus/io/factory.h"))
        self.assertTrue(source_sdk.selected("tools/configure/assemblies/native.toml"))

    def test_consumer_uses_authored_toml_for_each_maintained_family(self):
        with tempfile.TemporaryDirectory(prefix="SDK consumer inputs ") as value:
            directory = Path(value)
            for name, arm, chip in (("native", False, "native"),
                                    ("stm32", True, "stm32"),
                                    ("gd32", True, "gd32")):
                _, _, command = consumer_gate.prepare_consumer(
                    directory / "prefix", directory, name,
                    lambda arguments: None, arm=arm, chip=chip)
                assembly = next(argument for argument in command
                                if argument.startswith("-DNEXUS_ASSEMBLY_FILE="))
                self.assertTrue(assembly.endswith(".toml"), assembly)


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
        environment = os.environ.copy()
        for key in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                    "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH", "LIBRARY_PATH"):
            environment.pop(key, None)
        kwargs.setdefault("env", environment)
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
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4"])
        for application in ("installed_c", "installed_cpp"):
            self.run_ok([str(build / "bin" / application)])
        self.run_ok([str(build / "parent_plain")])
        compile_commands = json.loads((build / "compile_commands.json").read_text())
        self.assertGreater(len(compile_commands), 0)
        for entry in compile_commands:
            self.assertNotIn(str(ROOT) + "/", entry.get("command", ""))

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_arm_consumer_compiles_real_startup_and_linker(self):
        _, build, result = self.consumer("arm", arm=True, fixture=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4", "--target", "installed_c", "installed_cpp"])
        for application in ("installed_c", "installed_cpp"):
            self.assertGreater((build / "bin" / (application + ".elf")).stat().st_size, 0)
            resources = json.loads((build / "bin" / (application + ".resources.json")).read_text())
            self.assertEqual(resources["status"], "passed")
        self.check_private_includes(build)

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_gd32_consumer_compiles_real_startup_and_linker(self):
        _, build, result = self.consumer("gd32 arm", arm=True, chip="gd32", fixture=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4", "--target", "installed_c", "installed_cpp"])
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
            self.assertNotIn("/vendors/", command)
            self.assertNotIn("/soc/", command)
            self.assertNotIn(str(ROOT) + "/", command)
            self.assertNotIn("-DSTM32F407", command)
            self.assertNotIn("-DGD32F470", command)

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
