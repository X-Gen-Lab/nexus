"""Minimum-image matrix failures retain actual external build logs."""

from pathlib import Path
import shutil
import os
import signal
import subprocess
import sys
import tempfile
import tomllib
import unittest

from tools.evidence.common import EvidenceError, atomic_json
from tools.measurement.compare import compare, tool_identity, profile_input, run_logged


def authored_fixture(root):
    assembly = root / "fixture.toml"
    contents = ('schema = 2\nboard_package = "board"\n'
                'backend = "baremetal"\nclock = "fixed"\n'
                'optimization = "O3"\n'
                '[memory]\nmain_stack_bytes = 2048\n')
    assembly.write_text(contents)
    (root / "fixture-empty.toml").write_text(contents)
    return assembly


class MeasurementMatrixTests(unittest.TestCase):
    def test_parallelism_rejects_unbounded_or_noninteger_work_before_build(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for jobs in (0, -1, True, 1.5, "2"):
                with self.subTest(jobs=jobs), self.assertRaisesRegex(
                        EvidenceError, "parallel"):
                    compare(root, root / "absent.json", root / "build",
                            parallel=jobs)
            self.assertFalse((root / "build").exists())

    def test_actual_build_respects_explicit_single_job_and_retains_failure(self):
        cmake = shutil.which("cmake")
        ninja = shutil.which("ninja")
        if not cmake or not ninja:
            self.skipTest("CMake/Ninja unavailable; build control not exercised")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.mkdir()
            subprocess.run(["git", "init", "-q", str(source)], check=True)
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(bounded_measurement NONE)\n"
                "add_custom_target(expected_failure ALL COMMAND "
                "${CMAKE_COMMAND} -E false)\n")
            toolchain = source / "cmake/toolchains/arm-gcc.cmake"
            toolchain.parent.mkdir(parents=True)
            toolchain.write_text("# No compiler needed for this build boundary.\n")
            subprocess.run(["git", "-C", str(source), "add", "."], check=True)
            subprocess.run(["git", "-C", str(source), "-c", "user.name=Test",
                "-c", "user.email=test@example.invalid", "commit", "-qm", "fixture"], check=True)
            assembly = authored_fixture(root)
            report = compare(source, assembly, root / "build",
                compiler=str(Path(sys.executable).resolve()),
                cmake=str(Path(cmake).resolve()), ninja=str(Path(ninja).resolve()),
                workloads=("empty",), optimizations=("Os",), parallel=1)
            self.assertEqual(report["status"], "failed")
            commands = report["images"][0]["commands"]
            self.assertEqual(commands[0]["exit_code"], 0)
            self.assertNotEqual(commands[1]["exit_code"], 0)
            self.assertEqual(commands[1]["argv"][-2:], ["--parallel", "1"])
            self.assertEqual(report["build_parallelism"], 1)
            selected = Path(next(argument.split("=", 1)[1]
                for argument in commands[0]["argv"]
                if argument.startswith("-DNEXUS_ASSEMBLY_FILE=")))
            self.assertEqual(selected.suffix, ".toml")
            document = tomllib.loads(selected.read_text())
            self.assertEqual(document["schema"], 2)
            self.assertEqual(document["optimization"], "Os")

    def test_authored_toml_profiles_preserve_endpoint_and_board_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base = root / "profile.toml"
            selected = root / "profile-spi.toml"
            selected.write_text(
                'schema = 2\nboard_package = "board with spaces"\n'
                'backend = "baremetal"\nclock = "fixed"\n'
                '[memory]\nmain_stack_bytes = 2048\n'
                '[spi.bus]\nbinding = "spi0"\nmode = "short-poll"\n'
                '[spi_device.sensor]\ncontroller = "bus"\n'
                'cs_binding = "cs0"\nmode = 3\n')
            result = tomllib.loads(profile_input(
                root, base, "spi", root / "output"))
            self.assertEqual(result["spi_device"]["sensor"]["mode"], 3)
            self.assertEqual(result["board_package"], "../board with spaces")
            self.assertEqual(result["spi"]["bus"]["binding"], "spi0")

    def test_root_path_relocation_preserves_quoted_and_multiline_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base = root / "profile.toml"
            selected = root / "profile-empty.toml"
            body = ('[memory]\nmain_stack_bytes = 2048\n'
                    '# board_package = "nested comment remains"\n')
            selected.write_text('schema = 2\n"board_package" = """\\\n'
                                'board with # and spaces\\\n"""\n'
                                "'layout' = '''flash layout.json'''\n"
                                'backend = "baremetal"\nclock = "fixed"\n'
                                'optimization = "O2"\n' + body)
            rendered = profile_input(root, base, "empty", root / "output")
            result = tomllib.loads(rendered)
            self.assertEqual(result["board_package"],
                             "../board with # and spaces")
            self.assertEqual(result["layout"], "../flash layout.json")
            self.assertEqual(result["optimization"], "O2")
            self.assertTrue(rendered.endswith(body))

    def test_json_profile_cannot_bypass_the_production_toml_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            atomic_json(root / "profile-empty.json", {"board_package": "board"})
            with self.assertRaisesRegex(ValueError, "TOML"):
                profile_input(root, root / "profile.json", "empty", root / "out")

    def test_board_id_becomes_one_explicit_relocated_package(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "boards/reviewed"
            package.mkdir(parents=True)
            (package / "board.json").write_text("{}")
            (root / "profile-empty.toml").write_text(
                'schema = 2\nboard = "reviewed"\nbackend = "baremetal"\n'
                'clock = "fixed"\n[memory]\nmain_stack_bytes = 2048\n')
            result = tomllib.loads(profile_input(
                root, root / "profile.toml", "empty", root / "out"))
            self.assertNotIn("board", result)
            self.assertEqual(result["board_package"], "../boards/reviewed")

    def test_unknown_toml_profile_fields_fail_before_external_build(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base = root / "profile.toml"
            (root / "profile-empty.toml").write_text(
                'schema = 2\nboard_package = "board"\n'
                'backend = "baremetal"\nclock = "fixed"\n'
                'unused = true\n[memory]\nmain_stack_bytes = 2048\n')
            with self.assertRaisesRegex(ValueError, "unknown"):
                profile_input(root, base, "empty", root / "output")

    def test_missing_tool_is_never_a_passing_profile(self):
        with self.assertRaisesRegex(EvidenceError, "unavailable"):
            tool_identity("/does/not/exist/nexus-compiler", ["--version"])

    def test_actual_cmake_failure_retains_nonzero_raw_evidence(self):
        cmake = shutil.which("cmake")
        ninja = shutil.which("ninja")
        if not cmake or not ninja:
            self.skipTest("CMake/Ninja unavailable; actual build fault not exercised")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.mkdir()
            subprocess.run(["git", "init", "-q", str(source)], check=True)
            (source / "tracked.txt").write_text("source without a CMakeLists.txt\n")
            subprocess.run(["git", "-C", str(source), "add", "."], check=True)
            subprocess.run(["git", "-C", str(source), "-c", "user.name=Test",
                "-c", "user.email=test@example.invalid", "commit", "-qm", "fixture"], check=True)
            assembly = authored_fixture(root)
            report = compare(source, assembly, root / "build", compiler=str(Path(sys.executable).resolve()),
                cmake=str(Path(cmake).resolve()), ninja=str(Path(ninja).resolve()),
                workloads=("empty",), optimizations=("Os",))
            self.assertEqual(report["status"], "failed")
            execution = report["images"][0]["commands"][0]
            self.assertNotEqual(execution["exit_code"], 0)
            self.assertIn("CMakeLists.txt", Path(execution["raw"]["path"]).read_text())
            self.assertTrue((root / "build/matrix.json").is_file())



class MeasurementCommandTests(unittest.TestCase):
    def test_actual_external_nonzero_exit_and_raw_log_retained(self):
        with tempfile.TemporaryDirectory() as directory:
            report = run_logged([sys.executable, "-c", "print('actual failure'); raise SystemExit(37)"],
                                Path(directory) / "raw.log")
            self.assertEqual(report["exit_code"], 37)
            self.assertIn("actual failure", Path(report["raw"]["path"]).read_text())


    def test_missing_external_tool_is_nonzero(self):
        with tempfile.TemporaryDirectory() as directory:
            report = run_logged(["/does/not/exist"], Path(directory) / "raw.log")
            self.assertEqual(report["exit_code"], 127)
            self.assertIsNotNone(report["failure"])


    def test_actual_timeout_is_nonzero(self):
        with tempfile.TemporaryDirectory() as directory:
            report = run_logged([sys.executable, "-c", "import time; time.sleep(10)"],
                                Path(directory) / "raw.log", timeout_s=1)
            self.assertEqual(report["exit_code"], 124)


    def test_fast_success_cannot_exceed_the_actual_raw_log_bound(self):
        with tempfile.TemporaryDirectory() as directory:
            report = run_logged([
                sys.executable, "-c",
                "import sys; sys.stdout.buffer.write(b'x' * (33 * 1024 * 1024))"
            ], Path(directory) / "raw.log")
            self.assertEqual(report["exit_code"], 125)
            self.assertIn("32 MiB", report["failure"])
            self.assertGreater(Path(report["raw"]["path"]).stat().st_size,
                               32 * 1024 * 1024)


    def test_background_process_cannot_fake_success(self):
        with tempfile.TemporaryDirectory() as directory:
            code = "import subprocess; subprocess.Popen(['sleep','10'])"
            report = run_logged([sys.executable, "-c", code], Path(directory) / "raw.log")
            self.assertEqual(report["exit_code"], 125)


    def test_keyboard_interrupt_reaps_the_actual_command_process_group(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            child = root / "child.py"
            child.write_text(
                "import os, pathlib, signal, sys, time\n"
                "pathlib.Path(sys.argv[1]).write_text(str(os.getpid()))\n"
                "os.kill(os.getppid(), signal.SIGINT)\n"
                "time.sleep(30)\n")
            driver = root / "driver.py"
            driver.write_text(
                "import os, pathlib, sys\n"
                "from tools.measurement.compare import run_logged\n"
                "try:\n"
                "    run_logged([sys.executable, sys.argv[1], sys.argv[2]],\n"
                "               pathlib.Path(sys.argv[3]))\n"
                "except KeyboardInterrupt:\n"
                "    pid = int(pathlib.Path(sys.argv[2]).read_text())\n"
                "    try:\n"
                "        os.kill(pid, 0)\n"
                "    except ProcessLookupError:\n"
                "        raise SystemExit(0)\n"
                "    raise SystemExit(9)\n"
                "raise SystemExit(10)\n")
            pid_path = root / "child.pid"
            environment = dict(os.environ)
            environment["PYTHONPATH"] = str(Path(__file__).resolve().parents[2])
            try:
                result = subprocess.run(
                    [sys.executable, str(driver), str(child), str(pid_path),
                     str(root / "raw.log")], env=environment,
                    capture_output=True, text=True, timeout=10,
                    start_new_session=True)
                self.assertEqual(result.returncode, 0, result.stderr)
            finally:
                if pid_path.exists():
                    try:
                        os.killpg(int(pid_path.read_text()), signal.SIGKILL)
                    except ProcessLookupError:
                        # The fixture group may already have exited.
                        pass



if __name__ == "__main__":
    unittest.main()
