#!/usr/bin/env python3
"""One thin entry point that preserves native CMake/CTest command exit codes."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.dev import process, provenance
from tools.dev.presets import Presets, binary_directory


class ExecutionResult(int):
    """Keep integer exit semantics while carrying this invocation's receipt."""

    def __new__(cls, returncode, receipt_path):
        result = super().__new__(cls, returncode)
        result.receipt_path = receipt_path
        return result


def executable(name):
    local = ROOT / ".venv" / ("Scripts" if os.name == "nt" else "bin") / name
    found = str(local) if local.is_file() else shutil.which(name)
    if not found:
        raise ValueError(f"Missing {name}: install "
                         "dependencies/environment-tools.txt")
    return found


def execute(command, environment):
    print("+ " + " ".join(command), flush=True)
    result = process.run(command, cwd=ROOT, env=environment,
                         log=ROOT / "build/quality/developer-command.txt",
                         stage="development")
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    return ExecutionResult(result.returncode, result.receipt_path)


def maintenance_checks(environment):
    for script in ("capabilities.py", "ownership.py"):
        result = execute([sys.executable, "tools/maintenance/" + script],
                         environment)
        if result:
            return result
    return 0


def workspace_identity():
    """A manifest triggers complete verification, never a check bypass."""
    manifest = ROOT / ".nexus-source-sdk.json"
    if manifest.exists() or manifest.is_symlink():
        # Normal CMake/CTest commands do not depend on SDK packaging. Load the
        # owned verifier only when this scope claims an installed SDK identity.
        try:
            sys.path.insert(0, str(ROOT / "cmake/package"))
            import package_source_sdk as source_sdk
            return source_sdk.verify(ROOT)
        except (ImportError, OSError, ValueError, TypeError, KeyError) as error:
            message = "Invalid source SDK identity: " + str(error)
            raise ValueError(message) from error
    if not (ROOT / ".git").exists():
        raise ValueError("Use a development checkout or verified source SDK")
    return None


def capture_provenance(action, preset, environment, native, receipt, command):
    """Bind successful native commands to observed bytes, never qualification."""
    try:
        if receipt is None:
            raise ValueError("Actual invocation receipt is missing")
        kind = "build" if action == "build" else "configure"
        build = binary_directory(ROOT, preset, kind=kind,
                                 environment=environment)
        if action == "configure":
            for index, argument in enumerate(native):
                if argument == "-B":
                    if index + 1 >= len(native):
                        raise ValueError("Configure -B requires a directory")
                    build = (ROOT / native[index + 1]).resolve()
                elif argument.startswith("-B") and len(argument) > 2:
                    build = (ROOT / argument[2:]).resolve()
        provenance.capture(ROOT, build, receipt, action=action,
                           expected_command=command, expected_cwd=ROOT)
    except (OSError, ValueError, KeyError, TypeError) as error:
        destination = ROOT / "build/quality/developer-provenance-failures"
        destination.mkdir(parents=True, exist_ok=True)
        failure = destination / (str(time.time_ns()) + "-" + uuid.uuid4().hex
                                 + ".json")
        provenance.atomic_json(failure, {
            "schema": 1, "status": "capture_failed", "action": action,
            "preset": preset, "command_receipt": str(receipt),
            "error": str(error), "qualification": False,
        })
        raise ValueError("Build provenance failed; retained "
                         + str(failure)) from error


