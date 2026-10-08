#!/usr/bin/env python3
"""Provision via trusted station adapters and retain only public identity audit."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import math
from pathlib import Path
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.evidence.common import (EvidenceError, atomic_json, command, digest, fields,
                                     identifier, load_json, parse_json, reject_secret_fields,
                                     run_adapter, sha256_value, verify_file_identity)
from scripts.evidence.release_gate import promote
from scripts.hil.run_hil import EquipmentLease, same_board


def verified_release(path: Path) -> dict:
    report = load_json(path)
    if (report.get("kind") != "release_promotion_gate" or report.get("status") != "pass" or
            report.get("level") != "enterprise"):
        raise EvidenceError("manufacturing requires an enterprise evidence gate")
    for name in ("inventory", "policy", "physical_hil", "reviews", "signed_evidence"):
        verify_file_identity(report[name])
    signature = report["signature_verification"]
    verify_file_identity(signature["public_key"])
    verify_file_identity(signature["signature"])
    # Reverify the signature and every upstream identity; a status string is
    # never enough to authorize a manufacturing station.
    promote(Path(report["inventory"]["path"]), level="enterprise",
            policy_path=Path(report["policy"]["path"]),
            hil_path=Path(report["physical_hil"]["path"]),
            reviews_path=Path(report["reviews"]["path"]),
            signed_evidence=Path(report["signed_evidence"]["path"]),
            public_key=Path(signature["public_key"]["path"]),
            signature=Path(signature["signature"]["path"]))
    return load_json(report["inventory"]["path"])


def calibration_check(observed: dict, requirements: dict) -> None:
    fields(observed, {"schema_version", "status", "limits_id", "measurements"})
    reject_secret_fields(observed)
    if observed["schema_version"] != 1 or observed["status"] != "pass":
        raise EvidenceError("calibration failed")
    if observed["limits_id"] != requirements["limits_id"]:
        raise EvidenceError("calibration limits identity differs from station policy")
    if not isinstance(observed["measurements"], dict):
        raise EvidenceError("calibration measurements required")
    for metric, limits in requirements["measurements"].items():
        if metric not in observed["measurements"]:
            raise EvidenceError("required calibration measurement missing")
        measured = observed["measurements"][metric]
        fields(measured, {"value", "unit"})
        value = measured["value"]
        if (type(value) not in (int, float) or not math.isfinite(value) or
                measured["unit"] != limits["unit"] or not limits["minimum"] <= value <= limits["maximum"]):
            raise EvidenceError("calibration outside reviewed limits")


def execute(manifest_path: Path, audit_root: Path, report_path: Path, leases: Path) -> dict:
    report = {"schema_version": 1, "kind": "manufacturing_audit", "status": "fail",
              "attempt_id": uuid.uuid4().hex, "operations": [],
              "started_utc": datetime.now(timezone.utc).isoformat()}
    reservation = None
    try:
        manifest = load_json(manifest_path)
        reject_secret_fields(manifest)
        fields(manifest, {"schema_version", "kind", "lot_id", "station_id", "operator_role",
                          "board", "enterprise_gate", "adapter_files", "commands",
                          "operation_timeout_s", "key_reference", "identity_id",
                          "calibration_requirements"})
        if manifest["schema_version"] != 1 or manifest["kind"] != "manufacturing_station":
            raise EvidenceError("unsupported manufacturing manifest")
        for name in ("lot_id", "station_id", "operator_role", "key_reference", "identity_id"):
            identifier(manifest[name], name)
        fields(manifest["board"], {"id", "profile", "revision", "probe_serial"})
        for name, value in manifest["board"].items():
            identifier(value, name)
        if not isinstance(manifest["adapter_files"], list) or not manifest["adapter_files"]:
            raise EvidenceError("station adapter file identities required")
        for identity in manifest["adapter_files"]:
            verify_file_identity(identity)
        fields(manifest["commands"], {"identify", "flash", "verify_flash", "calibrate",
                                     "provision", "attest", "reset", "cleanup"})
        timeout = manifest["operation_timeout_s"]
        if type(timeout) not in (int, float) or not 0 < timeout <= 300:
            raise EvidenceError("invalid station operation timeout")
        limits = manifest["calibration_requirements"]
        fields(limits, {"limits_id", "measurements"})
        identifier(limits["limits_id"], "calibration limits id")
        if not isinstance(limits["measurements"], dict) or not limits["measurements"]:
            raise EvidenceError("reviewed calibration requirements required")
        for metric, bound in limits["measurements"].items():
            identifier(metric, "calibration metric")
            fields(bound, {"minimum", "maximum", "unit"})
            identifier(bound["unit"], "calibration unit")
            if (type(bound["minimum"]) not in (int, float) or type(bound["maximum"]) not in (int, float) or
                    not math.isfinite(bound["minimum"]) or not math.isfinite(bound["maximum"]) or
                    bound["minimum"] > bound["maximum"]):
                raise EvidenceError("invalid calibration limits")
        inventory = verified_release(Path(manifest["enterprise_gate"]))
        if (inventory["product"]["board_profile"] != manifest["board"]["profile"] or
                inventory["product"]["board_revision"] != manifest["board"]["revision"]):
            raise EvidenceError("manufacturing board differs from verified release")
        image = next((item for item in inventory["artifacts"] if item["role"] == "image"), None)
        if image is None:
            raise EvidenceError("verified firmware image missing")
        substitutions = {**manifest["board"], "firmware": image["path"],
                         "key_reference": manifest["key_reference"], "identity_id": manifest["identity_id"],
                         "device_uid": "PENDING_HARDWARE_IDENTIFICATION"}
        if any("{device_uid}" in argument for argument in manifest["commands"]["identify"]):
            raise EvidenceError("identify cannot depend on an unobserved device UID")
        for argv in manifest["commands"].values():
            command(argv, substitutions)
        report.update({name: manifest[name] for name in ("lot_id", "station_id", "operator_role", "board", "identity_id")})
        report.update({"manifest_sha256": digest(manifest_path), "firmware_sha256": image["sha256"],
                       "config_sha256": inventory["configuration"]["effective"]["sha256"],
                       "source_commit": inventory["source"]["commit"],
                       "enterprise_gate_sha256": digest(manifest["enterprise_gate"]),
                       "key_reference_sha256": hashlib.sha256(manifest["key_reference"].encode()).hexdigest()})

        def operation(name: str):
            output = run_adapter(command(manifest["commands"][name], substitutions), timeout)
            report["operations"].append({"name": name, "status": "pass"})
            return output

        with EquipmentLease(leases, manifest["board"]) as lease:
            try:
                identified = parse_json(operation("identify"))
                reject_secret_fields(identified)
                fields(identified, {"schema_version", "board", "device_uid"})
                if identified["schema_version"] != 1:
                    raise EvidenceError("invalid station identity response")
                same_board(identified["board"], manifest["board"])
                uid = identifier(identified["device_uid"], "device uid")
                report["device_uid"] = uid
                substitutions["device_uid"] = uid
                devices = audit_root / "devices"
                devices.mkdir(parents=True, exist_ok=True)
                pending_reservation = devices / uid
                try:
                    pending_reservation.mkdir()
                except FileExistsError as error:
                    raise EvidenceError("device UID already reserved; supervised recovery required") from error
                reservation = pending_reservation
                atomic_json(reservation / "intent.json", {**report, "status": "reserved"})
                identities = audit_root / "identities"
                identities.mkdir(parents=True, exist_ok=True)
                identity_reservation = identities / manifest["identity_id"]
                try:
                    identity_reservation.mkdir()
                except FileExistsError as error:
                    raise EvidenceError("identity ID already reserved; supervised recovery required") from error
                atomic_json(identity_reservation / "owner.json", {"device_uid": uid,
                            "attempt_id": report["attempt_id"], "identity_id": manifest["identity_id"]})
                operation("flash")
                readback = parse_json(operation("verify_flash"))
                fields(readback, {"schema_version", "device_uid", "firmware_sha256"})
                if (readback["schema_version"] != 1 or readback["device_uid"] != uid or
                        readback["firmware_sha256"] != image["sha256"] or digest(image["path"]) != image["sha256"]):
                    raise EvidenceError("manufacturing flash identity/readback mismatch")
                calibration = parse_json(operation("calibrate"))
                calibration_check(calibration, limits)
                report["calibration"] = calibration
                atomic_json(reservation / "calibrated.json", {**report, "status": "calibrated"})
                # Adapter/HSM resolves an opaque key reference; no key bytes,
                # passwords or enrollment credentials enter this process.
                provisioned = parse_json(operation("provision"))
                reject_secret_fields(provisioned)
                fields(provisioned, {"schema_version", "status", "device_uid", "identity_id"})
                if (provisioned["schema_version"] != 1 or provisioned["status"] != "pass" or
                        provisioned["device_uid"] != uid or provisioned["identity_id"] != manifest["identity_id"]):
                    raise EvidenceError("provisioning result identity mismatch")
                attested = parse_json(operation("attest"))
                reject_secret_fields(attested)
                fields(attested, {"schema_version", "status", "device_uid", "identity_id",
                                  "public_key_sha256", "certificate_sha256"})
                if (attested["schema_version"] != 1 or attested["status"] != "pass" or
                        attested["device_uid"] != uid or attested["identity_id"] != manifest["identity_id"]):
                    raise EvidenceError("device identity attestation mismatch")
                sha256_value(attested["public_key_sha256"])
                sha256_value(attested["certificate_sha256"])
                report["public_identity"] = attested
                operation("reset")
            finally:
                try:
                    operation("cleanup")
                except BaseException:
                    lease.quarantine("station teardown failed; equipment recovery required")
                    report["lease_status"] = "quarantined"
                    raise
        report["status"] = "pass"
    except (EvidenceError, OSError, KeyError, TypeError, StopIteration) as error:
        report["failure"] = str(error)
    report["finished_utc"] = datetime.now(timezone.utc).isoformat()
    if reservation is not None and reservation.is_dir():
        # Hash-linked public audit states persist even when provisioning failed.
        previous = reservation / "calibrated.json"
        if not previous.is_file():
            previous = reservation / "intent.json"
        if previous.is_file():
            report["previous_state_sha256"] = digest(previous)
        atomic_json(reservation / "result.json", report)
    atomic_json(report_path, report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--audit-root", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--leases", type=Path, required=True)
    args = parser.parse_args()
    result = execute(args.manifest, args.audit_root, args.report, args.leases)
    print(f"manufacturing: {result['status']}; public audit={args.report}")
    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
