#!/usr/bin/env python3
"""Execute explicit qualification plans through maintained fixed checkers."""

from __future__ import annotations

import argparse
import hashlib
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.dev.process import run as run_command
from tools.evidence.common import (EvidenceError, atomic_json, command, fields,
    file_identity, load_json, parse_json, regular_file, sha256_value,
    stream_digest, verify_file_identity)
from tools.evidence.identity import canonical_digest, observe_git_source

CHECKERS = {
    "tool_tests": ("tools/testing/run_tool_tests.py",),
    "configure": ("tools/dev/dev.py", "configure"),
    "build": ("tools/dev/dev.py", "build"),
    "native_contracts": ("scripts/ci/native_contracts.py",),
    "arch_compile": ("scripts/ci/arch_compile.py",),
    "cortex_runtime": ("scripts/ci/cortex_runtime.py",),
    "cache_control_compile": ("scripts/ci/cache_control_compile.py",),
    "os_profiles": ("scripts/ci/os_profiles.py",),
    "os_threadx": ("scripts/ci/os_threadx.py",),
    "stm32_memory": ("scripts/ci/stm32_memory.py",),
    "gd32_memory": ("scripts/ci/gd32_memory.py",),
    "stm32h743": ("scripts/ci/stm32h743.py",),
    "stm32u575": ("scripts/ci/stm32u575.py",),
    "u575_trustzone": ("scripts/ci/u575_trustzone.py",),
    "provider_assemblies": ("scripts/ci/provider_assemblies.py",),
    "source_sdk": ("cmake/package/package_source_sdk.py",),
    "quality_tools": ("scripts/ci/quality_tools.py",),
    "hil_prepare": ("tools/hil/prepare.py",),
    "tdd": ("scripts/ci/tdd_gate.py", "--all"),
    "sdk_consumers": ("cmake/package/verify_consumers.py",),
    "sdk_boundary_tests": ("tests/contracts/test_source_sdk.py",),
    "build_artifact_tests": ("tests/contracts/test_build_artifacts.py",),
    "resource_budget": ("tools/measurement/resources.py",),
    "retention": ("tools/measurement/retention.py",),
    "compatibility": ("tools/maintenance/compatibility.py",),
    "dependency_review": ("tools/maintenance/dependency_proposals.py",),
    "dependency_upgrade": ("tools/dev/dependencies.py", "upgrade-verify"),
    "reproduce": ("tools/evidence/reproduce.py",),
    "candidate_seal": ("tools/evidence/candidate.py", "seal"),
    "candidate_verify": ("tools/evidence/candidate.py", "verify"),
    "environment_verify": ("tools/evidence/container.py", "verify"),
}


def validate_plan(plan: dict, *, require_candidate: bool = False) -> dict:
    fields(plan, {"schema_version", "stages"}, {"formal"})
    if type(plan["schema_version"]) is not int or plan["schema_version"] != 1:
        raise EvidenceError("unsupported qualification plan")
    if "formal" in plan and type(plan["formal"]) is not bool:
        raise EvidenceError("formal plan flag must be boolean")
    if not isinstance(plan["stages"], list) or not plan["stages"]:
        raise EvidenceError("nonempty qualification stages required")
    names = set()
    from tools.evidence.common import identifier
    for stage in plan["stages"]:
        fields(stage, {"name", "checker", "args"}, {"timeout_s"})
        name = identifier(stage["name"], "stage name")
        if name in names or stage["checker"] not in CHECKERS:
            raise EvidenceError("duplicate stage or unsupported fixed checker")
        names.add(name)
        if not isinstance(stage["args"], list):
            raise EvidenceError("checker arguments must be an explicit list")
        command(["checker", *stage["args"]],
                {"root": "/source", "output": "/qualification"})
        timeout = stage.get("timeout_s", 1800)
        if isinstance(timeout, bool) or not isinstance(timeout, (int, float)) or not 0 < timeout <= 86400:
            raise EvidenceError("qualification deadline must be finite and bounded")
    if require_candidate and plan["stages"][-1]["checker"] != "candidate_verify":
        raise EvidenceError("candidate plan must finish with the owned candidate verifier")
    return plan


