#!/usr/bin/env python3
"""Prepare read-only dependency proposals; never change locks or approve upgrades.

A weekly inventory observes existing pins. Candidate archives and declared
license notices require exact identities. Evidence references remain unreviewed:
this tool cannot turn a JSON passed marker into qualification or merge authority.
"""
from __future__ import annotations
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure.metadata import (  # noqa: E402
    fields, identity, parse_json, read_json, regular, snapshot,
)

GATES = ("license_review", "configuration", "host_models", "cortex_runtime",
         "board_arm", "resource_budgets", "installed_sdk",
         "offline_reproduction", "static_analysis")
WORKLOADS = {
    "license_review": "Codex independent review of source origin, all redistributed files and exact license/notice changes",
    "configuration": "All resolver/kernel/configuration boundary tests and generated input-closure/schema/epoch checks",
    "host_models": "Fresh full pinned GoogleTest/GoogleMock execution, tools, CTest and maintained Native sanitizer profiles",
    "cortex_runtime": "All maintained CPU/ISA/FP/MVE/ABI runtime, protected-port and split-image software matrices",
    "board_arm": "Three exact maintained Board configurations with baremetal and FreeRTOS; changed external examples",
    "resource_budgets": "Every newly linked ELF independently checked; same-fixture before/after costs with explicit RAM domains",
    "installed_sdk": "Clean exact-revision source SDK closure, package version/epoch rejection and actual standalone consumers",
    "offline_reproduction": "Two fresh network-disabled builds in the bound environment, identical same ELF/BIN/map bytes",
    "static_analysis": "Owned-source ClangTidy/Cppcheck and style/comments; retain raw failures and reviewed vendor ownership scope",
}


def git(root, *arguments):
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("GIT_")}
    result = subprocess.run(["git", "-C", str(root), *arguments],
                            capture_output=True, text=True, env=environment,
                            timeout=20)
    if result.returncode:
        raise ValueError("Cannot observe locked Git source")
    return result.stdout.strip()


def verify_reference(record):
    fields(record, {"path", "sha256", "size"}, "file identity")
    if not isinstance(record["path"], str) or not Path(record["path"]).is_absolute():
        raise ValueError("File identity must use an absolute path")
    if not isinstance(record["sha256"], str) or not re.fullmatch(r"[0-9a-f]{64}", record["sha256"]):
        raise ValueError("Exact SHA256 required")
    if type(record["size"]) is not int or record["size"] <= 0:
        raise ValueError("Nonempty file identity required")
    actual = identity(Path(record["path"]))
    if actual != record:
        raise ValueError("File identity changed")
    return actual


