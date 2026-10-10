#!/usr/bin/env python3
"""Offline HIL admission and raw-evidence validation; never operate equipment."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, fields,
    file_identity, identifier, load_json, reject_secret_fields,
    verify_file_identity)
from tools.evidence.identity import check_identity, resolved_identity
from tools.measurement.resources import measure


def require(condition: bool, message: str) -> None:
    if not condition:
        raise EvidenceError(message)


def validate_fixture(fixture: dict) -> None:
    fields(fixture, {"schema_version", "kind", "board", "part", "transport",
        "probe_interfaces", "serial", "required_tests", "physical_status",
        "pending_qualification"})
    require(type(fixture["schema_version"]) is int and fixture["schema_version"] == 1 and
            fixture["kind"] == "hil_fixture", "unsupported HIL fixture")
    for key in ("board", "part"):
        identifier(fixture[key], key)
    require(fixture["transport"] == "swd", "only reviewed SWD transport")
    require(fixture["physical_status"] == "not_executed",
            "fixture template cannot establish physical qualification")
    require(fixture["required_tests"] == ["probe_identity", "flash_readback",
            "reset", "uart_echo"], "required physical checks cannot be weakened")
    require(isinstance(fixture["probe_interfaces"], list) and
            bool(fixture["probe_interfaces"]), "probe interface list empty")
    for interface in fixture["probe_interfaces"]:
        identifier(interface, "probe interface")
    require(len(fixture["probe_interfaces"]) == len(set(fixture["probe_interfaces"])),
            "duplicate probe interface")
    fields(fixture["serial"], {"controller", "tx", "rx", "baudrate",
                              "electrical_mode"})
    require(fixture["serial"]["electrical_mode"] == "ttl_3v3",
            "fixture serial electrical mode unsupported")
    for key in ("controller", "tx", "rx"):
        identifier(fixture["serial"][key], "fixture serial " + key)
    require(type(fixture["serial"]["baudrate"]) is int and
            fixture["serial"]["baudrate"] == 115200,
            "fixture console baud must be reviewed 115200")
    pending = fixture["pending_qualification"]
    require(isinstance(pending, list) and bool(pending),
            "pending physical scope must be explicit")
    names = set()
    for case in pending:
        fields(case, {"id", "fixture", "budget", "unit"})
        identifier(case["id"], "pending physical case")
        identifier(case["unit"], "pending physical unit")
        require(isinstance(case["fixture"], str) and bool(case["fixture"].strip()),
                "pending physical fixture description required")
        require(case["id"] not in names, "duplicate pending physical case")
        names.add(case["id"])
        maximum = case["budget"]
        require(maximum is None or (type(maximum) in (int, float) and
                0 <= maximum <= 2 ** 64 - 1 and math.isfinite(maximum)),
                "invalid pending physical measurement budget")


def validate_station(station: dict, fixture: dict) -> None:
    reject_secret_fields(station)
    fields(station, {"schema_version", "kind", "station_id", "board",
        "part", "pcb_revision", "chip_uid", "probe", "serial", "power",
        "outputs_disconnected", "budgets"})
    require(type(station["schema_version"]) is int and station["schema_version"] == 1
            and station["kind"] == "hil_station",
            "physical station description required")
    for key in ("station_id", "board", "part", "pcb_revision", "chip_uid"):
        identifier(station[key], key)
    require(station["board"] == fixture["board"] and
            station["part"] == fixture["part"], "station Board/part mismatch")
    require(station["outputs_disconnected"] is True,
            "safe product-output disconnect must be recorded")
    fields(station["probe"], {"serial", "interface", "transport", "tool",
                             "interface_config", "target_config"})
    identifier(station["probe"]["serial"], "observed probe serial")
    require(station["probe"]["interface"] in fixture["probe_interfaces"] and
            station["probe"]["transport"] == fixture["transport"],
            "probe interface/transport differs from fixture")
    for key in ("tool", "interface_config", "target_config"):
        verify_file_identity(station["probe"][key])
    fields(station["serial"], {"path", "baudrate", "electrical_mode",
                              "wiring_checked", "ground_connected"})
    require(isinstance(station["serial"]["path"], str) and
            station["serial"]["path"].startswith("/dev/serial/by-id/") and
            len(station["serial"]["path"]) > len("/dev/serial/by-id/"),
            "stable observed serial identity required")
    for key in ("baudrate", "electrical_mode"):
        require(station["serial"][key] == fixture["serial"][key],
                "serial configuration differs from fixture")
    require(station["serial"]["wiring_checked"] is True and
            station["serial"]["ground_connected"] is True,
            "serial wiring/ground inspection required")
    fields(station["power"], {"source", "voltage_mv"})
    identifier(station["power"]["source"], "power source")
    require(type(station["power"]["voltage_mv"]) is int and
            2700 <= station["power"]["voltage_mv"] <= 3600,
            "record observed supply within 2.7..3.6V")
    require(isinstance(station["budgets"], dict) and bool(station["budgets"]),
            "reviewed physical measurement budgets required")
    for name, budget in station["budgets"].items():
        identifier(name, "physical metric")
        fields(budget, {"maximum", "unit"}, {"minimum"})
        identifier(budget["unit"], "measurement unit")
        require(type(budget["maximum"]) in (int, float) and
                0 <= budget["maximum"] <= 2 ** 64 - 1 and
                math.isfinite(budget["maximum"]),
                "invalid physical measurement budget")
        minimum = budget.get("minimum", 0)
        require(type(minimum) in (int, float) and
                0 <= minimum <= budget["maximum"] and math.isfinite(minimum),
                "invalid physical measurement lower bound")


def validate_console(fixture: dict, resolved: dict) -> None:
    """The smoke console must name one exact resolved physical controller."""
    require(resolved["board"] == fixture["board"] and
            resolved["part"] == fixture["part"], "fixture/build Board mismatch")
    consoles = [controller for controller in resolved.get("controllers", []) if
                controller.get("kind") == "uart" and
                controller.get("controller") == fixture["serial"]["controller"]]
    require(len(consoles) == 1, "UART smoke requires exactly one selected fixture console")
    console = consoles[0]
    pins = {pin["function"]: pin["pin"] for pin in console["pins"]}
    require(pins.get("tx") == fixture["serial"]["tx"] and
            pins.get("rx") == fixture["serial"]["rx"] and
            console["baud"] == fixture["serial"]["baudrate"],
            "fixture console pins/baud differ from resolved route")


def admit(station_path: Path, fixture_path: Path, elf_path: Path,
          resolved_path: Path, resources_path: Path) -> dict:
    fixture, station = load_json(fixture_path), load_json(station_path)
    validate_fixture(fixture)
    validate_station(station, fixture)
    resolved, config = resolved_identity(resolved_path)
    validate_console(fixture, resolved)
    resources = load_json(resources_path)
    require(resources.get("kind") == "elf_resource_budget" and
            resources.get("status") == "passed", "passing ELF budget required")
    check_identity(resources["elf"], elf_path)
    check_identity(resources["resolved"], resolved_path)
    verify_file_identity(resources["budget"])
    recomputed = measure(elf_path, resolved_path, Path(resources["budget"]["path"]))
    require(recomputed == resources, "resource report is stale or inconsistent")
    return {"schema_version": 1, "kind": "hil_admission", "status": "ready",
            "physical_status": "not_executed", "station": file_identity(station_path),
            "fixture": file_identity(fixture_path), "elf": file_identity(elf_path),
            "resolved": config, "resources": file_identity(resources_path),
            "board": fixture["board"], "part": fixture["part"],
            "station_id": station["station_id"], "chip_uid": station["chip_uid"],
            "required_tests": fixture["required_tests"],
            "budgets": station["budgets"],
            "pending_qualification": fixture["pending_qualification"]}


def validate_raw(raw: dict, admission: dict, *, maximum_age_s: int = 86400,
                 now: datetime | None = None) -> dict:
    """A host/model or stale/empty/all-skipped report cannot qualify hardware."""
    fields(raw, {"schema_version", "kind", "status", "started_utc", "finished_utc",
        "station_id", "board", "part", "chip_uid", "elf_sha256",
        "resolved_sha256", "admission_sha256", "operations", "tests", "metrics"})
    require(type(raw["schema_version"]) is int and raw["schema_version"] == 1
            and raw["kind"] == "physical_hil" and
            raw["status"] == "passed", "passing physical raw evidence required")
    require(type(maximum_age_s) is int and maximum_age_s > 0,
            "positive evidence freshness limit required")
    try:
        require(isinstance(raw["started_utc"], str) and
                isinstance(raw["finished_utc"], str), "timestamp strings required")
        started = datetime.fromisoformat(raw["started_utc"].replace("Z", "+00:00"))
        finished = datetime.fromisoformat(raw["finished_utc"].replace("Z", "+00:00"))
    except (TypeError, ValueError) as error:
        raise EvidenceError("invalid physical evidence timestamps") from error
    now = now or datetime.now(timezone.utc)
    require(started.tzinfo is not None and finished.tzinfo is not None,
            "UTC-offset physical timestamps required")
    require(started <= finished <= now and
            (now - started).total_seconds() <= maximum_age_s,
            "physical report stale or future-dated")
    for key in ("station", "fixture", "elf", "resolved", "resources"):
        verify_file_identity(admission[key])
    for key in ("station_id", "board", "part", "chip_uid"):
        require(raw[key] == admission[key], "raw physical identity mismatch: " + key)
    require(raw["elf_sha256"] == admission["elf"]["sha256"] and
            raw["resolved_sha256"] == admission["resolved"]["sha256"],
            "raw physical firmware/config identity mismatch")
    from tools.evidence.identity import canonical_digest
    require(raw["admission_sha256"] == canonical_digest(admission),
            "raw physical evidence is not bound to admitted station/budget")
    require(isinstance(raw["operations"], list) and
            [operation.get("name") for operation in raw["operations"]] ==
            ["identify", "flash", "verify_flash", "reset", "serial", "cleanup"],
            "complete physical operation sequence required")
    for operation in raw["operations"]:
        fields(operation, {"name", "status", "exit_code", "raw"})
        require(operation["status"] == "passed" and type(operation["exit_code"]) is int
                and operation["exit_code"] == 0,
                "failed/skipped physical operation")
        verify_file_identity(operation["raw"])
    require(isinstance(raw["tests"], list) and bool(raw["tests"]),
            "empty physical test evidence rejected")
    ids = []
    for test in raw["tests"]:
        fields(test, {"name", "status", "executed_cases", "raw"})
        identifier(test["name"], "physical test")
        require(test["status"] == "passed" and type(test["executed_cases"]) is int
                and test["executed_cases"] > 0, "zero/skipped/failed physical test")
        verify_file_identity(test["raw"])
        ids.append(test["name"])
    require(len(ids) == len(set(ids)) and set(ids) == set(admission["required_tests"]),
            "physical required test enumeration differs from admission")
    require(isinstance(raw["metrics"], dict) and
            set(raw["metrics"]) == set(admission["budgets"]),
            "physical measurement enumeration differs from budgets")
    for name, metric in raw["metrics"].items():
        fields(metric, {"value", "unit", "raw"})
        budget = admission["budgets"][name]
        require(type(metric["value"]) in (int, float) and
                0 <= metric["value"] <= 2 ** 64 - 1 and
                math.isfinite(metric["value"]) and
                budget.get("minimum", 0) <= metric["value"] <=
                budget["maximum"] and metric["unit"] == budget["unit"],
                "physical measurement budget failed: " + name)
        verify_file_identity(metric["raw"])
    return {"status": "passed", "physical_status": "qualified_for_recorded_scope",
            "executed_tests": len(ids)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("station", "fixture", "elf", "resolved", "resources", "report"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--raw-evidence", type=Path)
    args = parser.parse_args()
    protected = {getattr(args, key).resolve() for key in
                 ("station", "fixture", "elf", "resolved", "resources")}
    if args.report.resolve() in protected:
        parser.exit(1, "report cannot overwrite admitted inputs\n")
    try:
        report = admit(args.station, args.fixture, args.elf, args.resolved, args.resources)
        if args.raw_evidence:
            report["physical_validation"] = validate_raw(load_json(args.raw_evidence), report)
            report["raw_evidence"] = file_identity(args.raw_evidence)
    except (EvidenceError, OSError, KeyError, TypeError) as error:
        report = {"schema_version": 1, "kind": "hil_admission", "status": "failed",
                  "physical_status": "not_executed", "failure": str(error)}
    atomic_json(args.report, report)
    print("HIL admission: " + report["status"] + "; equipment not operated")
    return 0 if report["status"] == "ready" else 1


if __name__ == "__main__":
    raise SystemExit(main())
