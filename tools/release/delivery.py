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
from urllib.parse import quote
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.dont_write_bytecode = True
TAG = "v0.1.0"


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
    if not re.fullmatch(r"[0-9a-f]{40}", value):
        raise ValueError("An exact 40-character commit is required")
    return value


def inventory(path):
    path = Path(path)
    if any(item.is_symlink() for item in (path, *path.parents)):
        raise ValueError("Release paths must not traverse symlinks")
    result = {}
    for item in sorted(path.rglob("*")):
        if item.is_symlink() or not (item.is_dir() or item.is_file()):
            raise ValueError("Release payload contains a nonregular path")
        if item.is_file():
            result[item.relative_to(path).as_posix()] = digest(item)
    if not result or len(result) > 100000:
        raise ValueError("Release payload inventory is empty or exceeds its bound")
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
            "u575_assembly", "multi_controller_assembly")}
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



def archive_evidence(inputs, destination):
    """Retain raw consumer outputs and qualification files with original paths."""
    inputs = Path(inputs)
    selections = [inputs / name for name in (
        "sdk/delivery/sdk-consumers", "sdk/delivery/staging-copy.json",
        "sdk/job-identities", "docs", "attestation", "ci-evidence",
        "ci-run.json", "ci-jobs.json")]
    expected = {}
    for selection in selections:
        paths = inventory(selection) if selection.is_dir() else {
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


def bundle(source, inputs, output, expected_commit, ci_run_id, release_run_id, attempt):
    from tools.documentation import delivery
    source, inputs, output = Path(source), Path(inputs), Path(output)
    if output.exists():
        raise ValueError("Release asset staging must be fresh")
    run = require_ci(read(inputs / "ci-run.json"), expected_commit)
    require_jobs(read(inputs / "ci-jobs.json"))
    if run["id"] != ci_run_id:
        raise ValueError("CI input belongs to a different run")
    sdk, docs = inputs / "sdk", inputs / "docs"
    prefix, checks = sdk / "delivery/source-sdk", sdk / "delivery/sdk-consumers"
    verify_sdk(source, prefix, expected_commit)
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
            documentation["version"] != "0.1.0"):
        raise ValueError("Documentation code/version identity differs from this release")
    doc_payload = docs / "documentation-package" / documentation["release_id"]
    manifest = doc_payload / delivery.MANIFEST
    delivery.verify(doc_payload, expected_manifest_sha256=digest(manifest))
    attestation = inputs / "attestation/delivery/attestation/bundle.json"
    if not isinstance(read(attestation), dict):
        raise ValueError("Actual source SDK attestation bundle is required")
    output.mkdir(parents=True)
    archive = output / "nexus-source-sdk-0.1.0.tar.gz"
    expected_files = inventory(prefix)
    with tarfile.open(archive, "w:gz") as stream:
        stream.add(prefix, arcname="nexus-source-sdk-0.1.0")
    with tempfile.TemporaryDirectory(prefix="nexus-release-archive-") as directory:
        with tarfile.open(archive) as stream:
            for member in stream.getmembers():
                path = Path(member.name)
                if (path.is_absolute() or ".." in path.parts or
                        not (member.isdir() or member.isfile())):
                    raise ValueError("SDK release archive contains an unsafe entry")
            stream.extractall(directory, filter="data")
        restored = Path(directory) / "nexus-source-sdk-0.1.0"
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
    archive_evidence(inputs, output / "nexus-verification-evidence-0.1.0.tar.gz")
    assets = [{"name": path.name, "size": path.stat().st_size, "sha256": digest(path)}
              for path in sorted(output.iterdir())]
    write(output / "release-manifest.json", {"schema_version": 1,
          "kind": "nexus_software_preview", "tag": TAG, "source_commit": expected_commit,
          "ci_run_id": ci_run_id, "ci_run_attempt": run["run_attempt"],
          "source_sdk_run_id": release_run_id, "assets": assets,
          "physical_hil": "not_executed", "software_candidate": False,
          "production_qualification": False,
          "evidence_layout": {"sdk_archive_root": "nexus-source-sdk-0.1.0",
                              "sdk_restore_path": "sdk/delivery/source-sdk",
                              "raw_records": "nexus-verification-evidence-0.1.0.tar.gz"}})


class GitHub:
    """GitHub HTTP is the sole remote publication boundary."""

    def __init__(self, repository):
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
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
            raise ValueError(f"GitHub {method} request failed with HTTP {error.code}") from error
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


