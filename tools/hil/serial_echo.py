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
    EvidenceError, atomic_json, fields, file_identity, load_json,
    verify_file_identity,
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


def _baud_constant(baudrate: int) -> int:
    """Admit only an explicit positive rate with a native termios constant."""
    require(type(baudrate) is int and baudrate > 0,
            "serial baudrate must be a positive integer")
    speed = getattr(termios, "B" + str(baudrate), None)
    require(type(speed) is int and speed > 0,
            "serial baudrate is not available on this host")
    return speed


def _device_binding(expected_device):
    """Validate a parent's exact device identity before any terminal access."""
    if expected_device is None:
        return None
    fields(expected_device, {"path", "st_dev", "st_ino", "st_rdev"})
    name = expected_device["path"]
    require(isinstance(name, str) and "\0" not in name and len(name) <= 4096,
            "bound serial target path is invalid")
    path = Path(name)
    require(path.is_absolute() and path.is_relative_to(Path("/dev"))
            and str(path) == name and ".." not in path.parts,
            "bound serial target path must be canonical under /dev")
    identity = tuple(expected_device[key] for key in
                     ("st_dev", "st_ino", "st_rdev"))
    require(all(type(value) is int and value >= 0 for value in identity),
            "bound serial device identity must use nonnegative integers")
    return path, identity


_MONITOR_PHASES = {"acquiring": 0, "owned": 1, "releasing": 2, "released": 3}


def _monitor_phase_path(control, expected_device):
    fields(control, {"path", "nonce"})
    require(isinstance(control["nonce"], str)
            and re.fullmatch(r"[0-9a-f]{32}", control["nonce"]) is not None,
            "monitor phase requires an exact session nonce")
    require(isinstance(control["path"], str)
            and Path(control["path"]).is_absolute(),
            "monitor phase requires an absolute record path")
    require(_device_binding(expected_device) is not None,
            "monitor phase requires a bound device")
    return _path(Path(control["path"]))


def _begin_monitor_phase(control, expected_device):
    path = _monitor_phase_path(control, expected_device)
    _json_new(path, {"schema": 1, "kind": "monitor_terminal_phase",
                    "nonce": control["nonce"], "device": expected_device,
                    "phase": "acquiring", "sequence": 0})


def _read_monitor_phase(control, expected_device):
    path = _monitor_phase_path(control, expected_device)
    record = load_json(path, max_bytes=8192)
    fields(record, {"schema", "kind", "nonce", "device", "phase", "sequence"})
    _device_binding(record["device"])
    require(type(record["schema"]) is int and record["schema"] == 1
            and record["kind"] == "monitor_terminal_phase"
            and record["nonce"] == control["nonce"]
            and record["device"] == expected_device,
            "monitor phase session or device identity changed")
    require(isinstance(record["phase"], str)
            and record["phase"] in _MONITOR_PHASES
            and type(record["sequence"]) is int
            and record["sequence"] == _MONITOR_PHASES[record["phase"]],
            "monitor phase or sequence is invalid")
    return record


def _advance_monitor_phase(control, expected_device, previous, phase):
    record = _read_monitor_phase(control, expected_device)
    require(record["phase"] == previous
            and _MONITOR_PHASES[phase] == _MONITOR_PHASES[previous] + 1,
            "monitor terminal phase transition changed")
    atomic_json(_monitor_phase_path(control, expected_device), {
        **record, "phase": phase, "sequence": _MONITOR_PHASES[phase]})


