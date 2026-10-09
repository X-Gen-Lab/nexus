"""Execute the real preset entry with finite CMake/CTest integration fixtures."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PresetValidationContracts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ("scripts/ci/ci_build.py", "scripts/validation/validate.py",
                     "scripts/validation/junit.py", "scripts/validation/__init__.py"):
            dest = self.root / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, dest)
        self.entry = self.root / "scripts/validation/validate.py"
        self.presets = {"version": 3,
            "configurePresets": [{"name": "native-check", "generator": "Unix Makefiles",
                "binaryDir": "${sourceDir}/build/${presetName}",
                "cacheVariables": {"NEXUS_PLATFORM": "native", "NEXUS_BUILD_TESTS": "ON"}}],
            "buildPresets": [{"name": "native-check", "configurePreset": "native-check"}],
            "testPresets": [{"name": "native-check", "configurePreset": "native-check"}]}
        (self.root / "CMakePresets.json").write_text(json.dumps(self.presets))
        self.configure_project()

    def configure_project(self, *, skip=False, empty=False):
        body = 'cmake_minimum_required(VERSION 3.21)\nproject(FiniteValidationFixture NONE)\nenable_testing()\n'
        if not empty:
            code = "import sys; sys.exit(77)" if skip else "print('finite runtime passed')"
            body += f'add_test(NAME real.finite COMMAND "{Path(sys.executable).as_posix()}" -c "{code}")\n'
            if skip:
                body += 'set_tests_properties(real.finite PROPERTIES SKIP_RETURN_CODE 77)\n'
        (self.root / "CMakeLists.txt").write_text(body)

    def run_entry(self, *args, env=None):
        return subprocess.run([sys.executable, str(self.entry), *args], cwd=self.root,
                              capture_output=True, text=True, timeout=30, env=env)

    def test_real_finite_case_and_junit(self):
        result = self.run_entry("--preset", "native-check", "--stage", "all")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = self.root / "build/native-check/ctest-results.xml"
        self.assertIn('name="real.finite"', report.read_text())

    def test_zero_tests_fail_via_real_ctest(self):
        self.configure_project(empty=True)
        result = self.run_entry("--preset", "native-check", "--stage", "all")
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_all_skip_fails_via_real_ctest_and_junit(self):
        self.configure_project(skip=True)
        result = self.run_entry("--preset", "native-check", "--stage", "all")
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_invalid_preset_and_old_build_options_fail(self):
        for args in (("--preset", "does-not-exist"), ("--build-dir", "build")):
            with self.subTest(args=args):
                self.assertNotEqual(self.run_entry(*args).returncode, 0)

    def test_embedded_test_request_fails_before_tool_execution(self):
        p = self.presets["configurePresets"][0]
        p["cacheVariables"].update(NEXUS_BUILD_TESTS="OFF", NEXUS_PLATFORM="stm32")
        (self.root / "CMakePresets.json").write_text(json.dumps(self.presets))
        result = self.run_entry("--preset", "native-check", "--stage", "test")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.root / "build").exists())

    def test_stale_report_cannot_survive_successful_command_without_output(self):
        report = self.root / "build/native-check/ctest-results.xml"
        report.parent.mkdir(parents=True)
        report.write_text('<testsuite tests="1"><testcase name="old.success"/></testsuite>')
        fake_tools = self.root / "fault-tools"
        fake_tools.mkdir()
        executable = fake_tools / "ctest"
        executable.write_text('#!' + sys.executable + '\nraise SystemExit(0)\n')
        executable.chmod(0o755)
        env = dict(os.environ, PATH=str(fake_tools) + os.pathsep + os.environ["PATH"])
        result = self.run_entry("--preset", "native-check", "--stage", "test", env=env)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(report.exists())

    def test_nonzero_ctest_cannot_be_hidden_by_passing_report(self):
        tools = self.root / "error-tools"
        tools.mkdir()
        executable = tools / "ctest"
        executable.write_text('#!' + sys.executable + '\n'
            'from pathlib import Path\nimport sys\n'
            'path=Path(sys.argv[sys.argv.index("--output-junit")+1])\n'
            'path.write_text(\'<testsuite tests="1"><testcase name="fabricated.success"/></testsuite>\')\n'
            'raise SystemExit(7)\n')
        executable.chmod(0o755)
        env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"])
        result = self.run_entry("--preset", "native-check", "--stage", "test", env=env)
        self.assertNotEqual(result.returncode, 0)

    def test_corrupt_fresh_report_cannot_be_hidden_by_success_exit(self):
        tools = self.root / "corrupt-tools"
        tools.mkdir()
        executable = tools / "ctest"
        executable.write_text('#!' + sys.executable + '\n'
            'from pathlib import Path\nimport sys\n'
            'path=Path(sys.argv[sys.argv.index("--output-junit")+1])\n'
            'path.write_text("<testsuite>")\n'
            'raise SystemExit(0)\n')
        executable.chmod(0o755)
        env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"])
        result = self.run_entry("--preset", "native-check", "--stage", "test", env=env)
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
