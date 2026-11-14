"""One kernel lock per managed build tree, retained across owned subprocesses.

POSIX children receive real lock descriptors, not a setting that bypasses
exclusion. Windows supports standalone commands; nested managed commands reject
explicitly because its byte locks do not provide the same inheritance contract.
Direct CMake invocations do not participate in this coordination protocol.
"""

from contextlib import contextmanager
from contextvars import ContextVar
import json
import os
from pathlib import Path
import stat
import threading

LEASES = "NEXUS_BUILD_LEASES"
_held = ContextVar("nexus_build_locks", default={})
_borrowed = {}
_borrowed_guard = threading.Lock()


def _directory(build):
    directory = Path(build).absolute()
    if any(part.is_symlink() for part in (directory, *directory.parents)):
        raise ValueError("Build lock cannot contain a symlink")
    return directory.resolve()


def _leases(environment):
    try:
        records = json.loads(environment.get(LEASES, "[]"))
        if (not isinstance(records, list) or any(
                not isinstance(item, dict) or set(item) != {"build", "fd"}
                or not isinstance(item["build"], str)
                or type(item["fd"]) is not int for item in records)):
            raise ValueError("Invalid inherited build lock")
        return records
    except (TypeError, json.JSONDecodeError) as error:
        raise ValueError("Invalid inherited build lock") from error


def _checked_descriptor(record):
    build = _directory(record["build"])
    if str(build) != record["build"] or record["fd"] < 0:
        raise ValueError("Invalid inherited build lock identity")
    path = build / ".nexus-build.lock"
    if path.is_symlink():
        raise ValueError("Build lock cannot contain a symlink")
    actual, expected = os.fstat(record["fd"]), path.stat()
    if (not stat.S_ISREG(actual.st_mode) or
            (actual.st_dev, actual.st_ino) != (expected.st_dev, expected.st_ino)):
        raise ValueError("Inherited descriptor differs from the build lock")
    return record["fd"]


def child_settings(environment):
    """Transfer active leases only with their real POSIX file descriptors."""
    result = dict(environment)
    records = {}
    for item in _leases(result):
        with _borrowed_guard:
            owner = _borrowed.get(item["build"])
            if owner is not None and owner[0] != threading.get_ident():
                continue
        if os.name == "posix":
            try:
                _checked_descriptor(item)
            except (OSError, ValueError):
                # An unmanaged intermediary may have closed inherited FDs.
                # Omit its claim; a later build entry must acquire a real lock.
                continue
        records[item["build"]] = item
    records.update({name: {"build": name, "fd": stream.fileno()}
                    for name, (owner, stream) in _held.get().items()
                    if owner == threading.get_ident() and not stream.closed})
    if not records:
        result.pop(LEASES, None)
        return result, {}
    values = list(records.values())
    descriptors = (tuple(_checked_descriptor(item) for item in values)
                   if os.name == "posix" else ())
    result[LEASES] = json.dumps(values, separators=(",", ":"))
    return result, {"pass_fds": descriptors} if os.name == "posix" else {}


@contextmanager
def _borrowed_lease(name, inherited):
    """A delegated descriptor belongs to one local thread until its join."""
    if inherited is None:
        yield None
        return
    if os.name != "posix":
        raise ValueError("Nested managed build commands require POSIX "
                         "descriptor inheritance")
    owner = threading.get_ident()
    with _borrowed_guard:
        current = _borrowed.get(name)
        if current is not None and current[0] != owner:
            inherited = None
        else:
            _borrowed[name] = (owner, 1 if current is None else current[1] + 1)
    try:
        yield inherited
    finally:
        if inherited is not None:
            with _borrowed_guard:
                _, count = _borrowed[name]
                if count == 1:
                    del _borrowed[name]
                else:
                    _borrowed[name] = (owner, count - 1)


@contextmanager
def build_lock(build):
    """Reject concurrent owners; compose on one thread or a delegated child."""
    directory = _directory(build)
    name = str(directory)
    held = _held.get()
    owner = threading.get_ident()
    if (name in held and held[name][0] == owner and
            not held[name][1].closed):
        yield
        return
    inherited = next((item for item in _leases(os.environ)
                      if item["build"] == name), None)
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / ".nexus-build.lock"
    if path.is_symlink():
        raise ValueError("Build lock cannot contain a symlink")
    with _borrowed_lease(name, inherited) as inherited:
        stream = (os.fdopen(os.dup(_checked_descriptor(inherited)), "a+b")
                  if inherited is not None else path.open("a+b"))
        with stream:
            try:
                if os.name == "posix":
                    import fcntl
                    fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
                else:
                    import msvcrt
                    stream.seek(0)
                    if not stream.read(1):
                        stream.write(b"\0")
                        stream.flush()
                    stream.seek(0)
                    msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            except OSError as error:
                raise ValueError("Build directory is already owned: " + name) from error
            token = _held.set({**held, name: (owner, stream)})
            try:
                yield
            finally:
                _held.reset(token)
                # Closing the last POSIX reference releases the lock. Explicit
                # LOCK_UN would also unlock a still-running inherited owner.
                if os.name != "posix":
                    stream.seek(0)
                    msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
