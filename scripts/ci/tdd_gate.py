#!/usr/bin/env python3
"""Execute the host GoogleTest/GoogleMock contract gate without cached evidence."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.validation.junit import validate_junit

RELEVANT_PREFIXES = (
    "core/", "arch/", "soc/", "boards/", "io/", "os/", "components/",
    "tests/", "cmake/", "tools/", "scripts/",
    "dependencies/googletest", "dependencies/source/googletest",
)
RELEVANT_FILES = {
    "CMakeLists.txt", "CMakePresets.json", ".pre-commit-config.yaml",
    "scripts/ci/tdd_gate.py",
}


def validate_google_report(report, *, not_before_ns=None, expected_cases=None):
    """Require completed GoogleTest cases in addition to valid fresh JUnit."""
    evidence = validate_junit(report, not_before_ns=not_before_ns)
    root = ET.parse(report).getroot()
    if root.tag != "testsuites" or root.get("name") != "AllTests":
        raise ValueError("Report is not the complete GoogleTest execution")
    for case in root.iter("testcase"):
        if case.get("status") != "run" or case.get("result") != "completed":
            raise ValueError("GoogleTest case did not complete execution")
    if evidence["skipped"]:
        raise ValueError("GoogleTest contracts may not be skipped")
    if expected_cases is not None and evidence["passed"] != expected_cases:
        raise ValueError("GoogleTest execution differs from discovered cases")
    return evidence


def relevant(name):
    """Keep documentation-only commits fast while exercising behavior changes."""
    return name in RELEVANT_FILES or name.startswith(RELEVANT_PREFIXES)


def check_staged_snapshot(root):
    """A worktree-only fix must not certify a different staged source snapshot."""
    result = subprocess.run(
        ["git", "diff", "--name-only", "-z"], cwd=root,
        capture_output=True, check=True,
    )
    dirty = [name.decode() for name in result.stdout.split(b"\0")
             if name and relevant(name.decode())]
    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard", "-z"], cwd=root,
        capture_output=True, check=True,
    )
    dirty.extend(name.decode() for name in untracked.stdout.split(b"\0")
                 if name and relevant(name.decode()))
    if dirty:
        raise ValueError("Unstaged behavioral changes differ from the commit: "
                         + ", ".join(dirty))


def staged_tree(root):
    """Capture the exact index content, including newly staged source changes."""
    result = subprocess.run(["git", "write-tree"], cwd=root,
                            capture_output=True, text=True, check=True)
    return result.stdout.strip()


def environment():
    """Clean child test settings without changing the gate's commit index."""
    result = {key: value for key, value in os.environ.items()
              if not key.startswith("GTEST_")}
    local = ROOT / ".venv" / ("Scripts" if os.name == "nt" else "bin")
    result["PATH"] = str(local) + os.pathsep + result.get("PATH", "")
    if result.get("SKIP") or result.get("NEXUS_SKIP_TDD"):
        raise ValueError("A hook bypass setting cannot establish TDD evidence")
    if any(name.startswith("GIT_") for name in result):
        local = subprocess.run(["git", "rev-parse", "--local-env-vars"],
                               cwd=ROOT, capture_output=True, text=True,
                               check=True)
        for name in local.stdout.splitlines():
            result.pop(name, None)
    return result


def execute(command, env, log):
    """Record the real subprocess result without accepting console success text."""
    print("+ " + " ".join(map(str, command)), flush=True)
    result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True,
                            text=True, check=False)
    log.write_text(result.stdout + result.stderr)
    log.with_suffix(".command.json").write_text(json.dumps({
        "schema": 1, "command": list(map(str, command)), "cwd": str(ROOT),
        "exit_code": result.returncode,
    }, indent=2) + "\n")
    if result.returncode:
        raise ValueError(f"Command failed ({result.returncode}); see {log}")
    return result.stdout


