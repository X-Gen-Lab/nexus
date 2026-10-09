#!/usr/bin/env python3
"""Exclusive physical HIL runner with readback identity and fail-closed results."""

from __future__ import annotations

import argparse
from contextlib import ExitStack
from datetime import datetime, timezone
import hashlib
import math
import os
from pathlib import Path
import socket
import shutil
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.evidence.common import (EvidenceError, atomic_json, command, digest, fields,
                                     identifier, load_json, parse_json, reject_secret_fields,
                                     regular_file, run_adapter, sha256_value,
                                     verify_file_identity)


class BoardLease:
    """Atomic directory lease. Crash remnants require supervised recovery."""

    def __init__(self, directory: Path, board_id: str):
        self.path = directory / identifier(board_id, "board id")
        self.token = uuid.uuid4().hex
        self.acquired = False
        self.quarantined = False

    def quarantine(self, reason: str) -> None:
        if self.acquired:
            self.quarantined = True
            owner = load_json(self.path / "owner.json")
            if owner.get("token") != self.token:
                raise EvidenceError("lease owner changed before quarantine")
            owner["status"] = "quarantined"
            owner["reason"] = reason
            atomic_json(self.path / "owner.json", owner)

    def __enter__(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        if any(path.is_symlink() for path in [self.path.parent, *self.path.parent.parents]):
            raise EvidenceError("lease directory must not be a symlink")
        try:
            self.path.mkdir()
        except FileExistsError as error:
            raise EvidenceError("board lease already exists; supervised recovery required") from error
        self.acquired = True
        try:
            atomic_json(self.path / "owner.json", {
                "schema_version": 1, "token": self.token, "pid": os.getpid(),
                "host": socket.gethostname(), "created_utc": utc_now(),
            })
        except Exception:
            self.path.rmdir()
            self.acquired = False
            raise
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self.acquired and not self.quarantined:
            owner = load_json(self.path / "owner.json")
            if owner.get("token") != self.token:
                raise EvidenceError("lease owner changed; refusing lease removal")
            (self.path / "owner.json").unlink()
            self.path.rmdir()
            self.acquired = False


class EquipmentLease:
    """Lease logical board and physical probe so manifest aliases cannot race."""

    def __init__(self, directory: Path, board: dict, serial_lease_id: str | None = None):
        self.stack = ExitStack()
        self.board = BoardLease(directory, board["id"])
        probe_key = "probe-" + hashlib.sha256(identifier(board["probe_serial"]).encode()).hexdigest()
        self.probe = BoardLease(directory, probe_key)
        self.serial = BoardLease(directory, serial_lease_id) if serial_lease_id else None

    def __enter__(self):
        try:
            self.stack.enter_context(self.board)
            self.stack.enter_context(self.probe)
            if self.serial:
                self.stack.enter_context(self.serial)
        except BaseException:
            self.stack.close()
            raise
        return self

    def quarantine(self, reason: str) -> None:
        # Preserve the physical probe first, even if a later audit write fails.
        self.probe.quarantine(reason)
        if self.serial:
            self.serial.quarantine(reason)
        self.board.quarantine(reason)

    def __exit__(self, exc_type, exc_value, traceback):
        return self.stack.__exit__(exc_type, exc_value, traceback)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def validate_manifest(manifest: dict, *, model: bool = False) -> dict:
    fields(manifest, {"schema_version", "kind", "lab_id", "board", "firmware",
                      "config_sha256", "adapter_files", "commands", "operation_timeout_s",
                      "total_timeout_s", "required_tests", "budgets"}, {"serial_lease_id"})
    if manifest["schema_version"] != 1:
        raise EvidenceError("unsupported HIL manifest schema")
    expected_kind = "orchestrator_model" if model else "physical_hil"
    if manifest["kind"] != expected_kind:
        raise EvidenceError(f"manifest kind must be {expected_kind}")
    identifier(manifest["lab_id"], "lab id")
    if "serial_lease_id" in manifest:
        identifier(manifest["serial_lease_id"], "serial lease id")
    fields(manifest["board"], {"id", "profile", "revision", "probe_serial"},
           {"chip_part", "chip_uid"})
    for key, value in manifest["board"].items():
        identifier(value, "board " + key)
    fields(manifest["firmware"], {"path", "sha256"})
    sha256_value(manifest["firmware"]["sha256"])
    if digest(manifest["firmware"]["path"]) != manifest["firmware"]["sha256"]:
        raise EvidenceError("firmware does not match expected hash")
    sha256_value(manifest["config_sha256"])
    if not isinstance(manifest["adapter_files"], list) or not manifest["adapter_files"]:
        raise EvidenceError("adapter file identities required")
    for identity in manifest["adapter_files"]:
        verify_file_identity(identity)
    fields(manifest["commands"], {"identify", "flash", "verify_flash", "reset", "serial", "cleanup"})
    substitutions = {"firmware": str(regular_file(manifest["firmware"]["path"])),
                     **manifest["board"]}
    for argv in manifest["commands"].values():
        expanded = command(argv, substitutions)
        if shutil.which(expanded[0]) is None:
            raise EvidenceError("required adapter executable unavailable")
    timeout = manifest["operation_timeout_s"]
    total = manifest["total_timeout_s"]
    if type(timeout) not in (int, float) or not 0 < timeout <= 300:
        raise EvidenceError("invalid operation timeout")
    if type(total) not in (int, float) or not timeout <= total <= 1800:
        raise EvidenceError("invalid total timeout")
    tests = manifest["required_tests"]
    if not isinstance(tests, list) or not tests or len(tests) != len(set(tests)):
        raise EvidenceError("required tests must be unique and nonempty")
    for test in tests:
        identifier(test, "test id")
    if not isinstance(manifest["budgets"], dict):
        raise EvidenceError("budgets must be an object")
    for metric, budget in manifest["budgets"].items():
        identifier(metric, "metric")
        fields(budget, {"maximum", "unit"})
        identifier(budget["unit"], "metric unit")
        if type(budget["maximum"]) not in (int, float) or not math.isfinite(budget["maximum"]) or budget["maximum"] < 0:
            raise EvidenceError("invalid metric budget")
    return substitutions


def same_board(observed: dict, expected: dict) -> None:
    fields(observed, {"id", "profile", "revision", "probe_serial"},
           {"chip_part", "chip_uid"})
    if observed != expected:
        raise EvidenceError("observed hardware identity differs from board manifest")


def validate_serial(result: dict, manifest: dict) -> None:
    fields(result, {"schema_version", "board", "firmware_sha256", "config_sha256", "tests", "metrics"})
    reject_secret_fields(result)
    if result["schema_version"] != 1:
        raise EvidenceError("unsupported serial result schema")
    same_board(result["board"], manifest["board"])
    if result["firmware_sha256"] != manifest["firmware"]["sha256"]:
        raise EvidenceError("serial firmware identity mismatch")
    if result["config_sha256"] != manifest["config_sha256"]:
        raise EvidenceError("serial configuration identity mismatch")
    tests = result["tests"]
    if not isinstance(tests, list) or not tests:
        raise EvidenceError("zero hardware tests rejected")
    ids = set()
    for test in tests:
        fields(test, {"id", "status"}, {"detail"})
        identifier(test["id"], "test id")
        if test["id"] in ids or test["status"] != "pass":
            raise EvidenceError("duplicate, failed or skipped hardware test")
        if "detail" in test and (not isinstance(test["detail"], str) or len(test["detail"]) > 4096):
            raise EvidenceError("invalid hardware test detail")
        ids.add(test["id"])
    if not set(manifest["required_tests"]).issubset(ids):
        raise EvidenceError("required hardware tests missing")
    if not isinstance(result["metrics"], dict):
        raise EvidenceError("metrics must be an object")
    for metric, budget in manifest["budgets"].items():
        if metric not in result["metrics"]:
            raise EvidenceError("required hardware measurement missing")
        observed = result["metrics"][metric]
        fields(observed, {"value", "unit"})
        value = observed["value"]
        if type(value) not in (int, float) or not math.isfinite(value) or value < 0:
            raise EvidenceError("invalid hardware measurement")
        if observed["unit"] != budget["unit"] or value > budget["maximum"]:
            raise EvidenceError(f"hardware budget exceeded or unit differs: {metric}")


def execute(manifest_path: Path, report_path: Path, lease_directory: Path,
            *, model: bool = False, execute_hardware: bool = False,
            adapter_runner=None) -> dict:
    report_safe = report_path.resolve() != manifest_path.resolve()
    report = {"schema_version": 1, "kind": "orchestrator_model" if model else
              "physical_hil" if execute_hardware else "physical_hil_preflight",
              "hardware_verified": False, "eligible_physical_hil": False,
              "status": "fail", "started_utc": utc_now(),
              "operations": []}
    try:
        if adapter_runner is not None and not model:
            raise EvidenceError("injected adapter runners require model mode")
        runner = adapter_runner or run_adapter
        manifest = load_json(manifest_path)
        substitutions = validate_manifest(manifest, model=model)
        protected = [manifest_path, Path(manifest["firmware"]["path"]),
                     *(Path(identity["path"]) for identity in manifest["adapter_files"])]
        if any(report_path.resolve() == path.resolve() or
               report_path.with_suffix(".transcript.json").resolve() == path.resolve() for path in protected):
            report_safe = False
            raise EvidenceError("report/transcript must not overwrite input, firmware or adapter files")
        report.update({"manifest_sha256": digest(manifest_path), "lab_id": manifest["lab_id"],
                       "board": manifest["board"], "firmware_sha256": manifest["firmware"]["sha256"],
                       "config_sha256": manifest["config_sha256"],
                       "adapter_files": manifest["adapter_files"],
                       "required_tests": manifest["required_tests"], "budgets": manifest["budgets"]})
        if not model and not execute_hardware:
            report.update({"status": "ready", "dry_run": True,
                           "planned_operations": list(manifest["commands"]),
                           "finished_utc": utc_now()})
            if report_safe:
                atomic_json(report_path, report)
            return report
        deadline = time.monotonic() + manifest["total_timeout_s"]

        def operation(name: str, *, cleanup: bool = False) -> bytes:
            remaining = deadline - time.monotonic()
            if remaining <= 0 and not cleanup:
                raise EvidenceError("HIL total execution deadline expired")
            timeout = min(manifest["operation_timeout_s"], 5 if cleanup else remaining)
            output = runner(command(manifest["commands"][name], substitutions), timeout)
            report["operations"].append({"name": name, "status": "pass"})
            return output

        with EquipmentLease(lease_directory, manifest["board"], manifest.get("serial_lease_id")) as lease:
            try:
                identified = parse_json(operation("identify"))
                fields(identified, {"schema_version", "board"})
                if identified["schema_version"] != 1:
                    raise EvidenceError("invalid identification response")
                same_board(identified["board"], manifest["board"])
                operation("flash")
                if digest(manifest["firmware"]["path"]) != manifest["firmware"]["sha256"]:
                    raise EvidenceError("firmware changed during flash")
                observed = parse_json(operation("verify_flash"))
                fields(observed, {"schema_version", "board", "firmware_sha256"})
                if observed["schema_version"] != 1:
                    raise EvidenceError("invalid readback response")
                same_board(observed["board"], manifest["board"])
                if observed["firmware_sha256"] != manifest["firmware"]["sha256"]:
                    raise EvidenceError("firmware flash readback digest mismatch")
                operation("reset")
                serial = parse_json(operation("serial"))
                validate_serial(serial, manifest)
                transcript = report_path.with_suffix(".transcript.json")
                atomic_json(transcript, serial)
                report["transcript"] = {"path": str(transcript.absolute()), "sha256": digest(transcript)}
                report["tests"] = serial["tests"]
                report["metrics"] = serial["metrics"]
            finally:
                try:
                    operation("cleanup", cleanup=True)
                except BaseException:
                    lease.quarantine("adapter teardown failed; equipment recovery required")
                    report["lease_status"] = "quarantined"
                    raise
        report["status"] = "pass"
        report["hardware_verified"] = not model
        report["eligible_physical_hil"] = not model
    except (EvidenceError, OSError, TypeError, KeyError) as error:
        report["failure"] = str(error)
    report["finished_utc"] = utc_now()
    if report_safe:
        atomic_json(report_path, report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--leases", type=Path, required=True)
    parser.add_argument("--model", action="store_true", help="label fake-adapter tests; ineligible for physical HIL")
    parser.add_argument("--execute", action="store_true", help="explicitly execute the reviewed physical manifest; otherwise validate only")
    args = parser.parse_args()
    if args.model and args.execute:
        parser.error("--model cannot be combined with physical --execute")
    report = execute(args.manifest, args.report, args.leases, model=args.model,
                     execute_hardware=args.execute)
    print(f"{report['kind']}: {report['status']}; report={args.report}")
    return 0 if report["status"] in ("pass", "ready") else 1


if __name__ == "__main__":
    raise SystemExit(main())