def preflight_quality_tools(plan: dict, root: Path, output: Path,
                            environment: dict) -> None:
    """Reject missing selected analyzers before executing any plan stage.

    This only checks executable availability. The original checker still owns
    its complete arguments, tool version, analysis scope and actual execution.
    """
    selector = argparse.ArgumentParser(add_help=False, allow_abbrev=False,
                                       exit_on_error=False)
    selector.add_argument("--tool")
    selector.add_argument("--root", type=Path, default=root)
    for stage in plan["stages"]:
        if stage["checker"] != "quality_tools":
            continue
        arguments = command(["selector", *stage["args"]],
                            {"root": str(root), "output": str(output)})[1:]
        try:
            selected, _ = selector.parse_known_args(arguments)
        except argparse.ArgumentError as error:
            raise EvidenceError(
                f"quality tool selection invalid for {stage['name']}: "
                f"{error}") from error
        if not selected.tool:
            raise EvidenceError(
                f"quality tool selection missing for {stage['name']}")
        working = (root / selected.root).resolve()
        search = os.pathsep.join(
            str(working / entry)
            for entry in environment.get("PATH", os.defpath).split(os.pathsep))
        executable = selected.tool
        if os.path.dirname(executable):
            executable = str(working / executable)
        if shutil.which(executable, path=search) is None:
            raise EvidenceError(
                f"quality tool unavailable for {stage['name']}: "
                f"{selected.tool}")


def execute(plan: dict, root: Path, output: Path, *, require_candidate: bool = False) -> dict:
    validate_plan(plan, require_candidate=require_candidate)
    root, output = root.absolute(), output.absolute()
    if output.exists() or any(path.is_symlink() for path in [output, *output.parents]):
        raise EvidenceError("qualification output must be a new nonsymlink directory")
    environment = os.environ.copy()
    if plan.get("formal"):
        environment["CCACHE_DISABLE"] = "1"
        for key in ("CMAKE_C_COMPILER_LAUNCHER", "CMAKE_CXX_COMPILER_LAUNCHER"):
            environment.pop(key, None)
    preflight_quality_tools(plan, root, output, environment)
    source = observe_git_source(root)
    if plan.get("formal") and source["dirty"]:
        raise EvidenceError("formal qualification requires clean Git source")
    output.mkdir(parents=True)
    stages = []
    report = {"schema_version": 1, "kind": "qualification_plan_execution",
              "status": "running", "source_before": source,
              "plan_sha256": canonical_digest(plan),
              "formal": plan.get("formal", False), "stages": stages,
              "physical_status": "not_executed", "software_qualified": False}
    atomic_json(output / "report.json", report)
    for stage in plan["stages"]:
        checker, *prefix = CHECKERS[stage["checker"]]
        script = root / checker
        if not script.is_file() or script.is_symlink():
            report.update(status="failed", failure="maintained checker unavailable")
            break
        argv = command([sys.executable, "-B", str(script), *prefix, *stage["args"]],
                       {"root": str(root), "output": str(output)})
        raw = output / (stage["name"] + ".raw")
        started = time.monotonic()
        result = run_command(argv, cwd=root, env=environment, log=raw,
                             timeout=stage.get("timeout_s", 1800), stage=stage["checker"])
        # Preserve even empty actual output; the command receipt proves execution.
        with raw.open("rb") as stream:
            raw_sha256 = stream_digest(stream)
        raw_identity = {"path": str(raw), "size": raw.stat().st_size,
                        "sha256": raw_sha256}
        execution = {"name": stage["name"], "checker": stage["checker"],
                     "argv": argv, "checker_source": file_identity(script),
                     "exit_code": result.returncode,
                     "duration_s": time.monotonic() - started,
                     "status": "passed" if result.returncode == 0 else "failed",
                     "raw": raw_identity}
        command_receipt = raw.with_suffix(".command.json")
        if command_receipt.is_file():
            execution["command_receipt"] = file_identity(command_receipt)
        stages.append(execution)
        atomic_json(output / "report.json", report)
        if result.returncode:
            report.update(status="failed", failure="actual checker returned nonzero")
            break
    report["source_after"] = observe_git_source(root)
    if report["source_after"] != source:
        report.update(status="failed", failure="source identity changed during execution")
    elif len(stages) == len(plan["stages"]) and all(stage["exit_code"] == 0 for stage in stages):
        report["status"] = "executed_passed"
    elif report["status"] == "running":
        report["status"] = "failed"
    # This is orchestration evidence, never an alternative candidate validator.
    atomic_json(output / "report.json", report)
    return report


