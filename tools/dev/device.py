"""Bounded daily probe and serial operations, separate from HIL acceptance."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import secrets
import sys

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.dev import process
from tools.dev.qualification import verify_provenance
from tools.evidence.common import (
    fields, file_identity, identifier, load_json, verify_file_identity,
)
from tools.evidence.identity import resolved_identity


def _path(value):
    path = Path(value).absolute()
    if any(item.is_symlink() for item in (path, *path.parents)):
        raise ValueError("Developer operation paths cannot traverse symlinks")
    return path.resolve()


def _tcl_word(value):
    """One Tcl word; OpenOCD -c strings must not interpret path contents."""
    escaped = str(value).replace("\\", "\\\\")
    for character in ('"', "$", "[", "]"):
        escaped = escaped.replace(character, "\\" + character)
    escaped = escaped.replace("\n", "\\n").replace("\r", "\\r")
    return '"' + escaped + '"'


def _image(target, provenance_path, elf):
    """Associate observed build bytes, not a physical firmware attestation."""
    provenance = load_json(provenance_path)
    verify_provenance(provenance)
    if (provenance.get("action") != "build"
            or not provenance.get("actual_execution")):
        raise ValueError("Actual completed build provenance is required")
    if provenance["configuration"] is None:
        raise ValueError("Build must bind its resolved configuration")
    resolved, _ = resolved_identity(
        verify_file_identity(provenance["configuration"]))
    if any(resolved.get(key) != target[key]
           for key in ("board", "part") if key in target):
        raise ValueError("Developer target Board/part differs from "
                         "built configuration")
    candidates = [item for item in provenance["artifacts"]
                  if Path(item["path"]).suffix == ".elf"]
    if elf is not None:
        candidates = [item for item in candidates
                      if _path(item["path"]) == _path(elf)]
    if len(candidates) != 1:
        raise ValueError("Select one exact ELF from build provenance")
    image = verify_file_identity(candidates[0])
    with image.open("rb") as stream:
        header = stream.read(20)
    if (len(header) != 20 or header[:6] != b"\x7fELF\x01\x01"
            or int.from_bytes(header[18:20], "little") != 40):
        raise ValueError("Probe action requires a little-endian ARM ELF")
    return candidates[0]


def plan(action, target_path, provenance_path=None, *, elf=None, timeout=300,
         port=3333):
    """Inspect only action inputs without opening a probe or serial device."""
    if action not in ("flash", "debug", "monitor"):
        raise ValueError("Unsupported developer equipment action")
    if type(timeout) is not int or not 1 <= timeout <= 3600:
        raise ValueError("Equipment timeout must be 1..3600 seconds")
    if type(port) is not int or not 1024 <= port <= 65535:
        raise ValueError("Debug port must be 1024..65535")
    target = load_json(target_path)
    required = {"schema", "kind", "board", "station_lock"}
    required |= {"serial"} if action == "monitor" else {"part", "probe"}
    fields(target, required, optional={"part", "probe", "serial"} - required)
    if (type(target["schema"]) is not int or target["schema"] != 1
            or target["kind"] != "nexus_development_target"):
        raise ValueError("Explicit developer target required")
    for key in ("board", "part"):
        if key in target:
            identifier(target[key], "target " + key)
    if not isinstance(target["station_lock"], str) or not Path(
            target["station_lock"]).is_absolute():
        raise ValueError("Absolute shared station lock required")
    _path(target["station_lock"])
    if provenance_path is None and (action != "monitor" or elf is not None):
        raise ValueError("Image operations require actual build provenance")
    image = (_image(target, provenance_path, elf)
             if provenance_path is not None else None)
    serial, probe_serial = None, None
    if action == "monitor":
        serial = target["serial"]
        fields(serial, {"path", "baudrate"})
        name = serial["path"]
        if (not isinstance(name, str) or "\0" in name or
                Path(name).parent != Path("/dev/serial/by-id") or
                str(Path(name)) != name or
                type(serial["baudrate"]) is not int or
                serial["baudrate"] != 115200):
            raise ValueError("Explicit by-id serial and 115200 8N1 required")
        command = [str(Path(sys.executable).resolve()), "-B",
                   str(Path(__file__).resolve()),
                   "_monitor", name, str(timeout)]
    else:
        probe = target["probe"]
        fields(probe, {"serial", "transport", "tool", "interface_config",
                       "target_config"})
        probe_serial = identifier(probe["serial"], "explicit probe serial")
        if probe["transport"] != "swd":
            raise ValueError("Reviewed SWD target required")
        paths = {key: verify_file_identity(probe[key]) for key in
                 ("tool", "interface_config", "target_config")}
        if not os.access(paths["tool"], os.X_OK):
            raise ValueError("Reviewed probe tool must be executable")
        command = [str(paths["tool"]), "-f", str(paths["interface_config"]),
                   "-c", "transport select swd", "-c",
                   "adapter serial " + _tcl_word(probe_serial),
                   "-f", str(paths["target_config"]), "-c", "bindto 127.0.0.1",
                   "-c", "telnet_port disabled", "-c", "tcl_port disabled"]
        if action == "flash":
            command += ["-c", "gdb_port disabled", "-c",
                        "program " + _tcl_word(image["path"])
                        + " verify reset exit"]
        else:
            command += ["-c", "gdb_port " + str(port), "-c", "init",
                        "-c", "reset halt"]
    return {
        "schema": 1, "kind": "nexus_development_operation", "action": action,
        "target": file_identity(target_path),
        "provenance": (file_identity(provenance_path)
                       if provenance_path is not None else None),
        "elf": image,
        "image_linkage": ("validated_build_identity" if image
                          else "unassociated"),
        "board": target["board"], "part": target.get("part"),
        "probe_serial": probe_serial, "serial": serial,
        "station_lock": target["station_lock"], "command": command,
        "timeout_seconds": timeout, "debug_port": port,
        "qualification": False, "physical_status": "not_executed",
        "scope": ("development host command only; "
                  "no physical qualification or MCU timing"),
    }


def execute(prepared, output):
    """Execute reviewed bytes with failure records and exclusive ownership."""
    output = _path(output)
    target = verify_file_identity(prepared["target"])
    provenance = (verify_file_identity(prepared["provenance"])
                  if prepared["provenance"] else None)
    image = verify_file_identity(prepared["elf"]) if prepared["elf"] else None
    current = plan(prepared["action"], target, provenance,
                   elf=image,
                   timeout=prepared["timeout_seconds"],
                   port=prepared["debug_port"])
    if current != prepared:
        raise ValueError("Developer plan inputs changed")
    if output.exists():
        raise ValueError("Equipment output must be a new directory")
    lock = _path(prepared["station_lock"])
    token = secrets.token_hex(16)
    try:
        with lock.open("x", encoding="utf-8") as stream:
            stream.write(token)
    except FileExistsError as error:
        raise ValueError("Station lock is busy or quarantined") from error
    started = False
    success = False
    try:
        output.mkdir()
        (output / "plan.json").write_text(json.dumps(prepared, indent=2) + "\n")
        # Revalidate before launch; copied plans are not authority.
        if plan(prepared["action"], target, provenance,
                elf=image,
                timeout=prepared["timeout_seconds"],
                port=prepared["debug_port"]) != prepared:
            raise ValueError("Developer inputs changed before execution")
        if lock.read_text() != token:
            raise ValueError("Station lock ownership changed")
        started = True
        result = process.run(
            prepared["command"], cwd=output, env=os.environ,
            log=output / "command.log",
            timeout=prepared["timeout_seconds"] + (
                5 if prepared["action"] == "monitor" else 0),
            stage="development-" + prepared["action"])
        validation_error = None
        try:
            if _path(lock).read_text() != token:
                raise ValueError("Station lock ownership changed")
            if plan(prepared["action"], target, provenance,
                    elf=image,
                    timeout=prepared["timeout_seconds"],
                    port=prepared["debug_port"]) != prepared:
                raise ValueError("Developer input identity changed")
        except (OSError, ValueError) as error:
            validation_error = "Post-command identity validation: " + str(error)
        success = result.returncode == 0 and validation_error is None
        report = {**prepared,
                  "exit_code": result.returncode or (
                      2 if validation_error else 0),
                  "child_exit_code": result.returncode,
                  "validation_error": validation_error,
                  "receipt": str(result.receipt_path),
                  "physical_status": "executed_unqualified",
                  "station_quarantined": not success}
        (output / "operation.json").write_text(
            json.dumps(report, indent=2) + "\n")
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        return report
    finally:
        if ((success or not started) and lock.is_file()
                and not lock.is_symlink()):
            if lock.read_text() == token:
                lock.unlink()


def _monitor(name, duration):
    """Reuse the existing stable-identity/exclusive/restore serial primitive."""
    import select
    import time
    from tools.hil.serial_echo import _device
    if type(duration) is not int or not 1 <= duration <= 3600:
        raise ValueError("Monitor duration must be 1..3600 seconds")
    maximum = 16 * 1024 * 1024
    total = 0
    deadline = time.monotonic() + duration
    with _device(name) as (descriptor, _identity):
        while time.monotonic() < deadline:
            ready, _, _ = select.select(
                [descriptor], [], [],
                min(0.1, max(0, deadline - time.monotonic())))
            if not ready:
                continue
            try:
                data = os.read(descriptor, 4096)
            except BlockingIOError:
                continue
            if not data:
                raise ValueError("Serial monitor disconnected (EOF)")
            total += len(data)
            if total > maximum:
                raise ValueError("Serial monitor exceeded 16 MiB output bound")
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()
    return 0


def main(action, argv=None):
    parser = argparse.ArgumentParser(prog="dev.py " + action)
    parser.add_argument("--target", type=Path, required=True)
    parser.add_argument("--provenance", type=Path)
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--port", type=int, default=3333)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args(argv)
    prepared = plan(action, arguments.target, arguments.provenance,
                    elf=arguments.elf, timeout=arguments.timeout,
                    port=arguments.port)
    if not arguments.execute:
        print(json.dumps(prepared, indent=2))
        return 0
    if arguments.output is None:
        raise ValueError("Execution requires a new --output directory")
    selection = (prepared["serial"]["path"] if action == "monitor"
                 else "probe " + prepared["probe_serial"])
    print("Executing " + action + " on " + prepared["board"] + " using "
          + selection, flush=True)
    if action == "debug":
        print("GDB: target extended-remote 127.0.0.1:" + str(arguments.port),
              flush=True)
    return execute(prepared, arguments.output)["exit_code"]


if __name__ == "__main__":
    try:
        if len(sys.argv) != 4 or sys.argv[1] != "_monitor":
            raise ValueError("Use the dev.py flash/debug/monitor entry points")
        raise SystemExit(_monitor(sys.argv[2], int(sys.argv[3])))
    except (OSError, ValueError) as error:
        print("Developer monitor failed: " + str(error), file=sys.stderr)
        raise SystemExit(2) from error
