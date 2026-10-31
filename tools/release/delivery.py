#!/usr/bin/env python3
"""Stage verified source bytes and publish an explicitly tagged software preview."""

from __future__ import annotations

import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from urllib.error import HTTPError
from urllib.parse import quote, urlsplit
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.dont_write_bytecode = True
INVENTORY_FILE_LIMIT = 100000
CI_EVIDENCE_FILE_LIMIT = 200000

from tools.configure.metadata import load_policy


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def write(path, value):
    Path(path).write_text(json.dumps(value, sort_keys=True, indent=2) + "\n",
                          encoding="utf-8")


def digest(path):
    checksum = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def commit(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{40}", value):
        raise ValueError("An exact 40-character commit is required")
    return value


def inventory(path, *, file_limit=None):
    path = Path(path)
    file_limit = INVENTORY_FILE_LIMIT if file_limit is None else file_limit
    if type(file_limit) is not int or file_limit <= 0:
        raise ValueError("Release inventory requires a positive file limit")
    if any(item.is_symlink() for item in (path, *path.parents)):
        raise ValueError("Release paths must not traverse symlinks")
    result = {}
    for item in sorted(path.rglob("*")):
        if item.is_symlink() or not (item.is_dir() or item.is_file()):
            raise ValueError("Release payload contains a nonregular path")
        if item.is_file():
            result[item.relative_to(path).as_posix()] = digest(item)
    if not result:
        raise ValueError(f"Release payload inventory is empty: path={path} count=0 limit={file_limit}")
    if len(result) > file_limit:
        raise ValueError(f"Release payload inventory exceeds its file limit: path={path} "
                         f"count={len(result)} limit={file_limit}")
    return result


def verify_sdk(source, prefix, expected_commit):
    subprocess.run([sys.executable,
        str(Path(source) / "cmake/package/package_source_sdk.py"), "--verify",
        str(Path(prefix) / "share/nexus/src")], check=True, timeout=300)
    identity = read(Path(prefix) / "share/nexus/src/.nexus-source-sdk.json")
    if (identity.get("source_revision") != commit(expected_commit) or
            identity.get("publishable") is not True or identity.get("source_dirty")):
        raise ValueError("SDK is not clean publishable source for the expected commit")
    return identity


def consumers(path, expected_commit):
    report = read(Path(path) / "verification.json")
    if (report.get("kind") != "source_sdk_consumer_verification" or
            report.get("status") != "passed" or
            report.get("source_revision") != expected_commit or
            report.get("physical_status") != "not_executed"):
        raise ValueError("Strict source SDK consumers have not passed for this commit")
    return report


def stage(source, prefix, consumer_output, output, expected_commit):
    source, prefix = Path(source).resolve(), Path(prefix).resolve()
    consumer_output, output = Path(consumer_output).resolve(), Path(output).resolve()
    if (prefix.is_relative_to(source) or consumer_output.is_relative_to(source) or
            not output.is_relative_to(source / "build") or output.exists()):
        raise ValueError("Prepare outside the checkout and use fresh owned build staging")
    verify_sdk(source, prefix, expected_commit)
    consumers(consumer_output, expected_commit)
    observed = {"source-sdk": inventory(prefix),
                "sdk-consumers": inventory(consumer_output)}
    output.mkdir(parents=True)
    for name, original in (("source-sdk", prefix),
                           ("sdk-consumers", consumer_output)):
        shutil.copytree(original, output / name)
        if inventory(original) != observed[name] or inventory(output / name) != observed[name]:
            raise ValueError("SDK staging copy differs from verified bytes")
    verify_sdk(source, output / "source-sdk", expected_commit)
    consumers(output / "sdk-consumers", expected_commit)
    write(output / "staging-copy.json", {"status": "verified_copy",
          "source_commit": expected_commit, "files_sha256": observed})


def require_ci(run, expected_commit, now=None):
    now = now or datetime.now(timezone.utc)
    if (not isinstance(run, dict) or run.get("path") != ".github/workflows/ci.yml" or
            run.get("event") not in {"workflow_dispatch", "push"} or
            run.get("head_sha") != commit(expected_commit) or
            run.get("status") != "completed" or
            run.get("conclusion") != "success" or
            run.get("head_branch") != "main"):
        raise ValueError("A successful full CI run on main for this exact commit is required")
    created = datetime.fromisoformat(run["created_at"].replace("Z", "+00:00"))
    if not timedelta(0) <= now - created <= timedelta(hours=24):
        raise ValueError("Full CI must be fresh (created within the last 24 hours)")
    if type(run.get("id")) is not int or type(run.get("run_attempt")) is not int:
        raise ValueError("Actual CI run and attempt identity are required")
    return run



def require_jobs(document):
    if not isinstance(document, dict):
        raise ValueError("Actual CI jobs are required")
    jobs = document.get("jobs", [])
    if (not jobs or document.get("total_count") != len(jobs) or
            any(job.get("status") != "completed" or
                job.get("conclusion") != "success" for job in jobs)):
        raise ValueError("Every required CI job must complete successfully without skips")
    names = {job["name"].split(" / ")[-1] for job in jobs}
    required = {"Detect Changes", "CI Status", "Build Docs",
                "Required commit style checks", "Required static analysis",
                "Configuration, evidence and SDK failure boundaries",
                "Cortex-M compiler and runtime contracts",
                "ThreadX kernel and lifecycle software contracts",
                "Optional MPU and TrustZone software contracts"}
    if not required <= names:
        raise ValueError("CI is missing a required qualification domain")
    matrices = {
        "Native contracts": ("native-debug", "native-release", "native-asan", "native-tsan"),
        "ARM link and budgets": (
            "stm32-qiming-baremetal", "stm32-qiming-freertos",
            "stm32-sky-baremetal", "stm32-sky-freertos",
            "gd32-liangshan-baremetal", "gd32-liangshan-freertos"),
        "Provider assembly audits": (
            "stm32_assembly", "gd32_assembly", "can_assembly", "usb_assembly",
            "ethernet_assembly", "timer_assembly", "g474_assembly", "h743_assembly",
            "u575_assembly", "multi_controller_assembly", "mainstream_assembly",
            "pin_state_fixture")}
    for label, selections in matrices.items():
        expected = {f"{label} ({selection})" for selection in selections}
        observed = {name for name in names if name.startswith(label + " (")}
        if observed != expected:
            raise ValueError("CI required matrix identities differ: " + label)
    return document


def verify_identity(path, artifact_root, expected_commit, job, run_id, attempt):
    from tools.evidence.identity import canonical_digest
    record = read(path)
    checksum = record.pop("record_sha256", None)
    if checksum != canonical_digest(record):
        raise ValueError("Job identity record digest mismatch")
    source = record.get("source", {})
    run = record.get("run", {})
    if (record.get("kind") != "ci_job_identity" or record.get("job") != job or
            record.get("job_result") != "success" or source.get("dirty") is not False or
            source.get("commit") != expected_commit or
            run.get("GITHUB_RUN_ID") != str(run_id) or
            run.get("GITHUB_RUN_ATTEMPT") != str(attempt) or
            record.get("missing_artifacts") or not record.get("artifacts")):
        raise ValueError("Successful exact-run source identity is required")
    for item in record["artifacts"]:
        name = item["relative_path"]
        relative = Path(name)
        if (relative.is_absolute() or ".." in relative.parts or
                relative.parts[0] != "build"):
            raise ValueError("Job artifact path escapes expected build ownership")
        observed = Path(artifact_root).joinpath(*relative.parts[1:])
        if (observed.is_symlink() or not observed.is_file() or
                observed.stat().st_size != item["size"] or digest(observed) != item["sha256"]):
            raise ValueError("Job artifact bytes changed: " + name)
    return record



def archive_evidence(inputs, destination, *, include_builder_ci=False):
    """Retain raw consumer outputs and qualification files with original paths."""
    inputs = Path(inputs)
    selections = [inputs / name for name in (
        "sdk/delivery/sdk-consumers", "sdk/delivery/staging-copy.json",
        "sdk/job-identities", "docs", "attestation", "ci-evidence",
        "ci-run.json", "ci-jobs.json")]
    if include_builder_ci:
        selections.extend(inputs / name for name in
                          ("builder-ci-run.json", "builder-ci-jobs.json"))
    expected = {}
    for selection in selections:
        limit = CI_EVIDENCE_FILE_LIMIT if selection == inputs / "ci-evidence" else INVENTORY_FILE_LIMIT
        paths = inventory(selection, file_limit=limit) if selection.is_dir() else {
            selection.name: digest(selection)}
        for name, checksum in paths.items():
            path = selection / name if selection.is_dir() else selection
            expected[path.relative_to(inputs).as_posix()] = checksum
    with tarfile.open(destination, "w:gz", dereference=True) as stream:
        for selection in selections:
            stream.add(selection, arcname=selection.relative_to(inputs).as_posix())
    observed = {}
    with tarfile.open(destination) as stream:
        for member in stream:
            if member.isdir():
                continue
            if not member.isfile() or member.name not in expected:
                raise ValueError("Verification evidence archive has an unexpected entry")
            checksum = hashlib.sha256()
            with stream.extractfile(member) as contents:
                for chunk in iter(lambda: contents.read(1024 * 1024), b""):
                    checksum.update(chunk)
            observed[member.name] = checksum.hexdigest()
    if observed != expected:
        raise ValueError("Verification evidence archive changed raw qualification bytes")


def bundle(source, inputs, output, expected_commit, ci_run_id, release_run_id, attempt,
           *, recover_tagged_source=False, builder_commit=None, builder_ci_run_id=None):
    from tools.documentation import delivery
    source, inputs, output = Path(source), Path(inputs), Path(output)
    version = load_policy(source)["semantic_version"]
    if output.exists():
        raise ValueError("Release asset staging must be fresh")
    run = require_ci(read(inputs / "ci-run.json"), expected_commit)
    require_jobs(read(inputs / "ci-jobs.json"))
    if run["id"] != ci_run_id:
        raise ValueError("CI input belongs to a different run")
    builder_run = None
    if recover_tagged_source:
        commit(builder_commit)
        if type(builder_ci_run_id) is not int or builder_ci_run_id <= 0:
            raise ValueError("Tagged-source recovery requires an explicit builder CI run")
        builder_run = require_ci(read(inputs / "builder-ci-run.json"), builder_commit)
        require_jobs(read(inputs / "builder-ci-jobs.json"))
        if builder_run["id"] != builder_ci_run_id:
            raise ValueError("Builder CI input belongs to a different run")
    elif builder_commit is not None or builder_ci_run_id is not None:
        raise ValueError("Separate builder inputs require explicit tagged-source recovery")
    sdk, docs = inputs / "sdk", inputs / "docs"
    prefix, checks = sdk / "delivery/source-sdk", sdk / "delivery/sdk-consumers"
    verify_sdk(source, prefix, expected_commit)
    if load_policy(prefix / "share/nexus/src")["semantic_version"] != version:
        raise ValueError("SDK version differs from this release source policy")
    consumers(checks, expected_commit)
    sdk_identity = sdk / "job-identities/source-sdk/identity.json"
    docs_identity = docs / "docs-job-identity/identity.json"
    verify_identity(sdk_identity, sdk, expected_commit, "source-sdk", release_run_id, attempt)
    verify_identity(docs_identity, docs, expected_commit, "documentation", ci_run_id,
                    run["run_attempt"])
    archives = list((docs / "documentation-package").glob("*.tar.gz"))
    if len(archives) != 1:
        raise ValueError("Exactly one immutable documentation archive is required")
    documentation = delivery.verify(archives[0])
    if (documentation["documentation_source"]["commit"] != expected_commit or
            documentation["code_source"]["commit"] != expected_commit or
            documentation["version"] != version):
        raise ValueError("Documentation code/version identity differs from this release")
    doc_payload = docs / "documentation-package" / documentation["release_id"]
    manifest = doc_payload / delivery.MANIFEST
    delivery.verify(doc_payload, expected_manifest_sha256=digest(manifest))
    attestation = inputs / "attestation/delivery/attestation/bundle.json"
    if not isinstance(read(attestation), dict):
        raise ValueError("Actual source SDK attestation bundle is required")
    output.mkdir(parents=True)
    sdk_root = "nexus-source-sdk-" + version
    raw_archive = "nexus-verification-evidence-" + version + ".tar.gz"
    archive = output / (sdk_root + ".tar.gz")
    expected_files = inventory(prefix)
    with tarfile.open(archive, "w:gz") as stream:
        stream.add(prefix, arcname=sdk_root)
    with tempfile.TemporaryDirectory(prefix="nexus-release-archive-") as directory:
        with tarfile.open(archive) as stream:
            for member in stream.getmembers():
                path = Path(member.name)
                if (path.is_absolute() or ".." in path.parts or
                        not (member.isdir() or member.isfile())):
                    raise ValueError("SDK release archive contains an unsafe entry")
            stream.extractall(directory, filter="data")
        restored = Path(directory) / sdk_root
        if inventory(restored) != expected_files:
            raise ValueError("SDK archive differs from verified installed bytes")
        verify_sdk(source, restored, expected_commit)
    copies = {"source-sdk-manifest.json": prefix / "share/nexus/src/.nexus-source-sdk.json",
              "source-sdk-consumers.json": checks / "verification.json",
              "source-sdk-identity.json": sdk_identity,
              "source-sdk-attestation.json": attestation,
              "documentation-manifest.json": manifest,
              "documentation-identity.json": docs_identity,
              "ci-run.json": inputs / "ci-run.json",
              "ci-jobs.json": inputs / "ci-jobs.json",
              archives[0].name: archives[0]}
    for name, original in copies.items():
        if (output / name).exists():
            raise ValueError("Release asset names collide")
        shutil.copyfile(original, output / name)
        if digest(original) != digest(output / name):
            raise ValueError("Release asset copy changed")
    archive_evidence(inputs, output / raw_archive, include_builder_ci=recover_tagged_source)
    assets = [{"name": path.name, "size": path.stat().st_size, "sha256": digest(path)}
              for path in sorted(output.iterdir())]
    release_manifest = {"schema_version": 1,
          "kind": "nexus_software_preview", "tag": "v" + version,
          "source_commit": expected_commit,
          "ci_run_id": ci_run_id, "ci_run_attempt": run["run_attempt"],
          "source_sdk_run_id": release_run_id, "assets": assets,
          "physical_hil": "not_executed", "software_candidate": False,
          "production_qualification": False,
          "evidence_layout": {"sdk_archive_root": sdk_root,
                              "sdk_restore_path": "sdk/delivery/source-sdk",
                              "raw_records": raw_archive}}
    if builder_run is not None:
        release_manifest.update(builder_ci_run_id=builder_ci_run_id,
                                builder_ci_run_attempt=builder_run["run_attempt"])
    write(output / "release-manifest.json", release_manifest)


LOCAL_MANIFEST = "local-release.json"
LOCAL_POLICY = "nexus-local-software-preview-v1"
LOCAL_UNASSESSED_EXITS = ["codeql", "pages_deployment", "server_branch_rules",
                          "fresh_recipe_builds", "fresh_daemon", "physical_hil"]


def _local_file(path):
    """Keep even empty original output; it is not a missing artifact."""
    from tools.evidence.common import regular_file
    path = regular_file(path, nonempty=False)
    return {"path": str(path), "size": path.stat().st_size,
            "sha256": digest(path), "mode": path.stat().st_mode & 0o777}


def _local_context(source, execution, expected_commit):
    from tools.dev import qualification
    from tools.evidence.common import EvidenceError, file_identity, load_json, parse_json
    from tools.evidence.identity import canonical_digest, git_source
    from tools.release import transport
    source = Path(source).resolve()
    observed = git_source(source)
    if observed["commit"] != commit(expected_commit) or observed["dirty"]:
        raise EvidenceError("Local admission requires the same clean source commit")
    plan_path = source / "tools/dev/plans/software.json"
    plan = load_json(plan_path)
    # The source-owned full plan is the authority. No external shorter plan,
    # caller-written success flag or old verification summary substitutes for
    # original checker attempts, output and current selected producer records.
    verified = qualification.verify_execution(plan, execution, source)
    inputs = qualification.execution_inputs(plan, execution, source)
    report = load_json(execution)
    stages = {checker: [stage for stage in report["stages"]
                        if stage["checker"] == checker]
              for checker in ("source_sdk", "sdk_consumers")}
    if any(len(selected) != 1 for selected in stages.values()):
        raise EvidenceError("Full local plan requires one SDK and consumer producer")

    def producer_output(checker):
        argv = stages[checker][0]["argv"]
        if argv.count("--output") != 1:
            raise EvidenceError("Explicit owned producer output is required")
        return Path(argv[argv.index("--output") + 1])

    sdk_manifest = producer_output("source_sdk") / "share/nexus/src/.nexus-source-sdk.json"
    summary = parse_json(Path(stages["source_sdk"][0]["raw"]["path"]).read_bytes())
    if (summary.get("kind") != "nexus_source_sdk_summary" or
            summary.get("sdk_kind") != "nexus-installed-source-sdk" or
            summary.get("publishable") is not True or
            summary.get("source_revision") != observed["commit"] or
            type(summary.get("files")) is not int or summary["files"] <= 0):
        raise EvidenceError("Original source SDK producer summary is required")
    sdk = load_json(sdk_manifest)
    if (sdk.get("kind") != "nexus-installed-source-sdk" or
            sdk.get("source_revision") != observed["commit"] or
            sdk.get("source_tree") != observed["tree"] or
            sdk.get("publishable") is not True or sdk.get("source_dirty") is not False or
            not isinstance(sdk.get("files_sha256"), dict) or
            len(sdk["files_sha256"]) != summary["files"] or
            sdk.get("snapshot_sha256") != canonical_digest(sdk["files_sha256"]) or
            sdk.get("snapshot_sha256") != summary.get("snapshot_sha256")):
        raise EvidenceError("SDK manifest differs from actual producer snapshot")
    consumer_root = producer_output("sdk_consumers")
    consumer_report = consumer_root / "verification.json"
    consumer = load_json(consumer_report)
    files, directories = transport.scan(consumer_root)
    transport.report(consumer, observed["commit"], files)
    if consumer.get("source_tree") != observed["tree"]:
        raise EvidenceError("Consumer producer source tree differs")
    sdk_root = sdk_manifest.parent
    argv = stages["sdk_consumers"][0]["argv"]
    if (argv.count("--prefix") != 1 or
            Path(argv[argv.index("--prefix") + 1]) != sdk_root.parents[2] or
            consumer["commands"][0]["argv"] != [argv[0], "-B",
                str(sdk_root / "cmake/package/package_source_sdk.py"), "--verify", str(sdk_root)]):
        raise EvidenceError("Consumer producer must verify the exact qualified SDK prefix")
    logs, receipts = set(), set()
    for actual in consumer["commands"]:
        name = actual["log"]
        if (Path(name).name != name or name in logs or
                actual.get("receipt") in receipts):
            raise EvidenceError("Consumer command log leaves its original producer")
        logs.add(name)
        receipts.add(actual.get("receipt"))
        log = consumer_root / name
        inputs.extend(qualification.completed_command_inputs(log,
                      expected_argv=actual["argv"]))
        receipt = load_json(log.with_suffix(".command.json"))
        if str(consumer_root / actual.get("receipt", "")) != receipt["attempt_receipt"]:
            raise EvidenceError("Consumer command differs from its original attempt")
        if any(actual.get(key) != receipt.get(key) for key in (
                "started_ns", "finished_ns", "duration_seconds", "outcome",
                "child_returncode", "withdrawal_complete")):
            raise EvidenceError("Consumer summary differs from its original receipt")
    if (logs != {path.name for path in consumer_root.glob("command-*.log")} or
            {str(Path(name).with_suffix(".command.json")) for name in logs} !=
            {path.name for path in consumer_root.glob("command-*.command.json")}):
        raise EvidenceError("Consumer commands do not cover the current original producer")
    inputs.extend((sdk_manifest, consumer_report))
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("GIT_")}
    dependencies = subprocess.check_output(["git", "-C", str(source),
        "ls-files", "-z", "--", "dependencies", ".gitmodules"], env=environment)
    inputs.extend(source / os.fsdecode(name) for name in dependencies.split(b"\0") if name)
    inputs.append(plan_path)
    evidence = [_local_file(path) for path in sorted(set(inputs))]
    if git_source(source) != observed:
        raise EvidenceError("Source changed during local admission")
    return {"source": observed, "version": load_policy(source)["semantic_version"],
            "plan": file_identity(plan_path), "qualification": verified,
            "evidence": evidence, "producers": {
                "sdk_manifest": file_identity(sdk_manifest),
                "sdk_snapshot_sha256": summary["snapshot_sha256"],
                "consumer_report": file_identity(consumer_report),
                "consumer_files_sha256": canonical_digest(files),
                "consumer_directories_sha256": canonical_digest(directories)}}


