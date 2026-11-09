"""Stable serial identity, exclusive ownership and terminal restoration.

Shared by daily developer operations and HIL adapters; no HIL admission or
workload framework is required by these host device primitives.
"""

from __future__ import annotations

from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import re
import stat
import termios

from tools.evidence.common import EvidenceError, atomic_json, fields, load_json


def require(condition: bool, message: str) -> None:
    if not condition:
        raise EvidenceError(message)


def _path(value: Path) -> Path:
    path = Path(value).absolute()
    require(not any(part.is_symlink() for part in (path, *path.parents)),
            "serial capture input/output paths must not traverse symlinks")
    return path


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