def inventory(root=ROOT):
    """Observe actual checkout pins and bytes without contacting an upstream."""
    root = Path(root).absolute()
    if (root / ".nexus-source-sdk.json").exists() or not (root / ".git").exists():
        raise ValueError("Dependency proposals require a development Git checkout")
    commit = git(root, "rev-parse", "HEAD")
    module_bytes, module_identity = snapshot(root / ".gitmodules")
    origins = configparser.ConfigParser(interpolation=None)
    origins.read_string(module_bytes.decode("utf-8"))
    urls = {origins[section]["path"]: origins[section]["url"]
            for section in origins.sections()}
    expected_modules = {"ext/freertos", "vendors/arm/CMSIS_5", "vendors/st/cmsis_device_f4"}
    if len(urls) != len(origins.sections()) or set(urls) != expected_modules:
        raise ValueError("Unregistered or duplicate source dependency requires catalogue review")
    records = []
    for name, path, notice in (
            ("freertos", "ext/freertos", "LICENSE.md"),
            ("cmsis5", "vendors/arm/CMSIS_5", "LICENSE.txt"),
            ("stm32f4_cmsis", "vendors/st/cmsis_device_f4", "LICENSE.md")):
        entry = git(root, "ls-tree", "HEAD", "--", path).split()
        if len(entry) != 4 or entry[:2] != ["160000", "commit"]:
            raise ValueError("Missing committed source Gitlink: " + path)
        checkout = root / path
        if git(checkout, "rev-parse", "HEAD") != entry[2] or git(checkout, "status", "--porcelain", "--untracked-files=normal"):
            raise ValueError("Source checkout differs from locked clean Gitlink: " + path)
        records.append({"id": name, "revision": entry[2], "kind": "git_source",
                        "path": path, "upstream_url": urls[path],
                        "identities": [module_identity, identity(checkout / notice)],
                        "license_review": "current_notices_observed"})
    google_path = root / "dependencies/googletest.lock.json"
    google_bytes, google_identity = snapshot(google_path)
    google = parse_json(google_bytes)
    archive = root / google["archive"]
    archive_identity = identity(archive)
    if archive_identity["sha256"] != google["sha256"] or archive_identity["size"] != google["bytes"]:
        raise ValueError("Pinned GoogleTest archive identity changed")
    origin = re.fullmatch(r"https://codeload\.github\.com/([A-Za-z0-9_.-]+)/([A-Za-z0-9_.-]+)/tar\.gz/([0-9a-f]{40})", google["url"])
    if origin is None or origin.group(3) != google["commit"]:
        raise ValueError("GoogleTest source URL differs from its exact commit")
    records.append({"id": "googletest", "revision": google["commit"],
                    "kind": "git_source", "upstream_url": "https://github.com/" + origin.group(1) + "/" + origin.group(2) + ".git",
                    "identities": [google_identity,
                    archive_identity, identity(root / "dependencies/source/googletest-LICENSE.txt")],
                    "license_review": "current_notices_observed"})
    vendor_path = root / "vendors/gigadevice/gd32f4xx/source.lock.json"
    vendor_bytes, vendor_identity = snapshot(vendor_path)
    vendor = parse_json(vendor_bytes)
    notices = sorted(vendor_path.parent.glob("LICENSES/*"))
    if not notices:
        raise ValueError("Vendor import license notices missing")
    # Reuse the source-lock file identities; this inventory is not an import
    # review or a replacement for verify_vendor_source/vendor_import_identity.
    records.append({"id": "gd32f4_library", "revision": vendor["version"],
                    "kind": "archive_source", "identities": [vendor_identity,
                    *[identity(path) for path in notices]],
                    "license_review": "current_notices_observed"})
    tool_path = root / "dependencies/toolchains.lock.json"
    tool_bytes, tool_identity = snapshot(tool_path)
    tool = parse_json(tool_bytes)["arm_gnu"]
    records.append({"id": "arm_gnu", "revision": tool["version"],
                    "kind": "tool_archive", "identities": [tool_identity],
                    "license_review": "candidate_distribution_review_required"})
    for name in ("environment.lock.json", "development-tools.txt", "environment-tools.txt"):
        path = root / "dependencies" / name
        item = identity(path)
        records.append({"id": name, "revision": item["sha256"],
                        "kind": "environment_manifest", "identities": [item],
                        "license_review": "candidate_distribution_review_required"})
    action_path = root / "dependencies/actions.lock.json"
    action_bytes, action_identity = snapshot(action_path)
    for action in parse_json(action_bytes)["actions"]:
        if not re.fullmatch(r"[0-9a-f]{40}", action["sha"]):
            raise ValueError("Action must be pinned to an exact commit")
        records.append({"id": action["repository"], "revision": action["sha"],
                        "kind": "git_source", "upstream_url": "https://github.com/" + action["repository"] + ".git",
                        "identities": [action_identity],
                        "license_review": "candidate_distribution_review_required"})
    if len({item["id"] for item in records}) != len(records):
        raise ValueError("Duplicate dependency identity")
    # Refuse a mixed observation if a parallel checkout or writer changed bytes.
    if git(root, "rev-parse", "HEAD") != commit:
        raise ValueError("Source commit changed during inventory")
    for item in records:
        for reference in item["identities"]:
            verify_reference(reference)
        if "path" in item and (git(root / item["path"], "rev-parse", "HEAD") != item["revision"] or
                               git(root / item["path"], "status", "--porcelain", "--untracked-files=normal")):
            raise ValueError("Source dependency changed during inventory")
    return {"schema_version": 1, "kind": "nexus-dependency-inventory",
            "owner": "codex", "mode": "weekly_read_only", "network_reads": 0,
            "source_revision": commit, "source_dirty": bool(git(root, "status", "--porcelain", "--untracked-files=all")),
            "dependencies": records, "locks_changed": False,
            "upgrade_approved": False, "software_qualified": False,
            "upstream_latest_checked": False}


