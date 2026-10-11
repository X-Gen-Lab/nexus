"""Real command failure, input-boundary and image-lock rejection behavior."""

import json
import importlib
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError, atomic_json, command, file_identity
from tools.evidence.container import inspect_image
from tools.evidence.reproduce import copy_tracked, reproduce, run_logged, tree_identity, verify_source_sdk


class ReproductionTests(unittest.TestCase):
    def test_cancellation_removes_the_external_daemon_owned_container(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            adapter = root / "docker-adapter.py"
            pid_path, removed = root / "daemon.pid", root / "removed"
            adapter.write_text(
                "#!" + sys.executable + "\n"
                "import os, pathlib, signal, subprocess, sys, time\n"
                "root = pathlib.Path(__file__).parent\n"
                "args = [arg for arg in sys.argv[1:] if not arg.startswith('--host=')]\n"
                "if args[0] == 'run':\n"
                "    child = subprocess.Popen([sys.executable, '-c',\n"
                "        'import time; time.sleep(30)'], start_new_session=True)\n"
                "    (root / 'daemon.pid').write_text(str(child.pid))\n"
                "    os.kill(os.getppid(), signal.SIGINT)\n"
                "    time.sleep(30)\n"
                "elif args == ['rm', '--force', 'nexus-test-container']:\n"
                "    os.killpg(int((root / 'daemon.pid').read_text()), signal.SIGKILL)\n"
                "    (root / 'removed').write_text('actual cleanup executed')\n"
                "else:\n"
                "    raise SystemExit(22)\n")
            adapter.chmod(0o755)
            driver = root / "driver.py"
            driver.write_text(
                "import pathlib, sys\n"
                "from tools.evidence.container import docker_prefix\n"
                "from tools.evidence import reproduce as module\n"
                "adapter, log = sys.argv[1], pathlib.Path(sys.argv[2])\n"
                "try:\n"
                "    module.run_container_logged(\n"
                "        [*docker_prefix(adapter), 'run', '--name', 'nexus-test-container'],\n"
                "        log, docker=adapter, container_name='nexus-test-container')\n"
                "except KeyboardInterrupt:\n"
                "    raise SystemExit(0 if (log.parent / 'removed').exists() else 9)\n"
                "raise SystemExit(10)\n")
            environment = dict(os.environ)
            environment["PYTHONPATH"] = str(Path(__file__).resolve().parents[2])
            try:
                result = subprocess.run(
                    [sys.executable, str(driver), str(adapter), str(root / "raw.log")],
                    env=environment, capture_output=True, text=True, timeout=10,
                    start_new_session=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(removed.read_text(), "actual cleanup executed")
                state = Path("/proc") / pid_path.read_text() / "stat"
                if state.exists():
                    self.assertEqual(state.read_text().split(") ", 1)[1].split()[0], "Z")
                self.assertTrue((root / "raw.log.cleanup.log").is_file())
            finally:
                if pid_path.exists():
                    try:
                        os.killpg(int(pid_path.read_text()), signal.SIGKILL)
                    except ProcessLookupError:
                        pass

    def test_cancelled_attempt_cannot_leave_an_old_qualification(self):
        module = importlib.import_module("tools.evidence.reproduce")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            qualification = root / "qualification.json"
            qualification.write_text("previous run qualification")
            argv = ["reproduce", "--spec", str(root / "spec.json"),
                    "--report", str(root / "report.json"),
                    "--qualification", str(qualification)]
            with patch.object(sys, "argv", argv), patch.object(
                    module, "reproduce", side_effect=KeyboardInterrupt):
                with self.assertRaises(KeyboardInterrupt):
                    module.main()
            self.assertFalse(qualification.exists())

    def test_container_argv_fits_a_bounded_explicit_argument_contract(self):
        self.assertEqual(len(command(["argument"] * 68)), 68)
        with self.assertRaises(EvidenceError):
            command(["argument"] * 129)

    def test_development_dirty_or_other_source_sdk_cannot_qualify(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = Path(directory) / ".nexus-source-sdk.json"
            atomic_json(manifest, {"kind": "provenance negative test fixture"})
            source = {"commit": "a" * 40, "tree": "b" * 40}
            base = {"publishable": True, "source_dirty": False,
                    "source_revision": source["commit"], "source_tree": source["tree"]}
            for changed in ({"publishable": False}, {"source_dirty": True},
                            {"source_revision": "c" * 40}, {"source_tree": "c" * 40}):
                # Isolate provenance rejection after the package validator returns.
                # This fixture is never accepted as a real source SDK.
                with self.subTest(changed=changed), patch(
                        "tools.evidence.reproduce.verify_sdk_package", return_value={**base, **changed}):
                    with self.assertRaisesRegex(EvidenceError, "same clean qualified"):
                        verify_source_sdk(file_identity(manifest), source)

    def test_ad_hoc_sdk_manifest_cannot_replace_complete_package(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = Path(directory) / ".nexus-source-sdk.json"
            atomic_json(manifest, {"dependencies": [], "publishable": True})
            with self.assertRaises(ValueError):
                verify_source_sdk(file_identity(manifest), {"commit": "a" * 40, "tree": "b" * 40})

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
                "from tools.evidence.reproduce import run_logged\n"
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
                        pass

    def test_tree_identity_rejects_empty_and_symlink(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(EvidenceError):
                tree_identity(root)
            (root / "input").write_text("declared input")
            original = tree_identity(root)
            (root / "input").write_text("changed declared input")
            self.assertNotEqual(tree_identity(root), original)
            (root / "link").symlink_to(root / "input")
            with self.assertRaises(EvidenceError):
                tree_identity(root)

    def test_clean_source_snapshot_does_not_include_host_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "checkout"
            source.mkdir()
            subprocess.run(["git", "init", "-q", str(source)], check=True)
            (source / "source.c").write_text("tracked source")
            subprocess.run(["git", "-C", str(source), "add", "source.c"], check=True)
            (source / "host-secret.txt").write_text("untracked must never be mounted")
            copy_tracked(source, root / "snapshot")
            self.assertTrue((root / "snapshot/source.c").is_file())
            self.assertFalse((root / "snapshot/host-secret.txt").exists())
            self.assertFalse((root / "snapshot/.git").exists())

    def test_missing_docker_cannot_establish_environment(self):
        with self.assertRaises(EvidenceError):
            inspect_image("nonexistent", executable="/does/not/exist")

    def test_invalid_spec_fails_with_explicit_unestablished_results(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            spec, report = root / "spec.json", root / "report.json"
            atomic_json(spec, {"schema_version": 1})
            result = reproduce(spec, report, docker="/does/not/exist")
            self.assertEqual(result["status"], "failed")
            self.assertEqual(result["hermetic_status"], "not_established")
            self.assertEqual(result["reproducible_status"], "not_established")
            self.assertEqual(json.loads(report.read_text()), result)


if __name__ == "__main__":
    unittest.main()
