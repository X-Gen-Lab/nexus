"""Capture actual top-level build execution and separately label planned inputs."""

from __future__ import annotations

from pathlib import Path
import re

from tools.evidence.common import (EvidenceError, atomic_json, file_identity,
                                   load_json, regular_file, stream_digest, command, fields,
                                   verify_file_identity)
from tools.evidence.identity import observe_git_source, verify_sdk_package


def capture(root: Path, build: Path, command_receipt: Path, *, action: str,
            expected_command=None, expected_cwd=None) -> dict:
    """Compiler databases describe plans, never prove compilation took place."""
    if action not in ("configure", "build"):
        raise EvidenceError("build provenance action must be configure or build")
    execution = load_json(command_receipt)
    if (expected_command is not None
            and execution.get("command") != list(expected_command)):
        raise EvidenceError("execution receipt belongs to a different command")
    if (expected_cwd is not None
            and execution.get("cwd") != str(Path(expected_cwd).resolve())):
        raise EvidenceError("execution receipt belongs to a different "
                            "working directory")
    if (execution.get("schema") != 1 or execution.get("exit_code") != 0 or
            execution.get("outcome") != "completed"):
        raise EvidenceError("only an actual completed command can produce build provenance")
    raw = []
    for role in ("stdout", "stderr"):
        identity = execution[role]
        path = regular_file(identity["path"], nonempty=False)
        with path.open("rb") as stream:
            sha256 = stream_digest(stream)
        actual = {"path": str(path), "bytes": path.stat().st_size,
                  "sha256": sha256}
        if identity != actual:
            raise EvidenceError("actual command output changed before provenance capture")
        raw.append(actual)
    inputs = [command_receipt]
    if (root / ".nexus-source-sdk.json").exists():
        package = verify_sdk_package(root)
        source = {"kind": "source_sdk", "source_revision": package["source_revision"],
                  "source_tree": package["source_tree"],
                  "manifest": file_identity(root / ".nexus-source-sdk.json")}
    else:
        source = observe_git_source(root)
    cache = regular_file(build / "CMakeCache.txt")
    inputs.append(cache)
    for match in re.finditer(r"^CMAKE_(?:C|CXX|ASM)_COMPILER:[^=]+=(.+)$",
                             cache.read_text(), re.MULTILINE):
        compiler = Path(match[1]).resolve()
        if compiler not in inputs:
            inputs.append(regular_file(compiler))
    inputs.extend(sorted((root / "dependencies").glob("*.lock.json")))
    compile_commands = build / "compile_commands.json"
    if compile_commands.is_file():
        inputs.append(compile_commands)
    generated = []
    for directory in (build / "generated", build / "nexus/generated",
                      build / "nexus-runtime/generated", build / "nexus-sdk/generated"):
        for name in ("resolved.json", "generator-identity.json", "nexus_config.h",
                     "nexus_factory.h", "bindings.c", "memory.ld"):
            path = directory / name
            if path.is_file():
                generated.append(path)
    inputs.extend(generated)
    artifacts = [path for path in sorted((build / "bin").glob("*"))
                 if path.is_file() and not path.is_symlink() and path.stat().st_size
                 and (path.suffix in (".elf", ".bin", ".map") or
                      path.name.endswith(".resources.json") or
                      path.name in ("nexus_firmware", "nexus_firmware.exe"))]
    # Configuration has no output ELF yet. The cache itself is its real output.
    if not artifacts:
        artifacts = [cache]
    resolved = next((path for path in generated if path.name == "resolved.json"), None)
    environment_lock = root / "dependencies/environment.lock.json"
    report = provenance(source, inputs, artifacts, [execution["command"]],
                        configuration=resolved,
                        environment=environment_lock if environment_lock.is_file() else None)
    report["actual_execution"] = {"receipt": file_identity(command_receipt),
                                  "stdout_stderr": raw,
                                  "duration_seconds": execution["duration_seconds"]}
    report["planned_compile_commands"] = (file_identity(compile_commands)
                                          if compile_commands.is_file() else None)
    report["scope"] = "actual top-level command and observed bytes; compile database is a plan, not per-object execution"
    report["action"] = action
    destination = build / "quality" / (action + "-provenance.json")
    atomic_json(destination, report)
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