def require_refs(api, expected_commit):
    for ref in ("heads/main", "tags/" + TAG):
        value = api.get("/git/ref/" + ref)
        if value is None:
            raise ValueError("Explicit existing main and release tag are required")
        target = value["object"]
        if ref.startswith("tags/") and target["type"] == "tag":
            target = api.get("/git/tags/" + target["sha"])["object"]
        if target.get("type") != "commit" or target.get("sha") != expected_commit:
            raise ValueError("Main or existing release tag differs from the expected commit")


def publish(output, expected_commit, ci_run_id, *, api):
    output = Path(output)
    release_manifest = read(output / "release-manifest.json")
    if (release_manifest.get("tag") != TAG or
            release_manifest.get("source_commit") != commit(expected_commit)):
        raise ValueError("Release manifest source/tag mismatch")
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
    jobs_path = ("/actions/runs/" + str(ci_run_id) + "/attempts/" +
                 str(observed_run["run_attempt"]) + "/jobs?per_page=100")
    require_jobs(api.get(jobs_path))
    require_refs(api, expected_commit)
    if api.get("/releases/tags/" + TAG) is not None:
        raise ValueError("Release already exists; inspect its bytes before any retry")
    release = api.create({"tag_name": TAG, "target_commitish": expected_commit,
        "name": "Nexus 0.1.0 software preview", "draft": True, "prerelease": True,
        "body": "Source SDK and bilingual offline documentation validated for commit `" +
        expected_commit + "`. See release-manifest.json for exact byte identities and CI run.\n\n"
        "This is a software preview. Physical HIL is not executed; this is not a production "
        "qualification, safety certification, signed firmware release or LTS promise.\n\n"
        "Nexus 0.1.0 源码软件预览：本次提交为 `" + expected_commit +
        "`，交付经验证的源码 SDK、双语离线文档与原始软件证据；精确字节身份和 CI "
        "见 release-manifest.json。物理 HIL 尚未执行；不构成生产资格、安全认证、"
        "已签名固件发布或 LTS 承诺。"})
    for item in assets:
        asset = api.upload(release, output / item["name"])
        if (asset.get("name") != item["name"] or asset.get("size") != item["size"] or
                hashlib.sha256(api.asset_bytes(asset)).hexdigest() != item["sha256"]):
            raise ValueError("Uploaded release asset bytes differ; release remains draft")
    remote = api.get("/releases/" + str(release["id"]) + "/assets?per_page=100")
    if {item["name"] for item in remote} != {item["name"] for item in assets}:
        raise ValueError("Uploaded release asset set differs; release remains draft")
    final_run = require_ci(api.get("/actions/runs/" + str(ci_run_id)), expected_commit)
    if final_run["run_attempt"] != observed_run["run_attempt"]:
        raise ValueError("CI was rerun during publication; release remains draft")
    require_jobs(api.get(jobs_path))
    require_refs(api, expected_commit)
    return api.publish(release["id"])


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    child = commands.add_parser("guard")
    child.add_argument("--expected-commit", required=True)
    child.add_argument("--actual-commit", required=True)
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
    child = commands.add_parser("check-ci")
    child.add_argument("--record", type=Path, required=True)
    child.add_argument("--jobs", type=Path, required=True)
    child.add_argument("--expected-commit", required=True)
    child = commands.add_parser("publish")
    child.add_argument("--output", type=Path, required=True)
    child.add_argument("--expected-commit", required=True)
    child.add_argument("--ci-run-id", type=int, required=True)
    child.add_argument("--repository", required=True)
    args = parser.parse_args(arguments)
    try:
        if args.command == "guard":
            if commit(args.expected_commit) != args.actual_commit:
                raise ValueError("Expected commit differs from actual workflow checkout")
        elif args.command == "stage":
            stage(args.source, args.prefix, args.consumers, args.output, args.expected_commit)
        elif args.command == "bundle":
            bundle(args.source, args.inputs, args.output, args.expected_commit,
                   args.ci_run_id, args.release_run_id, args.attempt)
        elif args.command == "check-ci":
            require_ci(read(args.record), args.expected_commit)
            require_jobs(read(args.jobs))
        else:
            publish(args.output, args.expected_commit, args.ci_run_id,
                    api=GitHub(args.repository))
    except (ValueError, OSError, subprocess.SubprocessError, KeyError) as error:
        print("Release rejected: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
