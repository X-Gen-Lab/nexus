#!/usr/bin/env python3
"""Prepare an unbound HIL work plan from one exact software configuration."""

from __future__ import annotations

import argparse
import copy
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, fields,
                                   file_identity, load_json)
from tools.evidence.identity import resolved_identity
from tools.hil.admit import validate_console, validate_fixture


def _declared_hardware(resolved: dict) -> dict:
    """Reuse configuration choice validation; do not manufacture observations."""
    from tools.configure import board_hardware
    hardware = resolved["hardware"]
    fields(hardware, {"options", "selected", "physical_verification"})
    if hardware["physical_verification"] != "not_established_by_configuration":
        raise EvidenceError("hardware declarations do not establish observation")
    if not isinstance(hardware["selected"], dict):
        raise EvidenceError("resolved hardware requires an explicit choice map")
    catalogue = board_hardware.options({"hardware_options": hardware["options"]})
    board_hardware.select(catalogue, hardware["selected"])
    return copy.deepcopy(hardware)


def preparation_plan(fixture: dict, resolved: dict) -> dict:
    """No station, equipment action or physical result is inferred here."""
    validate_fixture(fixture)
    validate_console(fixture, resolved)
    controllers = [{key: controller[key] for key in
                    ("id", "kind", "controller", "mode")}
                   for controller in resolved["controllers"]]
    pending = [{**case, "status": "not_executed"}
               for case in fixture["pending_qualification"]]
    report = {"schema_version": 1, "kind": "hil_preparation",
            "status": "prepared_unbound", "physical_status": "not_executed",
            "board": fixture["board"], "part": fixture["part"],
            "backend": resolved["backend"], "controllers": controllers,
            "required_tests": list(fixture["required_tests"]),
            "pending_qualification": pending,
            "unreviewed_budgets": [case["id"] for case in pending
                                   if case["budget"] is None],
            "station_prerequisites": [
                "observed exact part, PCB revision and chip UID",
                "observed probe serial and content-hashed tool/configuration",
                "stable serial path and reviewed 3.3V wiring/ground",
                "observed supply and disconnected product outputs",
                "reviewed measurement fixtures and numeric budgets",
                "passing linked ELF/configuration/resource admission"]}
    if "hardware" in resolved:
        report["hardware"] = _declared_hardware(resolved)
        report["station_prerequisites"].append(
            "independently observed component population, jumpers and connector "
            "personality matching every declared hardware selection; unselected "
            "options remain unknown")
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--resolved", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)
    if args.report.resolve() in {args.fixture.resolve(), args.resolved.resolve()}:
        parser.exit(1, "preparation report cannot overwrite its inputs\n")
    try:
        resolved, identity = resolved_identity(args.resolved)
        report = preparation_plan(load_json(args.fixture), resolved)
        report.update(fixture=file_identity(args.fixture), resolved=identity)
    except (EvidenceError, OSError, ValueError, KeyError, TypeError) as error:
        report = {"schema_version": 1, "kind": "hil_preparation",
                  "status": "failed", "physical_status": "not_executed",
                  "failure": str(error)}
    atomic_json(args.report, report)
    print("HIL preparation: " + report["status"] + "; equipment not operated")
    return 0 if report["status"] == "prepared_unbound" else 1


if __name__ == "__main__":
    raise SystemExit(main())
