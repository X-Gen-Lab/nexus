"""Run pinned TruffleHog and admit only exact reviewed unknown test fixtures.

Candidates and verifier errors may contain credentials. Reports retain only
hashes and Git locations; raw scanner output never goes to workflow artifacts.
"""
from __future__ import annotations

import argparse
import ast
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import urllib.request

HEX40 = re.compile(r"[0-9a-f]{40}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
MAX_OUTPUT = 16 * 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON field")
        result[key] = value
    return result


def nonfinite(_value):
    raise ValueError("Nonfinite JSON value")


def fields(value, keys):
    if not isinstance(value, dict) or set(value) != set(keys):
        raise ValueError("Unreviewed security policy fields")


def validate_policy(policy):
    fields(policy, ("schema_version", "version", "archive_url",
                    "archive_sha256", "binary_sha256", "reviewed_unknowns"))
    if type(policy["schema_version"]) is not int or policy["schema_version"] != 1:
        raise ValueError("Unreviewed policy schema")
    if (policy["version"] != "3.99.1" or policy["archive_url"] !=
            "https://github.com/trufflesecurity/trufflehog/releases/download/"
            "v3.99.1/trufflehog_3.99.1_linux_amd64.tar.gz"):
        raise ValueError("Unreviewed scanner release")
    for key in ("archive_sha256", "binary_sha256"):
        if not isinstance(policy[key], str) or not HEX64.fullmatch(policy[key]):
            raise ValueError("Exact scanner identity required")
    reviewed = policy["reviewed_unknowns"]
    if not isinstance(reviewed, list) or not reviewed:
        raise ValueError("Explicit reviewed unknowns required")
    seen = set()
    for item in reviewed:
        fields(item, ("detector", "detector_type", "source_type", "path",
                      "function", "raw_sha256", "context_sha256", "reason"))
        if (item["detector"] != "URI" or type(item["detector_type"]) is not int
                or item["detector_type"] != 17
                or type(item["source_type"]) is not int
                or item["source_type"] != 16):
            raise ValueError("Unreviewed detector or source")
        path = item["path"]
        if (not isinstance(path, str) or not re.fullmatch(r"[\w./-]+", path)
                or PurePosixPath(path).is_absolute()
                or ".." in PurePosixPath(path).parts
                or not path.startswith("tools/testing/test_")):
            raise ValueError("Exact test source path required")
        if (not isinstance(item["function"], str)
                or not re.fullmatch(r"test_[a-z_]+", item["function"])):
            raise ValueError("Exact test function required")
        for key in ("raw_sha256", "context_sha256"):
            if not isinstance(item[key], str) or not HEX64.fullmatch(item[key]):
                raise ValueError("Exact reviewed match identity required")
        if not isinstance(item["reason"], str) or not item["reason"]:
            raise ValueError("Review rationale required")
        identity = (path, item["raw_sha256"], item["context_sha256"])
        if identity in seen:
            raise ValueError("Duplicate reviewed match")
        seen.add(identity)
    return policy


def reviewed(candidate, root, policy):
    if (candidate.get("Verified") is not False
            or not isinstance(candidate.get("VerificationError"), str)
            or not candidate["VerificationError"]
            or type(candidate.get("DetectorType")) is not int
            or type(candidate.get("SourceType")) is not int
            or candidate.get("DecoderName") != "PLAIN"
            or candidate.get("SourceName") != "trufflehog - git"):
        return False
    metadata = candidate.get("SourceMetadata", {}).get("Data", {})
    if not isinstance(metadata, dict) or set(metadata) != {"Git"}:
        return False
    git = metadata["Git"]
    if not isinstance(git, dict):
        return False
    commit, path, line = (git.get(key) for key in ("commit", "file", "line"))
    if (not isinstance(commit, str) or not HEX40.fullmatch(commit)
            or type(line) is not int or line < 1
            or git.get("repository") != root.as_uri()):
        return False
    for item in policy["reviewed_unknowns"]:
        if (candidate.get("DetectorName") != item["detector"]
                or candidate.get("DetectorType") != item["detector_type"]
                or candidate.get("SourceType") != item["source_type"]
                or path != item["path"]
                or digest(candidate["Raw"].encode()) != item["raw_sha256"]):
            continue
        try:
            result = subprocess.run(["git", "show", commit + ":" + path],
                                    cwd=root, capture_output=True, timeout=20,
                                    check=True)
            text = result.stdout.decode("utf-8")
            nodes = [node for node in ast.walk(ast.parse(text))
                     if isinstance(node, ast.FunctionDef)
                     and node.name == item["function"]]
            if len(nodes) != 1:
                continue
            node = nodes[0]
            if not node.lineno <= line <= node.end_lineno:
                continue
            lines = text.splitlines(keepends=True)
            context = "".join(lines[node.lineno - 1:node.end_lineno]).encode()
            if digest(context) == item["context_sha256"]:
                return True
        except (OSError, UnicodeError, SyntaxError, subprocess.SubprocessError):
            pass  # Git/source failure cannot grant a reviewed exception.
    return False


def evaluate(stdout, stderr, status, root, policy):
    validate_policy(policy)
    if status not in (0, 183):
        raise ValueError("Scanner failed; candidates cannot grant acceptance")
    if len(stdout) > MAX_OUTPUT or len(stderr) > MAX_OUTPUT:
        raise ValueError("Scanner output exceeds reviewed bound")
    rows = []
    for line in stdout.splitlines():
        if not line.strip():
            continue
        try:
            candidate = json.loads(line, object_pairs_hook=unique,
                                   parse_constant=nonfinite)
        except (UnicodeError, json.JSONDecodeError) as error:
            raise ValueError("Malformed scanner JSON") from error
        if (not isinstance(candidate, dict)
                or not isinstance(candidate.get("Raw"), str)
                or not candidate["Raw"]):
            raise ValueError("Malformed scanner candidate")
        metadata = candidate.get("SourceMetadata")
        if (not isinstance(metadata, dict)
                or not isinstance(metadata.get("Data"), dict)):
            raise ValueError("Malformed candidate source metadata")
        accepted = reviewed(candidate, root, policy)
        git = candidate.get("SourceMetadata", {}).get("Data", {}).get("Git", {})
        if not isinstance(git, dict):
            git = {}
        path = git.get("file", "")
        commit = git.get("commit", "")
        rows.append({"raw_sha256": digest(candidate["Raw"].encode()),
                     "path": path if isinstance(path, str) and
                     re.fullmatch(r"[\w./-]{1,250}", path) else "invalid",
                     "commit": commit if isinstance(commit, str) and
                     HEX40.fullmatch(commit) else "invalid",
                     "classification": "reviewed_unknown_fixture" if accepted
                     else "blocking_candidate"})
    if (status == 183) != bool(rows):
        raise ValueError("Scanner --fail status and candidates disagree")
    accepted_count = sum(row["classification"] == "reviewed_unknown_fixture"
                         for row in rows)
    return {"schema_version": 1, "status": "passed" if accepted_count ==
            len(rows) else "failed", "scanner_exit_code": status,
            "result_selection": ["verified", "unknown"],
            "stdout_sha256": digest(stdout), "stderr_sha256": digest(stderr),
            "candidate_count": len(rows), "reviewed_unknown_count": accepted_count,
            "blocking_count": len(rows) - accepted_count, "candidates": rows}


def regular(path):
    path = Path(path).absolute()
    if not path.is_file() or any(p.is_symlink() for p in (path, *path.parents)):
        raise ValueError("Canonical regular file required")
    return path


def scanner(policy, directory, supplied=None):
    if supplied is None:
        directory = Path(directory).absolute()
        directory.mkdir(parents=True, exist_ok=True)
        if any(p.is_symlink() for p in (directory, *directory.parents)):
            raise ValueError("Canonical scanner directory required")
        supplied = directory / "trufflehog"
        if not supplied.exists():
            with urllib.request.urlopen(policy["archive_url"], timeout=60) as source:
                raw = source.read(40 * 1024 * 1024)
            if digest(raw) != policy["archive_sha256"]:
                raise ValueError("Pinned scanner archive identity mismatch")
            with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
                member = archive.getmember("trufflehog")
                if not member.isfile():
                    raise ValueError("Scanner archive member is not regular")
                with archive.extractfile(member) as stream:
                    binary = stream.read()
            if digest(binary) != policy["binary_sha256"]:
                raise ValueError("Pinned scanner binary identity mismatch")
            with supplied.open("xb") as stream:
                stream.write(binary)
            supplied.chmod(0o755)
    supplied = regular(supplied)
    if digest(supplied.read_bytes()) != policy["binary_sha256"]:
        raise ValueError("Pinned scanner binary identity mismatch")
    version = subprocess.run([str(supplied), "--version"], capture_output=True,
                             timeout=20, check=True)
    if version.stdout.strip() != b"trufflehog 3.99.1":
        raise ValueError("Pinned scanner version mismatch")
    return supplied


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path.cwd())
    parser.add_argument("--policy", type=Path,
                        default=Path(".github/security/trufflehog.json"))
    parser.add_argument("--scanner", type=Path)
    parser.add_argument("--tools-dir", type=Path, default=Path(".security-tools"))
    parser.add_argument("--base", default="")
    parser.add_argument("--head", required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)
    report = {"schema_version": 1, "status": "failed", "reason": "scan_not_completed"}
    try:
        root = args.source.absolute()
        if any(p.is_symlink() for p in (root, *root.parents)):
            raise ValueError("Canonical Git source required")
        if not HEX40.fullmatch(args.head) or (args.base and
                                             not HEX40.fullmatch(args.base)):
            raise ValueError("Exact Git scan references required")
        top = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=root,
                             capture_output=True, check=True, timeout=20)
        if Path(top.stdout.decode().strip()).absolute() != root:
            raise ValueError("Scanner source must be the actual Git root")
        for reference in (args.head, args.base):
            if reference and reference != "0" * 40:
                subprocess.run(["git", "rev-parse", "--verify",
                                reference + "^{commit}"], cwd=root,
                               capture_output=True, check=True, timeout=20)
        policy_path = regular(args.policy)
        raw_policy = policy_path.read_bytes()
        policy = validate_policy(json.loads(raw_policy, object_pairs_hook=unique,
                                            parse_constant=nonfinite))
        tool = scanner(policy, args.tools_dir, args.scanner)
        arguments = [str(tool), "git", root.as_uri(), "--branch", args.head,
                     "--results=verified,unknown", "--json", "--no-update",
                     "--fail", "--fail-on-scan-errors", "--no-ignore-tag"]
        if args.base and args.base != "0" * 40:
            arguments += ["--since-commit", args.base]
        result = subprocess.run(arguments, capture_output=True, timeout=900)
        report = evaluate(result.stdout, result.stderr, result.returncode, root,
                          policy)
        report.update(scanner_version=policy["version"],
                      scanner_sha256=policy["binary_sha256"],
                      policy_sha256=digest(raw_policy), head=args.head,
                      base=args.base, command=arguments)
    except (OSError, ValueError, KeyError, UnicodeError, tarfile.TarError,
            subprocess.SubprocessError):
        # Exception strings can embed URLs, candidates or child process output.
        report["status"] = "failed"
        report["reason"] = "scanner_or_policy_failure"
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print("Secret scan: " + report["status"] + "; candidates=" +
          str(report.get("candidate_count", "unknown")) + "; blocking=" +
          str(report.get("blocking_count", "unknown")))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
