#!/usr/bin/env python3
"""Prepare and validate OS hardware evidence without operating equipment."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (
    EvidenceError, atomic_json, fields, file_identity, identifier, load_json,
    verify_file_identity,
)
from tools.evidence.identity import canonical_digest, resolved_identity
from tools.hil.admit import admit, require
from tools.hil.measure import _freshness, _identity_paths

CASES = {
    "irq_wake": "OS49: capture IRQ latch and actual resumed-task timestamps",
    "context": "OS50: compare r4-r11 and enabled shared FP/MVE register bank",
    "stack": "OS51: observe every selected task stack and exception MSP peak",
    "deadline": "OS52: record per-call deadline and actual return timestamps",
    "lifecycle": "OS53: repeated admit/settle/join with no retained borrow",
    "low_power": "OS54: observe clock continuity and all latched wake events",
    "fail_stop": "OS55: record assert/stack-overflow triggers and safe outputs",
}
BUDGETS = {
    "irq_wake": ("maximum_us", "os_irq_latency", "maximum", "us"),
    "deadline": ("maximum_lateness_us", "os_deadline_lateness", "maximum", "us"),
    "low_power": ("maximum_lateness_us", "os_low_power_lateness", "maximum", "us"),
    "lifecycle": ("minimum_iterations", "os_lifecycle", "minimum", "count"),
    "fail_stop": ("maximum_response_us", "os_fail_response", "maximum", "us"),
}
MAX_COUNT = 100000
MAX_VALUE = 2 ** 64 - 1


def _integer(value, label, *, positive=False):
    require(type(value) is int and int(positive) <= value <= MAX_VALUE,
            "bounded integer " + label + " required")
    return value


def _enumeration(value, label):
    require(isinstance(value, list) and 0 < len(value) <= MAX_COUNT,
            "nonempty bounded " + label + " required")
    return value


def os_plan(resolved):
    """No reviewed station, measured values or hardware action is inferred."""
    require(isinstance(resolved, dict), "resolved OS build required")
    for key in ("board", "part"):
        identifier(resolved.get(key), key)
    require(resolved.get("backend") in ("native", "baremetal", "freertos"),
            "maintained OS backend required")
    return {"schema_version": 1, "kind": "os_hil_plan",
            "status": "prepared_unbound", "physical_status": "not_executed",
            "board": resolved["board"], "part": resolved["part"],
            "backend": resolved["backend"],
            "cases": [{"id": name, "definition": definition, "budget": None,
                       "status": "not_executed"}
                      for name, definition in CASES.items()],
            "prerequisites": [
                "review exact part/PCB, clock, IRQ priority and selected ABI",
                "retain admission, firmware, workload and instrument identities",
                "review all five OS station budgets before acquisition",
                "integer register bank always; FP/MVE bank only when selected",
                "record load/interference, nesting and actual observation window",
                "bind tickless timer port and continuous clock before low power",
                "product owns safe outputs and non-destructive failure fixture",
            ],
            "limits": "An unbound plan never qualifies hardware or a release"}


def _context(identity, admission):
    context = load_json(verify_file_identity(identity))
    fields(context, {"schema_version", "kind", "admission_sha256", "features",
                     "workload", "instrument", "load", "executor",
                     "irq_priorities", "cpu_clock_hz", "requirements", "stack_names"})
    require(type(context["schema_version"]) is int and
            context["schema_version"] == 1 and
            context["kind"] == "os_hil_context" and
            context["admission_sha256"] == canonical_digest(admission),
            "OS context must bind the exact admission")
    for key in ("workload", "instrument"):
        verify_file_identity(context[key])
    for key in ("load", "executor"):
        require(isinstance(context[key], str) and
                0 < len(context[key].strip()) <= 4096,
                "OS " + key + " description required")
    stacks = _enumeration(context["stack_names"], "workload stack inventory")
    require(all(isinstance(name, str) for name in stacks) and
            len(stacks) == len(set(stacks)) and "MSP" in stacks and len(stacks) >= 2,
            "unique MSP and all selected task stack identities required")
    for name in stacks:
        identifier(name, "workload stack")
    _integer(context["cpu_clock_hz"], "observed clock", positive=True)
    priorities = context["irq_priorities"]
    require(isinstance(priorities, dict) and 0 < len(priorities) <= 256,
            "observed OS IRQ priorities required")
    for name, value in priorities.items():
        identifier(name, "IRQ")
        require(type(value) is int and 0 <= value <= 255, "invalid IRQ priority")
    features = context["features"]
    fields(features, {"fpu", "mve"})
    require(all(type(value) is bool for value in features.values()),
            "explicit FP/MVE observations required")
    resolved = load_json(verify_file_identity(admission["resolved"]))
    cpu = resolved.get("cpu", {})
    require(cpu.get("arch") not in (None, "native") and
            features == {"fpu": cpu.get("fpu") != "none",
                         "mve": cpu.get("mve") != "none"},
            "OS observed register features differ from admitted CPU ABI")
    requirements = context["requirements"]
    fields(requirements, set(BUDGETS))
    for case, (key, metric, bound, unit) in BUDGETS.items():
        fields(requirements[case], {key})
        value = _integer(requirements[case][key], "reviewed " + key,
                         positive=case == "lifecycle")
        budget = admission.get("budgets", {}).get(metric)
        require(isinstance(budget, dict) and budget.get("unit") == unit and
                type(budget.get(bound)) is int and budget[bound] == value,
                "OS limit is not bound to reviewed station budget: " + metric)
    return context


def _timing(trace, case, context):
    fields(trace, {"samples"})
    samples = _enumeration(trace["samples"], case + " samples")
    limits = context["requirements"][case]
    delays = []
    for sequence, sample in enumerate(samples):
        keys = ({"sequence", "irq_us", "task_us"} if case == "irq_wake" else
                {"sequence", "deadline_us", "return_us"} if case == "deadline" else
                {"sequence", "before_us", "after_us", "wake_deadline_us",
                 "latched_events", "observed_events"})
        fields(sample, keys)
        require(type(sample["sequence"]) is int and
                sample["sequence"] == sequence, "OS trace samples out of order")
        for key, value in sample.items():
            _integer(value, key)
        if case == "irq_wake":
            require(sample["task_us"] >= sample["irq_us"], "negative IRQ delay")
            delay = sample["task_us"] - sample["irq_us"]
            maximum = limits["maximum_us"]
        elif case == "deadline":
            require(sample["return_us"] >= sample["deadline_us"],
                    "deadline sample must observe a timeout return")
            delay = sample["return_us"] - sample["deadline_us"]
            maximum = limits["maximum_lateness_us"]
        else:
            require(sample["after_us"] >= sample["before_us"] and
                    sample["wake_deadline_us"] >= sample["before_us"] and
                    sample["latched_events"] > 0 and
                    sample["latched_events"] == sample["observed_events"],
                    "low-power clock regressed or wake events were lost")
            delay = max(0, sample["after_us"] - sample["wake_deadline_us"])
            maximum = limits["maximum_lateness_us"]
        require(delay <= maximum, "OS timing budget failed: " + case)
        delays.append(delay)
    return {"samples": len(samples), "maximum_us": max(delays),
            "minimum_us": min(delays)}


def _registers(trace, context):
    fields(trace, {"registers"})
    rows = _enumeration(trace["registers"], "context registers")
    expected = {"r" + str(index) for index in range(4, 12)}
    if context["features"]["fpu"] or context["features"]["mve"]:
        expected |= {"s" + str(index) for index in range(32)}
    if context["features"]["mve"]:
        expected.add("vpr")
    seen = set()
    for row in rows:
        fields(row, {"name", "expected", "observed"})
        name = identifier(row["name"], "register")
        require(name in expected and name not in seen, "unreviewed/repeated register")
        seen.add(name)
        _integer(row["expected"], "expected register")
        _integer(row["observed"], "observed register")
        require(row["expected"] <= 2 ** 32 - 1 and
                row["expected"] == row["observed"], "context register corrupted")
    require(seen == expected, "context register bank incomplete")
    return {"registers": len(rows), "retained": True}


def _stacks(trace, context):
    fields(trace, {"stacks"})
    rows = _enumeration(trace["stacks"], "stack observations")
    names = set()
    for row in rows:
        fields(row, {"name", "capacity_bytes", "used_bytes"})
        name = identifier(row["name"], "stack")
        require(name not in names, "repeated stack")
        names.add(name)
        capacity = _integer(row["capacity_bytes"], "stack capacity", positive=True)
        used = _integer(row["used_bytes"], "stack peak", positive=True)
        require(used < capacity, "stack overflow/no measured headroom")
    require(names == set(context["stack_names"]),
            "stack observations must cover the complete declared workload")
    return {"stacks": len(rows), "observations": rows}


def _lifecycle(trace, context):
    fields(trace, {"iterations", "admitted", "settled", "joined", "retained",
                   "stale_accesses"})
    for key, value in trace.items():
        _integer(value, key)
    require(trace["iterations"] >=
            context["requirements"]["lifecycle"]["minimum_iterations"] and
            trace["admitted"] > 0 and trace["admitted"] == trace["settled"] and
            trace["joined"] >= trace["iterations"] and
            trace["retained"] == 0 and trace["stale_accesses"] == 0,
            "OS lifecycle retained borrows, missing joins or stale accesses")
    return dict(trace)


def _fail_stop(trace, context):
    fields(trace, {"scenarios"})
    rows = _enumeration(trace["scenarios"], "fail-stop scenarios")
    seen = set()
    for row in rows:
        fields(row, {"id", "trigger_us", "safe_us", "expected_output",
                     "observed_output"})
        require(row["id"] in ("assert", "stack_overflow") and
                row["id"] not in seen, "unreviewed/repeated fail-stop scenario")
        seen.add(row["id"])
        for key in ("trigger_us", "safe_us", "expected_output", "observed_output"):
            _integer(row[key], key)
        require(row["safe_us"] >= row["trigger_us"] and
                row["safe_us"] - row["trigger_us"] <=
                context["requirements"]["fail_stop"]["maximum_response_us"] and
                row["observed_output"] == row["expected_output"],
                "fail-stop response/output mismatch")
    require(seen == {"assert", "stack_overflow"}, "fail-stop scenarios incomplete")
    return {"scenarios": len(rows), "outputs_verified": True}


def validate_os_run(raw, admission, *, now=None, maximum_age_s=86400):
    """Validate retained observations; no synthetic result creates hardware truth."""
    fields(raw, {"schema_version", "kind", "started_utc", "finished_utc",
                 "station_id", "board", "part", "chip_uid", "elf_sha256",
                 "resolved_sha256", "admission_sha256", "context", "traces"})
    require(type(raw["schema_version"]) is int and raw["schema_version"] == 1 and
            raw["kind"] == "physical_os_hil_run", "physical OS run required")
    require(admission.get("kind") == "hil_admission" and
            admission.get("status") == "ready", "ready HIL admission required")
    inputs = [verify_file_identity(admission[key]) for key in
              ("station", "fixture", "elf", "resolved", "resources")]
    rebuilt = admit(*inputs)
    require(all(admission.get(key) == value for key, value in rebuilt.items()),
            "OS hardware admission is stale or inconsistent")
    _freshness(raw, maximum_age_s, now or datetime.now(timezone.utc))
    for key in ("station_id", "board", "part", "chip_uid"):
        require(raw[key] == admission[key], "OS hardware identity mismatch")
    require(raw["admission_sha256"] == canonical_digest(admission) and
            raw["elf_sha256"] == admission["elf"]["sha256"] and
            raw["resolved_sha256"] == admission["resolved"]["sha256"],
            "OS firmware/configuration/admission identity mismatch")
    context = _context(raw["context"], admission)
    cases = {}
    for identity in _enumeration(raw["traces"], "OS traces"):
        trace = load_json(verify_file_identity(identity))
        require(type(trace.get("schema_version")) is int and
                trace["schema_version"] == 1 and
                trace.get("kind") == "physical_os_trace" and
                trace.get("context_sha256") == raw["context"]["sha256"],
                "OS trace context identity mismatch")
        name = trace.get("case")
        require(isinstance(name, str) and name in CASES and name not in cases,
                "missing/repeated/unreviewed OS case")
        payload = {key: value for key, value in trace.items() if key not in
                   ("schema_version", "kind", "case", "context_sha256")}
        if name in ("irq_wake", "deadline", "low_power"):
            result = _timing(payload, name, context)
        elif name == "context":
            result = _registers(payload, context)
        elif name == "stack":
            result = _stacks(payload, context)
        elif name == "lifecycle":
            result = _lifecycle(payload, context)
        else:
            result = _fail_stop(payload, context)
        cases[name] = {**result, "raw": identity}
    require(set(cases) == set(CASES), "OS case enumeration incomplete")
    return {"status": "passed",
            "physical_status": "qualified_for_recorded_os_cases",
            "cases": cases, "context": raw["context"],
            "limits": "Recorded load/register patterns are not WCET or a release"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    plan = commands.add_parser("plan")
    plan.add_argument("--resolved", type=Path, required=True)
    validate = commands.add_parser("validate")
    validate.add_argument("--admission", type=Path, required=True)
    validate.add_argument("--raw-evidence", type=Path, required=True)
    for command in (plan, validate):
        command.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)
    if args.report.exists():
        parser.exit(1, "OS report requires an unused destination\n")
    inputs = ([args.resolved] if args.command == "plan" else
              [args.admission, args.raw_evidence])
    protected = {path.resolve() for path in inputs}
    if args.report.resolve() in protected:
        parser.exit(1, "OS report cannot overwrite retained inputs\n")
    try:
        if args.command == "plan":
            resolved, identity = resolved_identity(args.resolved)
            report = {**os_plan(resolved), "resolved": identity}
        else:
            admission, raw = (load_json(path) for path in inputs)
            protected |= _identity_paths(raw) | _identity_paths(admission)
            for identity in (raw.get("context"), admission.get("station"),
                             admission.get("resources")):
                if isinstance(identity, dict) and isinstance(identity.get("path"), str):
                    protected |= _identity_paths(load_json(identity["path"]))
            if args.report.resolve() in protected:
                parser.exit(1, "OS report cannot overwrite retained nested inputs\n")
            report = validate_os_run(raw, admission)
            report.update(schema_version=1, kind="os_hil_validation",
                          admission=file_identity(args.admission),
                          raw_evidence=file_identity(args.raw_evidence))
    except (EvidenceError, OSError, ValueError, KeyError, TypeError) as error:
        # Even failed identity verification cannot authorize overwriting its bytes.
        if args.report.resolve() in protected:
            parser.exit(1, "OS report cannot overwrite retained inputs\n")
        report = {"schema_version": 1, "kind": "os_hil_validation",
                  "status": "failed", "physical_status": "not_executed",
                  "failure": str(error)}
    atomic_json(args.report, report)
    print("OS HIL: " + report["status"] + "; equipment not operated")
    return 0 if report["status"] in ("prepared_unbound", "passed") else 1


if __name__ == "__main__":
    raise SystemExit(main())
