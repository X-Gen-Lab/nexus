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
MEASUREMENT_SCHEMA = 2


def measurement_plan(fixture: dict, resolved: dict) -> dict:
    """An unbound checklist cannot supply budgets or measured values."""
    validate_fixture(fixture)
    require(resolved["board"] == fixture["board"] and
            resolved["part"] == fixture["part"],
            "measurement fixture/build Board mismatch")
    return {
        "schema_version": 1, "kind": "hil_measurement_plan",
        "measurement_schema_version": MEASUREMENT_SCHEMA,
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
            "bind DWT endpoints/epoch and an independent admitted capture clock",
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


def _integer(value, minimum, maximum, label):
    require(type(value) is int and minimum <= value <= maximum,
            "invalid " + label)


def _reference_clock(reference, admission_digest, cycle_domain, cycle_epoch,
                     declared_cpu_hz):
    """Check an admitted external acquisition role, not a named MCU domain.

    These retained physical facts must come from the real station. Metadata
    consistency does not prove that an oscillator is physically independent.
    """
    fields(reference, {"domain", "epoch", "ticks_per_second", "method",
                       "instrument"})
    for key in ("domain", "epoch"):
        _integer(reference[key], 1, 2 ** 32 - 1, "reference " + key)
    _integer(reference["ticks_per_second"], 1, MAX_OBSERVATION,
             "reference rate")
    require(reference["method"] == "logic_analyzer",
            "DWT reference requires the external capture clock role")
    clock = load_json(verify_file_identity(reference["instrument"]))
    fields(clock, {"schema_version", "kind", "admission_sha256", "role",
                   "clock_source", "instrument_id", "domain", "epoch",
                   "ticks_per_second", "first_tick", "last_tick",
                   "uncertainty_ticks", "cycle_domain", "cycle_epoch",
                   "cpu_clock_lower_hz", "cpu_clock_upper_hz"})
    require(type(clock["schema_version"]) is int and
            clock["schema_version"] == 1 and
            clock["kind"] == "physical_hil_reference_clock" and
            clock["admission_sha256"] == admission_digest and
            clock["role"] == "reference_capture" and
            clock["clock_source"] == "external_instrument_oscillator",
            "unadmitted independent reference-clock acquisition facts")
    identifier(clock["instrument_id"], "reference instrument identity")
    for key, expected in (("cycle_domain", cycle_domain),
                          ("cycle_epoch", cycle_epoch)):
        _integer(clock[key], 1, 2 ** 32 - 1, "CPU frequency " + key)
        require(clock[key] == expected,
                "CPU frequency calibration belongs to another counter epoch")
    _integer(clock["cpu_clock_lower_hz"], 1, 2 ** 32 - 1,
             "CPU frequency lower bound")
    _integer(clock["cpu_clock_upper_hz"], clock["cpu_clock_lower_hz"],
             2 ** 32 - 1, "CPU frequency upper bound")
    require(clock["cpu_clock_lower_hz"] <= declared_cpu_hz <=
            clock["cpu_clock_upper_hz"],
            "declared CPU rate is outside the admitted frequency envelope")
    for key in ("domain", "epoch", "ticks_per_second"):
        require(type(clock[key]) is int and clock[key] == reference[key],
                "reference clock owner differs from acquisition context")
    for key in ("first_tick", "last_tick", "uncertainty_ticks"):
        _integer(clock[key], 0, MAX_OBSERVATION, "reference " + key)
    require(clock["first_tick"] < clock["last_tick"],
            "reference capture interval is empty or reversed")
    return clock


def _cycle_sample(sample, conditions, context):
    """Derive one sub-period cycle interval from actual paired endpoints."""
    fields(sample, {"sequence", "begin", "end"})
    reference = conditions["reference"]
    clock = conditions["_validated_reference_clock"]
    points = []
    for name in ("begin", "end"):
        point = sample[name]
        fields(point, {"counter", "cycle_domain", "cycle_epoch",
                       "cpu_clock_hz", "demcr", "dwt_control", "reference"})
        for key in ("counter", "demcr", "dwt_control"):
            _integer(point[key], 0, 2 ** 32 - 1, "DWT " + key)
        require(type(point["cycle_domain"]) is int and
                point["cycle_domain"] == conditions["cycle_domain"] and
                type(point["cycle_epoch"]) is int and
                point["cycle_epoch"] == conditions["cycle_epoch"] and
                type(point["cpu_clock_hz"]) is int and
                point["cpu_clock_hz"] == context["cpu_clock_hz"],
                "DWT counter identity or declared CPU rate changed")
        require(point["demcr"] & (1 << 24) and
                point["dwt_control"] & 1 and
                not point["dwt_control"] & (1 << 25),
                "DWT endpoint counter is unavailable")
        stamp = point["reference"]
        fields(stamp, {"ticks", "domain", "epoch", "ticks_per_second",
                       "uncertainty_ticks", "uncertainty_known"})
        for key in ("domain", "epoch", "ticks_per_second"):
            require(type(stamp[key]) is int and stamp[key] == reference[key],
                    "DWT reference epoch or rate changed")
        _integer(stamp["ticks"], clock["first_tick"], clock["last_tick"],
                 "reference capture ticks")
        _integer(stamp["uncertainty_ticks"], clock["uncertainty_ticks"],
                 MAX_OBSERVATION, "reference capture uncertainty")
        require(stamp["uncertainty_known"] is True,
                "unknown reference error cannot qualify a cycle interval")
        require(stamp["uncertainty_ticks"] <= stamp["ticks"] - clock["first_tick"]
                and stamp["uncertainty_ticks"] <=
                clock["last_tick"] - stamp["ticks"],
                "reference error interval leaves the admitted capture window")
        points.append((point, stamp))
    first, last = points[0][1], points[1][1]
    require(last["ticks"] >= first["ticks"], "reversed DWT reference interval")
    ticks = last["ticks"] - first["ticks"]
    error = first["uncertainty_ticks"] + last["uncertainty_ticks"]
    require(error <= MAX_OBSERVATION and ticks <= MAX_OBSERVATION - error,
            "DWT reference bound overflow")
    rate = reference["ticks_per_second"]
    # Integer bounds include one counter tick for sampling phase. Reference
    # uncertainty must already include capture pairing, skew and calibration.
    minimum_cycles = max(0, ticks - error) * clock["cpu_clock_lower_hz"] // rate
    maximum_cycles = ((ticks + error) * clock["cpu_clock_upper_hz"] + rate - 1) // rate
    lower, upper = max(0, minimum_cycles - 1), maximum_cycles + 1
    require(upper < 2 ** 32,
            "DWT reference interval does not exclude ambiguous wrap")
    value = (points[1][0]["counter"] - points[0][0]["counter"]) & 0xffffffff
    require(lower <= value <= upper,
            "DWT endpoints disagree with the admitted reference interval")
    return value


def _context(identity: dict, admission_digest: str, budgets: dict,
             admission: dict) -> dict:
    """Bind load and acquisition conditions separately from statistics."""
    context = load_json(verify_file_identity(identity))
    fields(context, {"schema_version", "kind", "admission_sha256", "workload",
                     "cpu_clock_hz", "load", "executor", "irq_priorities",
                     "measurements"})
    require(type(context["schema_version"]) is int and
            context["schema_version"] == MEASUREMENT_SCHEMA and
            context["kind"] == "hil_acquisition_context" and
            context["admission_sha256"] == admission_digest,
            "acquisition context is not bound to admission")
    from tools.hil.workloads import admitted_workload
    admitted_workload(context["workload"], admission)
    require(type(context["cpu_clock_hz"]) is int and
            0 < context["cpu_clock_hz"] <= 2 ** 32 - 1,
            "positive declared CPU clock rate required")
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
    extras = {"cycles": {"counter_bits", "cycle_domain", "cycle_epoch",
                          "reference"},
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
            for key in ("cycle_domain", "cycle_epoch"):
                _integer(conditions[key], 1, 2 ** 32 - 1, "DWT " + key)
            clock = _reference_clock(
                conditions["reference"], admission_digest,
                conditions["cycle_domain"], conditions["cycle_epoch"],
                context["cpu_clock_hz"])
            require(conditions["cycle_domain"] != clock["domain"],
                    "DWT counter cannot serve as its own independent reference")
            conditions["_validated_reference_clock"] = clock
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
            trace["schema_version"] == MEASUREMENT_SCHEMA
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
    limits = {"stack_used_peak": "stack_capacity_bytes",
              "loss": "expected_events"}
    upper = conditions.get(limits.get(name), MAX_OBSERVATION)
    samples = trace["samples"]
    require(isinstance(samples, list) and 0 < len(samples) <= MAX_SAMPLES,
            "measurement trace is empty or exceeds bounded sample count")
    values = []
    for sequence, sample in enumerate(samples):
        if name == "cycles":
            value = _cycle_sample(sample, conditions, context)
        else:
            fields(sample, {"sequence", "value"})
            value = sample["value"]
        require(type(sample["sequence"]) is int and
                sample["sequence"] == sequence,
                "measurement samples missing, repeated or out of order")
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
            raw["schema_version"] == MEASUREMENT_SCHEMA and
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
    context = _context(raw["context"], admission_digest, admission["budgets"],
                       admission)
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
            "limits": ("Recorded samples do not prove physical clock-owner "
                   "truth, independence, WCET, unobserved load or release"),
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
            report.update(schema_version=MEASUREMENT_SCHEMA,
                          kind="hil_measurement_validation",
                          admission=file_identity(args.admission),
                          raw_evidence=file_identity(args.raw_evidence))
    except (EvidenceError, OSError, ValueError, KeyError, TypeError) as error:
        report = {"schema_version": MEASUREMENT_SCHEMA,
                  "kind": "hil_measurement_validation",
                  "status": "failed", "physical_status": "not_executed",
                  "failure": str(error)}
    atomic_json(args.report, report)
    print("HIL measurements: " + report["status"] + "; equipment not operated")
    return 0 if report["status"] in {"prepared_unbound", "passed"} else 1


if __name__ == "__main__":
    raise SystemExit(main())
