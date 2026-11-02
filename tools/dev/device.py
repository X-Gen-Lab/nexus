"""Bounded daily probe and serial operations, separate from HIL acceptance."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import secrets
import shlex
import stat
import sys

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.dev import process
from tools.dev.qualification import verify_provenance
from tools.evidence.common import (
    fields, file_identity, identifier, load_json, parse_json,
    verify_file_identity,
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
    # Execution changes cwd to the owned run directory; preserve the path of
    # the bytes just verified rather than reinterpreting a relative identity.
    return {**candidates[0], "path": str(image)}


def _gdb_command(server, client, image):
    """Use GDB's official pipe transport with one safely quoted shell exec.

    GDB explicitly parses the pipe tail through /bin/sh. Quote every OpenOCD
    argument, keep Tcl words independently quoted, and reject line controls
    before that tail becomes one GDB command. No ELF path enters this string.
    """
    fields(client, {"tool"}, {"commands"})
    tool = verify_file_identity(client["tool"])
    if not os.access(tool, os.X_OK):
        raise ValueError("Reviewed GDB client must be executable")
    if any(any(ord(char) < 32 or ord(char) == 127 for char in argument)
           for argument in server):
        raise ValueError("GDB pipe requires one-line arguments without controls")
    command = [str(tool), "--nx", "--batch", "--quiet",
               "--init-eval-command=set auto-load off",
               "--se=" + image["path"],
               "--eval-command=target extended-remote | exec "
               + shlex.join(server)]
    if "commands" in client:
        command.append("--command=" + str(verify_file_identity(
            client["commands"])))
    else:
        command.append("--eval-command=info threads")
    return command + ["--eval-command=detach"]


def plan(action, target_path, provenance_path=None, *, elf=None, timeout=300,
         port=3333, client=False):
    """Inspect only action inputs without opening a probe or serial device."""
    if action not in ("flash", "debug", "monitor"):
        raise ValueError("Unsupported developer equipment action")
    if type(client) is not bool or (client and action != "debug"):
        raise ValueError("GDB client is an explicit debug-only selection")
    if type(timeout) is not int or not 1 <= timeout <= 3600:
        raise ValueError("Equipment timeout must be 1..3600 seconds")
    if client and port not in (3333, None):
        raise ValueError("Pipe GDB client does not use a TCP port")
    if not client and (type(port) is not int or not 1024 <= port <= 65535):
        raise ValueError("Debug port must be 1024..65535")
    target = load_json(target_path)
    required = {"schema", "kind", "board", "station_lock"}
    required |= {"serial"} if action == "monitor" else {"part", "probe"}
    fields(target, required, optional={"part", "probe", "serial",
                                       "gdb_client"} - required)
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
        if not sys.platform.startswith("linux"):
            raise ValueError("Developer serial monitoring requires Linux")
        from tools.hil.serial_echo import _baud_constant
        serial = target["serial"]
        fields(serial, {"path", "baudrate"})
        name = serial["path"]
        if (not isinstance(name, str) or "\0" in name or
                Path(name).parent != Path("/dev/serial/by-id") or
                str(Path(name)) != name):
            raise ValueError("Explicit by-id serial identity required")
        _baud_constant(serial["baudrate"])
        command = [str(Path(sys.executable).resolve()), "-B",
                   str(Path(__file__).resolve()),
                   "_monitor", name, str(timeout), str(serial["baudrate"])]
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
        elif client:
            if not sys.platform.startswith("linux"):
                raise ValueError("Reviewed pipe GDB workflow requires Linux")
            if "gdb_client" not in target:
                raise ValueError("Explicit reviewed GDB client required")
            command += ["-c", "gdb_port pipe", "-c",
                        "log_output openocd.log", "-c", "init", "-c",
                        "reset halt"]
            command = _gdb_command(command, target["gdb_client"], image)
            port = None
        else:
            command += ["-c", "gdb_port " + str(port), "-c", "init",
                        "-c", "reset halt"]
    prepared = {
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
    if client:
        prepared["gdb_client"] = target["gdb_client"]
    return prepared


def _monitor_snapshot(serial):
    """Hold the original terminal without configuring or consuming its input."""
    import termios
    from tools.hil.serial_echo import _alias_target

    target = _alias_target(serial["path"])
    expected = target.stat()
    if not stat.S_ISCHR(expected.st_mode):
        raise ValueError("Monitor guard requires a character device")
    descriptor = os.open(target, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK |
                         os.O_CLOEXEC | os.O_NOFOLLOW)
    try:
        actual = os.fstat(descriptor)
        identity = (actual.st_dev, actual.st_ino, actual.st_rdev)
        if (not stat.S_ISCHR(actual.st_mode)
                or identity != (expected.st_dev, expected.st_ino,
                                expected.st_rdev)
                or _alias_target(serial["path"]) != target):
            raise ValueError("Monitor identity changed during guard open")
        return {"descriptor": descriptor, "identity": identity,
                "attributes": termios.tcgetattr(descriptor),
                "alias": serial["path"], "target": target}
    except BaseException as primary:
        try:
            os.close(descriptor)
        except BaseException as cleanup:
            raise ValueError("Monitor guard failed: " + str(primary)
                             + "; descriptor close failed: "
                             + str(cleanup)) from primary
        raise


def _monitor_withdrawn(result):
    """Require completed owned-child withdrawal before terminal writes."""
    if result is None or result.receipt_path is None:
        return False
    try:
        receipt = load_json(result.receipt_path)
        return (receipt.get("withdrawal_complete") is True
                and receipt.get("withdrawal_stable") is True
                and receipt.get("outcome") in (
                    "completed", "cancelled", "timed_out", "launch_failed")
                and type(receipt.get("exit_code")) is int
                and receipt["exit_code"] == result.returncode)
    except (OSError, ValueError, TypeError):
        return False


def _restore_monitor(snapshot, *, withdrawal_confirmed, phase_control):
    """Restore only our retained descriptor, after the child is withdrawn.

    Each cleanup failure remains visible to the operation report. Continue
    through independent cleanup steps even after a driver error or signal.
    Never open a possibly replaced by-id device to perform restoration.
    """
    import fcntl
    import termios
    from tools.hil.serial_echo import (
        _advance_monitor_phase, _alias_target, _read_monitor_phase,
    )

    descriptor = snapshot["descriptor"]
    try:
        current = os.fstat(descriptor)
        if (not stat.S_ISCHR(current.st_mode)
                or (current.st_dev, current.st_ino, current.st_rdev)
                != snapshot["identity"]):
            raise ValueError("monitor guard descriptor identity changed")
    except BaseException as error:
        # A recycled descriptor is no longer ours to configure or close.
        return ["descriptor identity: " + str(error)]
    def close_only(error):
        errors = [error]
        try:
            os.close(descriptor)
        except BaseException as failure:
            errors.append("close guard: " + str(failure))
        return errors

    if not withdrawal_confirmed:
        # The child may still own this terminal. Closing our read-only guard
        # neither reconfigures it nor clears its exclusive access.
        return close_only("child withdrawal not confirmed; terminal unchanged")
    binding = {"path": str(snapshot["target"]),
               **dict(zip(("st_dev", "st_ino", "st_rdev"),
                          snapshot["identity"]))}
    try:
        phase = _read_monitor_phase(phase_control, binding)["phase"]
        if phase == "owned":
            _advance_monitor_phase(phase_control, binding, "owned", "releasing")
        elif phase != "released":
            return close_only("terminal ownership is not confirmed: " + phase)
    except BaseException as error:
        return close_only("terminal ownership record rejected: " + str(error))

    def readback():
        if termios.tcgetattr(descriptor) != snapshot["attributes"]:
            raise ValueError("saved terminal attributes were not restored")

    def alias_identity():
        if _alias_target(snapshot["alias"]) != snapshot["target"]:
            raise ValueError("monitor by-id identity changed during execution")

    actions = (
        ("restore attributes", lambda: termios.tcsetattr(
            descriptor, termios.TCSANOW, snapshot["attributes"])),
        ("restore readback", readback),
        ("release exclusive access", lambda: fcntl.ioctl(
            descriptor, termios.TIOCNXCL)),
        ("alias identity", alias_identity),
        ("close guard", lambda: os.close(descriptor)),
    )
    if phase == "released":
        # The child already relinquished ownership. Another process may now
        # own the terminal, so only inspect the alias and close our guard.
        actions = actions[-2:]
    errors = []
    for name, action in actions:
        try:
            action()
        except BaseException as error:
            errors.append(name + ": " + type(error).__name__ + ": "
                          + str(error))
    if not errors and phase == "owned":
        try:
            _advance_monitor_phase(phase_control, binding,
                                   "releasing", "released")
        except BaseException as error:
            errors.append("publish released phase: " + str(error))
    return errors


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
                   port=prepared["debug_port"],
                   client="gdb_client" in prepared)
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
                port=prepared["debug_port"],
                client="gdb_client" in prepared) != prepared:
            raise ValueError("Developer inputs changed before execution")
        if lock.read_text() != token:
            raise ValueError("Station lock ownership changed")
        execution_command = list(prepared["command"])
        binding = None
        phase_control = None
        snapshot = None
        terminal_errors = []
        result = None
        try:
            snapshot = (_monitor_snapshot(prepared["serial"])
                        if prepared["action"] == "monitor" else None)
            if snapshot is not None:
                from tools.hil.serial_echo import _begin_monitor_phase
                binding = {"path": str(snapshot["target"]),
                           **dict(zip(("st_dev", "st_ino", "st_rdev"),
                                      snapshot["identity"]))}
                phase_control = {
                    "path": str(output / "monitor-terminal-phase.json"),
                    "nonce": secrets.token_hex(16)}
                _begin_monitor_phase(phase_control, binding)
                execution_command.append(json.dumps({
                    "device": binding, "phase": phase_control},
                    sort_keys=True, separators=(",", ":")))
            started = True
            result = process.run(
                execution_command, cwd=output, env=os.environ,
                log=output / "command.log",
                timeout=prepared["timeout_seconds"] + (
                    5 if prepared["action"] == "monitor" else 0),
                stage="development-" + prepared["action"])
        finally:
            if snapshot is not None:
                terminal_errors = _restore_monitor(
                    snapshot, withdrawal_confirmed=_monitor_withdrawn(result),
                    phase_control=phase_control)
        validation_error = None
        try:
            if _path(lock).read_text() != token:
                raise ValueError("Station lock ownership changed")
            if plan(prepared["action"], target, provenance,
                    elf=image,
                    timeout=prepared["timeout_seconds"],
                    port=prepared["debug_port"],
                    client="gdb_client" in prepared) != prepared:
                raise ValueError("Developer input identity changed")
        except (OSError, ValueError) as error:
            validation_error = "Post-command identity validation: " + str(error)
        if terminal_errors:
            restoration = "Terminal restoration failed: " + "; ".join(
                terminal_errors)
            validation_error = (validation_error + "; " + restoration
                                if validation_error else restoration)
        success = result.returncode == 0 and validation_error is None
        report = {**prepared,
                  "planned_command": prepared["command"],
                  "command": execution_command,
                  "monitor_device_binding": binding,
                  "monitor_phase_control": phase_control,
                  "exit_code": result.returncode or (
                      2 if validation_error else 0),
                  "child_exit_code": result.returncode,
                  "validation_error": validation_error,
                  "receipt": str(result.receipt_path),
                  "terminal_restore": ({
                      "status": "failed" if terminal_errors else "restored",
                      "errors": terminal_errors,
                  } if snapshot is not None else None),
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


def _monitor(name, duration, baudrate=115200, *, expected_device=None,
             phase_control=None):
    """Reuse the existing stable-identity/exclusive/restore serial primitive."""
    import select
    import time
    from tools.hil.serial_echo import _device
    if type(duration) is not int or not 1 <= duration <= 3600:
        raise ValueError("Monitor duration must be 1..3600 seconds")
    maximum = 16 * 1024 * 1024
    total = 0
    deadline = time.monotonic() + duration
    with _device(name, baudrate=baudrate, read_only=True,
                 expected_device=expected_device,
                 phase_control=phase_control) as (descriptor, _identity):
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
    parser.add_argument("--port", type=int)
    parser.add_argument("--client", action="store_true",
                        help="Attach bounded batch GDB through OpenOCD pipe")
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args(argv)
    if arguments.client and arguments.port is not None:
        raise ValueError("Pipe GDB client does not use --port")
    prepared = plan(action, arguments.target, arguments.provenance,
                    elf=arguments.elf, timeout=arguments.timeout,
                    port=arguments.port if arguments.port is not None else 3333,
                    client=arguments.client)
    if not arguments.execute:
        print(json.dumps(prepared, indent=2))
        return 0
    if arguments.output is None:
        raise ValueError("Execution requires a new --output directory")
    selection = (prepared["serial"]["path"] if action == "monitor"
                 else "probe " + prepared["probe_serial"])
    print("Executing " + action + " on " + prepared["board"] + " using "
          + selection, flush=True)
    if action == "debug" and not arguments.client:
        print("GDB: target extended-remote 127.0.0.1:" + str(prepared["debug_port"]),
              flush=True)
    return execute(prepared, arguments.output)["exit_code"]


if __name__ == "__main__":
    try:
        if len(sys.argv) != 6 or sys.argv[1] != "_monitor":
            raise ValueError("Use the dev.py flash/debug/monitor entry points")
        if len(sys.argv[5]) > 4096:
            raise ValueError("Monitor device binding exceeds input limit")
        control = parse_json(sys.argv[5])
        fields(control, {"device", "phase"})
        raise SystemExit(_monitor(sys.argv[2], int(sys.argv[3]),
                                  int(sys.argv[4]),
                                  expected_device=control["device"],
                                  phase_control=control["phase"]))
    except (OSError, ValueError) as error:
        import traceback
        print("Developer monitor failed:", file=sys.stderr)
        # A cleanup library may use `raise ... from None`. Retain its hidden
        # context too, so a prior EOF or I/O failure remains in the raw receipt.
        pending, failures, seen = [error], [], set()
        while pending:
            failure = pending.pop()
            if id(failure) in seen:
                continue
            seen.add(id(failure))
            failures.append(failure)
            pending.extend(item for item in
                           (failure.__context__, failure.__cause__)
                           if item is not None)
        for failure in reversed(failures):
            traceback.print_exception(failure, chain=False, file=sys.stderr)
        raise SystemExit(2) from error
