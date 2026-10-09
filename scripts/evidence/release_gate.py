#!/usr/bin/env python3
"""Verify the same inventory for candidate or enterprise promotion; never rebuild."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import math
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.evidence.common import (EvidenceError, atomic_json, digest, fields,
                                     file_identity, identifier, load_json, regular_file,
                                     run_adapter, sha256_value, verify_file_identity)
from scripts.evidence.build_inventory import verify as verify_inventory, license_expression
from scripts.hil.run_hil import validate_serial


def verify_signature(inventory_path: Path, public_key: Path, signature: Path,
                     expected_key_sha256: str, openssl: str = "openssl") -> dict:
    """Detached RSA/ECDSA SHA-256 verification through maintained OpenSSL CLI.

    Signing material is never an input. The public-key fingerprint must come
    from reviewed product policy, separately from the downloaded evidence pack.
    """
    regular_file(inventory_path)
    key = regular_file(public_key)
    signature = regular_file(signature)
    sha256_value(expected_key_sha256)
    if digest(key) != expected_key_sha256:
        raise EvidenceError("signer public key does not match product trust policy")
    executable = shutil.which(openssl)
    if not executable:
        raise EvidenceError("maintained OpenSSL verifier unavailable")
    # Reject a private key even if a caller accidentally passes one.
    if b"PRIVATE KEY" in key.read_bytes():
        raise EvidenceError("only public signing keys are accepted")
    run_adapter([executable, "pkey", "-pubin", "-in", str(key), "-pubcheck", "-noout"], 10)
    run_adapter([executable, "dgst", "-sha256", "-verify", str(key), "-signature",
                 str(signature), str(inventory_path.absolute())], 10)
    version = run_adapter([executable, "version"], 10).decode("utf-8").strip()
    return {"kind": "maintained_tool_signature_verification", "status": "pass",
            "signed_payload_sha256": digest(inventory_path), "public_key": file_identity(key),
            "signature": file_identity(signature), "verifier": {"name": "OpenSSL", "version": version}}


def verify_hil(report_path: Path, inventory: dict, requirements: dict) -> dict:
    report = load_json(report_path)
    if (report.get("schema_version") != 1 or report.get("kind") != "physical_hil" or
            report.get("eligible_physical_hil") is not True or report.get("status") != "pass"):
        raise EvidenceError("passing physical HIL evidence required; model evidence is ineligible")
    if report.get("board", {}).get("profile") != inventory["product"]["board_profile"]:
        raise EvidenceError("HIL board profile differs from build")
    if report.get("board", {}).get("revision") != inventory["product"]["board_revision"]:
        raise EvidenceError("HIL board revision differs from build")
    image = next((item for item in inventory["artifacts"] if item["role"] == "image"), None)
    if image is None or image["sha256"] != report.get("firmware_sha256"):
        raise EvidenceError("HIL did not execute this firmware image")
    if report.get("config_sha256") != inventory["configuration"]["effective"]["sha256"]:
        raise EvidenceError("HIL configuration differs from build")
    operations = report.get("operations", [])
    if operations != [{"name": name, "status": "pass"} for name in
                      ("identify", "flash", "verify_flash", "reset", "serial", "cleanup")]:
        raise EvidenceError("complete flash/readback/reset/test/cleanup evidence required")
    transcript = report.get("transcript", {})
    fields(transcript, {"path", "sha256"})
    if digest(transcript["path"]) != transcript["sha256"]:
        raise EvidenceError("HIL transcript digest mismatch")
    serial = load_json(transcript["path"])
    # Reviewed release policy supplies the tests and budgets; a report cannot
    # weaken its own requirements to gain promotion.
    manifest = {"board": report["board"], "firmware": {"sha256": image["sha256"]},
                "config_sha256": report["config_sha256"],
                "required_tests": requirements["required_tests"], "budgets": requirements["budgets"]}
    validate_serial(serial, manifest)
    if serial["tests"] != report.get("tests") or serial["metrics"] != report.get("metrics"):
        raise EvidenceError("HIL summary and transcript differ")
    return file_identity(report_path)


def validate_policy(policy: dict) -> None:
    fields(policy, {"schema_version", "product_id", "board_profile", "board_revision",
                    "trusted_public_key_sha256", "required_tests", "budgets",
                    "required_reviews", "runtime_components"})
    if policy["schema_version"] != 1:
        raise EvidenceError("unsupported release policy schema")
    for name in ("product_id", "board_profile", "board_revision"):
        identifier(policy[name], name)
    sha256_value(policy["trusted_public_key_sha256"])
    for name in ("required_tests", "required_reviews"):
        value = policy[name]
        if not isinstance(value, list) or not value or len(value) != len(set(value)):
            raise EvidenceError(f"nonempty unique {name} required")
        for item in value:
            identifier(item, name)
    if not isinstance(policy["budgets"], dict) or not policy["budgets"]:
        raise EvidenceError("reviewed hardware resource/timing budgets required")
    for metric, budget in policy["budgets"].items():
        identifier(metric, "metric")
        fields(budget, {"maximum", "unit"})
        identifier(budget["unit"], "metric unit")
        if type(budget["maximum"]) not in (int, float) or not math.isfinite(budget["maximum"]) or budget["maximum"] < 0:
            raise EvidenceError("invalid reviewed hardware budget")
    if not isinstance(policy["runtime_components"], list):
        raise EvidenceError("reviewed toolchain/dynamic runtime inventory required")
    for component in policy["runtime_components"]:
        fields(component, {"name", "version", "license", "evidence"})
        for name in ("name", "version"):
            identifier(component[name], name)
        license_expression(component["license"])
        verify_file_identity(component["evidence"])


def assemble(inventory_path: Path, policy_path: Path, hil_path: Path,
             reviews_path: Path, output_path: Path) -> dict:
    """Create the exact public evidence payload for an external signer."""
    payload = {"schema_version": 1, "kind": "release_evidence_signature_payload",
               "inventory_sha256": digest(inventory_path), "policy_sha256": digest(policy_path),
               "physical_hil_sha256": digest(hil_path), "reviews_sha256": digest(reviews_path)}
    atomic_json(output_path, payload)
    return payload


def promote(inventory_path: Path, *, level: str, policy_path: Path | None = None,
            hil_path: Path | None = None, reviews_path: Path | None = None,
            public_key: Path | None = None, signature: Path | None = None,
            signed_evidence: Path | None = None, openssl: str = "openssl") -> dict:
    inventory = load_json(inventory_path)
    verify_inventory(inventory)
    report = {"schema_version": 1, "kind": "release_promotion_gate", "level": level,
              "status": "pass", "inventory": file_identity(inventory_path),
              "source_commit": inventory["source"]["commit"],
              "verified_utc": datetime.now(timezone.utc).isoformat()}
    if inventory["source"]["dirty"] or any(item["source"]["dirty"] for item in inventory["components"]):
        raise EvidenceError("dirty source or linked dependency blocks promotion")
    if level == "candidate":
        report["limitations"] = ["candidate draft only", "physical HIL and enterprise signatures not asserted"]
        return report
    if level != "enterprise":
        raise EvidenceError("unsupported promotion level")
    if None in (policy_path, hil_path, reviews_path, public_key, signature, signed_evidence):
        raise EvidenceError("enterprise promotion requires policy, physical HIL, reviews and signature")
    policy = load_json(policy_path)
    validate_policy(policy)
    for actual, expected in ((inventory["product"]["id"], policy["product_id"]),
                             (inventory["product"]["board_profile"], policy["board_profile"]),
                             (inventory["product"]["board_revision"], policy["board_revision"])):
        if actual != expected:
            raise EvidenceError("release policy and build product identity differ")
    coverage = inventory["dependency_coverage"]
    runtime = {component["name"] for component in policy["runtime_components"]}
    unresolved = {Path(path).name for path in coverage["unresolved_archives"]}
    if not unresolved.issubset(runtime) or not runtime:
        raise EvidenceError("system/toolchain/dynamic dependency review incomplete")
    reviews = load_json(reviews_path)
    fields(reviews, {"schema_version", "inventory_sha256", "reviews"})
    if reviews["schema_version"] != 1 or reviews["inventory_sha256"] != digest(inventory_path):
        raise EvidenceError("reviews are not bound to this inventory")
    if not isinstance(reviews["reviews"], list):
        raise EvidenceError("review evidence list required")
    accepted = set()
    for review in reviews["reviews"]:
        fields(review, {"id", "status", "owner_role", "evidence"})
        identifier(review["id"], "review id")
        identifier(review["owner_role"], "owner role")
        if review["id"] in accepted or review["status"] != "pass":
            raise EvidenceError("duplicate, failed or pending release review")
        verify_file_identity(review["evidence"])
        accepted.add(review["id"])
    if not set(policy["required_reviews"]).issubset(accepted):
        raise EvidenceError("required security/license/fault-recovery review evidence missing")
    report["policy"] = file_identity(policy_path)
    report["physical_hil"] = verify_hil(hil_path, inventory, policy)
    report["reviews"] = file_identity(reviews_path)
    payload = load_json(signed_evidence)
    expected_payload = {"schema_version": 1, "kind": "release_evidence_signature_payload",
                        "inventory_sha256": digest(inventory_path), "policy_sha256": digest(policy_path),
                        "physical_hil_sha256": digest(hil_path), "reviews_sha256": digest(reviews_path)}
    if payload != expected_payload:
        raise EvidenceError("signed evidence does not bind this inventory/policy/HIL/review set")
    report["signed_evidence"] = file_identity(signed_evidence)
    report["signature_verification"] = verify_signature(signed_evidence, public_key, signature,
                                                         policy["trusted_public_key_sha256"], openssl)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--level", choices=("candidate", "enterprise"))
    parser.add_argument("--report", type=Path)
    parser.add_argument("--assemble", type=Path, help="write public payload for an external signer")
    for name in ("policy", "hil", "reviews", "public-key", "signature", "signed-evidence"):
        parser.add_argument("--" + name, type=Path)
    parser.add_argument("--openssl", default="openssl")
    args = parser.parse_args()
    if args.assemble:
        if None in (args.policy, args.hil, args.reviews):
            parser.error("--assemble requires --policy --hil --reviews")
        try:
            assemble(args.inventory, args.policy, args.hil, args.reviews, args.assemble)
            print(f"public evidence payload prepared: {args.assemble}; signing remains external")
            return 0
        except (EvidenceError, OSError) as error:
            print(f"evidence assembly rejected: {error}", file=sys.stderr)
            return 1
    if args.level is None or args.report is None:
        parser.error("promotion requires --level and --report")
    try:
        report = promote(args.inventory, level=args.level, policy_path=args.policy,
                         hil_path=args.hil, reviews_path=args.reviews,
                         public_key=args.public_key, signature=args.signature,
                         signed_evidence=args.signed_evidence, openssl=args.openssl)
    except (EvidenceError, OSError, KeyError, TypeError) as error:
        report = {"schema_version": 1, "kind": "release_promotion_gate", "level": args.level,
                  "status": "blocked", "failure": str(error)}
    atomic_json(args.report, report)
    print(f"{args.level} promotion: {report['status']}; report={args.report}")
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
