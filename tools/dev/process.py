"""Bounded subprocess execution with retained attempt-level evidence."""

import contextlib
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import threading
import time
import uuid


def positive_integer(value, name):
    """Reject unlimited or malformed execution budgets."""
    try:
        result = int(value)
    except (ValueError, TypeError) as error:
        raise ValueError(name + " must be a positive integer") from error
    if result < 1 or str(result) != str(value):
        raise ValueError(name + " must be a positive integer")
    return result


def execution_environment(environment):
    """Bound Ninja by default while honoring an explicit caller budget."""
    result = dict(environment)
    selected = result.get("NEXUS_BUILD_JOBS",
                          result.get("CMAKE_BUILD_PARALLEL_LEVEL", "2"))
    result["CMAKE_BUILD_PARALLEL_LEVEL"] = str(positive_integer(
        selected, "NEXUS_BUILD_JOBS/CMAKE_BUILD_PARALLEL_LEVEL"))
    if "CTEST_PARALLEL_LEVEL" in result:
        positive_integer(result["CTEST_PARALLEL_LEVEL"], "CTEST_PARALLEL_LEVEL")
    return result


def command_timeout(environment, default=1800):
    """The caller can select a finite deadline, never disable it."""
    value = float(environment.get("NEXUS_COMMAND_TIMEOUT", default))
    if not 0 < value < float("inf"):
        raise ValueError("NEXUS_COMMAND_TIMEOUT must be finite and positive")
    return value


def _linux_process(pid):
    """Bind a process identity to its kernel start time, never just its PID."""
    try:
        with open(f"/proc/{pid}/stat", encoding="utf-8") as stream:
            contents = stream.read()
        fields = contents.rsplit(")", 1)[1].split()
        return {"pid": pid, "parent": int(fields[1]),
                "group": int(fields[2]), "start_time": int(fields[19]),
                "state": fields[0]}
    except (OSError, ValueError, IndexError):
        return None


def _linux_snapshot_descendants(known, deadline=None):
    snapshot = {}
    with os.scandir("/proc") as entries:
        for entry in entries:
            if deadline is not None and time.monotonic() >= deadline:
                break
            if entry.name.isdecimal():
                identity = _linux_process(int(entry.name))
                if identity is not None:
                    snapshot[identity["pid"]] = identity
    active = {pid for pid, identity in known.items()
              if pid in snapshot and snapshot[pid]["start_time"] ==
              identity["start_time"]}
    changed = True
    while changed:
        changed = False
        for pid, identity in snapshot.items():
            if pid not in active and identity["parent"] in active:
                known[pid] = identity
                active.add(pid)
                changed = True


def _linux_descendants(known, deadline=None):
    """Walk owned child lists; kernels without them use one PPID snapshot."""
    pending = list(known)
    visited = set()
    while pending:
        if deadline is not None and time.monotonic() >= deadline:
            return False
        pid = pending.pop()
        if pid in visited:
            continue
        visited.add(pid)
        current = _linux_process(pid)
        if (current is None or current["start_time"] !=
                known[pid]["start_time"]):
            continue
        try:
            tasks = list(Path("/proc", str(pid), "task").iterdir())
        except OSError:
            continue
        for task in tasks:
            try:
                children = (task / "children").read_text().split()
            except FileNotFoundError:
                # CONFIG_CHECKPOINT_RESTORE is optional on maintained Linux.
                _linux_snapshot_descendants(known, deadline)
                return False
            except OSError:
                continue
            for name in children:
                identity = _linux_process(int(name))
                if identity is None or identity["parent"] != pid:
                    continue
                child = identity["pid"]
                if child not in known:
                    known[child] = identity
                if known[child]["start_time"] == identity["start_time"]:
                    pending.append(child)
    return True


def _linux_signal(known, signum):
    # Descendants were captured before any parent was allowed to exit. Sessions
    # created by nested owned runners remain addressable after reparenting.
    selected = list(known.items())
    if signum != signal.SIGSTOP:
        selected.reverse()
    for pid, identity in selected:
        current = _linux_process(pid)
        if (current is None or current["start_time"] != identity["start_time"]
                or current["state"] == "Z"):
            continue
        try:
            if signum == signal.SIGSTOP and current["group"] == pid:
                # Every member of this owned leader's group stops together.
                os.killpg(pid, signum)
            else:
                os.kill(pid, signum)
        except ProcessLookupError:
            pass


def _linux_stopped(known):
    """Require a matching identity to be stopped or no longer executable."""
    for pid, identity in known.items():
        current = _linux_process(pid)
        if (current is not None
                and current["start_time"] == identity["start_time"]
                and current["state"] not in ("T", "t", "Z", "X")):
            return False
    return True


