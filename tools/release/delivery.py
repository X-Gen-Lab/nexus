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
            pass
    body = None
    if isinstance(contents, bytes) and len(contents) <= 16384:
        try:
            body = json.loads(contents)
        except (ValueError, RecursionError):
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
