#!/usr/bin/env python3
"""Prepare and validate bounded physical traces without operating equipment."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (
    EvidenceError, atomic_json, fields, file_identity, identifier, load_json,
    verify_file_identity,
)
from tools.evidence.identity import canonical_digest, resolved_identity
from tools.hil.admit import admit, require, validate_fixture


# These describe observations, not firmware configuration or Board capabilities.
METRICS = {
    "cycles": ("cycles", ("dwt_snapshot",), True),
    "irq_latency": ("us", ("logic_analyzer", "hardware_timer"), False),
    "service_interval": ("us", ("monotonic_timestamp",), False),
    "stack_used_peak": (
        "bytes", ("freertos_high_water", "stack_watermark"), True),
    "throughput": ("bytes_per_s", ("wire_byte_counter",), False),
    "loss": ("count", ("receiver_event_counter",), True),
    "duration": ("s", ("monotonic_timestamp",), False),
}
MAX_TRACES = len(METRICS)
MAX_SAMPLES = 100000
MAX_OBSERVATION = 2 ** 64 - 1


def measurement_plan(fixture: dict, resolved: dict) -> dict:
    """An unbound checklist cannot supply budgets or measured values."""
    validate_fixture(fixture)
    require(resolved["board"] == fixture["board"] and
            resolved["part"] == fixture["part"],
            "measurement fixture/build Board mismatch")
    return {
        "schema_version": 1, "kind": "hil_measurement_plan",
        "status": "prepared_unbound", "physical_status": "not_executed",
        "board": fixture["board"], "part": fixture["part"],
        "backend": resolved["backend"],
        "measurements": [
            {"id": name, "unit": unit, "methods": list(methods),
             "budget": None, "status": "not_executed"}
            for name, (unit, methods, _) in METRICS.items()
        ],
        "conditions": [
            "bind reviewed station, workload, firmware and numeric budgets",
            "measure DWT only when enabled; record frequency and wrap bounds",
            "record IRQ priorities, load and capture resolution",
            "keep required executor running while measuring service intervals",
            "report peak stack use from actual watermark",
            "observe bytes and loss on actual transport",
            "retain raw traces; qualify only the recorded measurements",
        ],
    }


def _freshness(raw: dict, maximum_age_s: int, now: datetime) -> None:
    require(type(maximum_age_s) is int and maximum_age_s > 0,
            "positive measurement freshness limit required")
    require(now.tzinfo is not None, "measurement current time needs UTC offset")
    try:
        require(isinstance(raw["started_utc"], str) and
                isinstance(raw["finished_utc"], str),
                "measurement timestamp strings required")
        started = datetime.fromisoformat(
            raw["started_utc"].replace("Z", "+00:00"))
        finished = datetime.fromisoformat(
            raw["finished_utc"].replace("Z", "+00:00"))
    except (ValueError, TypeError) as error:
        raise EvidenceError("invalid measurement timestamps") from error
    require(started.tzinfo is not None and finished.tzinfo is not None and
            started <= finished <= now and
            (now - started).total_seconds() <= maximum_age_s,
            "measurement report stale or future-dated")


def _positive_number(value, label: str) -> None:
    require(type(value) in (int, float) and 0 < value <= MAX_OBSERVATION
            and math.isfinite(value), "positive bounded " + label + " required")


def _context(identity: dict, admission_digest: str, budgets: dict) -> dict:
    """Bind load and acquisition conditions separately from statistics."""
    context = load_json(verify_file_identity(identity))
    fields(context, {"schema_version", "kind", "admission_sha256", "workload",
                     "cpu_clock_hz", "load", "executor", "irq_priorities",
                     "measurements"})
    require(type(context["schema_version"]) is int and
            context["schema_version"] == 1 and
            context["kind"] == "hil_acquisition_context" and
            context["admission_sha256"] == admission_digest,
            "acquisition context is not bound to admission")
    verify_file_identity(context["workload"])
    require(type(context["cpu_clock_hz"]) is int and
            0 < context["cpu_clock_hz"] <= 2 ** 32 - 1,
            "observed CPU clock required")
    for key in ("load", "executor"):
        require(isinstance(context[key], str) and
                0 < len(context[key].strip()) <= 4096,
                "acquisition " + key + " description required")
    priorities = context["irq_priorities"]
    require(isinstance(priorities, dict) and len(priorities) <= 256 and
            ("irq_latency" not in budgets or bool(priorities)),
            "observed IRQ priorities required")
    for name, priority in priorities.items():
        identifier(name, "IRQ identity")
        require(type(priority) is int and 0 <= priority <= 255,
                "invalid observed IRQ priority")
    measurements = context["measurements"]
    require(isinstance(measurements, dict) and
            set(measurements) == set(budgets),
            "acquisition enumeration differs from reviewed budgets")
    extras = {"cycles": {"counter_bits", "maximum_interval_cycles"},
              "stack_used_peak": {"stack_capacity_bytes"},
              "loss": {"expected_events"}}
    for name, conditions in measurements.items():
        require(name in METRICS, "unmaintained acquisition metric")
        fields(conditions, {"method", "instrument", "resolution",
                            "observation_window_s"} | extras.get(name, set()))
        require(conditions["method"] in METRICS[name][1],
                "unmaintained acquisition method")
        verify_file_identity(conditions["instrument"])
        _positive_number(conditions["resolution"], "capture resolution")
        _positive_number(conditions["observation_window_s"], "capture window")
        if name == "cycles":
            require(type(conditions["counter_bits"]) is int and
                    conditions["counter_bits"] == 32,
                    "Cortex-M DWT requires a 32-bit counter")
            maximum = conditions["maximum_interval_cycles"]
            require(type(maximum) is int and 0 < maximum < 2 ** 32,
                    "DWT interval must exclude ambiguous wrap")
        elif name in ("stack_used_peak", "loss"):
            key = ("stack_capacity_bytes" if name == "stack_used_peak"
                   else "expected_events")
            bound = conditions[key]
            require(type(bound) is int and 0 < bound <= MAX_OBSERVATION,
                    "positive bounded acquisition capacity required")
    return context


def _trace(identity: dict, admission_digest: str, context_identity: dict,
           context: dict) -> tuple[str, dict]:
    path = verify_file_identity(identity)
    trace = load_json(path, max_bytes=16 * 1024 * 1024)
    fields(trace, {"schema_version", "kind", "metric", "unit", "method",
                   "admission_sha256", "context_sha256", "samples"})
    require(type(trace["schema_version"]) is int and
            trace["schema_version"] == 1
            and trace["kind"] == "physical_hil_trace",
            "physical measurement trace required")
    name = trace["metric"]
    require(isinstance(name, str) and name in METRICS,
            "unmaintained measurement metric")
    unit, methods, integer_only = METRICS[name]
    require(trace["unit"] == unit and trace["method"] in methods,
            "measurement unit/method mismatch: " + name)
    require(trace["admission_sha256"] == admission_digest,
            "measurement trace is not bound to admission")
    require(trace["context_sha256"] == context_identity["sha256"],
            "measurement trace is not bound to acquisition context")
    require(name in context["measurements"] and trace["method"] ==
            context["measurements"][name]["method"],
            "trace acquisition method differs from bound context")
    conditions = context["measurements"][name]
    limits = {"cycles": "maximum_interval_cycles",
              "stack_used_peak": "stack_capacity_bytes",
              "loss": "expected_events"}
    upper = conditions.get(limits.get(name), MAX_OBSERVATION)
    samples = trace["samples"]
    require(isinstance(samples, list) and 0 < len(samples) <= MAX_SAMPLES,
            "measurement trace is empty or exceeds bounded sample count")
    values = []
    for sequence, sample in enumerate(samples):
        fields(sample, {"sequence", "value"})
        require(type(sample["sequence"]) is int and
                sample["sequence"] == sequence,
                "measurement samples missing, repeated or out of order")
        value = sample["value"]
        require(type(value) in (int, float) and
                0 <= value <= upper and math.isfinite(value)
                and (not integer_only or type(value) is int),
                "invalid measurement sample: " + name)
        values.append(value)
    # Mean is accumulated as scaled terms to avoid overflowing a finite sum.
    return name, {
        "unit": unit, "method": trace["method"], "samples": len(values),
        "minimum": min(values), "maximum": max(values),
        "mean": math.fsum(value / len(values) for value in values),
        "raw": identity,
    }


def validate_measurements(raw: dict, admission: dict, *,
                          maximum_age_s: int = 86400,
                          now: datetime | None = None) -> dict:
    """Derive statistics from retained traces and enforce reviewed budgets."""
    fields(raw, {"schema_version", "kind", "started_utc", "finished_utc",
                 "station_id", "board", "part", "chip_uid", "elf_sha256",
                 "resolved_sha256", "admission_sha256", "context", "traces"})
    require(type(raw["schema_version"]) is int and
            raw["schema_version"] == 1 and
            raw["kind"] == "physical_hil_measurements",
            "physical measurement record required")
    require(admission.get("kind") == "hil_admission" and
            admission.get("status") == "ready", "ready HIL admission required")
    paths = [verify_file_identity(admission[key]) for key in
             ("station", "fixture", "elf", "resolved", "resources")]
    rebuilt = admit(*paths)
    require(all(admission.get(key) == value for key, value in rebuilt.items()),
            "measurement admission is stale or inconsistent")
    _freshness(raw, maximum_age_s, now or datetime.now(timezone.utc))
    for key in ("station_id", "board", "part", "chip_uid"):
        require(raw[key] == admission[key],
                "measurement hardware identity mismatch")
    require(raw["elf_sha256"] == admission["elf"]["sha256"] and
            raw["resolved_sha256"] == admission["resolved"]["sha256"],
            "measurement firmware/configuration identity mismatch")
    admission_digest = canonical_digest(admission)
    require(raw["admission_sha256"] == admission_digest,
            "measurement record is not bound to admission")
    context = _context(raw["context"], admission_digest, admission["budgets"])
    traces = raw["traces"]
    require(isinstance(traces, list) and 0 < len(traces) <= MAX_TRACES,
            "measurement trace enumeration empty or excessive")
    metrics = {}
    for identity in traces:
        name, statistics = _trace(
            identity, admission_digest, raw["context"], context)
        require(name not in metrics, "duplicate measurement trace: " + name)
        metrics[name] = statistics
    require(set(metrics) == set(admission["budgets"]),
            "measurement enumeration differs from reviewed budgets")
    for name, statistics in metrics.items():
        budget = admission["budgets"][name]
        require(statistics["unit"] == budget["unit"] and
                statistics["maximum"] <= budget["maximum"] and
                statistics["minimum"] >= budget.get("minimum", 0),
                "physical measurement budget failed: " + name)
    return {
        "status": "passed",
        "physical_status": "qualified_for_recorded_measurements",
        "metrics": metrics, "context": raw["context"],
        "limits": ("Recorded samples do not prove WCET, unobserved load "
                   "or product release"),
    }


def _identity_paths(value) -> set[Path]:
    """Protect retained evidence and tool inputs, including nested paths."""
    paths = set()
    if isinstance(value, dict):
        if {"path", "sha256", "size"}.issubset(value):
            require(isinstance(value["path"], str),
                    "identity path must be a string")
            paths.add(Path(value["path"]).resolve())
        for child in value.values():
            paths.update(_identity_paths(child))
    elif isinstance(value, list):
        for child in value:
            paths.update(_identity_paths(child))
    return paths


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    plan = subparsers.add_parser("plan")
    plan.add_argument("--fixture", type=Path, required=True)
    plan.add_argument("--resolved", type=Path, required=True)
    validate = subparsers.add_parser("validate")
    validate.add_argument("--admission", type=Path, required=True)
    validate.add_argument("--raw-evidence", type=Path, required=True)
    for command in (plan, validate):
        command.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)
    inputs = [getattr(args, key) for key in
              (("fixture", "resolved") if args.command == "plan" else
               ("admission", "raw_evidence"))]
    if args.report.resolve() in {path.resolve() for path in inputs}:
        parser.exit(1, "measurement report cannot overwrite its inputs\n")
    try:
        if args.command == "plan":
            resolved, identity = resolved_identity(args.resolved)
            report = measurement_plan(load_json(args.fixture), resolved)
            report.update(fixture=file_identity(args.fixture),
                          resolved=identity)
        else:
            raw = load_json(args.raw_evidence)
            admission = load_json(args.admission)
            protected = _identity_paths(raw) | _identity_paths(admission)
            if args.report.resolve() in protected:
                parser.exit(1, "measurement report would overwrite inputs\n")
            # Inspect declared nested paths before hash verification. A failed
            # identity must not allow a failure report to replace raw evidence.
            if "context" in raw:
                protected.update(_identity_paths(load_json(
                    raw["context"]["path"])))
                if args.report.resolve() in protected:
                    parser.exit(
                        1, "measurement report would overwrite inputs\n")
            for key in ("station", "fixture", "resources"):
                if key in admission:
                    protected.update(_identity_paths(load_json(
                        admission[key]["path"])))
                    if args.report.resolve() in protected:
                        parser.exit(
                            1, "measurement report would overwrite inputs\n")
            if args.report.resolve() in protected:
                parser.exit(1, "measurement report would overwrite inputs\n")
            report = validate_measurements(raw, admission)
            report.update(schema_version=1, kind="hil_measurement_validation",
                          admission=file_identity(args.admission),
                          raw_evidence=file_identity(args.raw_evidence))
    except (EvidenceError, OSError, ValueError, KeyError, TypeError) as error:
        report = {"schema_version": 1, "kind": "hil_measurement_validation",
                  "status": "failed", "physical_status": "not_executed",
                  "failure": str(error)}
    atomic_json(args.report, report)
    print("HIL measurements: " + report["status"] + "; equipment not operated")
    return 0 if report["status"] in {"prepared_unbound", "passed"} else 1


if __name__ == "__main__":
    raise SystemExit(main())
