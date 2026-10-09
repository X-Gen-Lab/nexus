#!/usr/bin/env python3
"""Seal qualified software inputs and the same artifact bytes; no promotion/rebuild."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (EvidenceError, atomic_json, digest, fields,
    file_identity, identifier, load_json, stream_digest, verify_file_identity)
from tools.evidence.identity import resolved_identity, verify_source
from tools.evidence.container import verify as verify_environment
from tools.measurement.elf import Elf32, binary_from_elf
from tools.evidence.proof import validate_scope_proof, verify_sdk_file

REQUIRED_SCOPES = frozenset({"host_contracts", "arm_link", "resource_budget",
                           "source_sdk", "reproducibility", "hil_tooling"})


def collect_identities(value, *, identity_verifier=verify_file_identity) -> list[dict]:
    """Preserve every referenced raw/artifact byte, not only summary JSON."""
    found = []
    if isinstance(value, dict):
        if {"path", "sha256", "size"}.issubset(value):
            identity_verifier(value)
            found.append(value)
        else:
            for child in value.values():
                found.extend(collect_identities(child, identity_verifier=identity_verifier))
    elif isinstance(value, list):
        for child in value:
            found.extend(collect_identities(child, identity_verifier=identity_verifier))
    return found


def collect_evidence_closure(identities: list[dict], *, resolver=None) -> list[dict]:
    """Follow bounded JSON file references so raw nested reports remain reviewable."""
    pending = list(identities)
    result = []
    seen = set()
    while pending:
        identity = pending.pop()
        key = identity["sha256"]
        if key in seen:
            continue
        seen.add(key)
        path = (resolver(identity) if resolver is not None else
                verify_sdk_file(identity) if identity.get("role") == "sdk_file"
                else verify_file_identity(identity))
        result.append(identity)
        if identity.get("role") == "sdk_file" or path.stat().st_size > 4 * 1024 * 1024:
            continue
        if not path.read_bytes().lstrip().startswith(b"{"):
            continue
        try:
            value = load_json(path)
        except EvidenceError:
            if path.suffix == ".json":
                raise
            continue
        pending.extend(collect_identities(value, identity_verifier=resolver or verify_file_identity))
    return result


def validate_qualification(report: dict, manifest: dict, resolved: dict,
                           *, identity_verifier=verify_file_identity) -> None:
    fields(report, {"schema_version", "kind", "scope", "status", "source",
        "configuration_sha256", "elf_sha256", "executed_checks"},
        {"limitations", "evidence", "physical_status"})
    if (type(report["schema_version"]) is not int or report["schema_version"] != 1 or
            report["kind"] != "software_qualification"
            or report["scope"] not in REQUIRED_SCOPES or report["status"] != "passed"):
        raise EvidenceError("complete passing software qualification required")
    if (report["source"] != manifest["source"] or
            report["configuration_sha256"] != resolved["configuration_sha256"] or
            report["elf_sha256"] != manifest["artifacts"]["elf"]["sha256"]):
        raise EvidenceError("qualification source/configuration/ELF identity is stale")
    if not isinstance(report["executed_checks"], list) or not report["executed_checks"]:
        raise EvidenceError("empty software qualification rejected")
    names = set()
    for check in report["executed_checks"]:
        fields(check, {"name", "status", "exit_code", "raw"}, {"artifacts", "argv"})
        identifier(check["name"], "executed check")
        if (check["name"] in names or check["status"] != "passed" or
                type(check["exit_code"]) is not int or check["exit_code"] != 0):
            raise EvidenceError("duplicate/failed/skipped software check rejected")
        names.add(check["name"])
        if type(check["raw"].get("size")) is not int or check["raw"]["size"] <= 0:
            raise EvidenceError("empty execution raw evidence rejected")
        identity_verifier(check["raw"])
    def referenced(value):
        if isinstance(value, dict):
            if {"path", "sha256", "size"}.issubset(value):
                identity_verifier(value)
            else:
                for child in value.values():
                    referenced(child)
        elif isinstance(value, list):
            for child in value:
                referenced(child)
    referenced(report)


def validate_manifest(manifest: dict, *, sdk_packages_out=None) -> tuple[dict, list[dict]]:
    fields(manifest, {"schema_version", "candidate_id", "source", "dependencies",
        "environment", "configuration", "artifacts", "qualifications"})
    if type(manifest["schema_version"]) is not int or manifest["schema_version"] != 1:
        raise EvidenceError("unsupported software candidate schema")
    identifier(manifest["candidate_id"], "candidate id")
    verify_source(manifest["source"], clean=True)
    if not isinstance(manifest["dependencies"], list) or not manifest["dependencies"]:
        raise EvidenceError("complete dependency lock evidence required")
    for identity in manifest["dependencies"]:
        verify_file_identity(identity)
    config_path = verify_file_identity(manifest["configuration"])
    resolved, _ = resolved_identity(config_path)
    environment_path = verify_file_identity(manifest["environment"])
    verify_environment(environment_path)
    fields(manifest["artifacts"], {"elf", "bin", "map"})
    for identity in manifest["artifacts"].values():
        verify_file_identity(identity)
    elf = Elf32(Path(manifest["artifacts"]["elf"]["path"]).read_bytes())
    flash = resolved["flash"]
    if Path(manifest["artifacts"]["bin"]["path"]).read_bytes() != binary_from_elf(
            elf, flash["origin"], flash["origin"] + flash["size"]):
        raise EvidenceError("candidate BIN differs from linked ELF load bytes")
    reports = []
    scopes = set()
    for identity in manifest["qualifications"]:
        report_path = verify_file_identity(identity)
        report = load_json(report_path)
        validate_qualification(report, manifest, resolved)
        if report["scope"] in scopes:
            raise EvidenceError("duplicate qualification scope")
        scopes.add(report["scope"])
        reports.append(report)
    if scopes != REQUIRED_SCOPES:
        raise EvidenceError("incomplete required software qualification: " +
                            ", ".join(sorted(REQUIRED_SCOPES - scopes)))
    identities = collect_identities(manifest)
    for report in reports:
        identities.extend(collect_identities(report))
        extra, sdk = validate_scope_proof(report, manifest, resolved)
        identities.extend(extra)
        if sdk is not None and sdk_packages_out is not None:
            sdk_packages_out.append(sdk)
    return resolved, collect_evidence_closure(identities)


def seal(manifest_path: Path, output: Path) -> dict:
    manifest = load_json(manifest_path)
    sdk_packages = []
    resolved, identities = validate_manifest(manifest, sdk_packages_out=sdk_packages)
    identities.append(file_identity(manifest_path))
    if output.exists():
        raise EvidenceError("candidate destination must be new; never overwrite a seal")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".nexus-candidate-", dir=output.parent))
    try:
        payloads = temporary / "payloads"
        payloads.mkdir()
        records = {}
        for identity in identities:
            source = (verify_sdk_file(identity) if identity.get("role") == "sdk_file"
                      else verify_file_identity(identity))
            sha = identity["sha256"]
            if sha in records:
                continue
            target = payloads / sha
            shutil.copyfile(source, target)
            with target.open("rb") as stream:
                copied_sha = stream_digest(stream)
            if copied_sha != sha:
                raise EvidenceError("candidate copy differs from qualified artifact")
            records[sha] = {"path": "payloads/" + sha, "sha256": sha,
                            "size": identity["size"]}
        verify_source(manifest["source"], clean=True)
        sealed = {"schema_version": 1, "kind": "sealed_software_candidate",
            "candidate_id": manifest["candidate_id"], "status": "software_qualified",
            "source": manifest["source"], "configuration_sha256": resolved["configuration_sha256"],
            "input_manifest_sha256": digest(manifest_path),
            "artifacts": {role: records[identity["sha256"]] for role, identity in
                          manifest["artifacts"].items()},
            "payloads": list(records.values()), "sdk_packages": sdk_packages,
            "qualified_scopes": sorted(REQUIRED_SCOPES),
            "physical_status": "not_executed", "product_promotion": "not_performed",
            "signature_status": "unsigned",
            "limitations": ["software candidate only", "no physical, product, trust or LTS qualification"]}
        atomic_json(temporary / "candidate.json", sealed)
        temporary.replace(output)
        return sealed
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def verify(candidate_path: Path) -> dict:
    """Verify immutable packed bytes without silently rebuilding or editing them."""
    candidate = load_json(candidate_path, max_bytes=32 * 1024 * 1024)
    fields(candidate, {"schema_version", "kind", "candidate_id", "status", "source",
        "configuration_sha256", "input_manifest_sha256", "artifacts", "payloads",
        "qualified_scopes", "sdk_packages", "physical_status", "product_promotion", "signature_status",
        "limitations"})
    if (type(candidate["schema_version"]) is not int or candidate["schema_version"] != 1 or
            candidate["kind"] != "sealed_software_candidate"
            or candidate["status"] != "software_qualified" or
            set(candidate["qualified_scopes"]) != REQUIRED_SCOPES or
            candidate["physical_status"] != "not_executed"):
        raise EvidenceError("unsupported or incomplete software candidate")
    root = candidate_path.parent
    identities = {}
    for identity in candidate["payloads"]:
        fields(identity, {"path", "sha256", "size"})
        if identity["path"] != "payloads/" + identity["sha256"]:
            raise EvidenceError("noncanonical candidate payload path")
        path = root / identity["path"]
        if identity["size"] == 0:
            verify_sdk_file({**identity, "path": str(path), "role": "sdk_file"})
        else:
            verify_file_identity({**identity, "path": str(path)})
        if identity["sha256"] in identities:
            raise EvidenceError("duplicate candidate payload")
        identities[identity["sha256"]] = path
    manifest = load_json(identities[candidate["input_manifest_sha256"]])
    if manifest["source"] != candidate["source"] or manifest["source"]["dirty"]:
        raise EvidenceError("sealed candidate source mismatch or dirty source")
    def packed(identity):
        path = identities.get(identity["sha256"])
        if path is None or path.stat().st_size != identity["size"]:
            raise EvidenceError("missing referenced raw/artifact payload")
        return path
    for identity in manifest["dependencies"]:
        packed(identity)
    resolved, _ = resolved_identity(packed(manifest["configuration"]))
    if resolved["configuration_sha256"] != candidate["configuration_sha256"]:
        raise EvidenceError("sealed resolved configuration mismatch")
    packed(manifest["environment"])
    fields(candidate["artifacts"], {"elf", "bin", "map"})
    for role, identity in manifest["artifacts"].items():
        if identity["sha256"] != candidate["artifacts"][role]["sha256"]:
            raise EvidenceError("same-artifact seal changed")
        packed(identity)
    elf = Elf32(packed(manifest["artifacts"]["elf"]).read_bytes())
    flash = resolved["flash"]
    if packed(manifest["artifacts"]["bin"]).read_bytes() != binary_from_elf(
            elf, flash["origin"], flash["origin"] + flash["size"]):
        raise EvidenceError("sealed BIN differs from linked ELF")
    scopes = set()
    for identity in manifest["qualifications"]:
        report = load_json(packed(identity))
        validate_qualification(report, manifest, resolved, identity_verifier=packed)
        validate_scope_proof(report, manifest, resolved, resolver=packed,
                             sdk_packages=candidate["sdk_packages"])
        if report["scope"] in scopes:
            raise EvidenceError("duplicate sealed qualification")
        scopes.add(report["scope"])
    if scopes != REQUIRED_SCOPES:
        raise EvidenceError("sealed software qualification missing")
    collect_evidence_closure(collect_identities(manifest, identity_verifier=packed), resolver=packed)
    return candidate


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("seal", "verify"), nargs="?", default="seal")
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--candidate", type=Path)
    args = parser.parse_args()
    try:
        if args.operation == "seal":
            if args.manifest is None or args.output is None:
                parser.error("seal requires --manifest and --output")
            report = seal(args.manifest, args.output)
        else:
            if args.candidate is None:
                parser.error("verify requires --candidate")
            report = verify(args.candidate)
        print("Same-artifact software candidate: " + report["status"])
        return 0
    except (EvidenceError, OSError, ValueError, KeyError, TypeError) as error:
        print("Software candidate blocked: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