def _local_artifacts(source, context, artifacts, *, root=None, scratch):
    """Reuse owned package validators on one independent restore at a time."""
    from tools.documentation import delivery as documentation
    from tools.evidence import candidate, store
    from tools.evidence.common import EvidenceError, load_json
    from tools.evidence.identity import canonical_digest
    from tools.release import transport

    def locate(name):
        if root is None:
            return Path(name)
        relative = Path(name)
        if (relative.is_absolute() or not relative.parts or
                any(part in {".", ".."} for part in relative.parts) or
                "\\" in name):
            raise EvidenceError("Local artifact path escapes its member closure")
        return Path(root) / relative

    selected = [("sdk", artifacts["sdk"]), ("consumers", artifacts["consumers"])]
    selected.extend(("candidate", item) for item in artifacts["candidates"])
    configurations = set()
    candidate_ids = set()
    results = []
    for role, item in selected:
        envelope = locate(item["envelope"])
        envelope_identity = _local_file(envelope)
        if any(envelope_identity[key] != item["original_envelope"][key]
               for key in ("sha256", "size", "mode")):
            raise EvidenceError("Local package envelope bytes changed")
        package = load_json(envelope)
        if root is not None and item["archive"] != (
                "archives/" + package["archive"]["sha256"] + ".tar.gz"):
            raise EvidenceError("Local package archive differs from its member closure")
        archive = locate(item["archive"])
        archive_identity = _local_file(archive)
        if any(archive_identity[key] != item["original_archive"][key]
               for key in ("sha256", "size", "mode")):
            raise EvidenceError("Local package archive bytes changed")
        capacity = store.restore_capacity(package, archive_override=archive,
                                           reserve_bytes=16 * 1024 * 1024)
        # Candidate verification reconstructs its embedded SDK independently.
        # Keep one additional verified SDK restore footprint available while
        # the outer candidate tree is still present.
        needed = capacity["required_bytes"]
        if role == "candidate":
            sdk_item = artifacts["sdk"]
            sdk_needed = store.restore_capacity(load_json(locate(sdk_item["envelope"])),
                archive_override=locate(sdk_item["archive"]))["required_bytes"]
            needed += sdk_needed
            if shutil.disk_usage(tempfile.gettempdir()).free < sdk_needed + 16 * 1024 * 1024:
                raise EvidenceError("Insufficient candidate SDK verifier temporary capacity")
        if shutil.disk_usage(scratch).free < needed:
            raise EvidenceError(f"Insufficient local restore capacity: required={needed}")
        with tempfile.TemporaryDirectory(prefix=".local-input-", dir=scratch) as name:
            restored = Path(name) / "payload"
            store.restore_tree(package, restored, archive_override=archive)
            if role == "sdk":
                identity = verify_sdk(source, restored, context["source"]["commit"])
                version = load_policy(restored / "share/nexus/src")["semantic_version"]
                if version != context["version"]:
                    raise EvidenceError("SDK version differs from local release policy")
                result = {"manifest_sha256": digest(
                    restored / "share/nexus/src/.nexus-source-sdk.json"),
                    "snapshot_sha256": identity.get("snapshot_sha256")}
                if (result["manifest_sha256"] != context["producers"]["sdk_manifest"]["sha256"] or
                        result["snapshot_sha256"] != context["producers"]["sdk_snapshot_sha256"]):
                    raise EvidenceError("SDK differs from the actual qualified producer")
            elif role == "consumers":
                manifest = transport.verify(restored, context["source"]["commit"])
                if (digest(restored / "verification.json") !=
                        context["producers"]["consumer_report"]["sha256"] or
                        canonical_digest(manifest["files"]) !=
                        context["producers"]["consumer_files_sha256"] or
                        canonical_digest(manifest["directories"]) !=
                        context["producers"]["consumer_directories_sha256"]):
                    raise EvidenceError("Consumer transport differs from the actual qualified producer")
                result = {"consumer_policy": manifest["consumer_policy"],
                          "manifest_sha256": canonical_digest(manifest)}
            else:
                sealed = candidate.verify(restored / "candidate.json")
                if sealed["source"] != context["source"]:
                    raise EvidenceError("Candidate source differs from local execution")
                if ([item["manifest_sha256"] for item in sealed["sdk_packages"]] !=
                        [results[0]["manifest_sha256"]]):
                    raise EvidenceError("Candidate SDK differs from the qualified source SDK")
                configuration = sealed["configuration_sha256"]
                if (configuration in configurations or
                        sealed["candidate_id"] in candidate_ids):
                    raise EvidenceError("Duplicate candidate ID or configuration")
                configurations.add(configuration)
                candidate_ids.add(sealed["candidate_id"])
                result = {key: sealed[key] for key in ("candidate_id",
                    "configuration_sha256", "qualified_scopes", "physical_status",
                    "signature_status")}
                result["artifacts"] = {key: {field: identity[field] for field in
                    ("sha256", "size")} for key, identity in sealed["artifacts"].items()}
            results.append(result)
    document = artifacts["documentation"]
    archive = locate(document["archive"])
    if (root is not None and document["archive"] !=
            "documentation/" + document["original"]["sha256"] + ".tar.gz"):
        raise EvidenceError("Documentation archive differs from its member closure")
    archive_identity = _local_file(archive)
    if any(archive_identity[key] != document["original"][key]
           for key in ("sha256", "size", "mode")):
        raise EvidenceError("Documentation archive bytes changed")
    manifest = documentation.verify(archive)
    if (any(manifest[role].get(key) != context["source"][key]
            for role in ("documentation_source", "code_source")
            for key in ("commit", "tree", "dirty", "submodules")) or
            manifest["version"] != context["version"]):
        raise EvidenceError("Documentation source/version differs from local execution")
    return {"sdk": results[0], "consumers": results[1],
            "candidates": results[2:], "documentation": {
                "manifest_sha256": canonical_digest(manifest)}}