def doctor(environment, native):
    """Diagnose explicit target requirements; tools-only remains available."""
    parser = argparse.ArgumentParser(prog="dev.py doctor")
    parser.add_argument("--target",
                        choices=("tools", "native", "arm", "candidate", "docs"),
                        default="tools")
    args, remaining = parser.parse_known_args(native)
    if args.target == "docs":
        return execute([sys.executable, "tools/documentation/cli.py", "doctor"]
                       + remaining, environment)
    if remaining:
        raise ValueError("Unknown doctor arguments: " + " ".join(remaining))
    identity = workspace_identity()
    lock_path = ROOT / "dependencies/environment.lock.json"
    lock = json.loads(lock_path.read_text())
    minimum = tuple(int(part) for part in lock["python"]["minimum"].split("."))
    if sys.version_info[:len(minimum)] < minimum:
        raise ValueError("Python differs from the locked minimum "
                         + lock["python"]["minimum"])
    try:
        import tomllib
    except ImportError as error:
        raise ValueError("Python must provide stdlib tomllib") from error
    tomllib.loads("schema = 2")
    version = ".".join(map(str, sys.version_info[:3]))
    print(f"Python: {version} ({sys.executable}); stdlib tomllib available")
    for name in ("cmake", "ninja"):
        result = subprocess.run([executable(name), "--version"],
                                capture_output=True, text=True, check=True,
                                timeout=30)
        expected = lock["tools"][name]
        observed = re.search(r"\d+(?:\.\d+)+", result.stdout.splitlines()[0])
        if observed is None or observed.group() != expected:
            raise ValueError(f"{name} differs from locked {expected}")
        print(result.stdout.splitlines()[0])
    required = {
        "tools": (), "native": ("gcc", "g++"),
        "arm": ("arm-none-eabi-gcc",),
        "candidate": ("gcc", "g++", "arm-none-eabi-gcc"),
    }[args.target]
    for name in ("gcc", "g++", "arm-none-eabi-gcc"):
        compiler = shutil.which(name, path=environment["PATH"])
        if not compiler:
            if name in required:
                raise ValueError(f"Missing {name} required for {args.target}")
            print(f"{name}: unavailable; its target cannot be configured")
            continue
        result = subprocess.run([compiler, "-dumpfullversion"],
                                capture_output=True, text=True, check=True,
                                timeout=30)
        actual = result.stdout.strip()
        expected = lock["arm_compiler" if name.startswith("arm-") else
                        "native_compiler"]["version"]
        if name in required and actual != expected:
            raise ValueError(f"{name} differs from locked {expected}: {actual}")
        print(f"{name}: {actual} ({compiler})")
    if args.target == "candidate":
        docker = executable("docker")
        observed = subprocess.run([docker, "image", "inspect", "--format",
                                   "{{.Id}}",
                                   lock["qualified_oci"]["config_digest"]],
                                  capture_output=True, text=True, check=True,
                                  timeout=30)
        if observed.stdout.strip() != lock["qualified_oci"]["config_digest"]:
            raise ValueError("Candidate OCI differs from locked image identity")
        print("Candidate environment is available; qualification requires "
              "toolchain-tree and source identities in the execution plan")
    if identity is not None:
        print("Verified source SDK: " + identity["source_revision"])
        print("Tool diagnosis only: repository review/documentation "
              "governance materials are not packaged")
        return 0
    return maintenance_checks(environment)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("configure", "build", "test",
                                          "check", "doctor", "prepare",
                                          "qualify", "candidate", "cache", "docs"))
    parser.add_argument("--preset", default="native-debug")
    args, native = parser.parse_known_args(argv)
    try:
        environment = process.execution_environment(os.environ)
        # Prefer the pinned local tools without requiring shell activation.
        local_bin = ROOT / ".venv" / ("Scripts" if os.name == "nt" else "bin")
        environment["PATH"] = (str(local_bin) + os.pathsep
                               + environment.get("PATH", ""))
        if args.action == "docs":
            return execute([sys.executable, "tools/documentation/cli.py"] + native,
                           environment)
        if args.action == "doctor":
            return doctor(environment, native)
        if args.action in ("prepare", "qualify", "candidate", "cache"):
            modules = {"prepare": "dependencies", "qualify": "qualification",
                       "candidate": "qualification", "cache": "cache"}
            command = [sys.executable,
                       "tools/dev/" + modules[args.action] + ".py"]
            if args.action == "prepare":
                command.append("prepare")
                command += ["--preset", args.preset]
                if not any(arg == "--scope" or arg.startswith("--scope=")
                           for arg in native):
                    command += ["--scope", "preset"]
            if args.action == "candidate":
                command.append("--require-candidate")
            return execute(command + native, environment)
        if args.action == "check":
            if native:
                raise ValueError("check accepts no native CMake arguments")
            if workspace_identity() is not None:
                raise ValueError("check requires a development checkout with "
                                 "repository tests and governance materials")
            result = execute([sys.executable, "scripts/ci/style_gate.py",
                              "--all", "--report",
                              "build/quality/style-gate.json"], environment)
            if result:
                return result
            result = maintenance_checks(environment)
            if result:
                return result
            return execute([sys.executable, "scripts/ci/tdd_gate.py", "--all",
                            "--preset", args.preset], environment)
        tool = "ctest" if args.action == "test" else "cmake"
        command = [executable(tool)]
        if args.action == "build":
            command += ["--build"]
        command += ["--preset", args.preset]
        if args.action == "test":
            command += ["--output-on-failure", "--no-tests=error"]
        if args.action == "test":
            build = binary_directory(ROOT, args.preset, kind="test",
                                     environment=environment)
            selected_test = Presets(ROOT, environment).selected(
                "test", args.preset)
            report = build / "ctest-results.xml"
            sys.path.insert(0, str(ROOT))
            from scripts.validation.junit import prepare_report, validate_junit
            started = prepare_report(report)
            result = execute(command + ["--output-junit", str(report)] + native,
                             environment)
            if result:
                return result
            evidence = validate_junit(report, not_before_ns=started)
            filters = ("-R", "-E", "-I", "-L", "-LE", "--tests-regex",
                       "--exclude-regex", "--tests-information",
                       "--label-regex",
                       "--label-exclude", "--rerun-failed", "--tests-from-file",
                       "--exclude-from-file")
            focused = bool(selected_test.get("filter")) or any(
                arg.split("=", 1)[0] in filters
                or any(arg.startswith(prefix) and arg != prefix
                       for prefix in ("-R", "-E", "-I", "-L"))
                for arg in native)
            google_selection = {
                key: environment[key] for key in
                ("GTEST_FILTER", "GTEST_TOTAL_SHARDS", "GTEST_SHARD_INDEX")
                if key in environment
            }
            focused |= bool(google_selection)
            scope = "focused-development" if focused else "preset-development"
            report.with_suffix(".execution.json").write_text(json.dumps({
                "schema": 1, "kind": scope, "preset": args.preset,
                "native_arguments": native, "junit": str(report),
                "google_selection": google_selection,
                "execution": evidence, "qualification": False,
            }, indent=2) + "\n")
            print(f"Validated fresh {scope} execution: "
                  f"{evidence['passed']} test cases; full contract "
                  "qualification "
                  "uses check or a qualification plan")
            return 0
        result = execute(command + native, environment)
        if result == 0:
            capture_provenance(args.action, args.preset, environment, native,
                               getattr(result, "receipt_path", None),
                               command + native)
        return result
    except (OSError, ValueError, subprocess.CalledProcessError,
            subprocess.TimeoutExpired) as error:
        print(f"Nexus developer command rejected: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
