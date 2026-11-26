#!/usr/bin/env python3
"""Execute the host GoogleTest/GoogleMock contract gate without cached evidence."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.ci import native_contracts
from scripts.ci.native_contracts import environment
from tools.dev.change_paths import behavioral
from tools.dev.presets import configure_if_needed, selected_configure
from tools.dev import process
from tools.dev.build_lock import build_lock
from tools.testing.arm_links import affected_arm_links
from tools.testing.run_tool_tests import (
    SUITES as TOOL_SUITES, changed_suites, commit_tool_plan,
)


def relevant(name):
    """Keep documentation-only commits fast while exercising behavior changes."""
    return behavioral(name)


def check_staged_snapshot(root):
    """A worktree-only fix must not certify a different staged source snapshot."""
    result = subprocess.run(
        ["git", "diff", "--no-renames", "--name-only", "-z"], cwd=root,
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


def staged_paths(root):
    """Use the complete real index delta, including deletions and odd names."""
    result = subprocess.run(
        ["git", "diff", "--cached", "--no-renames", "--name-only", "-z"], cwd=root,
        capture_output=True, check=True,
    )
    return [os.fsdecode(name) for name in result.stdout.split(b"\0") if name]




def execute(command, env, log, *, cancel_event=None):
    """Record the real subprocess result without accepting console success text."""
    print("+ " + " ".join(map(str, command)), flush=True)
    default_timeout = 60 if log.stem.startswith("discovery-") else 1800
    if log.stem.startswith("execution-"):
        default_timeout = 300
    result = process.run(command, cwd=ROOT, env=env, log=log,
                         stage=log.stem, cancel_event=cancel_event,
                         reject_background=cancel_event is not None,
                         timeout=process.command_timeout(env, default_timeout))
    if result.returncode:
        raise ValueError(f"Command failed ({result.returncode}); see {log}")
    return result.stdout


def execute_gate(args, env, build, initial_tree=None, *, preset):
    """Keep configuration, tools and reports inside the same build lease."""
    output = build / "tdd"
    output.mkdir(parents=True, exist_ok=True)
    summary = output / "execution.json"
    summary.unlink(missing_ok=True)
    python = sys.executable
    scope = "all" if args.all else args.tools or (
        "commit" if args.staged else "changed")
    paths = staged_paths(ROOT) if args.staged else args.paths
    plan = (commit_tool_plan(paths) if scope == "commit" else
            dict.fromkeys(TOOL_SUITES if scope == "all" else
                          changed_suites(paths) if scope == "changed" else ()))
    suites = tuple(plan)
    target = "contracts" if suites else "native"
    execute([python, "tools/dev/dev.py", "doctor", "--target", target],
            env, output / "preflight-tools.txt")
    configure_log = output / "configure.txt"
    configure_log.unlink(missing_ok=True)

    def configure(options):
        execute([python, "tools/dev/dev.py", "configure", "--preset",
                 args.preset, *options], env, configure_log)
        return 0

    configure_if_needed(ROOT, preset, env, configure)
    execute([python, "tools/dev/dev.py", "build", "--preset", args.preset], env,
            output / "build.txt")
    tool_env = env.copy()
    tool_env.pop("NEXUS_TOOL_NATIVE_DATABASE", None)
    if args.preset == "native-debug":
        tool_env["NEXUS_TOOL_NATIVE_DATABASE"] = str(build / "compile_commands.json")
    tool_results = []
    # Keep the same complete selection, running cheap workflow/CI checks
    # before configuration and SDK consumers so failures stop work early.
    ordered = [name for name in reversed(TOOL_SUITES) if name in plan]
    ordered.extend(name for name in plan if name not in TOOL_SUITES)
    for suite in ordered:
        files = plan[suite]
        suffix = "-commit" if files is not None else ""
        tool_report = output / f"tools-{suite}{suffix}.json"
        tool_report.unlink(missing_ok=True)
        command = [python, "-B", "tools/testing/run_tool_tests.py", "--suite",
                   suite, "--report", str(tool_report)]
        if files is not None:
            command += ["--files", *files]
        execute(command, tool_env, output / f"tools-{suite}{suffix}.txt")
        tool_results.append(json.loads(tool_report.read_text()))
    hardware = (affected_arm_links(ROOT, paths, env, output, execute, tree=initial_tree)
                if scope == "commit" else None)
    runtime = native_contracts.runtime_results(
        args.preset, build, env, output, run=execute)
    if args.staged:
        check_staged_snapshot(ROOT)
        if staged_tree(ROOT) != initial_tree:
            raise ValueError("Staged index changed during contract execution")
    summary.write_text(json.dumps({
        "schema": 1, "kind": "development-contract-execution",
        "preset": args.preset, **runtime,
        "tool_suites": tool_results, "hardware": hardware,
        "scope": "commit-tools" if scope == "commit" else scope,
        "qualification": False,
        "description": "Actual host execution; not proof of historical TDD order "
                       "or physical hardware qualification",
    }, indent=2) + "\n")
    print(f"Executed {runtime['ctest']['passed']} CTest entries and "
          f"{runtime['passed']} GoogleTest cases; "
          f"evidence: {summary}")
    return 0


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", default="native-debug")
    parser.add_argument("--staged", action="store_true")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--tools", choices=("all", "commit", "changed", "none"),
                        help="Explicit tool-domain scope; GoogleTest always runs in full")
    parser.add_argument("paths", nargs="*")
    args = parser.parse_args(arguments)
    try:
        env = environment()
        if (args.staged and not args.all and args.tools != "all"
                and not any(map(relevant, staged_paths(ROOT)))):
            print("No staged behavioral changes require the GoogleTest gate")
            return 0
        initial_tree = None
        if args.staged:
            initial_tree = staged_tree(ROOT)
            check_staged_snapshot(ROOT)
        if args.preset not in ("native-debug", "native-release", "native-asan",
                               "native-tsan", "native-step-debug"):
            raise ValueError("TDD contracts require a maintained Native preset")
        preset = selected_configure(ROOT, args.preset, environment=env)
        build = Path(preset["binaryDir"])
        if not build.is_relative_to(ROOT.resolve()):
            raise ValueError("Native build directory is outside the checkout")
        with build_lock(build):
            return execute_gate(args, env, build, initial_tree, preset=preset)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"TDD gate rejected: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