def bundle_local(source, execution, sdk_package, consumer_package, documentation,
                 candidate_packages, output, expected_commit):
    """Prepare an unsigned local preview; never authorize a remote publication.

    This preserves existing package and original evidence bytes. It does not
    build, reseal candidates, run Actions or forge an OIDC attestation. Admission
    requires the original current clean source and complete producer records;
    the portable store may independently verify the resulting tree's bytes.
    """
    from tools.evidence.common import (EvidenceError, atomic_json, file_identity,
                                       load_json, regular_file, separate_outputs)
    from tools.evidence.identity import canonical_digest, git_source
    from tools.evidence import store
    source, output = Path(source).resolve(), Path(output).absolute()
    execution = regular_file(execution)
    if not isinstance(candidate_packages, (list, tuple)) or not 0 < len(candidate_packages) <= 64:
        raise EvidenceError("An explicit nonempty bounded candidate selection is required")
    packages = [regular_file(path) for path in
                (sdk_package, consumer_package, *candidate_packages)]
    documentation = regular_file(documentation)
    separate_outputs([output], [execution, documentation, *packages],
                     [execution.parent])
    if output.exists():
        raise EvidenceError("Local preview output must be fresh")
    context = _local_context(source, execution, expected_commit)
    selections = []
    for envelope in packages:
        package = load_json(envelope)
        archive = regular_file(package["archive"]["path"])
        # Strict package validation remains the store's responsibility. Bound
        # sizes before the first restore/copy, then verify all members below.
        if (type(package.get("bytes")) is not int or package["bytes"] <= 0 or
                type(package["archive"].get("size")) is not int or
                package["archive"]["size"] <= 0):
            raise EvidenceError("Local package capacity inputs are invalid")
        selections.append({"envelope": str(envelope), "archive": str(archive),
                           "original_envelope": _local_file(envelope),
                           "original_archive": _local_file(archive)})
    artifacts = {"sdk": selections[0], "consumers": selections[1],
                 "candidates": selections[2:], "documentation": {
                     "archive": str(documentation), "original": _local_file(documentation)}}
    input_files = {identity["path"]: identity["size"] for identity in context["evidence"]}
    input_files.update({str(path): path.stat().st_size for path in packages})
    input_files.update({item["archive"]: Path(item["archive"]).stat().st_size
                        for item in selections})
    input_files[str(documentation)] = documentation.stat().st_size
    capacity = [store.restore_capacity(load_json(path)) for path in packages]
    temporary_bytes = max(item["required_bytes"] for item in capacity)
    temporary_bytes += capacity[0]["required_bytes"]
    # Copies allocate blocks and directory metadata even for tiny/empty raw
    # streams. This is a conservative admission estimate, not a disk quota.
    copy_bytes = sum(((size + 4095) // 4096 + 2) * 4096
                     for size in input_files.values())
    required = copy_bytes + temporary_bytes + 16 * 1024 * 1024
    parent = output.parent
    existing_parent = next(path for path in (parent, *parent.parents) if path.exists())
    if shutil.disk_usage(existing_parent).free < required:
        raise EvidenceError(f"Insufficient local release capacity: required={required}")
    parent.mkdir(parents=True, exist_ok=True)
    verified = _local_artifacts(source, context, artifacts, scratch=parent)
    with tempfile.TemporaryDirectory(prefix=".local-preview-", dir=parent) as name:
        staged = Path(name) / "payload"
        staged.mkdir()

        def preserve(original, relative, expected):
            if _local_file(original) != expected:
                raise EvidenceError("Original local preview input changed before copy")
            destination = staged / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.exists():
                observed = _local_file(destination)
                if any(observed[key] != expected[key]
                       for key in ("size", "sha256", "mode")):
                    raise EvidenceError("Local payload name collision")
            else:
                shutil.copyfile(original, destination)
                destination.chmod(Path(original).stat().st_mode & 0o777)
            observed = _local_file(destination)
            if (any(observed[key] != expected[key]
                    for key in ("size", "sha256", "mode")) or
                    _local_file(original) != expected):
                raise EvidenceError("Local preview copy changed bytes")
            return relative

        evidence = []
        for identity in context["evidence"]:
            evidence.append({"original": identity, "payload": preserve(
                identity["path"], "evidence/" + identity["sha256"] +
                "." + format(identity["mode"], "03o"), identity)})
        for index, item in enumerate(selections):
            package = load_json(item["envelope"])
            item["envelope"] = preserve(item["envelope"], f"packages/{index}.json",
                                        item["original_envelope"])
            item["archive"] = preserve(item["archive"],
                "archives/" + package["archive"]["sha256"] + ".tar.gz",
                item["original_archive"])
            store.verify_package(package, archive_override=staged / item["archive"])
            item["verified"] = (verified["sdk"] if index == 0 else
                verified["consumers"] if index == 1 else verified["candidates"][index - 2])
        document = artifacts["documentation"]
        document["archive"] = preserve(document["archive"],
            "documentation/" + document["original"]["sha256"] + ".tar.gz",
            document["original"])
        document["verified"] = verified["documentation"]
        # Original reports/receipts are untouched. Observe again after copy so
        # a concurrent source or producer change cannot approve this bundle.
        if _local_context(source, execution, expected_commit) != context:
            raise EvidenceError("Local execution inputs changed during preparation")
        if git_source(source) != context["source"]:
            raise EvidenceError("Source changed during local preview preparation")
        record = {"schema_version": 1, "kind": "nexus_local_software_preview_bundle",
            "policy": LOCAL_POLICY, "status": "verified_local_inputs",
            "source": context["source"], "version": context["version"],
            "plan": context["plan"], "qualification": context["qualification"],
            "producers": context["producers"],
            "evidence": evidence, "artifacts": artifacts,
            "members": store.inventory(staged), "signature_status": "unsigned",
            "trust": "local_observation_no_oidc", "physical_status": "not_executed",
            "publication_authorized": False, "production_qualification": False,
            "unassessed_publication_exits": LOCAL_UNASSESSED_EXITS}
        record["record_sha256"] = canonical_digest(record)
        atomic_json(staged / LOCAL_MANIFEST, record)
        staged.rename(output)
    return record


def verify_local(source, output, expected_commit):
    """Recheck exact live admission and every immutable packaged member."""
    from tools.evidence.common import EvidenceError, fields, load_json
    from tools.evidence.identity import canonical_digest
    from tools.evidence import store
    output = Path(output).absolute()
    original_manifest = _local_file(output / LOCAL_MANIFEST)
    record = load_json(output / LOCAL_MANIFEST, max_bytes=64 * 1024 * 1024)
    fields(record, {"schema_version", "kind", "policy", "status", "source", "version",
        "plan", "qualification", "producers", "evidence", "artifacts", "members", "signature_status",
        "trust", "physical_status", "publication_authorized", "production_qualification",
        "unassessed_publication_exits", "record_sha256"})
    if (type(record["schema_version"]) is not int or record["schema_version"] != 1 or
            record["kind"] != "nexus_local_software_preview_bundle" or
            record["policy"] != LOCAL_POLICY or record["status"] != "verified_local_inputs" or
            record["signature_status"] != "unsigned" or
            record["trust"] != "local_observation_no_oidc" or
            record["physical_status"] != "not_executed" or
            record["publication_authorized"] is not False or
            record["production_qualification"] is not False or
            record["unassessed_publication_exits"] != LOCAL_UNASSESSED_EXITS or
            record["record_sha256"] != canonical_digest({key: value for key, value in
                record.items() if key != "record_sha256"})):
        raise EvidenceError("Invalid unsigned local preview policy or record")
    actual_members = [item for item in store.inventory(output)
                      if item["path"] != LOCAL_MANIFEST]
    if actual_members != record["members"]:
        raise EvidenceError("Local preview member closure changed")
    execution = Path(record["qualification"]["report"]["path"])
    context = _local_context(source, execution, expected_commit)
    if any(record[key] != context[key] for key in (
            "source", "version", "plan", "qualification", "producers")):
        raise EvidenceError("Verified local source/plan/execution differs from preparation")
    if [item["original"] for item in record["evidence"]] != context["evidence"]:
        raise EvidenceError("Local evidence closure differs from verified original execution")
    for item in record["evidence"]:
        fields(item, {"original", "payload"})
        if item["payload"] != ("evidence/" + item["original"]["sha256"] +
                "." + format(item["original"]["mode"], "03o")):
            raise EvidenceError("Execution payload differs from its member closure")
        identity = _local_file(output / item["payload"])
        if any(identity[key] != item["original"][key] for key in ("size", "sha256", "mode")):
            raise EvidenceError("Original execution copy bytes changed")
    artifacts = record["artifacts"]
    fields(artifacts, {"sdk", "consumers", "candidates", "documentation"})
    if not isinstance(artifacts["candidates"], list) or not artifacts["candidates"]:
        raise EvidenceError("An explicit candidate selection remains required")
    selections = [artifacts["sdk"], artifacts["consumers"], *artifacts["candidates"]]
    for index, item in enumerate(selections):
        fields(item, {"envelope", "archive", "original_envelope", "original_archive", "verified"})
        if item["envelope"] != f"packages/{index}.json":
            raise EvidenceError("Package envelope differs from its member closure")
        package = load_json(output / item["envelope"])
        if item["archive"] != "archives/" + package["archive"]["sha256"] + ".tar.gz":
            raise EvidenceError("Package archive differs from its member closure")
    fields(artifacts["documentation"], {"archive", "original", "verified"})
    capacity = [store.restore_capacity(load_json(output / item["envelope"]),
                archive_override=output / item["archive"])["required_bytes"]
                for item in selections]
    required = max(capacity) + capacity[0] + 16 * 1024 * 1024
    if shutil.disk_usage(output.parent).free < required:
        raise EvidenceError(f"Insufficient local verification capacity: required={required}")
    verified = _local_artifacts(source, context, artifacts, root=output,
                                scratch=output.parent)
    for role in ("sdk", "consumers", "documentation"):
        if verified[role] != artifacts[role]["verified"]:
            raise EvidenceError("Verified local artifact differs from preparation")
    if verified["candidates"] != [item["verified"] for item in artifacts["candidates"]]:
        raise EvidenceError("Verified candidates differ from preparation")
    if _local_context(source, execution, expected_commit) != context:
        raise EvidenceError("Local execution inputs changed during verification")
    if (_local_file(output / LOCAL_MANIFEST) != original_manifest or
            [item for item in store.inventory(output) if item["path"] != LOCAL_MANIFEST] !=
            record["members"]):
        raise EvidenceError("Local preview bytes changed during verification")
    return record


def _github_http_failure(method, url, error, token):
    """Retain bounded GitHub diagnostics without credentials or response dumps."""
    def safe_text(value, limit=256):
        if not isinstance(value, str):
            return None
        if token:
            value = value.replace(token, "[REDACTED]").replace(
                quote(token, safe=""), "[REDACTED]")
        if re.search(r"<[^>]*>", value):
            return None
        value = re.sub(r"https?://[^\s<>\"']+", "[URL omitted]", value,
                       flags=re.IGNORECASE)
        value = re.sub(r"[\x00-\x1f\x7f]", " ", value)
        return " ".join(value.split())[:limit]

    endpoint = "[unrecognized endpoint omitted]"
    try:
        parsed = urlsplit(url)
        if parsed.scheme == "https" and parsed.hostname in ("api.github.com", "uploads.github.com"):
            endpoint = safe_text(parsed.hostname + (parsed.path or "/"), 512)
    except ValueError:
        # Malformed untrusted URLs retain the redacted fallback endpoint.
        pass
    details = {"method": method if re.fullmatch(r"[A-Z]{1,12}", method) else "UNKNOWN",
               "endpoint": endpoint, "status": error.code}
    try:
        contents = error.read(16385)
    except (OSError, ValueError):
        contents = b""
    finally:
        try:
            error.close()
        except (OSError, ValueError):
            # Cleanup failure must not replace the bounded HTTP diagnostic.
            pass
    body = None
    if isinstance(contents, bytes) and len(contents) <= 16384:
        try:
            body = json.loads(contents)
        except (ValueError, RecursionError):
            # Invalid or deeply nested JSON has no safe response-body fallback.
            pass
    if isinstance(body, dict):
        if message := safe_text(body.get("message"), 512):
            details["message"] = message
        errors = body.get("errors")
        if isinstance(errors, list):
            selected = []
            for item in errors[:5]:
                if isinstance(item, dict):
                    item = {key: text for key in ("resource", "code", "field", "message")
                            if (text := safe_text(item.get(key)))}
                else:
                    item = safe_text(item)
                if item:
                    selected.append(item)
            if selected:
                details["errors"] = selected
        documentation = body.get("documentation_url")
        if (isinstance(documentation, str) and (not token or token not in documentation)
                and re.fullmatch(r"https://docs\.github\.com/[A-Za-z0-9/_.#-]{1,500}", documentation)):
            details["documentation_url"] = documentation
    headers = {}
    for name in ("X-GitHub-Request-Id", "X-Accepted-GitHub-Permissions",
                 "X-RateLimit-Limit", "X-RateLimit-Remaining", "X-RateLimit-Used",
                 "X-RateLimit-Reset", "X-RateLimit-Resource", "Retry-After"):
        if error.headers and (value := safe_text(error.headers.get(name))):
            headers[name] = value
    if headers:
        details["headers"] = headers
    for field in ("errors", "message", "documentation_url", "headers"):
        rendered = "GitHub request failed: " + json.dumps(details, ensure_ascii=True)
        if len(rendered) <= 4096:
            return rendered
        details.pop(field, None)
        details["truncated"] = True
    return "GitHub request failed: " + json.dumps(details, ensure_ascii=True)


class GitHub:
    """GitHub HTTP is the sole remote publication boundary."""

    def __init__(self, repository):
        if not isinstance(repository, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
            raise ValueError("Invalid repository")
        self.repository = repository
        self.base = "https://api.github.com/repos/" + repository
        self.token = os.environ["GH_TOKEN"]

    def request(self, method, url, payload=None, *, binary=False, missing=False):
        headers = {"Authorization": "Bearer " + self.token,
                   "Accept": "application/octet-stream" if binary else "application/vnd.github+json",
                   "X-GitHub-Api-Version": "2022-11-28"}
        if isinstance(payload, bytes):
            headers["Content-Type"] = "application/octet-stream"
        elif payload is not None:
            payload = json.dumps(payload).encode()
            headers["Content-Type"] = "application/json"
        try:
            with urlopen(Request(url, data=payload, headers=headers, method=method), timeout=120) as response:
                contents = response.read()
        except HTTPError as error:
            if missing and error.code == 404:
                return None
            raise ValueError(_github_http_failure(method, url, error, self.token)) from None
        return contents if binary else json.loads(contents)

    def get(self, path):
        return self.request("GET", self.base + path, missing=True)

    def create(self, value):
        return self.request("POST", self.base + "/releases", value)

    def upload(self, release, path):
        url = release["upload_url"].split("{", 1)[0] + "?name=" + quote(path.name, safe="")
        return self.request("POST", url, path.read_bytes())

    def asset_bytes(self, asset):
        return self.request("GET", self.base + "/releases/assets/" + str(asset["id"]), binary=True)

    def publish(self, release_id):
        return self.request("PATCH", self.base + "/releases/" + str(release_id),
                            {"draft": False, "prerelease": True})


def require_refs(api, expected_commit, tag, *, builder_commit=None):
    builder_commit = expected_commit if builder_commit is None else commit(builder_commit)
    for ref, expected in (("heads/main", builder_commit), ("tags/" + tag, expected_commit)):
        value = api.get("/git/ref/" + ref)
        if value is None:
            raise ValueError("Explicit existing main and release tag are required")
        target = value["object"]
        if ref.startswith("tags/") and target["type"] == "tag":
            target = api.get("/git/tags/" + target["sha"])["object"]
        if target.get("type") != "commit" or target.get("sha") != expected:
            raise ValueError("Main or existing release tag differs from the expected commit")


def guard(expected_commit, actual_commit, *, source=ROOT, api=None,
          publishing=False, recover_tagged_source=False):
    expected_commit, actual_commit = commit(expected_commit), commit(actual_commit)
    if not recover_tagged_source and expected_commit != actual_commit:
        raise ValueError("Expected commit differs from actual workflow checkout")
    if recover_tagged_source and (publishing is not True or api is None):
        raise ValueError("Tagged-source recovery requires explicit publication")
    source = Path(source)
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    status = subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=all"],
                                     cwd=source, text=True)
    if head != actual_commit or status:
        raise ValueError("Delivery requires the exact clean builder checkout")
    if recover_tagged_source:
        policy = source / "dependencies/platform-compatibility.json"
        tag = "v" + load_policy(source)["semantic_version"]
        require_refs(api, expected_commit, tag, builder_commit=actual_commit)
        original = subprocess.check_output(["git", "show",
            expected_commit + ":dependencies/platform-compatibility.json"], cwd=source)
        if original != policy.read_bytes():
            raise ValueError("Tagged source and trusted builder compatibility policies differ")


def publish(output, expected_commit, ci_run_id, *, api, source=None,
            recover_tagged_source=False, builder_commit=None, builder_ci_run_id=None):
    from tools.release.publication import observe_boundary, recheck_boundary
    version = load_policy(ROOT if source is None else source)["semantic_version"]
    tag = "v" + version
    output = Path(output)
    release_manifest = read(output / "release-manifest.json")
    if (release_manifest.get("tag") != tag or
            release_manifest.get("source_commit") != commit(expected_commit)):
        raise ValueError("Release manifest source/tag mismatch")
    if recover_tagged_source:
        if (not isinstance(builder_commit, str) or
                release_manifest.get("builder_commit") != commit(builder_commit) or
                release_manifest.get("builder_workflow_ref") !=
                api.repository + "/.github/workflows/release.yml@refs/heads/main" or
                type(builder_ci_run_id) is not int or builder_ci_run_id <= 0 or
                release_manifest.get("builder_ci_run_id") != builder_ci_run_id):
            raise ValueError("Tagged-source recovery builder identity differs")
    elif (builder_commit is not None and builder_commit != expected_commit) or builder_ci_run_id is not None:
        raise ValueError("Separate builder inputs require explicit tagged-source recovery")
    builder_commit = expected_commit if builder_commit is None else builder_commit
    assets = release_manifest["assets"]
    assets = assets + [{"name": "release-manifest.json",
        "size": (output / "release-manifest.json").stat().st_size,
        "sha256": digest(output / "release-manifest.json")}]
    if (len({item["name"] for item in assets}) != len(assets) or
            {item["name"] for item in assets} != {path.name for path in output.iterdir()}):
        raise ValueError("Release asset closure differs from the reviewed manifest")
    for item in assets:
        if Path(item["name"]).name != item["name"]:
            raise ValueError("Release asset name must be a basename")
        path = output / item["name"]
        if path.is_symlink() or path.stat().st_size != item["size"] or digest(path) != item["sha256"]:
            raise ValueError("Release asset bytes changed before upload")
    observed_run = require_ci(api.get("/actions/runs/" + str(ci_run_id)), expected_commit)
    if (observed_run["id"] != ci_run_id or release_manifest.get("ci_run_id") != ci_run_id or
            type(release_manifest.get("ci_run_attempt")) is not int or
            observed_run["run_attempt"] != release_manifest["ci_run_attempt"]):
        raise ValueError("Source CI run or attempt differs from release inputs")
    jobs_path = ("/actions/runs/" + str(ci_run_id) + "/attempts/" +
                 str(observed_run["run_attempt"]) + "/jobs?per_page=100")
    require_jobs(api.get(jobs_path))
    builder_run = None
    if recover_tagged_source:
        builder_run = require_ci(api.get("/actions/runs/" + str(builder_ci_run_id)), builder_commit)
        if (builder_run["id"] != builder_ci_run_id or
                type(release_manifest.get("builder_ci_run_attempt")) is not int or
                builder_run["run_attempt"] != release_manifest.get("builder_ci_run_attempt")):
            raise ValueError("Builder CI run or attempt differs from release inputs")
        builder_jobs_path = ("/actions/runs/" + str(builder_ci_run_id) + "/attempts/" +
                             str(builder_run["run_attempt"]) + "/jobs?per_page=100")
        require_jobs(api.get(builder_jobs_path))
    require_refs(api, expected_commit, tag, builder_commit=builder_commit)
    if api.get("/releases/tags/" + tag) is not None:
        raise ValueError("Release already exists; inspect its bytes before any retry")
    # The verified SDK, documentation and CI producers are all owned by this
    # repository. A filesystem path (including a private build workspace) does
    # not establish a second origin or authorize a visibility claim.
    def repository(name):
        if name.casefold() != api.repository.casefold():
            raise ValueError("Release producer repository differs")
        return api.get("")

    def revision(name, selected):
        if name.casefold() != api.repository.casefold():
            raise ValueError("Release source repository differs")
        return api.get("/commits/" + selected)

    origins = [{"repository": api.repository, "commit": expected_commit}]
    if builder_commit != expected_commit:
        origins.append({"repository": api.repository, "commit": builder_commit})
    boundary = observe_boundary(api.repository, origins,
                                repository=repository, revision=revision)
    release = api.create({"tag_name": tag, "target_commitish": expected_commit,
        "name": "Nexus " + version + " software preview",
        "draft": True, "prerelease": True,
        "body": "Source SDK and bilingual offline documentation validated for commit `" +
        expected_commit + "`. See release-manifest.json for exact byte identities and CI run.\n\n"
        "This is a software preview. Physical HIL is not executed; this is not a production "
        "qualification, safety certification, signed firmware release or LTS promise.\n\n"
        "Nexus " + version + " 源码软件预览：本次提交为 `" + expected_commit +
        "`，交付经验证的源码 SDK、双语离线文档与原始软件证据；精确字节身份和 CI "
        "见 release-manifest.json。物理 HIL 尚未执行；不构成生产资格、安全认证、"
        "已签名固件发布或 LTS 承诺。"})
    for item in assets:
        recheck_boundary(boundary, repository=repository, revision=revision)
        asset = api.upload(release, output / item["name"])
        if (asset.get("name") != item["name"] or asset.get("size") != item["size"] or
                hashlib.sha256(api.asset_bytes(asset)).hexdigest() != item["sha256"]):
            raise ValueError("Uploaded release asset bytes differ; release remains draft")
    remote = api.get("/releases/" + str(release["id"]) + "/assets?per_page=100")
    if {item["name"] for item in remote} != {item["name"] for item in assets}:
        raise ValueError("Uploaded release asset set differs; release remains draft")
    final_run = require_ci(api.get("/actions/runs/" + str(ci_run_id)), expected_commit)
    if (final_run["id"] != observed_run["id"] or
            final_run["run_attempt"] != observed_run["run_attempt"]):
        raise ValueError("CI was rerun during publication; release remains draft")
    require_jobs(api.get(jobs_path))
    if builder_run is not None:
        final_builder = require_ci(api.get("/actions/runs/" + str(builder_ci_run_id)), builder_commit)
        if (final_builder["id"] != builder_run["id"] or
                final_builder["run_attempt"] != builder_run["run_attempt"]):
            raise ValueError("Builder CI was rerun during publication; release remains draft")
        require_jobs(api.get(builder_jobs_path))
    require_refs(api, expected_commit, tag, builder_commit=builder_commit)
    recheck_boundary(boundary, repository=repository, revision=revision)
    return api.publish(release["id"])


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    child = commands.add_parser("guard")
    child.add_argument("--expected-commit", required=True)
    child.add_argument("--actual-commit", required=True)
    child.add_argument("--publish", action="store_true")
    child.add_argument("--recover-tagged-source", action="store_true")
    child.add_argument("--repository")
    child = commands.add_parser("stage")
    child.add_argument("--source", type=Path, required=True)
    child.add_argument("--prefix", type=Path, required=True)
    child.add_argument("--consumers", type=Path, required=True)
    child.add_argument("--output", type=Path, required=True)
    child.add_argument("--expected-commit", required=True)
    child = commands.add_parser("bundle")
    child.add_argument("--source", type=Path, default=ROOT)
    child.add_argument("--inputs", type=Path, required=True)
    child.add_argument("--output", type=Path, required=True)
    child.add_argument("--expected-commit", required=True)
    child.add_argument("--ci-run-id", type=int, required=True)
    child.add_argument("--release-run-id", type=int, required=True)
    child.add_argument("--attempt", type=int, required=True)
    child.add_argument("--recover-tagged-source", action="store_true")
    child.add_argument("--builder-commit")
    child.add_argument("--builder-ci-run-id", type=int)
    child = commands.add_parser("bundle-local", aliases=["prepare-local"])
    child.add_argument("--source", type=Path, default=ROOT)
    child.add_argument("--execution", type=Path, required=True)
    child.add_argument("--sdk-package", type=Path, required=True)
    child.add_argument("--consumer-package", type=Path, required=True)
    child.add_argument("--documentation", type=Path, required=True)
    child.add_argument("--candidate-package", type=Path, action="append", required=True)
    child.add_argument("--output", type=Path, required=True)
    child.add_argument("--expected-commit", required=True)
    child = commands.add_parser("verify-local")
    child.add_argument("--source", type=Path, default=ROOT)
    child.add_argument("--output", type=Path, required=True)
    child.add_argument("--expected-commit", required=True)
    child = commands.add_parser("check-ci")
    child.add_argument("--record", type=Path, required=True)
    child.add_argument("--jobs", type=Path, required=True)
    child.add_argument("--expected-commit", required=True)
    child = commands.add_parser("publish")
    child.add_argument("--source", type=Path, default=ROOT)
    child.add_argument("--output", type=Path, required=True)
    child.add_argument("--expected-commit", required=True)
    child.add_argument("--ci-run-id", type=int, required=True)
    child.add_argument("--repository", required=True)
    child.add_argument("--recover-tagged-source", action="store_true")
    child.add_argument("--builder-commit")
    child.add_argument("--builder-ci-run-id", type=int)
    args = parser.parse_args(arguments)
    try:
        if args.command == "guard":
            guard(args.expected_commit, args.actual_commit, publishing=args.publish,
                  recover_tagged_source=args.recover_tagged_source,
                  api=GitHub(args.repository) if args.recover_tagged_source else None)
        elif args.command == "stage":
            stage(args.source, args.prefix, args.consumers, args.output, args.expected_commit)
        elif args.command == "bundle":
            bundle(args.source, args.inputs, args.output, args.expected_commit,
                   args.ci_run_id, args.release_run_id, args.attempt,
                   recover_tagged_source=args.recover_tagged_source,
                   builder_commit=args.builder_commit, builder_ci_run_id=args.builder_ci_run_id)
        elif args.command in {"bundle-local", "prepare-local"}:
            result = bundle_local(args.source, args.execution, args.sdk_package,
                args.consumer_package, args.documentation, args.candidate_package,
                args.output, args.expected_commit)
            print("Local preview: " + result["status"])
        elif args.command == "verify-local":
            result = verify_local(args.source, args.output, args.expected_commit)
            print("Local preview: " + result["status"])
        elif args.command == "check-ci":
            require_ci(read(args.record), args.expected_commit)
            require_jobs(read(args.jobs))
        else:
            publish(args.output, args.expected_commit, args.ci_run_id,
                    api=GitHub(args.repository), source=args.source,
                    recover_tagged_source=args.recover_tagged_source,
                    builder_commit=args.builder_commit, builder_ci_run_id=args.builder_ci_run_id)
    except (ValueError, OSError, subprocess.SubprocessError, KeyError) as error:
        print("Release rejected: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
