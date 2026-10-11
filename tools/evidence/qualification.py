"""Create narrowly bound qualification records from actual command executions."""

from __future__ import annotations

from pathlib import Path

from tools.evidence.common import EvidenceError, command, digest, verify_file_identity
from tools.evidence.identity import resolved_identity


def create_qualification(scope: str, source: dict, resolved: Path, elf: Path,
                         checks: list[dict], *, evidence: list[dict] | None = None) -> dict:
    """No missing/failed/skipped or empty command collection becomes passed."""
    from tools.evidence.candidate import REQUIRED_SCOPES
    if scope not in REQUIRED_SCOPES or not checks:
        raise EvidenceError("nonempty supported qualification scope required")
    selected, _ = resolved_identity(resolved)
    executed = []
    names = set()
    for check in checks:
        name = check.get("name")
        if not isinstance(name, str) or not name or name in names:
            raise EvidenceError("executed qualification name missing/duplicated")
        names.add(name)
        if type(check.get("exit_code")) is not int or check["exit_code"] != 0:
            raise EvidenceError("failed/not executed qualification cannot pass")
        if check.get("status", "passed") != "passed" or check.get("failure"):
            raise EvidenceError("failed/skipped qualification cannot pass")
        verify_file_identity(check["raw"])
        record = {"name": name, "status": "passed", "exit_code": check["exit_code"],
                  "raw": check["raw"]}
        if "argv" in check:
            command(check["argv"])
            record["argv"] = check["argv"]
        if "artifacts" in check:
            if not isinstance(check["artifacts"], list):
                raise EvidenceError("execution artifacts must enumerate file identities")
            for identity in check["artifacts"]:
                verify_file_identity(identity)
            record["artifacts"] = check["artifacts"]
        executed.append(record)
    for identity in evidence or []:
        verify_file_identity(identity)
    return {"schema_version": 1, "kind": "software_qualification", "scope": scope,
            "status": "passed", "source": source,
            "configuration_sha256": selected["configuration_sha256"],
            "elf_sha256": digest(elf), "executed_checks": executed,
            "evidence": evidence or [], "physical_status": "not_executed"}
