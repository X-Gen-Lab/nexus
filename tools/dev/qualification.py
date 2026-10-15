#!/usr/bin/env python3
"""Execute explicit qualification plans through maintained fixed checkers."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.dev.process import run as run_command
from tools.evidence.common import (EvidenceError, atomic_json, command, fields,
    file_identity, load_json, regular_file, stream_digest, verify_file_identity)
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


def execute(plan: dict, root: Path, output: Path, *, require_candidate: bool = False) -> dict:
    validate_plan(plan, require_candidate=require_candidate)
    root, output = root.absolute(), output.absolute()
    if output.exists() or any(path.is_symlink() for path in [output, *output.parents]):
        raise EvidenceError("qualification output must be a new nonsymlink directory")
    source = observe_git_source(root)
    if plan.get("formal") and source["dirty"]:
        raise EvidenceError("formal qualification requires clean Git source")
    output.mkdir(parents=True)
    stages = []
    environment = os.environ.copy()
    if plan.get("formal"):
        environment["CCACHE_DISABLE"] = "1"
        for key in ("CMAKE_C_COMPILER_LAUNCHER", "CMAKE_CXX_COMPILER_LAUNCHER"):
            environment.pop(key, None)
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
