"""Shared bounded adapters, strict input parsing and durable public reports."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time
from typing import Any


class EvidenceError(ValueError):
    """A required delivery invariant could not be established."""


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result = {}
    for key, value in pairs:
        if key in result:
            raise EvidenceError(f"duplicate JSON field: {key}")
        result[key] = value
    return result


def parse_json(data: str | bytes) -> dict[str, Any]:
    def invalid_constant(_value):
        raise EvidenceError("nonfinite JSON numbers are not accepted")
    try:
        value = json.loads(data, object_pairs_hook=_unique_object, parse_constant=invalid_constant)
    except (UnicodeError, json.JSONDecodeError) as error:
        raise EvidenceError("invalid JSON document") from error
    if not isinstance(value, dict):
        raise EvidenceError("JSON document must be an object")
    return value


def load_json(path: str | Path) -> dict[str, Any]:
    file = regular_file(path)
    if file.stat().st_size > 4 * 1024 * 1024:
        raise EvidenceError("JSON document exceeds 4 MiB")
    return parse_json(file.read_bytes())


def fields(value: dict, required: set[str], optional: set[str] = frozenset()) -> None:
    if not isinstance(value, dict):
        raise EvidenceError("expected object")
    missing = required - value.keys()
    unknown = value.keys() - required - optional
    if missing or unknown:
        raise EvidenceError(f"invalid fields; missing={sorted(missing)}, unknown={sorted(unknown)}")


def identifier(value: Any, label: str = "identifier") -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}", value):
        raise EvidenceError(f"invalid {label}")
    return value


def sha256_value(value: Any) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value):
        raise EvidenceError("expected lowercase SHA-256")
    return value


def regular_file(path: str | Path, *, nonempty: bool = True) -> Path:
    file = Path(path).absolute()
    if any(part.is_symlink() for part in [file, *file.parents]):
        raise EvidenceError(f"symlink file path is not accepted: {file.name}")
    if not file.is_file() or (nonempty and file.stat().st_size == 0):
        raise EvidenceError(f"required regular file missing or empty: {file.name}")
    return file


def digest(path: str | Path) -> str:
    with regular_file(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def file_identity(path: str | Path) -> dict[str, Any]:
    file = regular_file(path)
    return {"path": str(file), "sha256": digest(file), "size": file.stat().st_size}


def verify_file_identity(identity: dict[str, Any]) -> Path:
    fields(identity, {"path", "sha256", "size"}, {"role"})
    sha256_value(identity["sha256"])
    if type(identity["size"]) is not int or identity["size"] <= 0:
        raise EvidenceError("invalid artifact size")
    file = regular_file(identity["path"])
    if file.stat().st_size != identity["size"] or digest(file) != identity["sha256"]:
        raise EvidenceError(f"artifact identity changed: {file.name}")
    return file


def atomic_json(path: str | Path, value: dict[str, Any]) -> None:
    destination = Path(path).absolute()
    destination.parent.mkdir(parents=True, exist_ok=True)
    if any(part.is_symlink() for part in [destination, *destination.parents]):
        raise EvidenceError("report destination must not be a symlink")
    data = (json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + "\n").encode()
    descriptor, temporary = tempfile.mkstemp(prefix=".nexus-report-", dir=destination.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, destination)
        if os.name == "posix":
            parent = os.open(destination.parent, os.O_RDONLY)
            try:
                os.fsync(parent)
            finally:
                os.close(parent)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def command(argv: Any, substitutions: dict[str, str] | None = None) -> list[str]:
    if not isinstance(argv, list) or not argv or len(argv) > 64:
        raise EvidenceError("adapter command must be a nonempty argv list")
    substitutions = substitutions or {}
    expanded = []
    for argument in argv:
        if not isinstance(argument, str) or not argument or len(argument) > 4096 or "\0" in argument:
            raise EvidenceError("invalid adapter argument")
        for key, replacement in substitutions.items():
            argument = argument.replace("{" + key + "}", replacement)
        if re.search(r"\{[A-Za-z_][A-Za-z0-9_]*\}", argument):
            raise EvidenceError("unknown command placeholder")
        expanded.append(argument)
    return expanded


def run_adapter(argv: list[str], timeout_s: float, *, cwd: str | Path | None = None,
                output_limit: int = 1024 * 1024, env: dict[str, str] | None = None) -> bytes:
    """Run trusted adapter argv without a shell and kill its process group on timeout.

    Outputs are never interpolated into errors. Temporary files bound Python memory;
    adapters must also bound their own diagnostic output and filesystem consumption.
    """
    if isinstance(timeout_s, bool) or not isinstance(timeout_s, (int, float)) or not 0 < timeout_s <= 300:
        raise EvidenceError("adapter timeout must be in (0, 300] seconds")
    command(argv)
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        try:
            process = subprocess.Popen(argv, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                       stdout=stdout, stderr=stderr, shell=False,
                                       start_new_session=os.name == "posix")
        except OSError as error:
            raise EvidenceError("adapter executable unavailable") from error

        def stop_process_group():
            if os.name == "posix":
                try:
                    os.killpg(process.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
            else:
                process.terminate()
            try:
                process.wait(timeout=0.5)
            except subprocess.TimeoutExpired:
                if os.name == "posix":
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                else:
                    process.kill()
                process.wait(timeout=2)
            if os.name == "posix":
                # The parent may exit on SIGTERM while a child ignores it.
                # Hardware ownership must not outlive the failed command.
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass

        deadline = time.monotonic() + timeout_s
        try:
            while process.poll() is None:
                if (os.fstat(stdout.fileno()).st_size > output_limit or
                        os.fstat(stderr.fileno()).st_size > output_limit):
                    raise EvidenceError("adapter output exceeds configured limit")
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise EvidenceError("adapter timed out; process group stopped")
                try:
                    process.wait(timeout=min(remaining, 0.05))
                except subprocess.TimeoutExpired:
                    pass
        except BaseException:
            stop_process_group()
            raise
        if os.name == "posix":
            try:
                os.killpg(process.pid, 0)
            except ProcessLookupError:
                pass
            else:
                # A successful parent must not leave a flasher or a serial
                # reader running after hardware ownership is released.
                os.killpg(process.pid, signal.SIGKILL)
                raise EvidenceError("adapter left background processes; process group stopped")
        stdout.seek(0, os.SEEK_END)
        stderr.seek(0, os.SEEK_END)
        if stdout.tell() > output_limit or stderr.tell() > output_limit:
            raise EvidenceError("adapter output exceeds configured limit")
        if process.returncode != 0:
            raise EvidenceError(f"adapter failed with exit code {process.returncode}")
        stdout.seek(0)
        return stdout.read(output_limit)


def reject_secret_fields(value: Any) -> None:
    """Reject credential-shaped fields before any public audit report is written."""
    if isinstance(value, dict):
        for key, child in value.items():
            if re.search(r"private|secret|password|credential|access_token", key, re.IGNORECASE):
                raise EvidenceError("adapter response contains forbidden credential field")
            reject_secret_fields(child)
    elif isinstance(value, list):
        for child in value:
            reject_secret_fields(child)