def _linux_freeze(known, budget=5):
    """Stop parents before inspecting children; never run a TERM handler."""
    deadline = time.monotonic() + budget
    _linux_signal(known, signal.SIGSTOP)
    while not _linux_stopped(known) and time.monotonic() < deadline:
        time.sleep(0.001)
    stable = False
    while time.monotonic() < deadline:
        previous = {(pid, identity["start_time"])
                    for pid, identity in known.items()}
        _linux_descendants(known, deadline)
        _linux_signal(known, signal.SIGSTOP)
        while not _linux_stopped(known) and time.monotonic() < deadline:
            time.sleep(0.001)
        current = {(pid, identity["start_time"])
                   for pid, identity in known.items()}
        if (time.monotonic() < deadline and previous == current
                and _linux_stopped(known)):
            stable = True
            break
    for pid, identity in known.items():
        current = _linux_process(pid)
        identity["freeze_state"] = (
            current["state"] if current is not None and
            current["start_time"] == identity["start_time"] else "gone")
        identity["freeze_stable"] = stable
    return stable


def _terminate(process):
    """Withdraw the whole owned process group before returning cancellation."""
    if Path("/proc/self/stat").is_file() and os.name == "posix":
        identity = _linux_process(process.pid)
        known = {} if identity is None else {process.pid: identity}
        stable = _linux_freeze(known)
        _linux_signal(known, signal.SIGKILL)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            # Userspace cannot force uninterruptible kernel work to be reaped.
            # Retain the original failure and publish outstanding identities.
            pass
        deadline = time.monotonic() + 0.5
        while time.monotonic() < deadline:
            remaining = False
            for pid, owned in known.items():
                current = _linux_process(pid)
                if (current is not None and
                        current["start_time"] == owned["start_time"]
                        and current["state"] not in ("Z", "X")):
                    remaining = True
                    break
            if not remaining:
                break
            time.sleep(0.005)
        for pid, owned in known.items():
            current = _linux_process(pid)
            owned["withdrawal_state"] = (
                current["state"] if current is not None and
                current["start_time"] == owned["start_time"] else "gone")
            owned["withdrawal_complete"] = (
                owned["withdrawal_state"] in ("gone", "Z", "X"))
        return list(known.values()), stable
    if os.name == "posix":
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            process.wait(timeout=10)
            return [], True
        try:
            process.wait(timeout=0.2)
        except subprocess.TimeoutExpired:
            pass
        # The parent may exit before its children. Kill the group regardless.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    else:
        # CREATE_NEW_PROCESS_GROUP alone does not kill descendant processes.
        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       check=False, timeout=10)
        if process.poll() is None:
            process.kill()
    process.wait(timeout=10)
    return [], True


def _identity(path):
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": digest}


def _write_json(path, record):
    """Publish complete receipts while preserving the last complete view."""
    temporary = path.with_name(path.name + ".tmp-" + uuid.uuid4().hex)
    try:
        with temporary.open("x") as stream:
            json.dump(record, stream, indent=2)
            stream.write("\n")
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def run(command, *, cwd, env, log=None, timeout=None, stage="command",
        input=None, cancel_event=None):
    """Execute with cancellation deferred until cleanup and receipts finish."""
    state = {"withdrawing": False}
    previous = {}

    def cancel(signum, frame):
        del signum, frame
        if not state["withdrawing"]:
            state["withdrawing"] = True
            raise KeyboardInterrupt

    if threading.current_thread() is threading.main_thread():
        for signum in (signal.SIGINT, signal.SIGTERM):
            previous[signum] = signal.getsignal(signum)
            signal.signal(signum, cancel)
    try:
        return _run(command, cwd=cwd, env=env, log=log, timeout=timeout,
                    stage=stage, state=state, input=input,
                    cancel_event=cancel_event)
    finally:
        for signum, handler in previous.items():
            signal.signal(signum, handler)