def verify_execution(plan: dict, report_path: Path, root: Path) -> dict:
    """Verify a complete original execution for the current clean source.

    This admits an existing maintained plan's commands, not a new execution or
    software candidate. Partial plans and interrupted reports are not merged.
    Original attempt receipts and even empty actual output remain mandatory.
    """
    from tools.dev import timings

    validate_plan(plan)
    if plan.get("formal") is not True:
        raise EvidenceError("local software gate requires a formal plan")
    root = Path(root).resolve()
    report_path = regular_file(report_path)
    output = report_path.parent
    observed = {}

    def observe_file(identity, *, empty=False):
        fields(identity, {"path", "size", "sha256"})
        sha256_value(identity["sha256"])
        if type(identity["size"]) is not int or identity["size"] < int(not empty):
            raise EvidenceError("invalid execution evidence byte count")
        path = regular_file(identity["path"], nonempty=not empty)
        with path.open("rb") as stream:
            digest = stream_digest(stream)
        if path.stat().st_size != identity["size"] or digest != identity["sha256"]:
            raise EvidenceError("execution evidence changed")
        previous = observed.setdefault(str(path), identity)
        if previous != identity:
            raise EvidenceError("execution evidence has conflicting identities")
        return path

    def read_json(identity):
        path = observe_file(identity)
        with path.open("rb") as stream:
            contents = stream.read(4 * 1024 * 1024 + 1)
        if (len(contents) > 4 * 1024 * 1024 or
                len(contents) != identity["size"] or
                hashlib.sha256(contents).hexdigest() != identity["sha256"]):
            raise EvidenceError("execution JSON changed or exceeds its bound")
        return parse_json(contents)

    report_identity = file_identity(report_path)
    report = read_json(report_identity)
    fields(report, {"schema_version", "kind", "status", "source_before",
                    "source_after", "plan_sha256", "formal", "stages",
                    "physical_status", "software_qualified"})
    source = observe_git_source(root)
    if (source["dirty"] is not False or
            any(not line.startswith(" ") for line in source["submodules"]) or
            report["source_before"] != source or report["source_after"] != source):
        raise EvidenceError("local software gate requires the same clean source")
    if (type(report["schema_version"]) is not int or report["schema_version"] != 1 or
            report["kind"] != "qualification_plan_execution" or
            report["status"] != "executed_passed" or report["formal"] is not True or
            report["plan_sha256"] != canonical_digest(plan) or
            report["physical_status"] != "not_executed" or
            report["software_qualified"] is not False or
            not isinstance(report["stages"], list) or
            len(report["stages"]) != len(plan["stages"])):
        raise EvidenceError("complete matching formal execution required")
    for stage, actual in zip(plan["stages"], report["stages"]):
        fields(actual, {"name", "checker", "argv", "checker_source", "exit_code",
                        "duration_s", "status", "raw", "command_receipt"})
        script, *prefix = CHECKERS[stage["checker"]]
        expected_argv = command(
            [sys.executable, "-B", str(root / script), *prefix, *stage["args"]],
            {"root": str(root), "output": str(output)})
        if (actual["name"] != stage["name"] or
                actual["checker"] != stage["checker"] or
                actual["argv"] != expected_argv or actual["status"] != "passed" or
                type(actual["exit_code"]) is not int or actual["exit_code"] != 0 or
                type(actual["duration_s"]) not in (int, float) or
                not math.isfinite(actual["duration_s"]) or actual["duration_s"] < 0 or
                actual["checker_source"] != file_identity(root / script)):
            raise EvidenceError("stage differs from the executed owned checker")
        observe_file(actual["checker_source"])
        raw = output / (stage["name"] + ".raw")
        if (actual["raw"]["path"] != str(raw) or
                actual["command_receipt"]["path"] !=
                str(raw.with_suffix(".command.json"))):
            raise EvidenceError("stage evidence navigation differs from execution")
        observe_file(actual["raw"], empty=True)
        receipt = read_json(actual["command_receipt"])
        attempt_path = Path(receipt.get("attempt_receipt", ""))
        attempt_root = output / (raw.stem + ".attempts")
        if (attempt_path.name != "receipt.json" or
                attempt_path.parent.parent != attempt_root):
            raise EvidenceError("original unique attempt receipt required")
        attempt = read_json(file_identity(attempt_path))
        if attempt != {key: value for key, value in receipt.items()
                       if key != "attempt_receipt"}:
            raise EvidenceError("command view differs from its original attempt")
        if (receipt.get("command") != expected_argv or
                receipt.get("cwd") != str(root) or
                receipt.get("stage") != stage["checker"] or
                receipt.get("outcome") != "completed" or
                type(receipt.get("exit_code")) is not int or
                receipt["exit_code"] != 0 or
                type(receipt.get("child_returncode")) is not int or
                receipt["child_returncode"] != 0 or
                type(receipt.get("timeout_seconds")) not in (int, float) or
                receipt.get("timeout_seconds") != stage.get("timeout_s", 1800) or
                receipt.get("withdrawal_complete") is not True or
                receipt.get("withdrawal_stable") is not True or
                receipt.get("withdrawn_processes") != []):
            raise EvidenceError("completed owned checker receipt required")
        for name in ("stdout", "stderr"):
            identity = receipt.get(name, {})
            fields(identity, {"path", "bytes", "sha256"})
            if identity["path"] != str(attempt_path.parent / (name + ".txt")):
                raise EvidenceError("attempt output navigation differs")
            observe_file({"path": identity["path"], "size": identity["bytes"],
                          "sha256": identity["sha256"]}, empty=True)
        # The runner's conventional log is the decoded stdout followed by
        # stderr; a newly hashed replacement summary is not that execution.
        actual_output = "".join(Path(receipt[name]["path"]).read_text(
            errors="replace") for name in ("stdout", "stderr"))
        if raw.read_text() != actual_output:
            raise EvidenceError("execution log differs from actual output")
        # Reuse the receipt reader's timestamp, cost and raw-output contract.
        timings.summarize([attempt_path])
    for identity in tuple(observed.values()):
        observe_file(identity, empty=identity["size"] == 0)
    if observe_git_source(root) != source:
        raise EvidenceError("source changed during execution verification")
    return {"schema_version": 1, "kind": "verified_qualification_plan_execution",
            "status": "verified", "source": source, "report": report_identity,
            "plan_sha256": canonical_digest(plan),
            "stage_count": len(plan["stages"]), "software_qualified": False,
            "physical_status": "not_executed"}


