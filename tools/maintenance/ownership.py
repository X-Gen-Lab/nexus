#!/usr/bin/env python3
"""Validate review assignments and explicitly prepare a CODEOWNERS file.

Validation does not verify membership or enable GitHub branch protection.
An unassigned role template is a pending report, never a configured review gate.
"""

import argparse
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
ACCOUNT = re.compile(
    r"@[A-Za-z0-9](?:[A-Za-z0-9-]{0,38})"
    r"(?:/[A-Za-z0-9][A-Za-z0-9_-]*)?\Z")
ROLE = re.compile(r"[a-z][a-z0-9-]*\Z")
REVIEW_PATH = re.compile(r"/[A-Za-z0-9_.*?/-]+\Z")


def fields(document, required, optional, context):
    if (not isinstance(document, dict)
            or document.keys() - required - optional
            or required - document.keys()):
        raise ValueError(f"Invalid {context} fields")


def validate(document, require_assigned=False):
    """Validate named seats, unique accounts, backups and review paths."""
    fields(document, {"schema_version", "assignment_status", "roles",
                      "interfaces", "note"}, set(), "team")
    if (type(document["schema_version"]) is not int
            or document["schema_version"] != 1):
        raise ValueError("Unsupported team schema")
    statuses = {"roles_defined_named_team_assignment_pending", "assigned"}
    if (not isinstance(document["assignment_status"], str)
            or document["assignment_status"] not in statuses):
        raise ValueError("Unknown assignment status")
    if not isinstance(document["note"], str) or not document["note"].strip():
        raise ValueError("Team requires an explicit limitations note")
    if not isinstance(document["roles"], list) or not document["roles"]:
        raise ValueError("Team has no roles")
    ids, accounts, paths, pending, seats = set(), set(), set(), [], 0
    for role in document["roles"]:
        fields(role, {"id", "seats", "scope", "backup_role", "review_paths",
                      "members"}, set(), "role")
        identity = role["id"]
        if (not isinstance(identity, str) or not ROLE.fullmatch(identity)
                or identity in ids):
            raise ValueError("Invalid or duplicate role identity")
        ids.add(identity)
        if type(role["seats"]) is not int or role["seats"] < 1:
            raise ValueError("Role seats must be positive integers")
        seats += role["seats"]
        for name in ("scope", "review_paths", "members"):
            values = role[name]
            if (not isinstance(values, list)
                    or any(not isinstance(value, str) or not value
                           for value in values)
                    or len(values) != len(set(values))):
                raise ValueError(f"Invalid role {name}")
        if not role["scope"] or not role["review_paths"]:
            raise ValueError("Role requires scopes and review paths")
        for path in role["review_paths"]:
            if (not REVIEW_PATH.fullmatch(path) or ".." in path.split("/")
                    or path in paths):
                raise ValueError("Invalid CODEOWNERS review path")
            paths.add(path)
        if role["members"] and len(role["members"]) != role["seats"]:
            raise ValueError("Named members must fill the declared seats")
        if not role["members"]:
            pending.append(identity)
        for account in role["members"]:
            if not ACCOUNT.fullmatch(account) or account in accounts:
                raise ValueError("Invalid or duplicate named account")
            accounts.add(account)
    if seats != 10:
        raise ValueError("Team must preserve the declared ten seats")
    for role in document["roles"]:
        if (not isinstance(role["backup_role"], str)
                or role["backup_role"] not in ids
                or role["backup_role"] == role["id"]):
            raise ValueError("Role requires a different maintained backup role")
    fields(document["interfaces"], {"security", "manufacturing",
                                    "field-maintenance"}, set(), "interfaces")
    if any(not isinstance(value, str) or value not in ids
           for value in document["interfaces"].values()):
        raise ValueError("Interface references an unknown role")
    if pending and (require_assigned
                    or document["assignment_status"] == "assigned"):
        raise ValueError("Team is unassigned: " + ", ".join(pending))
    if not pending and document["assignment_status"] != "assigned":
        raise ValueError("Named team must explicitly mark status assigned")
    return {"status": "pending" if pending else "assigned", "seats": seats,
            "unassigned_roles": pending, "github_access_verified": False,
            "branch_protection_verified": False}


def render(document):
    """Produce review ownership only after every seat has a named account."""
    validate(document, require_assigned=True)
    roles = {role["id"]: role for role in document["roles"]}
    paths = {}
    for role in document["roles"]:
        reviewers = role["members"] + roles[role["backup_role"]]["members"]
        for path in role["review_paths"]:
            if path in paths:
                raise ValueError("Review paths must have one accountable role")
            paths[path] = reviewers
    # GitHub uses the last matching entry. Put broad patterns before overrides.
    lines = ["# Generated from maintainer-roles.json; verify account access.",
             "# CODEOWNERS requires branch rules to enforce reviews."]
    for path in sorted(paths, key=lambda value: (len(value.split("/")), value)):
        lines.append(path + " " + " ".join(paths[path]))
    return "\n".join(lines) + "\n"


def write(document, output):
    text = render(document)
    if output.resolve() == (ROOT / ".github/CODEOWNERS").resolve():
        raise ValueError("Generate a proposed file before installing ownership")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8")


def unique_pairs(pairs):
    document = {}
    for key, value in pairs:
        if key in document:
            raise ValueError("Duplicate team JSON key: " + key)
        document[key] = value
    return document


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path,
                        default=ROOT / ".github/maintainer-roles.json")
    parser.add_argument("--require-assigned", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(arguments)
    try:
        document = json.loads(args.input.read_text(encoding="utf-8"),
                              object_pairs_hook=unique_pairs)
        report = validate(document, require_assigned=args.require_assigned)
        if args.output:
            write(document, args.output)
        print(json.dumps(report, sort_keys=True))
    except (OSError, ValueError) as error:
        print(f"Review ownership rejected: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
