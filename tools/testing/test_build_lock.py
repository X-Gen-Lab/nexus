"""Keep managed build commands exclusive without blocking composed commands."""

from contextlib import redirect_stderr, redirect_stdout
from contextvars import copy_context
import importlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

from tools.dev import dev, process
from scripts.ci import native_contracts
from tools.testing import gate_fixture

ROOT = Path(__file__).resolve().parents[2]


class ManagedBuildLockTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.build = self.root / "build/native-debug"
        (self.root / "CMakePresets.json").write_text(json.dumps({
            "version": 6,
            "configurePresets": [{"name": "native-debug",
                                  "binaryDir": str(self.build)}],
            "buildPresets": [{"name": "native-debug",
                              "configurePreset": "native-debug"}],
            "testPresets": [{"name": "native-debug",
                             "configurePreset": "native-debug"}],
        }))

    def held_by_child(self, build):
        command = [sys.executable, "-B", "-c",
                   "import pathlib,sys,time; "
                   "from tools.dev.native_matrix import build_lock; "
                   "lease=build_lock(pathlib.Path(sys.argv[1])); "
                   "lease.__enter__(); print('locked',flush=True); "
                   "time.sleep(30)", str(build)]
        child = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, text=True)

        def close():
            child.kill()
            child.communicate(timeout=5)

        self.addCleanup(close)
        self.assertEqual(child.stdout.readline().strip(), "locked")
        return child

    def test_all_managed_commands_reject_a_busy_build_before_execution(self):
        self.held_by_child(self.build)
        for action in ("configure", "build", "test", "check"):
            with self.subTest(action=action), patch.object(dev, "ROOT", self.root), \
                    patch.object(dev, "workspace_identity", return_value=None), \
                    patch.object(dev, "executable", side_effect=lambda name: name), \
                    patch.object(dev, "execute", return_value=0) as execute, \
                    patch.object(dev, "capture_provenance"), \
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(dev.main([action]), 2)
                self.assertEqual(execute.call_count, 0)

    def test_configure_locks_the_actual_overridden_binary_directory(self):
        override = self.root / "other build"
        self.held_by_child(override)
        with patch.object(dev, "ROOT", self.root), \
                patch.object(dev, "executable", side_effect=lambda name: name), \
                patch.object(dev, "execute", return_value=0) as execute, \
                patch.object(dev, "capture_provenance"), \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(dev.main(["configure", "-B", str(override)]), 2)
        execute.assert_not_called()

    def test_configure_rejects_a_different_source_preset_graph(self):
        other = str(self.root / "other source")
        for arguments in (["-S", other], ["-S" + other], ["-S=" + other]):
            with self.subTest(arguments=arguments), \
                    patch.object(dev, "ROOT", self.root), \
                    patch.object(dev, "executable", return_value="cmake"), \
                    patch.object(dev, "capture_provenance"), \
                    patch.object(dev, "execute", return_value=0) as execute, \
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(dev.main(["configure", *arguments]), 2)
                self.assertEqual(execute.call_count, 0)

    def test_tdd_rejects_a_busy_build_before_mutating_reports(self):
        fixture = gate_fixture.GateFixture()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        report = fixture.build / "tdd/execution.json"
        report.parent.mkdir()
        report.write_text('{"old":true}')
        self.held_by_child(fixture.build)
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            return fixture.execute(command, environment, log)

        status, output = fixture.run_gate(execute=execute)
        self.assertEqual(status, 1, output)
        self.assertEqual(commands, [])
        self.assertTrue(json.loads(report.read_text())["old"])

    def test_native_runtime_rejects_a_busy_build_without_replacing_reports(self):
        self.held_by_child(self.build)
        report = self.build / "tdd/execution.json"
        report.parent.mkdir()
        report.write_text('{"old":true}')
        with patch.object(native_contracts, "ROOT", self.root), \
                patch.object(native_contracts, "execute") as execute, \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(native_contracts.main(["--preset", "native-debug"]), 1)
        self.assertEqual(execute.call_count, 0)
        self.assertTrue(json.loads(report.read_text())["old"])

    def test_same_thread_can_reenter_but_another_thread_cannot(self):
        locks = importlib.import_module("tools.dev.build_lock")
        failures = []

        def contender():
            try:
                with locks.build_lock(self.build):
                    failures.append("entered busy directory")
            except ValueError:
                pass

        with locks.build_lock(self.build):
            with locks.build_lock(self.build):
                with locks.build_lock(self.root / "other"):
                    thread = threading.Thread(target=contender)
                    thread.start()
                    thread.join(timeout=5)
                    self.assertFalse(thread.is_alive())
        self.assertEqual(failures, [])
        with locks.build_lock(self.build):
            pass

    def test_copied_context_does_not_grant_another_thread_a_lease(self):
        locks = importlib.import_module("tools.dev.build_lock")
        observations = []

        def contender():
            environment, settings = locks.child_settings({})
            observations.append(locks.LEASES in environment or bool(settings))
            try:
                with locks.build_lock(self.build):
                    observations.append("entered busy directory")
            except ValueError:
                pass

        with locks.build_lock(self.build):
            context = copy_context()
            thread = threading.Thread(target=context.run, args=(contender,))
            thread.start()
            thread.join(timeout=5)
            self.assertFalse(thread.is_alive())
        self.assertEqual(observations, [False])

    def test_a_saved_context_cannot_reuse_a_closed_lease(self):
        locks = importlib.import_module("tools.dev.build_lock")
        with locks.build_lock(self.build):
            context = copy_context()
        self.held_by_child(self.build)

        def stale():
            environment, settings = locks.child_settings({})
            self.assertNotIn(locks.LEASES, environment)
            self.assertEqual(settings, {})
            with self.assertRaisesRegex(ValueError, "already owned"):
                with locks.build_lock(self.build):
                    self.fail("Closed lease replaced directory exclusion")

        context.run(stale)

    def test_child_reentry_uses_inherited_lock_while_foreign_process_is_rejected(self):
        locks = importlib.import_module("tools.dev.build_lock")
        command = [sys.executable, "-B", "-c",
                   "import pathlib,sys; from tools.dev.build_lock import build_lock; "
                   "lease=build_lock(pathlib.Path(sys.argv[1])); "
                   "lease.__enter__(); print('joined'); lease.__exit__(None,None,None)",
                   str(self.build)]
        with locks.build_lock(self.build):
            observed = process.run(command, cwd=ROOT, env=os.environ,
                                   log=self.root / "child.txt")
            if os.name != "posix":
                self.assertNotEqual(observed.returncode, 0)
                self.assertIn("POSIX", observed.stderr)
                return
            self.assertEqual(observed.returncode, 0, observed.stderr)
            self.assertIn("joined", observed.stdout)
            foreign = subprocess.run(command, cwd=ROOT, capture_output=True,
                                     text=True, timeout=5)
            self.assertNotEqual(foreign.returncode, 0)
            self.assertIn("already owned", foreign.stderr)

    def test_exception_releases_the_directory_and_symlink_is_rejected(self):
        locks = importlib.import_module("tools.dev.build_lock")
        with self.assertRaisesRegex(RuntimeError, "cancelled"):
            with locks.build_lock(self.build):
                raise RuntimeError("cancelled")
        with locks.build_lock(self.build):
            pass
        alias = self.root / "alias"
        alias.symlink_to(self.build, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            with locks.build_lock(alias):
                self.fail("Aliased build directory was accepted")

    def test_cancelled_child_releases_its_owned_build_lock(self):
        locks = importlib.import_module("tools.dev.build_lock")
        marker = self.root / "ready"
        cancellation = threading.Event()
        command = [sys.executable, "-B", "-c",
                   "import pathlib,sys,time; "
                   "from tools.dev.build_lock import build_lock; "
                   "lease=build_lock(pathlib.Path(sys.argv[1])); "
                   "lease.__enter__(); pathlib.Path(sys.argv[2]).touch(); "
                   "time.sleep(30)", str(self.build), str(marker)]

        def cancel():
            deadline = time.monotonic() + 5
            while not marker.exists() and time.monotonic() < deadline:
                time.sleep(0.005)
            cancellation.set()

        thread = threading.Thread(target=cancel)
        thread.start()
        try:
            result = process.run(command, cwd=ROOT, env=os.environ,
                                 log=self.root / "cancel.txt", timeout=8,
                                 cancel_event=cancellation)
        finally:
            thread.join(timeout=6)
        self.assertTrue(marker.exists())
        self.assertEqual(result.returncode, 130)
        with locks.build_lock(self.build):
            pass

    def test_an_environment_claim_without_the_matching_inode_cannot_delegate(self):
        locks = importlib.import_module("tools.dev.build_lock")
        if os.name != "posix":
            with locks.build_lock(self.build):
                environment, settings = locks.child_settings({})
                self.assertEqual(settings, {})
                self.assertIn(locks.LEASES, environment)
            return
        with locks.build_lock(self.build):
            pass
        with (self.root / "foreign").open("w+b") as foreign:
            environment = {locks.LEASES: json.dumps([
                {"build": str(self.build), "fd": foreign.fileno()}])}
            child, settings = locks.child_settings(environment)
            self.assertNotIn(locks.LEASES, child)
            self.assertEqual(settings, {})
            with patch.dict(os.environ, environment), \
                    self.assertRaisesRegex(ValueError, "differs"):
                with locks.build_lock(self.build):
                    self.fail("Foreign descriptor authorized the directory")

    def test_unmanaged_grandchild_can_run_tools_without_inheriting_a_lock(self):
        locks = importlib.import_module("tools.dev.build_lock")
        if os.name != "posix":
            self.assertEqual(locks.child_settings({}), ({}, {}))
            return
        command = [sys.executable, "-B", "-c",
                   "import os,pathlib,sys\n"
                   "from tools.dev.process import run\n"
                   "from tools.dev.build_lock import build_lock\n"
                   "result=run([sys.executable,'-c','print(42)'],"
                   "cwd=pathlib.Path.cwd(),env=os.environ)\n"
                   "assert result.returncode==0,result.stderr\n"
                   "print(result.stdout)\n"
                   "try:\n"
                   "    with build_lock(pathlib.Path(sys.argv[1])):\n"
                   "        raise AssertionError('Lost FD bypassed exclusion')\n"
                   "except (OSError,ValueError): print('blocked')\n",
                   str(self.build)]
        with locks.build_lock(self.build):
            environment, _ = locks.child_settings(os.environ)
            child = subprocess.run(command, cwd=ROOT, env=environment,
                                   capture_output=True, text=True, timeout=5)
        self.assertEqual(child.returncode, 0, child.stderr)
        self.assertIn("42", child.stdout)
        self.assertIn("blocked", child.stdout)

    def test_delegated_process_does_not_grant_its_active_lease_to_another_thread(self):
        locks = importlib.import_module("tools.dev.build_lock")
        if os.name != "posix":
            self.assertEqual(locks.child_settings({}), ({}, {}))
            return
        script = (
            "import contextvars,json,os,pathlib,sys,threading\n"
            "from tools.dev.build_lock import build_lock,child_settings,LEASES\n"
            "build=pathlib.Path(sys.argv[1]); observations=[]\n"
            "def nested():\n"
            "    with build_lock(build): pass\n"
            "def contender():\n"
            "    env,_=child_settings(os.environ)\n"
            "    claims=json.loads(env.get(LEASES,'[]'))\n"
            "    observations.append(any(item['build']==str(build) for item in claims))\n"
            "    try:\n"
            "        with build_lock(build): observations.append('entered')\n"
            "    except ValueError: pass\n"
            "with build_lock(build):\n"
            "    contextvars.Context().run(nested)\n"
            "    thread=threading.Thread(target=contender)\n"
            "    thread.start();thread.join()\n"
            "assert observations==[False],observations\n"
            "print('isolated')\n")
        with locks.build_lock(self.build):
            child = process.run([sys.executable, "-B", "-c", script, str(self.build)],
                                cwd=ROOT, env=os.environ, log=self.root / "threads.txt")
        self.assertEqual(child.returncode, 0, child.stderr)
        self.assertIn("isolated", child.stdout)


if __name__ == "__main__":
    unittest.main()