def provenance(source: dict, inputs: list[Path], artifacts: list[Path],
               commands: list[list[str]], *, configuration: Path | None = None,
               environment: Path | None = None) -> dict:
    if not commands or not inputs or not artifacts:
        raise EvidenceError("build provenance requires actual inputs/artifacts/commands")
    for argv in commands:
        command(argv)
    record = {"schema_version": 1, "kind": "build_provenance", "source": source,
              "inputs": [file_identity(path) for path in inputs],
              "artifacts": [file_identity(path) for path in artifacts],
              "commands": commands, "configuration": file_identity(configuration) if configuration else None,
              "environment": file_identity(environment) if environment else None,
              "status": "identities_recorded", "software_qualified": False}
    verify_provenance(record)
    return record


def verify_provenance(record: dict) -> dict:
    fields(record, {"schema_version", "kind", "source", "inputs", "artifacts",
                   "commands", "configuration", "environment", "status", "software_qualified"},
                   {"actual_execution", "planned_compile_commands", "scope", "action"})
    if (type(record["schema_version"]) is not int or record["schema_version"] != 1 or
            record["kind"] != "build_provenance" or
            record["status"] != "identities_recorded" or
            record["software_qualified"] is not False):
        raise EvidenceError("unsupported build provenance")
    for role in ("inputs", "artifacts"):
        if not isinstance(record[role], list) or not record[role]:
            raise EvidenceError("nonempty provenance closure required")
        for identity in record[role]:
            verify_file_identity(identity)
    for role in ("configuration", "environment"):
        if record[role] is not None:
            verify_file_identity(record[role])
    if not isinstance(record["commands"], list) or not record["commands"]:
        raise EvidenceError("provenance requires nonempty command observations")
    for argv in record["commands"]:
        command(argv)
    execution = record.get("actual_execution")
    if execution is not None:
        fields(execution, {"receipt", "stdout_stderr", "duration_seconds"})
        receipt_file = verify_file_identity(execution["receipt"])
        actual = load_json(receipt_file)
        if (actual.get("schema") != 1 or actual.get("exit_code") != 0 or
                actual.get("outcome") != "completed" or
                actual.get("command") not in record["commands"] or
                actual.get("duration_seconds") != execution["duration_seconds"]):
            raise EvidenceError("provenance execution does not match actual command receipt")
        if execution["stdout_stderr"] != [actual.get("stdout"), actual.get("stderr")]:
            raise EvidenceError("provenance raw navigation differs from actual receipt")
        for identity in execution["stdout_stderr"]:
            fields(identity, {"path", "bytes", "sha256"})
            file = regular_file(identity["path"], nonempty=False)
            with file.open("rb") as stream:
                sha256 = stream_digest(stream)
            if (file.stat().st_size != identity["bytes"] or
                    sha256 != identity["sha256"]):
                raise EvidenceError("actual provenance output changed")
    if record.get("planned_compile_commands") is not None:
        verify_file_identity(record["planned_compile_commands"])
    return {"status": "identities_verified", "software_qualified": False}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--require-candidate", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = execute(load_json(args.plan), args.root, args.output,
                         require_candidate=args.require_candidate)
        print("Qualification plan: " + result["status"])
        return 0 if result["status"] == "executed_passed" else 1
    except (EvidenceError, OSError, ValueError, subprocess.SubprocessError) as error:
        print("Qualification plan rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
