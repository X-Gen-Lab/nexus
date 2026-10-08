#!/usr/bin/env python3
"""Validate requirement/backlog/role mappings without inventing completion evidence."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.evidence.common import (EvidenceError, atomic_json, fields, identifier,
                                     load_json, verify_file_identity)


def validate(requirements_path: Path, backlog_path: Path, roles_path: Path) -> dict:
    roles = load_json(roles_path)
    fields(roles, {"schema_version", "assignment_status", "roles", "interfaces", "note"})
    if roles["schema_version"] != 1 or not isinstance(roles["roles"], list) or not roles["roles"]:
        raise EvidenceError("maintainer role schema required")
    role_ids = set()
    seats = 0
    for role in roles["roles"]:
        fields(role, {"id", "seats", "scope", "backup_role"})
        identifier(role["id"], "owner role")
        if role["id"] in role_ids or type(role["seats"]) is not int or role["seats"] <= 0:
            raise EvidenceError("duplicate role or invalid allocation")
        if not isinstance(role["scope"], list) or not role["scope"] or any(not isinstance(scope, str) or not scope for scope in role["scope"]):
            raise EvidenceError("role responsibility scopes required")
        seats += role["seats"]
        role_ids.add(role["id"])
    if seats != 10:
        raise EvidenceError("approved maintainer allocation totals 10 people")
    for role in roles["roles"]:
        if role["backup_role"] not in role_ids or role["backup_role"] == role["id"]:
            raise EvidenceError("distinct valid backup role required")
    if any(role not in role_ids for role in roles["interfaces"].values()):
        raise EvidenceError("interface owner must be an allocated role")
    with backlog_path.open(newline="", encoding="utf-8") as stream:
        backlog = list(csv.DictReader(stream))
    backlog_ids = {item["id"] for item in backlog}
    if not backlog or len(backlog_ids) != len(backlog):
        raise EvidenceError("backlog identities must be nonempty and unique")
    document = load_json(requirements_path)
    fields(document, {"schema_version", "product_id", "product_status", "requirements"})
    if document["schema_version"] != 1:
        raise EvidenceError("unsupported requirement schema")
    identifier(document["product_id"], "product id")
    requirements = document["requirements"]
    if not isinstance(requirements, list) or not requirements:
        raise EvidenceError("zero requirements rejected")
    seen = set()
    statuses = {"planned": 0, "implemented": 0, "host_validated": 0, "hardware_validated": 0}
    for requirement in requirements:
        fields(requirement, {"id", "backlog_ids", "owner_role", "statement", "verification_kind",
                             "status", "acceptance", "evidence"})
        identity = requirement["id"]
        if not isinstance(identity, str) or not re.fullmatch(r"NEX-REQ-\d{3}", identity) or identity in seen:
            raise EvidenceError("unique NEX-REQ-nnn identity required")
        seen.add(identity)
        if requirement["owner_role"] not in role_ids:
            raise EvidenceError("requirement owner is not an allocated role")
        linked = requirement["backlog_ids"]
        if not isinstance(linked, list) or not linked or len(linked) != len(set(linked)) or not set(linked).issubset(backlog_ids):
            raise EvidenceError("requirement links missing or unknown backlog identities")
        for name in ("statement", "acceptance"):
            if not isinstance(requirement[name], str) or not requirement[name].strip():
                raise EvidenceError("requirement statement and acceptance must be concrete")
        if requirement["verification_kind"] not in {"host_contract", "host_and_hardware", "hardware_measurement", "manufacturing_station"}:
            raise EvidenceError("invalid verification kind")
        status = requirement["status"]
        if status not in statuses:
            raise EvidenceError("unsupported requirement status")
        statuses[status] += 1
        evidence = requirement["evidence"]
        if not isinstance(evidence, list):
            raise EvidenceError("requirement evidence list required")
        if status in {"host_validated", "hardware_validated"} and not evidence:
            raise EvidenceError("validated requirement needs executed evidence")
        physical = False
        for item in evidence:
            fields(item, {"kind", "identity"})
            verify_file_identity(item["identity"])
            if item["kind"] == "physical_hil":
                report = load_json(item["identity"]["path"])
                if (report.get("kind") != "physical_hil" or report.get("status") != "pass" or
                        report.get("eligible_physical_hil") is not True):
                    raise EvidenceError("physical requirement cannot use synthetic HIL")
                physical = True
            elif item["kind"] not in {"host_contract", "manufacturing_audit", "measurement"}:
                raise EvidenceError("unknown requirement evidence kind")
        if status == "hardware_validated" and not physical and requirement["verification_kind"] != "manufacturing_station":
            raise EvidenceError("hardware validated status needs passing physical HIL")
        if status == "hardware_validated" and requirement["verification_kind"] == "manufacturing_station":
            if not any(item["kind"] == "manufacturing_audit" and
                       load_json(item["identity"]["path"]).get("status") == "pass" for item in evidence):
                raise EvidenceError("manufacturing validation needs executed station audit")
    return {"schema_version": 1, "kind": "requirement_traceability", "status": "pass",
            "requirements": len(requirements), "status_counts": statuses, "allocated_people": seats,
            "named_team_assignment": roles["assignment_status"],
            "limitation": "schema consistency is not product acceptance or hardware execution"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--requirements", type=Path, default=Path("docs/requirements/industrial-reference.json"))
    parser.add_argument("--backlog", type=Path, default=Path("docs/strategy/backlog.csv"))
    parser.add_argument("--roles", type=Path, default=Path(".github/maintainer-roles.json"))
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = validate(args.requirements, args.backlog, args.roles)
    except (EvidenceError, OSError, KeyError, TypeError) as error:
        report = {"schema_version": 1, "kind": "requirement_traceability", "status": "fail", "failure": str(error)}
    atomic_json(args.report, report)
    print(f"traceability: {report['status']}; report={args.report}")
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