def observe_upstream(current, output, runner=None):
    """Capture bounded public refs; a network error remains not_observed."""
    if os.name != "posix":
        raise ValueError("Bounded upstream capture requires the maintained POSIX runner")
    output = Path(output).absolute()
    if output != output.resolve(strict=False):
        raise ValueError("Upstream output must be canonical without parent traversal")
    if output.exists() or any(path.is_symlink() for path in (output, *output.parents)):
        raise ValueError("Upstream capture requires fresh non-symlink output")
    if output.is_relative_to(ROOT) and not output.is_relative_to(ROOT / "build"):
        raise ValueError("Upstream capture must not write into source or locks")
    for item in current["dependencies"]:
        origin = item.get("upstream_url")
        if origin is not None and (not isinstance(origin, str) or not re.fullmatch(
                r"https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+\.git", origin)):
            raise ValueError("Upstream requires an exact public approved GitHub origin")
    if runner is None:
        # Reuse the maintained bounded subprocess capture and kill/reap
        # mechanism. No HIL admission, recipe or equipment action is invoked.
        from tools.hil.acquire import _command
        runner = _command
    git_tool, env_tool = shutil.which("git"), shutil.which("env")
    if not git_tool or not env_tool:
        raise ValueError("Git and env required for bounded upstream capture")
    output.mkdir(parents=True, exist_ok=False)
    observations = []
    for index, item in enumerate(current["dependencies"]):
        record = {"dependency": item["id"], "current_revision": item["revision"],
                  "status": "not_observed", "refs": []}
        origin = item.get("upstream_url")
        if origin is None:
            record["reason"] = "non_git_publication_requires_separate_origin_review"
            observations.append(record)
            continue
        step = {"name": f"request-{index:04d}", "tool": identity(Path(env_tool)),
                "arguments": ["GIT_CONFIG_NOSYSTEM=1", "GIT_CONFIG_GLOBAL=/dev/null",
                              "GIT_TERMINAL_PROMPT=0", "GIT_ASKPASS=/bin/false",
                              str(regular(git_tool)), "-c", "credential.helper=",
                              "-c", "http.extraHeader=", "ls-remote", "--refs",
                              "--tags", "--heads", origin],
                "timeout_s": 10, "maximum_output_bytes": 1024 * 1024}
        record["source_url"] = origin
        record["git_tool"] = identity(Path(git_tool))
        with tempfile.TemporaryDirectory(prefix="nexus-upstream-git-") as isolated:
            directory = Path(isolated)
            # Capture output may live under build/. Git itself must not read
            # that repository's local URL rewrites, includes or auth headers.
            # Stop discovery before the temporary parent, which can itself
            # belong to a checkout in managed development environments.
            step["arguments"].insert(0, "GIT_CEILING_DIRECTORIES=" + str(directory.parent))
            position = step["arguments"].index(str(regular(git_tool))) + 1
            step["arguments"][position:position] = ["-C", isolated]
            request = runner(step, {}, output)
        record["request"] = request
        if request["status"] == "captured":
            try:
                raw_path = Path(request["stdout"]["path"])
                if identity(raw_path) != request["stdout"]:
                    raise ValueError("Remote raw response identity changed")
                lines = raw_path.read_bytes().decode("utf-8").splitlines()
                if not lines or len(lines) > 8192:
                    raise ValueError("Remote refs empty or exceed record bound")
                refs = []
                for line in lines:
                    match = re.fullmatch(r"([0-9a-f]{40})\t(refs/(?:tags|heads)/[^\s\x00-\x1f]+)", line)
                    if match is None:
                        raise ValueError("Remote response is not an exact ref listing")
                    refs.append({"revision": match.group(1), "ref": match.group(2)})
                record.update(status="observed", refs=refs)
            except (ValueError, UnicodeError, OSError) as error:
                record["reason"] = str(error)
        else:
            record["reason"] = "network_command_failed_or_bounded_capture_rejected"
        observations.append(record)
    result = {"schema_version": 1, "kind": "nexus-upstream-ref-observations",
              "status": "observations_retained", "observations": observations,
              "locks_changed": False, "upgrade_approved": False,
              "software_qualified": False,
              "network_requests_attempted": sum(
                  bool(item.get("request", {}).get("process_started")) for item in observations)}
    (output / "observations.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def propose(plan, current):
    """Validate candidate input bytes; every execution and legal gate stays open."""
    fields(plan, {"schema_version", "dependency", "proposed_revision", "source_url",
                  "archive", "license", "evidence"}, "proposal input")
    if type(plan["schema_version"]) is not int or plan["schema_version"] != 1:
        raise ValueError("Unsupported dependency proposal schema")
    selected = [item for item in current["dependencies"] if item["id"] == plan["dependency"]]
    if len(selected) != 1:
        raise ValueError("Unknown dependency")
    selected = selected[0]
    revision = plan["proposed_revision"]
    if not isinstance(revision, str) or not revision or len(revision) > 128 or revision == selected["revision"]:
        raise ValueError("Distinct pinned proposed revision required")
    if selected["kind"] == "git_source" and not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("Git source proposal requires full commit")
    if selected["kind"] != "git_source" and not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.+_-]{0,127}", revision):
        raise ValueError("Pinned archive/version identity required")
    if not isinstance(plan["source_url"], str):
        raise ValueError("Public source URL required")
    url = urlsplit(plan["source_url"])
    if url.scheme != "https" or not url.hostname or url.username or url.password or url.query or url.fragment:
        raise ValueError("Public HTTPS source URL without credentials required")
    archive = verify_reference(plan["archive"])
    license_record = plan["license"]
    fields(license_record, {"expression", "notices"}, "declared license")
    expression = license_record["expression"]
    if not isinstance(expression, str) or not expression.strip() or expression.lower() in {"unknown", "unreviewed"}:
        raise ValueError("Explicit declared license expression required")
    if not isinstance(license_record["notices"], list) or not license_record["notices"]:
        raise ValueError("Exact license notices required")
    notices = [verify_reference(record) for record in license_record["notices"]]
    evidence = plan["evidence"]
    if not isinstance(evidence, dict) or set(evidence) - set(GATES):
        raise ValueError("Unknown evidence gate")
    matrix = {}
    for gate in GATES:
        references = evidence.get(gate)
        if references is not None and (not isinstance(references, list) or not references):
            raise ValueError("Evidence gate requires nonempty identity references")
        matrix[gate] = {"status": "referenced_unreviewed" if references else "not_executed",
                        "workload": WORKLOADS[gate],
                        "references": [verify_reference(record) for record in references or []]}
    # Deliberately do not parse a claim of passed, invoke a report's command,
    # change a lock, or approve an upgrade. Existing formal validators must
    # independently qualify the exact evaluated source and artifacts.
    return {"schema_version": 1, "kind": "nexus-dependency-upgrade-proposal",
            "owner": "codex", "mode": "weekly_read_only",
            "status": "requires_review_and_execution", "dependency": selected["id"],
            "current_revision": selected["revision"], "proposed_revision": revision,
            "source_url": plan["source_url"], "archive": archive,
            "declared_license": {"expression": expression, "notices": notices},
            "gate_matrix": matrix, "locks_changed": False,
            "upgrade_approved": False, "software_qualified": False,
            "physical_status": "not_executed", "network_reads": 0}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--plan", type=Path)
    parser.add_argument("--observe-upstream", action="store_true")
    parser.add_argument("--upstream-output", type=Path)
    args = parser.parse_args(argv)
    try:
        current = inventory(args.root)
        report = propose(read_json(args.plan), current) if args.plan else current
        if args.observe_upstream:
            if args.upstream_output is None:
                raise ValueError("Explicit fresh --upstream-output required")
            report["upstream_observations"] = observe_upstream(current, args.upstream_output)
        elif args.upstream_output is not None:
            raise ValueError("Upstream output requires explicit --observe-upstream")
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"Dependency proposal rejected: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
