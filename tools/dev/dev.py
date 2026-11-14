#!/usr/bin/env python3
"""One thin entry point that preserves native CMake/CTest command exit codes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import uuid

if __name__ == "__main__":
    sys.dont_write_bytecode = True

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


def execute(command, environment, *, cancel_event=None):
    """Retain isolated output; publish one atomic latest-result navigation."""
    print("+ " + " ".join(command), flush=True)
    directory = (ROOT / "build/quality/developer-commands" /
                 (str(time.time_ns()) + "-" + uuid.uuid4().hex))
    log = directory / "command.txt"
    result = process.run(command, cwd=ROOT, env=environment,
                         log=log, stage="development",
                         cancel_event=cancel_event)
    # The public JSON is the only latest-result authority. Both its log and
    # receipt refer to immutable invocation paths, so parallel completions
    # cannot combine one command's output with another command's identity.
    record = json.loads(result.receipt_path.read_text())
    with log.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    record.update({"attempt_receipt": str(result.receipt_path),
                   "log": {"path": str(log), "bytes": log.stat().st_size,
                           "sha256": digest}})
    provenance.atomic_json(
        ROOT / "build/quality/developer-command.command.json", record)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    print("Command log: " + str(log), flush=True)
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
                        choices=("tools", "native", "arm", "contracts",
                                 "docs", "workflows"),
                        default="tools")
    args, remaining = parser.parse_known_args(native)
    if args.target == "workflows":
        return execute([sys.executable, "scripts/ci/lint_workflows.py",
                        "--doctor"] + remaining, environment)
    if args.target == "docs":
        return execute([sys.executable, "tools/documentation/cli.py", "doctor"]
                       + remaining, environment)
    if remaining:
        raise ValueError("Unknown doctor arguments: " + " ".join(remaining))
    if args.target == "contracts":
        from tools.configure import authored
        schema = ROOT / "tools/configure/assembly.schema.json"
        if json.loads(schema.read_text()) != authored.editor_schema():
            raise ValueError("Assembly editor schema is stale; regenerate "
                             "tools/configure/assembly.schema.json from "
                             "tools.configure.authored.editor_schema()")
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
        "contracts": ("gcc", "g++", "arm-none-eabi-gcc"),
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
                                          "cache", "docs",
                                          "create", "flash", "debug", "monitor",
                                          "timings", "artifacts", "support",
                                          "matrix"))
    parser.add_argument("--preset")
    args, native = parser.parse_known_args(argv)
    try:
        if args.action == "matrix":
            if args.preset is not None:
                raise ValueError("matrix selects --presets, not --preset")
            from tools.dev import native_matrix
            return native_matrix.main(native)
        if args.action == "support":
            if args.preset is not None:
                raise ValueError("support requires an exact --assembly; --preset is not evidence")
            from tools.maintenance import capabilities
            return capabilities.main(native, support_command=True)
        if args.action == "artifacts":
            if args.preset is not None:
                raise ValueError("artifacts requires actual --provenance; "
                                 "--preset does not identify a build")
            from tools.dev import artifacts
            return artifacts.main(native)
        args.preset = args.preset or "native-debug"
        environment = process.execution_environment(os.environ)
        # Prefer the pinned local tools without requiring shell activation.
        local_bin = ROOT / ".venv" / ("Scripts" if os.name == "nt" else "bin")
        environment["PATH"] = (str(local_bin) + os.pathsep
                               + environment.get("PATH", ""))
        if args.action == "create":
            from tools.dev import projects
            return projects.main(native)
        if args.action in ("flash", "debug", "monitor"):
            from tools.dev import device
            return device.main(args.action, native)
        if args.action == "timings":
            from tools.dev import timings
            return timings.main(native)
        if args.action == "docs":
            return execute([sys.executable, "tools/documentation/cli.py"] + native,
                           environment)
        if args.action == "doctor":
            return doctor(environment, native)
        if args.action in ("prepare", "cache"):
            modules = {"prepare": "dependencies", "cache": "cache"}
            command = [sys.executable,
                       "tools/dev/" + modules[args.action] + ".py"]
            if args.action == "prepare":
                command.append("prepare")
                command += ["--preset", args.preset]
                if not any(arg == "--scope" or arg.startswith("--scope=")
                           for arg in native):
                    command += ["--scope", "preset"]
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
            presets = Presets(ROOT, environment)
            selected_test = presets.selected("test", args.preset)
            build = Path(presets.selected(
                "configure", selected_test["configurePreset"])["binaryDir"])
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
                  "uses check")
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
