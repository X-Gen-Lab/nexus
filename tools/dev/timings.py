"""Summarize existing execution receipts; never reuse qualification results."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path

from tools.evidence.common import (
    file_identity, load_json, regular_file, sha256_value, stream_digest)

MAXIMUM_RECEIPTS = 10000
OUTCOMES = {"completed", "timed_out", "cancelled", "launch_failed", "running"}


def _phase(command):
    """Conservative display grouping; original stage and argv remain present."""
    words = [str(item).replace("\\", "/") for item in command]
    names = {word.rsplit("/", 1)[-1] for word in words}
    if any("tools/documentation/" in word for word in words):
        return "documentation"
    if any("source_sdk" in word or word.endswith("test_source_sdk.py")
           for word in words):
        return "sdk"
    if "dependencies.py" in names or "install_dev_tools.py" in names:
        return "preparation"
    if names & {"ctest", "ctest.exe"} or "unittest" in words:
        return "test"
    if names & {"cmake", "cmake.exe"}:
        return "compile" if "--build" in words else "configure"
    return "other"


def _receipts(paths):
    def canonical(value):
        path = Path(value).absolute()
        if any(item.is_symlink() for item in (path, *path.parents)):
            raise ValueError("Receipt discovery does not follow symlinks")
        return path.resolve()

    candidates = set()
    for value in paths:
        path = canonical(value)
        if path.is_dir():
            for candidate in path.rglob("receipt.json"):
                candidates.add(canonical(candidate))
                if len(candidates) > MAXIMUM_RECEIPTS:
                    raise ValueError("Select at most 10000 attempt receipts")
        else:
            data = load_json(path)
            candidates.add(canonical(data.get("attempt_receipt", path)))
    if not candidates:
        raise ValueError("No actual attempt receipts were selected")
    if len(candidates) > MAXIMUM_RECEIPTS:
        raise ValueError("Too many attempt receipts")
    return sorted(candidates)


def _raw(identity):
    if (not isinstance(identity, dict)
            or set(identity) != {"path", "bytes", "sha256"}):
        raise ValueError("Receipt raw output identity is incomplete")
    size = identity["bytes"]
    if type(size) is not int or size < 0:
        raise ValueError("Receipt raw output size is invalid")
    sha256_value(identity["sha256"])
    path = regular_file(identity["path"], nonempty=False)
    if path.stat().st_size != size:
        raise ValueError("Receipt raw output identity changed")
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(128 * 1024), b""):
            digest.update(block)
    if digest.hexdigest() != identity["sha256"]:
        raise ValueError("Receipt raw output identity changed")


def _resource_usage(value):
    """Admit wait accounting without inventing a process-tree peak."""
    if value is None:
        return None
    fields = {"method", "scope", "user_cpu_seconds", "system_cpu_seconds",
              "maximum_process_rss_bytes"}
    if (not isinstance(value, dict) or set(value) != fields
            or value["method"] != "linux_wait4"
            or value["scope"] != "waited_process_and_reaped_descendants"):
        raise ValueError("Unknown resource observation scope or method")
    for name in ("user_cpu_seconds", "system_cpu_seconds"):
        if (type(value[name]) not in (float, int)
                or not math.isfinite(value[name]) or value[name] < 0):
            raise ValueError("Invalid observed CPU cost")
    if (type(value["maximum_process_rss_bytes"]) is not int
            or value["maximum_process_rss_bytes"] < 0):
        raise ValueError("Invalid observed maximum process RSS")
    return value


def summarize(paths):
    """Use monotonic cost and wall interval union, never summed wall."""
    attempts, phases, outcomes, intervals = [], {}, {}, []
    for path in _receipts(paths):
        data = load_json(path)
        if type(data.get("schema")) is not int or data["schema"] != 1:
            raise ValueError("Unsupported execution receipt schema")
        stage, command = data.get("stage"), data.get("command")
        if (not isinstance(stage, str) or not stage
                or not isinstance(command, list) or not command
                or any(not isinstance(word, str) for word in command)):
            raise ValueError("Receipt needs original stage and command")
        outcome = data.get("outcome")
        if outcome not in OUTCOMES:
            raise ValueError("Unknown execution receipt outcome")
        usage = _resource_usage(data.get("resource_usage"))
        if usage is not None and outcome != "completed":
            raise ValueError("Incomplete execution cannot claim accounting")
        start, finish = data.get("started_ns"), data.get("finished_ns")
        duration, code = data.get("duration_seconds"), data.get("exit_code")
        if type(start) is not int or start < 0:
            raise ValueError("Invalid execution receipt start")
        if outcome == "running":
            if any(value is not None for value in (finish, duration, code)):
                raise ValueError("Running receipt cannot contain final cost")
        else:
            if (type(finish) is not int or finish < start or
                    type(duration) not in (float, int) or
                    not math.isfinite(duration) or duration < 0 or
                    type(code) is not int):
                raise ValueError("Invalid completed execution receipt cost")
            expected = {"timed_out": 124, "cancelled": 130,
                        "launch_failed": 127}
            if outcome in expected and code != expected[outcome]:
                raise ValueError("Receipt outcome and exit code disagree")
            for key in ("stdout", "stderr"):
                _raw(data.get(key))
            intervals.append((start, finish))
        phase = _phase(command)
        entry = phases.setdefault(phase, {"attempts": 0, "seconds": 0.0,
                                          "failed": 0, "incomplete": 0})
        entry["attempts"] += 1
        entry["seconds"] += duration or 0
        entry["failed"] += int(code is not None and code != 0)
        entry["incomplete"] += int(outcome == "running")
        outcomes[outcome] = outcomes.get(outcome, 0) + 1
        attempts.append({"receipt": str(path), "stage": stage, "phase": phase,
                         "command": command, "outcome": outcome,
                         "exit_code": code,
                         "duration_seconds": duration,
                         "resource_usage": usage})
    merged = []
    for start, finish in sorted(intervals):
        if merged and start <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], finish)
        else:
            merged.append([start, finish])
    measured = [row["resource_usage"] for row in attempts
                if row["resource_usage"] is not None]
    return {
        "schema": 1, "kind": "development_receipt_costs",
        "qualification": False,
        "scope": ("selected host attempts only; "
                  "running costs and unobserved stages unknown"),
        "attempt_count": len(attempts), "outcomes": outcomes,
        "incomplete_attempts": outcomes.get("running", 0),
        "cumulative_attempt_seconds": sum(
            row["seconds"] for row in phases.values()),
        "observed_wall_seconds": sum(
            end - start for start, end in merged) / 1e9,
        "wall_clock_note": ("interval union excludes idle gaps; "
                            "assumes a shared stable host clock"),
        "cpu_seconds": None, "peak_rss_bytes": None, "cache_hits": None,
        "measured_resource_attempts": len(measured),
        "maximum_observed_process_rss_bytes": max(
            (row["maximum_process_rss_bytes"] for row in measured),
            default=None),
        "unobserved_metrics": ("CPU is per waited attempt; nested attempts can "
                               "overlap accounting. No aggregate CPU, parallel "
                               "tree peak RSS or cache counters are inferred. "
                               "Historical/interrupted costs stay unknown."),
        "phases": phases, "attempts": attempts,
    }


def _cost_input_identity(path):
    """Retain original empty streams as well as ordinary file identities."""
    path = regular_file(path, nonempty=False)
    if path.stat().st_size:
        return file_identity(path)
    with path.open("rb") as stream:
        return {"path": str(path), "size": path.stat().st_size,
                "sha256": stream_digest(stream)}


def compare_executions(plan_path, cold_report, warm_report, *, root,
                       cold_environment, warm_environment,
                       cold_cache_state, warm_cache_state):
    """Compare complete current-source plans; cache conditions are declarations.

    Environment reports bind observed sealed input bytes. They do not prove
    that a checker ran in an isolated container. This analysis neither reuses
    successful tests nor grants qualification to a report or changed source.
    Costs use each plan's original stage attempts. Native producer checks read
    current outputs, not independent historical inner artifacts for both runs.
    """
    from tools.dev import qualification

    for value in (cold_cache_state, warm_cache_state):
        if (not isinstance(value, str) or not value.strip() or
                len(value) > 1024 or "\0" in value):
            raise ValueError(
                "explicit bounded cache-state declarations required")
    cold_report = regular_file(cold_report)
    warm_report = regular_file(warm_report)
    if cold_report.samefile(warm_report):
        raise ValueError("independent cold and warm executions required")
    plan_identity = file_identity(plan_path)
    plan = load_json(plan_path)
    inputs = {plan_identity["path"]: plan_identity}
    runs, attempts = [], set()
    for report_path, environment_path, cache_state in (
            (cold_report, cold_environment, cold_cache_state),
            (warm_report, warm_environment, warm_cache_state)):
        report_identity = file_identity(report_path)
        report = load_json(report_path)
        if "environment_report" not in report:
            raise ValueError("comparison requires an environment report bound "
                             "by the original execution")
        environment = qualification.observe_environment_report(environment_path)
        if report["environment_report"] != environment:
            raise ValueError("environment report differs from the bound input")
        closure = qualification.execution_inputs(plan, report_path, root)
        if file_identity(report_path) != report_identity:
            raise ValueError("execution report changed during comparison")
        for path in closure:
            identity = _cost_input_identity(path)
            previous = inputs.setdefault(identity["path"], identity)
            if previous != identity:
                raise ValueError("comparison input changed between executions")
        selected = []
        for stage in report["stages"]:
            view = load_json(stage["command_receipt"]["path"])
            path = regular_file(view["attempt_receipt"])
            info = path.stat()
            key = (info.st_dev, info.st_ino)
            if key in attempts:
                raise ValueError(
                    "independent original attempt receipts required")
            attempts.add(key)
            selected.append(path)
        cost = summarize(selected)
        if (cost["incomplete_attempts"] or
                any(row["outcome"] != "completed" or row["exit_code"] != 0
                    for row in cost["attempts"])):
            raise ValueError("complete successful comparison attempts required")
        runs.append({"execution": file_identity(report_path),
                     "environment_report": environment,
                     "cache_state_declared": cache_state,
                     "source": report["source_before"], "cost": cost})
    cold, warm = runs
    if cold["source"] != warm["source"]:
        raise ValueError(
            "comparison requires identical clean source identities")
    if any(cold["environment_report"][key] != warm["environment_report"][key]
           for key in ("size", "sha256")):
        raise ValueError(
            "comparison requires identical environment input bytes")
    for identity in inputs.values():
        if _cost_input_identity(identity["path"]) != identity:
            raise ValueError("comparison input changed during analysis")
    for path in {Path(cold_environment), Path(warm_environment)}:
        identity = qualification.observe_environment_report(path)
        if identity != inputs[identity["path"]]:
            raise ValueError("environment input changed during analysis")
    if qualification.observe_git_source(Path(root)) != cold["source"]:
        raise ValueError("source changed during comparison")
    cold_seconds = cold["cost"]["observed_wall_seconds"]
    warm_seconds = warm["cost"]["observed_wall_seconds"]
    return {
        "schema": 1, "kind": "development_execution_cost_comparison",
        "qualification": False, "plan": plan_identity,
        "stage_count": len(plan["stages"]), "cold": cold, "warm": warm,
        "wall_seconds_saved": cold_seconds - warm_seconds,
        "wall_speedup": cold_seconds / warm_seconds if warm_seconds else None,
        "cpu_seconds_saved": None, "peak_rss_bytes_saved": None,
        "cache_hits": None,
        "scope": ("original stage-level costs of complete current-source plans; "
                  "current Native producer checks do not establish independent "
                  "historical inner artifacts for both runs; observed input "
                  "identity is not isolated-process environment proof; cache "
                  "state is declared and cache counters remain unknown; "
                  "wall interval unions exclude idle gaps and assume a shared "
                  "stable host clock; no aggregate nested CPU is inferred"),
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="*", type=Path)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--compare", action="store_true")
    parser.add_argument("--plan", type=Path)
    parser.add_argument("--cold-report", type=Path)
    parser.add_argument("--warm-report", type=Path)
    parser.add_argument("--cold-environment", type=Path)
    parser.add_argument("--warm-environment", type=Path)
    parser.add_argument("--cold-cache-state")
    parser.add_argument("--warm-cache-state")
    parser.add_argument("--root", type=Path)
    arguments = parser.parse_args(argv)
    comparisons = [arguments.plan, arguments.cold_report, arguments.warm_report,
                   arguments.cold_environment, arguments.warm_environment,
                   arguments.cold_cache_state, arguments.warm_cache_state,
                   arguments.root]
    if arguments.compare:
        if arguments.inputs or any(value is None for value in comparisons):
            parser.error("--compare requires plan, both reports/environments, "
                         "both declared cache states and root, without inputs")
        result = compare_executions(
            arguments.plan, arguments.cold_report, arguments.warm_report,
            root=arguments.root, cold_environment=arguments.cold_environment,
            warm_environment=arguments.warm_environment,
            cold_cache_state=arguments.cold_cache_state,
            warm_cache_state=arguments.warm_cache_state)
        print(f"Cold wall union: "
              f"{result['cold']['cost']['observed_wall_seconds']:.3f}s; "
              f"warm: {result['warm']['cost']['observed_wall_seconds']:.3f}s; "
              f"saved: {result['wall_seconds_saved']:.3f}s; development only")
        if arguments.json:
            with arguments.json.open("x", encoding="utf-8") as stream:
                json.dump(result, stream, indent=2)
                stream.write("\n")
        return 0
    if not arguments.inputs or any(value is not None for value in comparisons):
        parser.error("receipt summary requires inputs; comparison options "
                     "require --compare")
    result = summarize(arguments.inputs)
    print("Phase                 Attempts   Cumulative s  Failed  Incomplete")
    for phase, data in sorted(result["phases"].items()):
        print(f"{phase:21} {data['attempts']:8} {data['seconds']:14.3f}"
              f" {data['failed']:7} {data['incomplete']:11}")
    print(f"Observed wall union: {result['observed_wall_seconds']:.3f}s; "
          f"resource observations: {result['measured_resource_attempts']}; "
          "aggregate CPU/tree RSS/cache unknown; development only")
    if arguments.json:
        with arguments.json.open("x", encoding="utf-8") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
    return 0
