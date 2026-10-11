#!/usr/bin/env python3
"""Capture one admitted POSIX serial echo without qualifying hardware."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import re
import secrets
import select
import stat
import sys
import termios
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (
    EvidenceError, file_identity, load_json, verify_file_identity,
)
from tools.hil.admit import admit, require


_write = os.write
MAXIMUM_STREAM_BYTES = 65536
TIMING_SCOPE = (
    "Host open/configure/quiet-drain/read/write/close interval; "
    "host syscall timestamps, not MCU or wire latency"
)


def _path(value: Path) -> Path:
    path = Path(value).absolute()
    require(not any(part.is_symlink() for part in (path, *path.parents)),
            "serial capture input/output paths must not traverse symlinks")
    return path


def _reviewed(path: Path) -> tuple[dict, dict]:
    admission = load_json(path)
    require(admission.get("kind") == "hil_admission" and
            admission.get("status") == "ready", "ready admission required")
    inputs = [verify_file_identity(admission[name]) for name in
              ("station", "fixture", "elf", "resolved", "resources")]
    require(admit(*inputs) == admission,
            "serial admission is stale or inconsistent")
    return admission, load_json(inputs[0])


def _serial_name(value: str) -> Path:
    require(isinstance(value, str) and "\x00" not in value,
            "stable serial identity required")
    path = Path(value)
    require(path.parent == Path("/dev/serial/by-id") and
            str(path) == value and path.name not in ("", ".", ".."),
            "serial device must be one /dev/serial/by-id identity")
    return path


def _alias_target(value: str) -> Path:
    """The by-id leaf may be a normal device symlink, unlike evidence files."""
    path = _serial_name(value)
    require(not any(parent.is_symlink() for parent in path.parents),
            "serial identity directories must not traverse symlinks")
    target = path.resolve(strict=True)
    require(target.is_relative_to(Path("/dev")),
            "serial identity must resolve to a /dev device")
    return target


@contextmanager
def _device(name: str):
    target = _alias_target(name)
    expected = target.stat()
    require(stat.S_ISCHR(expected.st_mode),
            "serial identity must name a character device")
    descriptor = os.open(target, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK |
                         os.O_CLOEXEC | os.O_NOFOLLOW)
    original = None
    exclusive = False
    try:
        actual = os.fstat(descriptor)
        require(stat.S_ISCHR(actual.st_mode) and
                actual.st_rdev == expected.st_rdev and
                actual.st_ino == expected.st_ino and
                _alias_target(name) == target,
                "serial device identity changed during open")
        fcntl.ioctl(descriptor, termios.TIOCEXCL)
        exclusive = True
        original = termios.tcgetattr(descriptor)
        attributes = termios.tcgetattr(descriptor)
        attributes[0] = attributes[1] = attributes[3] = 0
        attributes[2] = termios.CLOCAL | termios.CREAD | termios.CS8
        attributes[4] = attributes[5] = termios.B115200
        attributes[6][termios.VMIN] = 0
        attributes[6][termios.VTIME] = 0
        termios.tcsetattr(descriptor, termios.TCSANOW, attributes)
        yield descriptor, {
            "path": str(target), "major": os.major(actual.st_rdev),
            "minor": os.minor(actual.st_rdev), "exclusive": "TIOCEXCL",
            "baudrate": 115200, "data_bits": 8, "parity": "none",
            "stop_bits": 1,
        }
    finally:
        try:
            if original is not None:
                termios.tcsetattr(descriptor, termios.TCSANOW, original)
        finally:
            try:
                if exclusive:
                    fcntl.ioctl(descriptor, termios.TIOCNXCL)
            finally:
                os.close(descriptor)


def _json_new(path: Path, value: dict) -> None:
    """O_EXCL keeps a late competing output from replacing retained bytes."""
    data = (json.dumps(value, indent=2, sort_keys=True, allow_nan=False) +
            "\n").encode()
    descriptor = os.open(_path(path), os.O_WRONLY | os.O_CREAT | os.O_EXCL |
                         os.O_NOFOLLOW, 0o600)
    with os.fdopen(descriptor, "wb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())


def _raw_identity(path: Path) -> dict:
    data = _path(path).read_bytes()
    return {"path": str(path), "sha256": hashlib.sha256(data).hexdigest(),
            "size": len(data)}


class _Trace:
    """Each byte range records the actual host syscall that transferred it."""
    def __init__(self, directory: Path):
        self.paths = {name: directory / (name + ".bin") for name in
                      ("tx", "rx", "drain")}
        self.streams = {}
        self.sizes = dict.fromkeys(self.paths, 0)
        self.events = []
        try:
            for name, path in self.paths.items():
                descriptor = os.open(path, os.O_WRONLY | os.O_CREAT |
                                     os.O_EXCL | os.O_NOFOLLOW, 0o600)
                self.streams[name] = os.fdopen(descriptor, "wb")
        except BaseException:
            self.close()
            raise

    def retain(self, name: str, data: bytes, started: int, finished: int):
        # The read bound permits one overflow byte. Retain every byte already
        # observed, then reject without issuing any further device read.
        self.streams[name].write(data)
        self.streams[name].flush()
        self.events.append({
            "sequence": len(self.events), "direction": name,
            "offset": self.sizes[name], "size": len(data),
            "started_host_ns": started, "finished_host_ns": finished,
        })
        self.sizes[name] += len(data)
        require(self.sizes[name] <= MAXIMUM_STREAM_BYTES,
                "serial raw stream exceeded capture bound")

    def close(self):
        for stream in self.streams.values():
            if not stream.closed:
                try:
                    stream.flush()
                    os.fsync(stream.fileno())
                finally:
                    stream.close()


def _ready(descriptor: int, deadline: float, *, write: bool = False) -> bool:
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return False
        try:
            readers, writers, _ = select.select(
                [] if write else [descriptor],
                [descriptor] if write else [], [], remaining)
            return bool(writers if write else readers)
        except InterruptedError:
            # A signal does not extend the absolute host capture limit.
            continue


def _read(descriptor: int, trace: _Trace, name: str) -> bytes | None:
    started = time.monotonic_ns()
    try:
        data = os.read(descriptor, min(4096,
            MAXIMUM_STREAM_BYTES - trace.sizes[name] + 1))
    except (BlockingIOError, InterruptedError):
        return None
    finished = time.monotonic_ns()
    require(bool(data), "serial device hung up during capture")
    trace.retain(name, data, started, finished)
    return data


def _quiet(descriptor: int, trace: _Trace, deadline: float, quiet_s: float):
    quiet_until = time.monotonic() + quiet_s
    while True:
        require(time.monotonic() < deadline,
                "serial capture deadline expired before quiet drain")
        if not _ready(descriptor, min(deadline, quiet_until)):
            if time.monotonic() >= quiet_until:
                return
            raise EvidenceError("serial capture deadline expired during drain")
        if _read(descriptor, trace, "drain") is not None:
            quiet_until = time.monotonic() + quiet_s


def _echo(descriptor: int, trace: _Trace, deadline: float) -> None:
    # Generate after quiet drain so pre-existing input cannot supply the echo.
    challenge = secrets.token_bytes(32)
    sent = 0
    while sent < len(challenge):
        require(_ready(descriptor, deadline, write=True),
                "serial capture deadline expired during write")
        started = time.monotonic_ns()
        try:
            count = _write(descriptor, challenge[sent:])
        except (BlockingIOError, InterruptedError):
            continue
        finished = time.monotonic_ns()
        require(0 < count <= len(challenge) - sent,
                "serial write made no valid progress")
        trace.retain("tx", challenge[sent:sent + count], started, finished)
        sent += count
    received = bytearray()
    while len(received) < len(challenge):
        require(_ready(descriptor, deadline),
                "serial capture deadline expired before complete echo")
        data = _read(descriptor, trace, "rx")
        if data is None:
            continue
        received.extend(data)
        require(challenge.startswith(received),
                "serial echo differs from fresh random challenge")
    require(bytes(received) == challenge, "incomplete serial echo")


def capture_serial(admission_path: Path, session: str, record_path: Path,
                   *, timeout_s: float = 3.0, quiet_s: float = 0.05) -> dict:
    """Retain raw I/O; an echo capture never establishes HIL qualification.

    Limits bound this host operation, not MCU deadlines or product budgets.
    Only the serial device is opened; admission revalidation reads files.
    """
    require(isinstance(session, str) and
            re.fullmatch(r"[0-9a-f]{32}", session) is not None,
            "fresh acquisition session token required")
    for value, label in ((timeout_s, "timeout"), (quiet_s, "quiet window")):
        require(type(value) in (int, float) and math.isfinite(value) and
                value > 0,
                "finite positive host capture " + label + " required")
    require(timeout_s <= 60 and quiet_s <= min(0.5, timeout_s / 2),
            "host capture limits outside reviewed bounded range")
    admission_path, record_path = (_path(path) for path in
                                   (admission_path, record_path))
    admission, station = _reviewed(admission_path)
    serial = station["serial"]
    _serial_name(serial["path"])
    require(serial["baudrate"] == 115200 and
            serial["electrical_mode"] == "ttl_3v3",
            "admitted serial configuration must be 115200 TTL 3.3V")
    directory = _path(record_path.with_name(record_path.name + ".serial"))
    protected = [admission_path, *[Path(admission[name]["path"]) for name in
                 ("station", "fixture", "elf", "resolved", "resources")]]
    require(not record_path.exists() and not directory.exists() and
            not any(path == record_path or path.is_relative_to(directory)
                    for path in protected),
            "fresh serial capture must not overwrite inputs or old output")
    original_admission = file_identity(admission_path)
    record_path.parent.mkdir(parents=True, exist_ok=True)
    _path(record_path)
    directory.mkdir(exist_ok=False, mode=0o700)
    trace = _Trace(directory)
    started = time.monotonic_ns()
    deadline = time.monotonic() + timeout_s
    report = {
        "schema_version": 1, "kind": "hil_serial_capture", "status": "failed",
        "session": session, "admission": original_admission,
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "backend": "posix_termios", "serial_path": serial["path"],
        "timing_scope": TIMING_SCOPE, "timeout_s": timeout_s,
        "quiet_s": quiet_s, "qualification": "not_established",
        "failure": "capture did not complete",
    }
    failure = None
    try:
        with _device(serial["path"]) as (descriptor, device):
            report["device"] = device
            _quiet(descriptor, trace, deadline, quiet_s)
            _echo(descriptor, trace, deadline)
        verify_file_identity(original_admission)
        _reviewed(admission_path)
        report["status"] = "captured"
        del report["failure"]
    except (EvidenceError, OSError, KeyError) as error:
        failure = EvidenceError("serial capture rejected: " + str(error))
        report["failure"] = str(error)
    finally:
        trace.close()
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        report["elapsed_host_us"] = (time.monotonic_ns() - started) // 1000
        report.update({name: _raw_identity(path) for name, path in
                       trace.paths.items()})
        events = directory / "events.json"
        _json_new(events, {"schema_version": 1, "clock": "host_monotonic_ns",
                           "timing_scope": TIMING_SCOPE,
                           "events": trace.events})
        report["events"] = file_identity(events)
        _json_new(directory / "capture.json", report)
    if failure is not None:
        raise failure
    observation = {
        "schema_version": 1, "kind": "hil_adapter_observation",
        "operation": "serial", "session": session,
        "observations": {key: value for key, value in report.items() if key in
            ("status", "backend", "serial_path", "device", "admission",
             "tx", "rx", "drain", "events", "elapsed_host_us", "timing_scope",
             "qualification", "timeout_s", "quiet_s")},
    }
    _json_new(record_path, observation)
    return observation


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--admission", type=Path, required=True)
    parser.add_argument("--session", required=True)
    parser.add_argument("--record", type=Path, required=True)
    parser.add_argument("--timeout-s", type=float, default=3.0)
    parser.add_argument("--quiet-s", type=float, default=0.05)
    args = parser.parse_args(argv)
    try:
        capture_serial(args.admission, args.session, args.record,
                       timeout_s=args.timeout_s, quiet_s=args.quiet_s)
    except (EvidenceError, OSError, KeyError) as error:
        print("Serial capture rejected: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
