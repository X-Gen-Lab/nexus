"""Every source-consuming CMake entry rechecks actual SDK bytes."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]


class SDKVerificationCallTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.prefix = self.root / "prefix"
        self.source = self.prefix / "share/nexus/src"
        self.package = self.prefix / "lib/cmake/Nexus"
        self.package.mkdir(parents=True)
        (self.source / "cmake/package").mkdir(parents=True)
        (self.source / "cmake/platform").mkdir(parents=True)
        (self.source / ".nexus-source-sdk.json").write_text(json.dumps(
            {"source_revision": "fixture", "publishable": True}) + "\n")
        (self.source / "payload").write_text("approved")
        (self.source / "cmake/package/package_source_sdk.py").write_text(
            "import json,sys\nfrom pathlib import Path\n"
            "root=Path(sys.argv[2])\n"
            "count=root.parent/'count'\n"
            "count.write_text(str(int(count.read_text())+1) if count.exists() else '1')\n"
            "if (root/'payload').read_text() != 'approved':\n"
            "    sys.exit('actual payload changed')\n"
            "print(json.dumps({'source_revision':'fixture','publishable':True}))\n")
        shutil.copyfile(ROOT / "cmake/platform/SDK.cmake",
                        self.source / "cmake/platform/SDK.cmake")
        (self.source / "CMakeLists.txt").write_text(
            'set(NEXUS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")\n'
            'include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform/SDK.cmake")\n'
            'add_library(nexus_config INTERFACE)\n'
            'add_library(Nexus::Config ALIAS nexus_config)\n'
            'set_target_properties(nexus_config PROPERTIES NEXUS_SOURCE_DIR '
            '"${CMAKE_CURRENT_SOURCE_DIR}")\n')
        template = (ROOT / "cmake/package/NexusConfig.cmake.in").read_text()
        (self.package / "NexusConfig.cmake").write_text(
            template.replace("@NEXUS_SDK_VERSION@", "1.0.0"))

    def tearDown(self):
        self.temporary.cleanup()

    def configure(self, extra=""):
        (self.root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(verify_call NONE)\n'
            f'set(Python3_EXECUTABLE "{sys.executable}")\n'
            f'set(Nexus_DIR "{self.package}")\n'
            'find_package(Nexus CONFIG REQUIRED)\n' + extra)
        return subprocess.run([shutil.which("cmake"), "-S", str(self.root),
                               "-B", str(self.root / "build"), "-G", "Ninja"],
                              capture_output=True, text=True, timeout=30,
                              check=False)

    def test_package_and_provider_entries_each_verify(self):
        result = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.source.parent / "count").read_text(), "2")

    def test_repeated_find_and_later_sdk_include_each_verify_again(self):
        result = self.configure(
            'find_package(Nexus CONFIG REQUIRED)\n'
            f'set(NEXUS_SOURCE_DIR "{self.source}")\n'
            f'include("{self.source}/cmake/platform/SDK.cmake")\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.source.parent / "count").read_text(), "4")

    def test_modified_payload_rejected_by_next_find_even_same_manifest(self):
        result = self.configure(
            f'file(WRITE "{self.source}/payload" "changed")\n'
            'find_package(Nexus CONFIG REQUIRED)\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("actual payload changed", result.stdout + result.stderr)
        self.assertEqual((self.source.parent / "count").read_text(), "3")


class SDKConsumerParallelTests(unittest.TestCase):
    """Real CLI and templates; external CMake/SDK processes are test doubles."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="SDK CLI budgets ")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.prefix = self.directory / "prefix with spaces"
        self.output = self.directory / "consumer results with spaces"
        self.sdk = self.prefix / "share/nexus/src"
        policy = self.sdk / "dependencies/platform-compatibility.json"
        policy.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / "dependencies/platform-compatibility.json", policy)
        (self.sdk / ".nexus-source-sdk.json").write_text(json.dumps({
            "publishable": True, "source_dirty": False,
            "source_revision": "a" * 40, "source_tree": "b" * 40}))
        specification = importlib.util.spec_from_file_location(
            "nexus_consumer_parallel_contract",
            ROOT / "cmake/package/verify_consumers.py")
        self.consumer = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(self.consumer)
        self.commands = []
        self.failed_build = False
        self.failed_sdk = False

    def external_process(self, command, **options):
        command = [str(argument) for argument in command]
        self.commands.append(command)
        code = 0
        if command[0] == "cmake" and "-S" in command:
            source = Path(command[command.index("-S") + 1])
            build = Path(command[command.index("-B") + 1])
            build.mkdir(parents=True)
            (build / "resolved-sdk.txt").write_text(str(self.sdk))
            products = [path for path in (source / "main.c", source / "main.cpp")
                        if path.exists()]
            (build / "compile_commands.json").write_text(json.dumps([
                {"file": str(path), "command": "cc -c " + str(path)}
                for path in products]))
            config = build / "configuration"
            config.mkdir()
            (config / "bindings.c").write_text("/* external CMake fixture */")
            (config / "resolved.json").write_text("{}")
            (build / "resolved-config.txt").write_text(str(config))
            binary = build / "bin"
            binary.mkdir()
            for name in ("installed_c", "installed_cpp", "installed_modern"):
                (binary / (name + ".resources.json")).write_text(
                    '{"status":"passed"}')
                (binary / (name + ".elf")).write_bytes(b"test double")
        elif command[0] == "cmake" and "--build" in command:
            code = 42 if self.failed_build else 0
        elif any(argument.endswith("retention.py") for argument in command):
            Path(command[command.index("--output") + 1]).write_text(
                '{"status":"passed"}')
        elif any(argument.endswith("package_source_sdk.py")
                 for argument in command):
            code = 9 if self.failed_sdk else 0
        return subprocess.CompletedProcess(command, code, "external fixture\n", "")

    def execute(self, extra=()):
        with mock.patch.object(self.consumer.subprocess, "run",
                               side_effect=self.external_process):
            status = self.consumer.main([
                "--prefix", str(self.prefix), "--output", str(self.output),
                *extra])
        report = json.loads((self.output / "verification.json").read_text())
        return status, report

    def builds(self):
        return [command for command in self.commands
                if command[0] == "cmake" and "--build" in command]

    def assert_complete_matrix(self):
        builds = self.builds()
        expected = {"native", "stm32", "gd32"} | {
            family + " " + backend
            for family in ("h743", "u575", "f103", "gd303", "g474")
            for backend in ("baremetal", "freertos")}
        self.assertEqual({Path(command[2]).name.removesuffix(" build")
                          for command in builds}, expected)
        self.assertEqual(len(builds), 13)
        self.assertEqual(sum(len(command) - command.index("--target") - 1
                             for command in builds), 16)
        configurations = [command for command in self.commands
                          if command[0] == "cmake" and "-S" in command]
        self.assertEqual(len(configurations), 13)
        for command in configurations:
            self.assertIn("-DNEXUS_EXPECTED_SOURCE_REVISION=" + "a" * 40,
                          command)
        retention = [command for command in self.commands
                     if any(arg.endswith("retention.py") for arg in command)]
        self.assertEqual(len(retention), 10)
        native = [command for command in self.commands if len(command) == 1]
        self.assertEqual({Path(command[0]).name for command in native},
                         {"installed_c", "installed_cpp"})
        self.assertEqual(len(native), 2)
        self.assertIn("--verify", self.commands[0])

    def test_explicit_one_bounds_every_build_without_reducing_the_matrix(self):
        with mock.patch.dict("os.environ", {"CMAKE_BUILD_PARALLEL_LEVEL": "99"}):
            status, report = self.execute(("--parallel", "1"))
        self.assertEqual(status, 0)
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["physical_status"], "not_executed")
        self.assert_complete_matrix()
        self.assertEqual([command[command.index("--parallel") + 1]
                          for command in self.builds()], ["1"] * 13)

    def test_default_retains_three_four_job_and_ten_two_job_builds(self):
        status, report = self.execute()
        self.assertEqual(status, 0)
        self.assertEqual(report["source_revision"], "a" * 40)
        self.assertEqual(report["source_tree"], "b" * 40)
        self.assert_complete_matrix()
        self.assertEqual([command[command.index("--parallel") + 1]
                          for command in self.builds()], ["4"] * 3 + ["2"] * 10)

    def test_positive_budget_is_applied_to_every_modern_and_legacy_build(self):
        status, _ = self.execute(("--parallel", "3"))
        self.assertEqual(status, 0)
        self.assert_complete_matrix()
        self.assertEqual([command[command.index("--parallel") + 1]
                          for command in self.builds()], ["3"] * 13)

    def test_invalid_budget_rejects_before_any_output_or_external_command(self):
        for value in ("0", "-1", "01", "+1", "1.5", "True", "", " 1"):
            with self.subTest(value=value), mock.patch.object(
                    self.consumer.subprocess, "run") as command:
                errors = io.StringIO()
                with contextlib.redirect_stderr(errors), self.assertRaises(
                        SystemExit) as failure:
                    self.consumer.main([
                        "--prefix", str(self.prefix), "--output", str(self.output),
                        "--parallel", value])
                self.assertEqual(failure.exception.code, 2)
                self.assertIn("positive integer", errors.getvalue())
                self.assertFalse(self.output.exists())
                command.assert_not_called()

    def test_explicit_budget_preserves_a_real_subprocess_failure(self):
        self.failed_build = True
        with contextlib.redirect_stderr(io.StringIO()):
            status, report = self.execute(("--parallel", "1"))
        self.assertEqual(status, 1)
        self.assertEqual(report["status"], "failed")
        self.assertEqual(len(self.builds()), 1)
        self.assertEqual(report["commands"][-1]["exit_code"], 42)

    def test_explicit_budget_cannot_skip_sdk_verification(self):
        self.failed_sdk = True
        with contextlib.redirect_stderr(io.StringIO()):
            status, report = self.execute(("--parallel", "1"))
        self.assertEqual(status, 1)
        self.assertEqual(report["status"], "failed")
        self.assertEqual(self.builds(), [])
        self.assertEqual(report["commands"][-1]["exit_code"], 9)


if __name__ == "__main__":
    unittest.main()