@contextmanager
def _device(name: str, *, baudrate: int = 115200, read_only: bool = False,
            expected_device: dict | None = None,
            phase_control: dict | None = None):
    speed = _baud_constant(baudrate)
    require(type(read_only) is bool, "serial access mode must be boolean")
    binding = _device_binding(expected_device)
    if phase_control is not None:
        require(_read_monitor_phase(phase_control, expected_device)["phase"]
                == "acquiring", "monitor acquisition phase changed")
    target = _alias_target(name)
    require(binding is None or target == binding[0],
            "serial alias differs from parent-bound target")
    expected = target.stat()
    require(stat.S_ISCHR(expected.st_mode),
            "serial identity must name a character device")
    require(binding is None or (expected.st_dev, expected.st_ino,
                                expected.st_rdev) == binding[1],
            "serial device differs from parent-bound identity")
    access = os.O_RDONLY if read_only else os.O_RDWR
    descriptor = os.open(target, access | os.O_NOCTTY | os.O_NONBLOCK |
                         os.O_CLOEXEC | os.O_NOFOLLOW)
    original = None
    exclusive = False
    try:
        actual = os.fstat(descriptor)
        require(stat.S_ISCHR(actual.st_mode) and
                actual.st_dev == expected.st_dev and
                actual.st_rdev == expected.st_rdev and
                actual.st_ino == expected.st_ino and
                _alias_target(name) == target,
                "serial device identity changed during open")
        require(binding is None or (actual.st_dev, actual.st_ino,
                                    actual.st_rdev) == binding[1],
                "opened serial device differs from parent-bound identity")
        fcntl.ioctl(descriptor, termios.TIOCEXCL)
        exclusive = True
        if phase_control is not None:
            _advance_monitor_phase(phase_control, expected_device,
                                   "acquiring", "owned")
        original = termios.tcgetattr(descriptor)
        attributes = termios.tcgetattr(descriptor)
        attributes[0] = attributes[1] = attributes[3] = 0
        attributes[2] = termios.CLOCAL | termios.CREAD | termios.CS8
        attributes[4] = attributes[5] = speed
        attributes[6][termios.VMIN] = 0
        attributes[6][termios.VTIME] = 0
        termios.tcsetattr(descriptor, termios.TCSANOW, attributes)
        require(termios.tcgetattr(descriptor)[4:6] == [speed, speed],
                "serial baudrate setting was not applied by the driver")
        yield descriptor, {
            "path": str(target), "major": os.major(actual.st_rdev),
            "minor": os.minor(actual.st_rdev), "exclusive": "TIOCEXCL",
            "baudrate": baudrate, "data_bits": 8, "parity": "none",
            "stop_bits": 1,
        }
    finally:
        try:
            if exclusive and phase_control is not None:
                # Publish before clearing exclusive access. A parent observing
                # this phase cannot repeat cleanup after another opener wins.
                _advance_monitor_phase(phase_control, expected_device,
                                       "owned", "releasing")
            try:
                if original is not None:
                    termios.tcsetattr(descriptor, termios.TCSANOW, original)
                    require(termios.tcgetattr(descriptor) == original,
                            "saved serial attributes were not restored")
            finally:
                if exclusive:
                    fcntl.ioctl(descriptor, termios.TIOCNXCL)
        finally:
            os.close(descriptor)
        if exclusive and phase_control is not None:
            _advance_monitor_phase(phase_control, expected_device,
                                   "releasing", "released")


def _new_stream(path: Path):
    """Transfer an exclusive raw descriptor only after fdopen succeeds."""
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL |
                         os.O_NOFOLLOW, 0o600)
    try:
        return os.fdopen(descriptor, "wb")
    except BaseException:
        try:
            os.close(descriptor)
        except BaseException:
            pass  # Retain the conversion error; do not retry a recycled fd.
        raise


def _json_new(path: Path, value: dict) -> None:
    """O_EXCL keeps a late competing output from replacing retained bytes."""
    data = (json.dumps(value, indent=2, sort_keys=True, allow_nan=False) +
            "\n").encode()
    with _new_stream(_path(path)) as stream:
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
                self.streams[name] = _new_stream(path)
        except BaseException:
            try:
                self.close()
            except BaseException:
                pass  # All streams were attempted; preserve creation failure.
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
        # Defer even KeyboardInterrupt/SystemExit until every owned stream has
        # been attempted, then preserve the first failure instead of a later one.
        failure = None
        for stream in self.streams.values():
            if not stream.closed:
                try:
                    stream.flush()
                    os.fsync(stream.fileno())
                except BaseException as error:
                    if failure is None:
                        failure = error
                finally:
                    try:
                        stream.close()
                    except BaseException as error:
                        if failure is None:
                            failure = error
        if failure is not None:
            raise failure


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
