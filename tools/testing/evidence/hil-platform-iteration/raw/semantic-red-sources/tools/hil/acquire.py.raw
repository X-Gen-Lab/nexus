#!/usr/bin/env python3
"""Execute reviewed station adapters without declaring physical HIL pass."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path
import secrets
import selectors
import signal
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (
    EvidenceError, atomic_json, fields, file_identity, load_json,
    verify_file_identity,
)
from tools.evidence.identity import canonical_digest
from tools.hil.admit import admit, require


OPERATIONS = ("identify", "flash", "verify_flash", "reset", "serial", "cleanup")
PLACEHOLDERS = {"{record}", "{session}", "{admission}", "{elf}", "{output}"}
LOCK_FAILURE = "station lock ownership changed; stop equipment actions"


def _path(path: Path) -> Path:
    absolute = Path(path).absolute()
    require(not any(part.is_symlink()
                    for part in (absolute, *absolute.parents)),
            "acquisition paths must not traverse symlinks")
    return absolute


def _reviewed(recipe_path: Path, admission_path: Path) -> tuple[dict, dict]:
    recipe, admission = load_json(recipe_path), load_json(admission_path)
    require(admission.get("kind") == "hil_admission" and
            admission.get("status") == "ready", "ready admission required")
    paths = [verify_file_identity(admission[name]) for name in
             ("station", "fixture", "elf", "resolved", "resources")]
    rebuilt = admit(*paths)
    require(all(admission.get(key) == value for key, value in rebuilt.items()),
            "acquisition admission is stale or inconsistent")
    fields(recipe, {"schema_version", "kind", "admission_sha256", "workload",
                    "station_lock", "operations"})
    require(type(recipe["schema_version"]) is int and
            recipe["schema_version"] == 1 and
            recipe["kind"] == "hil_acquisition_recipe",
            "reviewed acquisition recipe required")
    require(recipe["admission_sha256"] == canonical_digest(admission),
            "acquisition recipe is not bound to admission")
    verify_file_identity(recipe["workload"])
    lock = recipe["station_lock"]
    require(isinstance(lock, str) and Path(lock).is_absolute(),
            "absolute station-owned exclusive lock required")
    _path(Path(lock))
    steps = recipe["operations"]
    require(isinstance(steps, list) and
            [step.get("name") for step in steps] == list(OPERATIONS),
            "complete ordered acquisition operations required")
    station = load_json(paths[0])
    for step in steps:
        fields(step, {"name", "tool", "inputs", "arguments", "timeout_s",
                      "maximum_output_bytes"})
        tool = verify_file_identity(step["tool"])
        require(os.access(tool, os.X_OK), "adapter tool is not executable")
        require(isinstance(step["inputs"], list) and
                0 < len(step["inputs"]) <= 64,
                "adapter input closure must be explicit and bounded")
        for identity in step["inputs"]:
            verify_file_identity(identity)
        arguments = step["arguments"]
        require(isinstance(arguments, list) and 0 < len(arguments) <= 128 and
                all(isinstance(arg, str) and 0 < len(arg) <= 4096 and
                    "\x00" not in arg for arg in arguments),
                "bounded adapter argument array required")
        require({"{record}", "{session}", "{admission}"}.issubset(arguments),
                "adapter must receive fresh output, session and admission")
        require(type(step["timeout_s"]) is int and
                1 <= step["timeout_s"] <= 3600,
                "bounded adapter timeout required")
        require(type(step["maximum_output_bytes"]) is int and
                1 <= step["maximum_output_bytes"] <= 16 * 1024 * 1024,
                "bounded adapter output required")
        if step["name"] in OPERATIONS[:4]:
            identities = [step["tool"], *step["inputs"]]
            require(station["probe"]["tool"] in identities,
                    "probe operation must bind the admitted tool bytes")
    return recipe, admission


def acquisition_plan(recipe_path: Path, admission_path: Path) -> dict:
    """Resolve reviewed inputs without opening instruments or starting tools."""
    recipe, admission = _reviewed(recipe_path, admission_path)
    return {
        "schema_version": 1, "kind": "hil_acquisition_plan",
        "status": "prepared_bound", "physical_status": "not_executed",
        "recipe": file_identity(recipe_path),
        "admission": file_identity(admission_path),
        "admission_sha256": canonical_digest(admission),
        "station_id": admission["station_id"], "board": admission["board"],
        "part": admission["part"], "chip_uid": admission["chip_uid"],
        "workload": recipe["workload"], "operations": recipe["operations"],
        "limits": "No equipment operated; reviewed adapters need execution",
    }


def _captured_identity(path: Path) -> dict:
    """Raw stream bytes can be empty; retain their exact empty-file identity."""
    data = _path(path).read_bytes()
    return {"path": str(path), "sha256": hashlib.sha256(data).hexdigest(),
            "size": len(data)}


def _kill_group(process: subprocess.Popen) -> None:
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait(timeout=5)


def _capture_output(process, step, stdout, stderr, started, result):
    """Read bounded streams; the caller always reaps the process on exit."""
    with selectors.DefaultSelector() as selector:
        for pipe, destination in ((process.stdout, stdout),
                                  (process.stderr, stderr)):
            os.set_blocking(pipe.fileno(), False)
            selector.register(pipe, selectors.EVENT_READ, destination)
        captured = 0
        while selector.get_map():
            remaining = step["timeout_s"] - (time.monotonic() - started)
            if remaining <= 0:
                result["timed_out"] = True
                return
            for key, _events in selector.select(min(remaining, 0.05)):
                data = os.read(key.fileobj.fileno(), 65536)
                if not data:
                    selector.unregister(key.fileobj)
                    continue
                capacity = step["maximum_output_bytes"] - captured
                key.data.write(data[:capacity])
                captured += len(data[:capacity])
                if len(data) > capacity:
                    result["output_exceeded"] = True
                    return
        if process.poll() is None:
            remaining = step["timeout_s"] - (time.monotonic() - started)
            try:
                process.wait(timeout=max(0, remaining))
            except subprocess.TimeoutExpired:
                result["timed_out"] = True


def _command(step: dict, substitutions: dict, output: Path) -> dict:
    name = step["name"]
    arguments = [str(verify_file_identity(step["tool"]))]
    arguments.extend(substitutions.get(arg, arg) for arg in step["arguments"])
    paths = {stream: output / (name + "." + stream + ".bin")
             for stream in ("stdout", "stderr")}
    result = {"name": name, "argv": arguments,
              "started_utc": datetime.now(timezone.utc).isoformat(),
              "exit_code": None, "process_started": False, "timed_out": False,
              "cancelled": False,
              "output_exceeded": False, "residual_processes": False}
    started = time.monotonic()
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("GIT_")}
    with paths["stdout"].open("xb") as stdout, \
            paths["stderr"].open("xb") as stderr:
        try:
            with subprocess.Popen(arguments, stdin=subprocess.DEVNULL,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE,
                                  cwd=output, env=environment,
                                  start_new_session=True) as process:
                result["process_started"] = True
                try:
                    _capture_output(process, step, stdout, stderr,
                                    started, result)
                except BaseException as error:
                    # Cancellation must not escape Popen's unbounded context
                    # wait while an instrument process still holds equipment.
                    result["failure"] = type(error).__name__ + ": " + str(error)
                    result["cancelled"] = isinstance(
                        error, (KeyboardInterrupt, SystemExit))
                finally:
                    if process.poll() is None:
                        _kill_group(process)
                    result["exit_code"] = process.wait()
                    try:
                        os.killpg(process.pid, 0)
                    except ProcessLookupError:
                        pass
                    else:
                        result["residual_processes"] = True
                        _kill_group(process)
        except OSError as error:
            result["failure"] = str(error)
    result.update({stream: _captured_identity(path)
                   for stream, path in paths.items()})
    result["finished_utc"] = datetime.now(timezone.utc).isoformat()
    result["elapsed_s"] = time.monotonic() - started
    failures = ("timed_out", "output_exceeded", "residual_processes",
                "cancelled")
    result["status"] = (
        "captured" if result["exit_code"] == 0 and
        "failure" not in result and
        not any(result[key] for key in failures) else "failed")
    return result


def _observation(path: Path, step: dict, token: str, admission: dict) -> dict:
    value = load_json(path)
    fields(value, {"schema_version", "kind", "operation", "session",
                   "observations"})
    require(type(value["schema_version"]) is int and
            value["schema_version"] == 1
            and value["kind"] == "hil_adapter_observation" and
            value["operation"] == step["name"] and value["session"] == token,
            "adapter observation is stale or not bound to this operation")
    facts = value["observations"]
    require(isinstance(facts, dict) and bool(facts),
            "nonempty raw instrument observations required")
    if step["name"] == "identify":
        station = load_json(verify_file_identity(admission["station"]))
        for name in ("part", "chip_uid"):
            require(facts.get(name) == admission[name],
                    "observed target identity mismatch: " + name)
        require(facts.get("probe_serial") == station["probe"]["serial"],
                "observed probe serial mismatch")
    return file_identity(path)


def _owns_station(lock: Path, token: str) -> bool:
    try:
        return lock.read_text().splitlines()[0] == token
    except (OSError, IndexError):
        return False


def execute_acquisition(recipe_path: Path, admission_path: Path,
                        output: Path) -> dict:
    """Capture one fresh attempt; qualification is a separate validation."""
    recipe_path, admission_path, output = (
        _path(path) for path in (recipe_path, admission_path, output))
    plan = acquisition_plan(recipe_path, admission_path)
    recipe, admission = _reviewed(recipe_path, admission_path)
    protected = [recipe_path, admission_path,
                 *[Path(admission[key]["path"]) for key in
                   ("station", "fixture", "elf", "resolved", "resources")],
                 Path(recipe["workload"]["path"])]
    protected += [Path(identity["path"]) for step in recipe["operations"]
                  for identity in (step["tool"], *step["inputs"])]
    lock = _path(Path(recipe["station_lock"]))
    require(not output.exists() and
            not any(path == output or path.is_relative_to(output)
                    for path in [*protected, lock]),
            "fresh acquisition output must not overwrite retained inputs")
    require(not any(path == lock or path.is_relative_to(lock)
                    for path in protected),
            "station lock must not overwrite an acquisition input")
    token = secrets.token_hex(16)
    try:
        descriptor = os.open(lock, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError as error:
        message = "station locked or quarantined; manual inspection required"
        raise EvidenceError(message) from error
    with os.fdopen(descriptor, "w") as record:
        record.write(token + "\n" + str(output) + "\n")
    report = {**plan, "kind": "hil_acquisition_capture", "status": "failed",
              "physical_status": "not_executed", "session": token,
              "started_utc": datetime.now(timezone.utc).isoformat(),
              "operations": [], "station_quarantined": True,
              "station_lock": str(lock),
              "limits": ("Raw capture only; existing HIL/OS validators "
                         "establish recorded scope")}
    try:
        output.mkdir(parents=True, exist_ok=False)
        atomic_json(output / "plan.json", plan)
        for step in recipe["operations"]:
            if step["name"] != "cleanup" and "failure" in report:
                continue
            record = output / (step["name"] + ".observation.json")
            try:
                require(_owns_station(lock, token), LOCK_FAILURE)
                verify_file_identity(plan["recipe"])
                verify_file_identity(plan["admission"])
                _reviewed(recipe_path, admission_path)
                result = _command(step, {
                    "{record}": str(record), "{session}": token,
                    "{admission}": str(admission_path),
                    "{elf}": str(verify_file_identity(admission["elf"])),
                    "{output}": str(output)}, output)
                report["operations"].append(result)
                if result["process_started"]:
                    report["physical_status"] = "executed_unqualified"
                if result["cancelled"]:
                    report["cancelled"] = True
                require(result["status"] == "captured",
                        result.get("failure", "adapter command failed: " +
                                   step["name"]))
                require(_owns_station(lock, token), LOCK_FAILURE)
                result["observation"] = _observation(
                    record, step, token, admission)
                verify_file_identity(plan["recipe"])
                verify_file_identity(plan["admission"])
                _reviewed(recipe_path, admission_path)
                if step["name"] == "cleanup":
                    report["station_quarantined"] = False
            except (EvidenceError, OSError, ValueError, KeyError) as error:
                report.setdefault("failure", str(error))
                if (report["operations"] and
                        report["operations"][-1]["name"] == step["name"]):
                    report["operations"][-1]["status"] = "failed"
                    report["operations"][-1]["failure"] = str(error)
                if step["name"] == "cleanup":
                    report["cleanup_failure"] = str(error)
                if not _owns_station(lock, token) or report.get("cancelled"):
                    report["station_quarantined"] = True
                    break
        if "failure" not in report:
            report["status"] = "captured"
    finally:
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        if not report["station_quarantined"]:
            try:
                require(lock.read_text().splitlines()[0] == token,
                        "station lock ownership changed; retain quarantine")
            except (EvidenceError, OSError, IndexError) as error:
                report["station_quarantined"] = True
                report["status"] = "failed"
                report.setdefault("failure", str(error))
        if output.is_dir():
            atomic_json(output / "capture.json", report)
        if not report["station_quarantined"]:
            lock.unlink()
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("plan", "execute"))
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument("--admission", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.mode == "execute":
            report = execute_acquisition(
                args.recipe, args.admission, args.output)
        else:
            output = _path(args.output)
            require(not output.exists() and output not in
                    {_path(args.recipe), _path(args.admission)},
                    "plan must not overwrite inputs or a retained report")
            report = acquisition_plan(args.recipe, args.admission)
            atomic_json(output, report)
    except (EvidenceError, OSError, KeyError, TypeError) as error:
        print("HIL acquisition rejected: " + str(error), file=sys.stderr)
        return 1
    print("HIL acquisition: " + report["status"] + "; " +
          report["physical_status"])
    return 0 if report["status"] != "failed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