def discovered_count(output):
    """Count only test names in the framework's own unfiltered discovery output."""
    suite = None
    count = 0
    for line in output.splitlines():
        text = line.split("#", 1)[0].rstrip()
        if text and not text[0].isspace() and text.endswith("."):
            suite = text
        elif suite is not None and text.startswith("  ") and text.strip():
            count += 1
    if count == 0:
        raise ValueError("GoogleTest discovery contains zero cases")
    return count


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", default="native-debug")
    parser.add_argument("--staged", action="store_true")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("paths", nargs="*")
    args = parser.parse_args(arguments)
    try:
        env = environment()
        if args.staged and not args.all and not any(map(relevant, args.paths)):
            print("No staged behavioral changes require the GoogleTest gate")
            return 0
        if args.staged:
            initial_tree = staged_tree(ROOT)
            check_staged_snapshot(ROOT)
        presets = json.loads((ROOT / "CMakePresets.json").read_text())
        preset = next((item for item in presets["configurePresets"]
                       if item["name"] == args.preset), None)
        if preset is None or not args.preset.startswith("native-"):
            raise ValueError("TDD contracts require a maintained Native preset")
        build = Path(preset["binaryDir"].replace("${sourceDir}", str(ROOT)))
        output = build / "tdd"
        output.mkdir(parents=True, exist_ok=True)
        summary = output / "execution.json"
        summary.unlink(missing_ok=True)
        python = sys.executable
        tool_results = []
        for suite in ("testing", "configure"):
            tool_report = output / f"tools-{suite}.json"
            tool_report.unlink(missing_ok=True)
            execute([python, "-B", "tools/testing/run_tool_tests.py", "--suite",
                     suite, "--report", str(tool_report)], env,
                    output / f"tools-{suite}.txt")
            tool_results.append(json.loads(tool_report.read_text()))
        execute([python, "tools/dev/dev.py", "configure", "--preset",
                 args.preset], env, output / "configure.txt")
        execute([python, "tools/dev/dev.py", "build", "--preset", args.preset,
                 "--target", "nexus_google_contracts"], env,
                output / "build.txt")
        manifest = json.loads((build / "google-contracts.json").read_text())
        if (manifest.get("schema") != 1 or not manifest.get("executables")
                or len(manifest["executables"]) != len(set(manifest["executables"]))):
            raise ValueError("Missing or duplicate GoogleTest executable plan")
        results = []
        for index, name in enumerate(manifest["executables"]):
            binary = Path(name)
            if not binary.is_file() or not binary.is_relative_to(build):
                raise ValueError("GoogleTest binary is missing or outside build")
            listing = execute([str(binary), "--gtest_filter=*",
                               "--gtest_list_tests"], env,
                              output / f"discovery-{index}.txt")
            count = discovered_count(listing)
            report = output / f"google-{index}.xml"
            report.unlink(missing_ok=True)
            started = time.time_ns()
            execute([str(binary), "--gtest_filter=*", "--gtest_repeat=1",
                     "--gtest_also_run_disabled_tests",
                     "--gtest_output=xml:" + str(report)], env,
                    output / f"execution-{index}.txt")
            evidence = validate_google_report(report, not_before_ns=started,
                                              expected_cases=count)
            results.append({"binary": binary.name, **evidence})
        if args.staged:
            check_staged_snapshot(ROOT)
            if staged_tree(ROOT) != initial_tree:
                raise ValueError("Staged index changed during contract execution")
        summary.write_text(json.dumps({
            "schema": 1, "kind": "development-contract-execution",
            "preset": args.preset, "passed": sum(r["passed"] for r in results),
            "suites": results, "tool_suites": tool_results,
            "scope": "Actual host execution; not proof of historical TDD order "
                     "or physical hardware qualification",
        }, indent=2) + "\n")
        print(f"Executed {sum(r['passed'] for r in results)} GoogleTest cases; "
              f"evidence: {summary}")
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"TDD gate rejected: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