def _run(command, *, cwd, env, log, timeout, stage, state, input=None,
         cancel_event=None):
    """Return actual results and preserve every failure across later retries.

    A timeout returns 124 after process-tree withdrawal; cancellation returns
    130. Launch errors return 127. Raw stdout/stderr remain in independent
    attempt directories. The conventional log is only a latest-result view.
    """
    command = [os.fspath(item) for item in command]
    if not command or any("\0" in item for item in command):
        raise ValueError("Execution requires a nonempty valid argument vector")
    if input is not None and not isinstance(input, bytes):
        raise ValueError("Execution input must be bytes")
    env = execution_environment(env)
    timeout = command_timeout(env) if timeout is None else float(timeout)
    if not 0 < timeout < float("inf"):
        raise ValueError("Command timeout must be finite and positive")
    temporary = None
    if log is None:
        temporary = tempfile.TemporaryDirectory(prefix="nexus-command-")
        attempt = Path(temporary.name)
    else:
        log = Path(log)
        if log.is_symlink() or any(part.is_symlink() for part in log.parents):
            raise ValueError("Execution log cannot use a symlink path")
        attempt_root = log.parent / (log.stem + ".attempts")
        if attempt_root.is_symlink():
            raise ValueError("Execution attempt root cannot be a symlink")
        attempt = attempt_root / (str(time.time_ns()) + "-" + uuid.uuid4().hex)
        attempt.mkdir(parents=True, exist_ok=False)
    stdout_path, stderr_path = attempt / "stdout.txt", attempt / "stderr.txt"
    stdin_path = attempt / "stdin.bin" if input is not None else None
    if stdin_path is not None:
        stdin_path.write_bytes(input)
    started_ns, started = time.time_ns(), time.monotonic()
    outcome, returncode, process = "completed", 127, None
    withdrawn = []
    withdrawal_stable = True
    started_record = {
        "schema": 1, "command": command, "cwd": str(Path(cwd).resolve()),
        "stage": stage, "started_ns": started_ns, "outcome": "running",
        "exit_code": None, "timeout_seconds": timeout,
        "stdout_path": str(stdout_path), "stderr_path": str(stderr_path),
    }
    if stdin_path is not None:
        started_record["stdin"] = _identity(stdin_path)
    # A parent deadline can kill this runner. Retain a truthful started receipt
    # before launching its child; an interrupted attempt cannot look completed.
    _write_json(attempt / "receipt.json", started_record)
    if log is not None:
        receipt_view = log.with_suffix(".command.json")
        if receipt_view.is_symlink():
            raise ValueError("Execution receipt cannot overwrite a symlink")
        _write_json(receipt_view, started_record)
    with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr, \
            (stdin_path.open("rb") if stdin_path is not None else
             contextlib.nullcontext(None)) as stdin:
        try:
            if cancel_event is not None and cancel_event.is_set():
                raise KeyboardInterrupt
            settings = ({"start_new_session": True} if os.name == "posix" else
                        {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP})
            process = subprocess.Popen(command, cwd=cwd, env=env,
                                       stdin=stdin, stdout=stdout,
                                       stderr=stderr, **settings)
            if cancel_event is None:
                returncode = process.wait(timeout=timeout)
            else:
                deadline = time.monotonic() + timeout
                while True:
                    if cancel_event.is_set():
                        raise KeyboardInterrupt
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise subprocess.TimeoutExpired(command, timeout)
                    try:
                        returncode = process.wait(timeout=min(0.05, remaining))
                        break
                    except subprocess.TimeoutExpired:
                        continue
        except subprocess.TimeoutExpired:
            outcome, returncode = "timed_out", 124
            state["withdrawing"] = True
            withdrawn, withdrawal_stable = _terminate(process)
        except KeyboardInterrupt:
            outcome, returncode = "cancelled", 130
            state["withdrawing"] = True
            if process is not None:
                withdrawn, withdrawal_stable = _terminate(process)
        except OSError as error:
            outcome, returncode = "launch_failed", 127
            stderr.write((str(error) + "\n").encode())
        finally:
            # Further signals cannot interrupt withdrawal or retained receipts.
            # Restore the caller handlers only after _run has retained evidence.
            state["withdrawing"] = True
    record = {
        "schema": 1, "command": command, "cwd": str(Path(cwd).resolve()),
        "stage": stage, "started_ns": started_ns, "finished_ns": time.time_ns(),
        "duration_seconds": time.monotonic() - started,
        "timeout_seconds": timeout, "outcome": outcome,
        "exit_code": returncode,
        "child_returncode": process.returncode if process is not None else None,
        "withdrawn_processes": withdrawn,
        "withdrawal_stable": withdrawal_stable,
        "withdrawal_complete": withdrawal_stable and all(
            item.get("withdrawal_complete", True) for item in withdrawn),
        "build_parallelism": int(env["CMAKE_BUILD_PARALLEL_LEVEL"]),
        "stdout": _identity(stdout_path), "stderr": _identity(stderr_path),
    }
    if stdin_path is not None:
        record["stdin"] = _identity(stdin_path)
    _write_json(attempt / "receipt.json", record)
    out = stdout_path.read_text(errors="replace")
    err = stderr_path.read_text(errors="replace")
    if log is not None:
        if log.with_suffix(".command.json").is_symlink():
            raise ValueError("Execution receipt cannot overwrite a symlink")
        log.write_text(out + err)
        record["attempt_receipt"] = str(attempt / "receipt.json")
        _write_json(log.with_suffix(".command.json"), record)
    if temporary is not None:
        temporary.cleanup()
    result = subprocess.CompletedProcess(command, returncode, out, err)
    result.receipt_path = attempt / "receipt.json" if log is not None else None
    return result
