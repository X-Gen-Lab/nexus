"""Execute preset, change classification and subprocess boundary regressions."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

from scripts.ci import tdd_gate


class ChangeClassificationTests(unittest.TestCase):
    def test_all_dependency_identities_require_behavior_gate(self):
        for name in ("ext/freertos", "vendors/st/cmsis_device_h7",
                     "dependencies/environment.lock.json", ".gitmodules",
                     "nexus.lock", "CMakeUserPresets.json"):
            with self.subTest(name=name):
                self.assertTrue(tdd_gate.relevant(name))

    def test_readme_does_not_require_behavior_gate(self):
        self.assertFalse(tdd_gate.relevant("tools/configure/README.md"))

    def test_ci_and_hook_use_identical_behavior_decisions(self):
        from tools.dev.change_paths import classify
        for name in ("ext/freertos", "tools/configure/README.md",
                     ".github/workflows/ci.yml", "new-provider.dat"):
            with self.subTest(name=name):
                result = classify([name])
                self.assertEqual(tdd_gate.relevant(name),
                                 result["code"] or result["workflows"])

    def test_real_git_rename_and_deletion_keep_both_behavior_sides(self):
        from tools.dev.change_paths import changed_paths, classify
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            environment = os.environ.copy()
            environment.update({"GIT_AUTHOR_NAME": "Fixture",
                "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
                "GIT_COMMITTER_NAME": "Fixture",
                "GIT_COMMITTER_EMAIL": "fixture@example.invalid"})

            def git(*arguments):
                result = subprocess.run(["git", *arguments], cwd=root,
                    env=environment, capture_output=True, text=True,
                    check=True, timeout=10)
                return result.stdout.strip()

            git("init", "-q")
            (root / "core").mkdir()
            (root / "core/source.c").write_text("original\n")
            (root / "README.md").write_text("deleted\n")
            git("add", ".")
            git("commit", "-qm", "initial fixture")
            base = git("rev-parse", "HEAD")
            (root / "docs").mkdir()
            (root / "core/source.c").rename(root / "docs/retired.md")
            (root / "README.md").unlink()
            git("add", "-A")
            git("commit", "-qm", "rename fixture")
            paths = changed_paths(root, base)
            self.assertEqual(set(paths),
                {"core/source.c", "docs/retired.md", "README.md"})
            self.assertTrue(classify(paths)["code"])


class PresetResolutionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def write(self, name, content):
        (self.root / name).write_text(json.dumps(content))

    def resolve(self, name, kind="configure"):
        from tools.dev.presets import selected_configure
        return selected_configure(self.root, name, kind=kind)

    def test_include_multiple_inheritance_and_user_preset(self):
        self.write("base.json", {"version": 6, "configurePresets": [
            {"name": "first", "hidden": True, "generator": "Ninja",
             "binaryDir": "${sourceDir}/build/${presetName}",
             "cacheVariables": {"WINNER": "first", "BASE": "yes"}},
            {"name": "second", "hidden": True,
             "cacheVariables": {"WINNER": "second", "SECOND": "yes"}}]})
        self.write("CMakePresets.json", {"version": 6, "include": ["base.json"],
            "configurePresets": [{"name": "shared",
                "inherits": ["first", "second"],
                "cacheVariables": {"LOCAL": "yes"}}]})
        self.write("CMakeUserPresets.json", {"version": 6,
            "configurePresets": [{"name": "personal", "inherits": "shared"}],
            "testPresets": [{"name": "personal-test",
                             "configurePreset": "personal"}]})
        actual = self.resolve("personal-test", "test")
        self.assertEqual(actual["binaryDir"], str(self.root / "build/personal"))
        self.assertEqual(actual["cacheVariables"], {
            "WINNER": "first", "BASE": "yes", "SECOND": "yes",
            "LOCAL": "yes"})

    def test_hidden_and_false_condition_are_not_selectable(self):
        self.write("CMakePresets.json", {"version": 6, "configurePresets": [
            {"name": "hidden", "hidden": True},
            {"name": "disabled", "condition": False}]})
        for name in ("hidden", "disabled"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.resolve(name)

    def test_include_and_inheritance_cycles_are_rejected(self):
        self.write("CMakePresets.json", {"version": 6, "include": ["base.json"]})
        self.write("base.json", {"version": 6, "include": ["CMakePresets.json"]})
        with self.assertRaises(ValueError):
            self.resolve("missing")
        self.write("CMakePresets.json", {"version": 6, "configurePresets": [
            {"name": "first", "inherits": "second"},
            {"name": "second", "inherits": "first"}]})
        with self.assertRaises(ValueError):
            self.resolve("first")

    def test_environment_reference_and_unsupported_macro(self):
        self.write("CMakePresets.json", {"version": 6, "configurePresets": [
            {"name": "good", "environment": {"BUILD_BASE": "$penv{TMPDIR}"},
             "binaryDir": "$env{BUILD_BASE}/${presetName}"},
            {"name": "bad", "binaryDir": "${unknown}/build"}]})
        from tools.dev.presets import selected_configure
        actual = selected_configure(self.root, "good",
                                    environment={"TMPDIR": str(self.root)})
        self.assertEqual(actual["binaryDir"], str(self.root / "good"))
        with self.assertRaises(ValueError):
            self.resolve("bad")

    def test_unsupported_condition_is_rejected(self):
        self.write("CMakePresets.json", {"version": 6, "configurePresets": [
            {"name": "bad", "condition": {"type": "invented"}}]})
        with self.assertRaises(ValueError):
            self.resolve("bad")

    def test_test_name_resolves_its_configure_name(self):
        self.write("CMakePresets.json", {"version": 6,
            "configurePresets": [{"name": "native-debug",
                                  "binaryDir": "build/${presetName}"}],
            "testPresets": [{"name": "contracts",
                             "configurePreset": "native-debug"}]})
        self.assertEqual(self.resolve("contracts", "test")["binaryDir"],
                         str(self.root / "build/native-debug"))

    def test_resolved_inherited_values_match_real_cmake_cache(self):
        import shutil
        cmake = shutil.which("cmake")
        self.assertIsNotNone(cmake, "CMake is required by maintained tool tests")
        (self.root / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.31)\nproject(preset NONE)\n")
        self.write("base.json", {"version": 6, "configurePresets": [
            {"name": "first", "hidden": True, "generator": "Ninja",
             "binaryDir": "${sourceDir}/build/${presetName}",
             "cacheVariables": {"WINNER": "first", "BASE": "yes"}},
            {"name": "second", "hidden": True,
             "cacheVariables": {"WINNER": "second", "SECOND": "yes"}}]})
        self.write("CMakePresets.json", {"version": 6, "include": ["base.json"],
            "configurePresets": [{"name": "shared",
                                  "inherits": ["first", "second"]}]})
        self.write("CMakeUserPresets.json", {"version": 6,
            "configurePresets": [{"name": "personal", "inherits": "shared"}]})
        selected = self.resolve("personal")
        actual = subprocess.run([cmake, "--preset", "personal"], cwd=self.root,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(actual.returncode, 0, actual.stdout + actual.stderr)
        cache = (Path(selected["binaryDir"]) / "CMakeCache.txt").read_text()
        self.assertIn("WINNER:UNINITIALIZED=first", cache)
        self.assertIn("BASE:UNINITIALIZED=yes", cache)
        self.assertIn("SECOND:UNINITIALIZED=yes", cache)

    def test_duplicate_preset_and_reverse_file_inheritance_rejected(self):
        self.write("CMakePresets.json", {"version": 6, "configurePresets": [
            {"name": "project", "inherits": "personal"}]})
        self.write("CMakeUserPresets.json", {"version": 6,
            "configurePresets": [{"name": "personal", "binaryDir": "build"}]})
        with self.assertRaises(ValueError):
            self.resolve("project")
        self.write("CMakeUserPresets.json", {"version": 6,
            "configurePresets": [{"name": "project", "binaryDir": "build"}]})
        with self.assertRaises(ValueError):
            self.resolve("project")

    def test_build_and_test_generator_and_configure_environment_match_cmake(self):
        import shutil
        cmake = shutil.which("cmake")
        self.assertIsNotNone(cmake)
        for kind in ("build", "test"):
            with self.subTest(kind=kind):
                self.write("CMakePresets.json", {"version": 6,
                    "configurePresets": [{"name": "cfg", "generator": "Ninja",
                        "binaryDir": "${sourceDir}/build",
                        "environment": {"CONFIG_ENV": "inherited"}}],
                    kind + "Presets": [{"name": "selected", "configurePreset": "cfg",
                        "condition": {"type": "allOf", "conditions": [
                            {"type": "equals", "lhs": "${generator}", "rhs": "Ninja"},
                            {"type": "equals", "lhs": "$env{CONFIG_ENV}",
                             "rhs": "inherited"}]}}]})
                observed = subprocess.run([cmake, "--list-presets=" + kind],
                    cwd=self.root, capture_output=True, text=True, timeout=30)
                self.assertEqual(observed.returncode, 0, observed.stderr)
                self.assertIn('"selected"', observed.stdout)
                from tools.dev.presets import Presets
                selected = Presets(self.root).selected(kind, "selected")
                self.assertEqual(selected["environment"]["CONFIG_ENV"], "inherited")

    def test_inherit_configure_environment_false_and_override_match_cmake(self):
        import shutil
        cmake = shutil.which("cmake")
        self.assertIsNotNone(cmake)
        self.write("CMakePresets.json", {"version": 6,
            "configurePresets": [{"name": "cfg", "generator": "Ninja",
                "binaryDir": "${sourceDir}/build",
                "environment": {"CONFIG_ENV": "inherited"}}],
            "testPresets": [
                {"name": "separate", "configurePreset": "cfg",
                 "inheritConfigureEnvironment": False,
                 "condition": {"type": "equals", "lhs": "$env{CONFIG_ENV}", "rhs": ""}},
                {"name": "override", "configurePreset": "cfg",
                 "environment": {"CONFIG_ENV": "overridden"},
                 "condition": {"type": "equals", "lhs": "$env{CONFIG_ENV}",
                               "rhs": "overridden"}}]})
        environment = os.environ.copy()
        environment.pop("CONFIG_ENV", None)
        observed = subprocess.run([cmake, "--list-presets=test"], cwd=self.root,
            env=environment, capture_output=True, text=True, timeout=30)
        self.assertEqual(observed.returncode, 0, observed.stderr)
        from tools.dev.presets import Presets
        for name in ("separate", "override"):
            self.assertIn('"' + name + '"', observed.stdout)
            selected = Presets(self.root, environment).selected("test", name)
            if name == "separate":
                self.assertNotIn("CONFIG_ENV", selected.get("environment", {}))
            else:
                self.assertEqual(selected["environment"]["CONFIG_ENV"], "overridden")
class ProcessExecutionTests(unittest.TestCase):
    def test_binary_stdin_is_observed_and_retained(self):
        from tools.dev.process import run
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            supplied = b"compiler input\x00\xff\n"
            result = run([sys.executable, "-c",
                          "import sys; sys.stdout.buffer.write(sys.stdin.buffer.read())"],
                         cwd=root, env={}, log=root / "stdin.txt", timeout=5,
                         input=supplied)
            self.assertEqual(result.returncode, 0)
            record = json.loads((root / "stdin.command.json").read_text())
            self.assertEqual(Path(record["stdin"]["path"]).read_bytes(), supplied)
            self.assertEqual(record["stdin"]["bytes"], len(supplied))
            self.assertEqual(Path(record["stdout"]["path"]).read_bytes(), supplied)

    def test_shared_cancellation_withdraws_thread_owned_descendants(self):
        import concurrent.futures
        import threading
        from tools.dev.process import run
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            event = threading.Event()
            ready = root / "ready"
            child = "import time; time.sleep(30)"
            code = ("import pathlib,subprocess,sys,time; "
                    f"subprocess.Popen([sys.executable,'-c',{child!r}]); "
                    f"pathlib.Path({str(ready)!r}).write_text('ready'); "
                    "time.sleep(30)")
            with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
                future = pool.submit(run, [sys.executable, "-c", code],
                                     cwd=root, env={}, log=root / "cancel.txt",
                                     timeout=20, cancel_event=event)
                deadline = time.monotonic() + 5
                while not ready.exists() and not future.done() and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(ready.exists())
                event.set()
                self.assertEqual(future.result(timeout=5).returncode, 130)
            record = json.loads((root / "cancel.command.json").read_text())
            self.assertEqual(record["outcome"], "cancelled")
            self.assertTrue(record["withdrawal_complete"])
            if Path("/proc/self/stat").is_file():
                self.assertGreaterEqual(len(record["withdrawn_processes"]), 2)

    def test_preexisting_cancellation_never_launches_child(self):
        import threading
        from tools.dev.process import run
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            event = threading.Event()
            event.set()
            marker = root / "launched"
            result = run([sys.executable, "-c",
                          f"from pathlib import Path; Path({str(marker)!r}).touch()"],
                         cwd=root, env={}, log=root / "pre-cancel.txt", timeout=5,
                         cancel_event=event)
            self.assertEqual(result.returncode, 130)
            self.assertFalse(marker.exists())
            record = json.loads((root / "pre-cancel.command.json").read_text())
            self.assertIsNone(record["child_returncode"])

    def test_environment_parallelism_is_bounded_and_explicit(self):
        from tools.dev.process import execution_environment
        self.assertEqual(execution_environment({})[
            "CMAKE_BUILD_PARALLEL_LEVEL"], "2")
        actual = execution_environment({"NEXUS_BUILD_JOBS": "3"})
        self.assertEqual(actual["CMAKE_BUILD_PARALLEL_LEVEL"], "3")
        for value in ("0", "-1", "unlimited"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                execution_environment({"NEXUS_BUILD_JOBS": value})

    def test_real_failure_and_retry_preserve_both_receipts(self):
        from tools.dev.process import run
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "execution.txt"
            failed = run([sys.executable, "-c",
                          "print('original failure'); raise SystemExit(7)"],
                         cwd=root, env=os.environ.copy(), log=log, timeout=10)
            passed = run([sys.executable, "-c", "print('retry success')"],
                         cwd=root, env=os.environ.copy(), log=log, timeout=10)
            self.assertEqual((failed.returncode, passed.returncode), (7, 0))
            attempts = list(root.glob("execution.attempts/*/receipt.json"))
            self.assertEqual(len(attempts), 2)
            records = [json.loads(path.read_text()) for path in attempts]
            self.assertEqual({record["exit_code"] for record in records}, {7, 0})
            original = next(path.parent for path in attempts
                            if json.loads(path.read_text())["exit_code"] == 7)
            self.assertIn("original failure", (original / "stdout.txt").read_text())

    def test_timeout_reaps_actual_descendant_process(self):
        from tools.dev.process import run
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            marker = root / "survived.txt"
            child = ("import pathlib,time; time.sleep(3); "
                     f"pathlib.Path({str(marker)!r}).write_text('survived')")
            parent = ("import subprocess,sys,time; "
                      f"subprocess.Popen([sys.executable,'-c',{child!r}]); "
                      "print('started',flush=True); time.sleep(10)")
            started = time.monotonic()
            result = run([sys.executable, "-c", parent], cwd=root,
                         env=os.environ.copy(), log=root / "timeout.txt",
                         timeout=0.15)
            self.assertEqual(result.returncode, 124)
            self.assertLess(time.monotonic() - started, 5)
            time.sleep(1.1)
            self.assertFalse(marker.exists())
            record = json.loads((root / "timeout.command.json").read_text())
            self.assertEqual(record["outcome"], "timed_out")
            self.assertIn("started", (root / "timeout.txt").read_text())

    def test_missing_executable_is_recorded_as_launch_failure(self):
        from tools.dev.process import run
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = run([str(root / "missing")], cwd=root,
                         env={}, log=root / "missing.txt", timeout=1)
            self.assertEqual(result.returncode, 127)
            record = json.loads((root / "missing.command.json").read_text())
            self.assertEqual(record["outcome"], "launch_failed")

    def test_repeated_cancellation_during_timeout_cannot_escape_cleanup(self):
        import signal
        from tools.dev import dev
        from tools.dev.process import _linux_process
        linux = os.name == "posix" and Path("/proc/self/stat").is_file()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            marker, started = root / "survived.txt", root / "started.txt"
            owned_pid, leaf_pid = root / "owned.pid", root / "leaf.pid"
            withdrawing, proceed = root / "withdrawing", root / "proceed"
            term_seen, int_seen = root / "term-seen", root / "int-seen"
            child = ("import os,pathlib,signal,time; "
                     "signal.signal(signal.SIGTERM,signal.SIG_IGN); "
                     f"pathlib.Path({str(leaf_pid)!r}).write_text(str(os.getpid())); "
                     f"pathlib.Path({str(started)!r}).write_text('started'); "
                     "time.sleep(30); "
                     f"pathlib.Path({str(marker)!r}).write_text('survived')")
            parent = ("import os,pathlib,signal,subprocess,sys,time; "
                      "signal.signal(signal.SIGTERM,signal.SIG_IGN); "
                      f"pathlib.Path({str(owned_pid)!r}).write_text(str(os.getpid())); "
                      f"subprocess.Popen([sys.executable,'-c',{child!r}]); "
                      "time.sleep(10)")
            # Hold the real timeout withdrawal at its entry until both real
            # cancellation handlers have returned. Elapsed sleeps cannot prove
            # that run() has not already restored the caller's signal handlers.
            launcher = ("import os,pathlib,signal,sys,time\n"
                        f"sys.path.insert(0,{str(dev.ROOT)!r})\n"
                        "from tools.dev import process as execution\n"
                        "terminate=execution._terminate\n"
                        "def synchronized_terminate(child):\n"
                        " handlers={s:signal.getsignal(s) for s in "
                        "(signal.SIGTERM,signal.SIGINT)}\n"
                        " def observed(signum,frame):\n"
                        "  handlers[signum](signum,frame)\n"
                        f"  path={str(term_seen)!r} if signum==signal.SIGTERM "
                        f"else {str(int_seen)!r}\n"
                        "  pathlib.Path(path).write_text('handled')\n"
                        " for signum in handlers: signal.signal(signum,observed)\n"
                        f" pathlib.Path({str(withdrawing)!r}).write_text('ready')\n"
                        " deadline=time.monotonic()+5\n"
                        f" while not pathlib.Path({str(proceed)!r}).exists():\n"
                        "  if time.monotonic()>=deadline: "
                        "raise RuntimeError('withdrawal handshake timed out')\n"
                        "  time.sleep(0.001)\n"
                        " return terminate(child)\n"
                        "execution._terminate=synchronized_terminate\n"
                        f"result=execution.run([sys.executable,'-c',{parent!r}], "
                        f"cwd={str(root)!r},env=os.environ.copy(), "
                        f"log=pathlib.Path({str(root / 'timeout.txt')!r}), "
                        "timeout=0.35); raise SystemExit(result.returncode)")
            process = subprocess.Popen([sys.executable, "-c", launcher],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

            def wait_for(path):
                deadline = time.monotonic() + 5
                while not path.exists() and time.monotonic() < deadline:
                    if process.poll() is not None:
                        out, err = process.communicate(timeout=5)
                        self.fail(f"Launcher exited before {path.name}: " + out + err)
                    time.sleep(0.001)
                self.assertTrue(path.exists(), "Missing handshake: " + path.name)

            identities = []
            try:
                wait_for(withdrawing)
                wait_for(started)
                if linux:
                    identities = [_linux_process(int(path.read_text()))
                                  for path in (owned_pid, leaf_pid)]
                    self.assertTrue(all(identity is not None for identity in identities))
                process.send_signal(signal.SIGTERM)
                wait_for(term_seen)
                process.send_signal(signal.SIGINT)
                wait_for(int_seen)
                proceed.write_text("continue actual withdrawal")
                out, err = process.communicate(timeout=5)
                self.assertEqual(process.returncode, 124, out + err)
                self.assertFalse(marker.exists())
                record = json.loads((root / "timeout.command.json").read_text())
                self.assertEqual(record["outcome"], "timed_out")
                self.assertTrue(record["withdrawal_complete"])
                if linux:
                    withdrawn = {item["pid"] for item in record["withdrawn_processes"]}
                    self.assertTrue({item["pid"] for item in identities} <= withdrawn)
                    for identity in identities:
                        current = _linux_process(identity["pid"])
                        self.assertTrue(current is None or
                                        current["start_time"] != identity["start_time"] or
                                        current["state"] in {"Z", "X"})
            finally:
                # A failed handshake must not leave the real fixture running.
                proceed.touch()
                if process.poll() is None:
                    process.kill()
                if os.name == "posix" and owned_pid.exists():
                    pid = int(owned_pid.read_text())
                    owner = _linux_process(pid) if linux else None
                    original = identities[0] if identities else None
                    if not linux or (owner is not None and (original is None or
                            owner["start_time"] == original["start_time"])):
                        try:
                            os.killpg(pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                process.communicate(timeout=5)

    def test_nested_runner_timeout_and_cancel_withdraw_all_owned_sessions(self):
        import signal
        from tools.dev import dev
        for mode in ("timeout", "cancel"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                marker, started = root / "survived.txt", root / "started.txt"
                pid_file = root / "leaf.pid"
                leaf = ("import os,pathlib,signal,time; "
                        "signal.signal(signal.SIGTERM,signal.SIG_IGN); "
                        f"pathlib.Path({str(pid_file)!r}).write_text(str(os.getpid())); "
                        f"pathlib.Path({str(started)!r}).write_text('ready'); "
                        "time.sleep(3); "
                        f"pathlib.Path({str(marker)!r}).write_text('survived')")
                inner = ("import os,pathlib,sys; "
                         f"sys.path.insert(0,{str(dev.ROOT)!r}); "
                         "from tools.dev.process import run; "
                         f"result=run([sys.executable,'-c',{leaf!r}], "
                         f"cwd={str(root)!r},env=os.environ.copy(), "
                         f"log=pathlib.Path({str(root / 'inner.txt')!r}), "
                         "timeout=10); raise SystemExit(result.returncode)")
                timeout = 0.35 if mode == "timeout" else 10
                outer = ("import os,pathlib,sys; "
                         f"sys.path.insert(0,{str(dev.ROOT)!r}); "
                         "from tools.dev.process import run; "
                         f"result=run([sys.executable,'-c',{inner!r}], "
                         f"cwd={str(root)!r},env=os.environ.copy(), "
                         f"log=pathlib.Path({str(root / 'outer.txt')!r}), "
                         f"timeout={timeout}); raise SystemExit(result.returncode)")
                process = subprocess.Popen([sys.executable, "-c", outer],
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                deadline = time.monotonic() + 5
                while not started.exists() and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(started.exists())
                if mode == "cancel":
                    process.send_signal(signal.SIGTERM)
                out, err = process.communicate(timeout=5)
                time.sleep(0.05)
                survived = marker.exists()
                from tools.dev.process import _linux_process
                live_leaf = _linux_process(int(pid_file.read_text()))
                alive = live_leaf is not None and live_leaf["state"] != "Z"
                if os.name == "posix" and pid_file.exists():
                    try:
                        os.killpg(int(pid_file.read_text()), signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                self.assertEqual(process.returncode,
                                 124 if mode == "timeout" else 130, out + err)
                self.assertFalse(survived, "Nested owned leaf escaped withdrawal")
                self.assertFalse(alive, "Nested owned leaf is still running")
                self.assertTrue((root / "inner.command.json").is_file())
                receipt = json.loads((root / "outer.command.json").read_text())
                self.assertEqual(receipt["outcome"],
                                 "timed_out" if mode == "timeout" else "cancelled")

    def test_timeout_never_executes_a_handler_that_forks_and_reparents(self):
        from tools.dev.process import run, _linux_process
        import signal
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            leaf_pid, handler = root / "leaf.pid", root / "handler.txt"
            leaf = ("import os,pathlib,time; "
                    f"pathlib.Path({str(leaf_pid)!r}).write_text(str(os.getpid())); "
                    "time.sleep(10)")
            parent = ("import os,pathlib,signal,subprocess,sys,time\n"
                      "def cancelled(*args):\n"
                      f" pathlib.Path({str(handler)!r}).write_text('handler ran')\n"
                      f" subprocess.Popen([sys.executable,'-c',{leaf!r}], "
                      "start_new_session=True)\n"
                      " os._exit(0)\n"
                      "signal.signal(signal.SIGTERM,cancelled)\n"
                      "time.sleep(10)\n")
            result = run([sys.executable, "-c", parent], cwd=root,
                         env=os.environ.copy(), log=root / "fork.txt",
                         timeout=0.2)
            time.sleep(0.05)
            escaped = _linux_process(int(leaf_pid.read_text())) if leaf_pid.exists() else None
            if escaped is not None:
                try:
                    os.killpg(escaped["pid"], signal.SIGKILL)
                except ProcessLookupError:
                    pass
            self.assertEqual(result.returncode, 124)
            self.assertFalse(handler.exists(), "Cleanup executed a forking signal handler")
            self.assertIsNone(escaped, "New session escaped after parent exited")

    def test_freeze_deadline_does_not_certify_a_partial_inspection(self):
        from tools.dev import process
        identity = {"pid": 123, "parent": 1, "group": 123,
                    "start_time": 1, "state": "T"}
        known = {123: identity.copy()}

        def slow_inspection(owned, deadline):
            del owned, deadline
            time.sleep(0.03)

        with patch.object(process, "_linux_signal"), \
                patch.object(process, "_linux_process", return_value=identity), \
                patch.object(process, "_linux_descendants", side_effect=slow_inspection):
            stable = process._linux_freeze(known, budget=0.01)
        self.assertFalse(stable)
        self.assertFalse(known[123]["freeze_stable"])

    def test_partial_freeze_cannot_publish_complete_withdrawal(self):
        from tools.dev import process
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(process, "_linux_freeze", return_value=False):
                result = process.run([sys.executable, "-c", "import time; time.sleep(2)"],
                    cwd=root, env=os.environ.copy(), log=root / "partial.txt", timeout=0.05)
            self.assertEqual(result.returncode, 124)
            record = json.loads((root / "partial.command.json").read_text())
            self.assertFalse(record["withdrawal_complete"])


class DeveloperWorkflowTests(unittest.TestCase):
    def test_interleaved_real_commands_keep_each_own_provenance_receipt(self):
        from concurrent.futures import ThreadPoolExecutor
        import threading
        from tools.dev import dev
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.31)\nproject(interleaved NONE)\n")
            (root / ".gitignore").write_text("build/\n")
            (root / "CMakePresets.json").write_text(json.dumps({"version": 6,
                "configurePresets": [{"name": name, "generator": "Ninja",
                    "binaryDir": "${sourceDir}/build/" + name} for name in ("a", "b")]}))
            environment = os.environ.copy()
            environment.update({"GIT_AUTHOR_NAME": "Fixture",
                "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
                "GIT_COMMITTER_NAME": "Fixture",
                "GIT_COMMITTER_EMAIL": "fixture@example.invalid"})
            for command in (["git", "init", "-q"], ["git", "add", "."],
                            ["git", "commit", "-qm", "fixture"]):
                subprocess.run(command, cwd=root, env=environment, check=True,
                               capture_output=True, timeout=10)
            original = dev.execute
            first_completed, second_completed = threading.Event(), threading.Event()

            def interleaved(command, environment):
                result = original(command, environment)
                if command[command.index("--preset") + 1] == "a":
                    first_completed.set()
                    if not second_completed.wait(5):
                        raise ValueError("Second command did not complete")
                else:
                    second_completed.set()
                return result

            with patch.object(dev, "ROOT", root), \
                    patch.object(dev, "execute", side_effect=interleaved), \
                    ThreadPoolExecutor(max_workers=2) as pool:
                first = pool.submit(dev.main, ["configure", "--preset", "a"])
                self.assertTrue(first_completed.wait(5))
                second = pool.submit(dev.main, ["configure", "--preset", "b"])
                self.assertEqual(second.result(timeout=5), 0)
                self.assertEqual(first.result(timeout=5), 0)
            receipts = []
            for name in ("a", "b"):
                report = root / "build" / name / "quality/configure-provenance.json"
                record = json.loads(report.read_text())
                command = record["commands"][0]
                self.assertEqual(command[command.index("--preset") + 1], name)
                receipt = Path(record["actual_execution"]["receipt"]["path"])
                receipts.append(receipt)
                actual = json.loads(receipt.read_text())
                self.assertIn(str(root / "build" / name),
                              Path(actual["stdout"]["path"]).read_text())
            self.assertNotEqual(receipts[0], receipts[1])
            self.assertTrue(all(path.name == "receipt.json" for path in receipts))
            from tools.dev import provenance
            expected = json.loads(receipts[0].read_text())["command"]
            with self.assertRaises(ValueError):
                provenance.capture(root, root / "build/a", receipts[1],
                    action="configure", expected_command=expected,
                    expected_cwd=root)
            with self.assertRaises(ValueError):
                provenance.capture(root, root / "build/a", receipts[0],
                    action="configure", expected_command=expected,
                    expected_cwd=root / "other")

    def test_real_configure_and_noop_build_capture_verified_provenance(self):
        from tools.dev import dev
        from tools.dev.qualification import verify_provenance
        from contextlib import redirect_stdout, redirect_stderr
        import io
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.31)\nproject(workflow NONE)\n")
            (root / ".gitignore").write_text("build/\n")
            (root / "CMakePresets.json").write_text(json.dumps({"version": 6,
                "configurePresets": [{"name": "fixture", "generator": "Ninja",
                    "binaryDir": "${sourceDir}/build/fixture"}],
                "buildPresets": [{"name": "fixture", "configurePreset": "fixture"}]}))
            environment = os.environ.copy()
            environment.update({"GIT_AUTHOR_NAME": "Fixture",
                "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
                "GIT_COMMITTER_NAME": "Fixture",
                "GIT_COMMITTER_EMAIL": "fixture@example.invalid"})
            for command in (["git", "init", "-q"], ["git", "add", "."],
                            ["git", "commit", "-qm", "fixture"]):
                subprocess.run(command, cwd=root, env=environment, check=True,
                               capture_output=True, timeout=10)
            for action in ("configure", "build"):
                output = io.StringIO()
                with patch.object(dev, "ROOT", root), \
                        redirect_stdout(output), redirect_stderr(output):
                    result = dev.main([action, "--preset", "fixture"])
                self.assertEqual(result, 0, output.getvalue())
                report = root / "build/fixture/quality" / (action + "-provenance.json")
                record = json.loads(report.read_text())
                verify_provenance(record)
                self.assertEqual(record["action"], action)
                self.assertEqual(record["status"], "identities_recorded")
                self.assertIn("plan, not per-object execution", record["scope"])

    def test_configure_and_build_automatically_capture_actual_command_provenance(self):
        from tools.dev import dev, provenance
        for action in ("configure", "build"):
            with self.subTest(action=action):
                with patch.object(dev, "execute", return_value=dev.ExecutionResult(
                        0, dev.ROOT / "build/quality/own/receipt.json")), \
                        patch.object(dev, "executable", return_value="cmake"), \
                        patch.object(provenance, "capture", return_value={}) as capture:
                    result = dev.main([action, "--preset", "native-debug"])
                self.assertEqual(result, 0)
                capture.assert_called_once_with(dev.ROOT,
                    dev.ROOT / "build/native-debug",
                    dev.ROOT / "build/quality/own/receipt.json",
                    action=action, expected_command=["cmake"] +
                    (["--build"] if action == "build" else []) +
                    ["--preset", "native-debug"], expected_cwd=dev.ROOT)

    def test_configure_binary_directory_override_is_preserved_in_provenance(self):
        from tools.dev import dev, provenance
        with patch.object(dev, "execute", return_value=dev.ExecutionResult(
                0, dev.ROOT / "build/quality/own/receipt.json")), \
                patch.object(dev, "executable", return_value="cmake"), \
                patch.object(provenance, "capture", return_value={}) as capture:
            self.assertEqual(dev.main(["configure", "-B", "build/override"]), 0)
        self.assertEqual(capture.call_args.args[1], dev.ROOT / "build/override")

    def test_failed_command_never_captures_success_and_capture_failure_is_reported(self):
        from tools.dev import dev, provenance
        from contextlib import redirect_stdout, redirect_stderr
        import io
        with patch.object(dev, "execute", return_value=7), \
                patch.object(dev, "executable", return_value="cmake"), \
                patch.object(provenance, "capture") as capture:
            self.assertEqual(dev.main(["build"]), 7)
        capture.assert_not_called()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakePresets.json").write_text(json.dumps({"version": 6,
                "configurePresets": [{"name": "cfg", "binaryDir": "build/cfg"}]}))
            with patch.object(dev, "ROOT", root), \
                    patch.object(dev, "execute", return_value=dev.ExecutionResult(
                        0, root / "build/quality/own/receipt.json")), \
                    patch.object(dev, "executable", return_value="cmake"), \
                    patch.object(provenance, "capture", side_effect=ValueError("changed output")), \
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                result = dev.main(["configure", "--preset", "cfg"])
            self.assertEqual(result, 2)
            failure = root / "build/quality/developer-provenance-failures"
            receipts = list(failure.glob("*.json"))
            self.assertEqual(len(receipts), 1)
            record = json.loads(receipts[0].read_text())
            self.assertEqual(record["status"], "capture_failed")
            self.assertIn("changed output", record["error"])
    def test_arm_doctor_fails_when_required_compiler_is_missing(self):
        from tools.dev import dev
        from tools.testing.test_dev import DeveloperCommandTests
        from contextlib import redirect_stderr, redirect_stdout
        import io
        fixture = DeveloperCommandTests()
        output = io.StringIO()
        with patch.object(dev, "workspace_identity", return_value=None), \
                patch.object(dev, "executable", side_effect=lambda name: name), \
                patch.object(dev.subprocess, "run", side_effect=fixture.probe), \
                patch.object(dev.shutil, "which", return_value=None), \
                redirect_stdout(output), redirect_stderr(output):
            result = dev.main(["doctor", "--target", "arm"])
        self.assertEqual(result, 2)
        self.assertIn("Missing arm-none-eabi-gcc required for arm", output.getvalue())

    def test_dispatch_keeps_prepare_opt_in_network_and_candidate_endpoint(self):
        from tools.dev import dev
        with patch.object(dev, "execute", return_value=7) as execute:
            result = dev.main(["prepare", "--scope", "arm", "--family", "stm32h743"])
        self.assertEqual(result, 7)
        self.assertEqual(execute.call_args.args[0], [sys.executable,
            "tools/dev/dependencies.py", "prepare", "--preset", "native-debug",
            "--scope", "arm",
            "--family", "stm32h743"])
        with patch.object(dev, "execute", return_value=0) as execute:
            self.assertEqual(dev.main(["prepare", "--preset",
                                       "stm32-nucleo-h743-baremetal-empty"]), 0)
        self.assertEqual(execute.call_args.args[0], [sys.executable,
            "tools/dev/dependencies.py", "prepare", "--preset",
            "stm32-nucleo-h743-baremetal-empty", "--scope", "preset"])
        with patch.object(dev, "execute", return_value=0) as execute:
            self.assertEqual(dev.main(["candidate", "--plan", "candidate.json",
                                       "--output", "out"]), 0)
        self.assertIn("--require-candidate", execute.call_args.args[0])

    def test_focused_test_follows_test_configure_mapping_and_labels_receipt(self):
        from contextlib import redirect_stdout
        import io
        from tools.dev import dev
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakePresets.json").write_text(json.dumps({"version": 6,
                "configurePresets": [{"name": "config", "binaryDir": "build/config"}],
                "testPresets": [{"name": "focused", "configurePreset": "config"}]}))
            report = root / "build/config/ctest-results.xml"

            def execute(command, environment):
                del environment
                report.write_text('<testsuite tests="1"><testcase name="chosen"/></testsuite>')
                return 0

            with patch.object(dev, "ROOT", root), \
                    patch.object(dev, "executable", return_value="ctest"), \
                    patch.object(dev, "execute", side_effect=execute), \
                    redirect_stdout(io.StringIO()):
                result = dev.main(["test", "--preset", "focused", "-R", "chosen"])
            self.assertEqual(result, 0)
            record = json.loads(report.with_suffix(".execution.json").read_text())
            self.assertEqual(record["kind"], "focused-development")
            self.assertFalse(record["qualification"])

    def test_all_new_presets_have_real_authored_assemblies_and_matching_builds(self):
        from tools.dev import dev
        from tools.dev.presets import selected_configure
        rows = json.loads((dev.ROOT / "CMakePresets.json").read_text())
        configurations = rows["configurePresets"]
        builds = {row["configurePreset"] for row in rows["buildPresets"]}
        names = {row["name"] for row in configurations}
        self.assertIn("native-step-debug", names)
        self.assertEqual(len({name for name in names
                              if name.startswith("stm32-nucleo-")}), 8)
        self.assertEqual(len({name for name in names
                              if name.startswith("fixture-stm32-g474-")}), 14)
        for row in configurations:
            self.assertIn(row["name"], builds)
            selected = selected_configure(dev.ROOT, row["name"])
            assembly = selected["cacheVariables"]["NEXUS_ASSEMBLY_FILE"]
            self.assertTrue(Path(assembly).is_file(), assembly)
            if "g474" in row["name"]:
                self.assertTrue(row["name"].startswith("fixture-"))
                self.assertIn("unqualified", row["displayName"])


if __name__ == "__main__":
    unittest.main()
