#!/usr/bin/env python3
"""Validate a board runner's evidence shape and artifact association.

This does not execute a board test or prove the provenance of a report. The
trusted HIL runner owns fixture leases, flashing and physical observations.
"""
import argparse
import hashlib
import json
import re
from datetime import datetime
from pathlib import Path


class EvidenceError(ValueError):
    pass


def validate(report, profile, firmware):
    if not isinstance(report, dict) or not isinstance(profile, dict):
        raise EvidenceError("Report and profile must be objects")
    if report.get("schema_version") != 1 or report.get("execution_kind") != "hardware":
        raise EvidenceError("A version 1 physical hardware report is required")
    if not profile.get("board") or report.get("profile") != profile["id"]:
        raise EvidenceError("Report must match a concrete board profile")
    board = report.get("board", {})
    if not isinstance(board, dict):
        raise EvidenceError("Board identity must be an object")
    if board.get("id") != profile["board"]:
        raise EvidenceError("Board identity does not match the profile")
    revision = board.get("pcb_revision")
    if not isinstance(revision, str) or not revision.strip() or revision.lower() in {"unknown", "not-run", "unverified"}:
        raise EvidenceError("Physical PCB revision must be observed")
    uid = board.get("silicon_uid")
    if not isinstance(uid, str) or not re.fullmatch(r"[0-9a-fA-F]{24}", uid):
        raise EvidenceError("Observed 96-bit silicon UID is required")
    if not report.get("fixture_lease_id"):
        raise EvidenceError("A fixture lease identity is required")
    identity = report.get("build", {})
    if not isinstance(identity, dict):
        raise EvidenceError("Build identity must be an object")
    if identity.get("firmware_sha256") != hashlib.sha256(firmware.read_bytes()).hexdigest():
        raise EvidenceError("The tested firmware digest does not match the artifact")
    commit = identity.get("source_commit")
    if not isinstance(commit, str) or not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
        raise EvidenceError("An exact Git source commit is required")
    config_digest = identity.get("effective_config_sha256")
    if not isinstance(config_digest, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", config_digest):
        raise EvidenceError("Effective build configuration digest is required")
    if not identity.get("toolchain_version") or not identity.get("dependency_commits"):
        raise EvidenceError("Toolchain and dependency identities are required")
    try:
        start = datetime.fromisoformat(report["started_at"].replace("Z", "+00:00"))
        end = datetime.fromisoformat(report["finished_at"].replace("Z", "+00:00"))
        if start.tzinfo is None or end.tzinfo is None or end < start:
            raise ValueError("invalid timestamp order")
    except (KeyError, TypeError, ValueError) as exc:
        raise EvidenceError("Timestamped execution interval is required") from exc
    cases = report.get("cases", [])
    if not isinstance(cases, list) or any(not isinstance(case, dict) for case in cases):
        raise EvidenceError("Test cases must be a list of objects")
    by_id = {case.get("id"): case for case in cases}
    if len(by_id) != len(cases):
        raise EvidenceError("Duplicate test case identifiers")
    required = profile.get("required_hil", [])
    if not required:
        raise EvidenceError("Profile does not define a hardware acceptance matrix")
    for case_id in required:
        case = by_id.get(case_id, {})
        if case.get("status") != "passed" or not case.get("observations"):
            raise EvidenceError(f"Missing physical observations for passed case {case_id}")
    return len(required)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--matrix", type=Path,
                        default=Path(__file__).resolve().parents[2] / "profiles/support-matrix.json")
    args = parser.parse_args()
    try:
        profiles = json.loads(args.matrix.read_text())["profiles"]
        profile = next(item for item in profiles if item["id"] == args.profile)
        count = validate(json.loads(args.report.read_text()), profile, args.firmware)
    except (EvidenceError, OSError, ValueError, KeyError, StopIteration) as exc:
        parser.exit(1, f"HIL evidence rejected: {exc}\n")
    print(f"HIL evidence structure accepted: {count} required cases; runner provenance remains external")


if __name__ == "__main__":
    main()
