#!/usr/bin/env python3
"""One thin entry point that preserves native CMake/CTest command exit codes."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def executable(name):
    local = ROOT / ".venv" / ("Scripts" if os.name == "nt" else "bin") / name
    found = str(local) if local.is_file() else shutil.which(name)
    if not found:
        raise ValueError(f"Missing {name}: install dependencies/environment-tools.txt")
    return found


def execute(command, environment):
    print("+ " + " ".join(command), flush=True)
    return subprocess.call(command, cwd=ROOT, env=environment)


def maintenance_checks(environment):
    for script in ("capabilities.py", "ownership.py"):
        result = execute([sys.executable, "tools/maintenance/" + script],
                         environment)
        if result:
            return result
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("configure", "build", "test", "check", "doctor"))
    parser.add_argument("--preset", default="native-debug")
    args, native = parser.parse_known_args(argv)
    try:
        environment = os.environ.copy()
        # Prefer the pinned local tools without requiring shell activation.
        local_bin = ROOT / ".venv" / ("Scripts" if os.name == "nt" else "bin")
        environment["PATH"] = str(local_bin) + os.pathsep + environment.get("PATH", "")
        if args.action == "doctor":
            lock = json.loads((ROOT / "dependencies/environment.lock.json").read_text())
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
                                        capture_output=True, text=True, check=True)
                expected = lock["tools"][name]
                if expected not in result.stdout.splitlines()[0]:
                    raise ValueError(f"{name} differs from locked {expected}")
                print(result.stdout.splitlines()[0])
            for name in ("gcc", "g++", "arm-none-eabi-gcc"):
                compiler = shutil.which(name, path=environment["PATH"])
                if compiler:
                    result = subprocess.run([compiler, "-dumpfullversion"],
                                            capture_output=True, text=True, check=True)
                    print(f"{name}: {result.stdout.strip()} ({compiler})")
                else:
                    print(f"{name}: unavailable; its target cannot be configured")
            return maintenance_checks(environment)
        if args.action == "check":
            if native:
                raise ValueError("check accepts no native CMake arguments")
            result = execute([sys.executable, "scripts/ci/style_gate.py", "--all",
                              "--report", "build/quality/style-gate.json"], environment)
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
            presets = json.loads((ROOT / "CMakePresets.json").read_text())
            selected = next((item for item in presets["configurePresets"]
                             if item["name"] == args.preset), None)
            if selected is None:
                raise ValueError("Unknown configured test preset")
            build = Path(selected["binaryDir"].replace("${sourceDir}", str(ROOT)))
            report = build / "ctest-results.xml"
            report.unlink(missing_ok=True)
            started = time.time_ns()
            result = execute(command + ["--output-junit", str(report)] + native,
                             environment)
            if result:
                return result
            sys.path.insert(0, str(ROOT))
            from scripts.validation.junit import validate_junit
            evidence = validate_junit(report, not_before_ns=started)
            print(f"Validated fresh execution: {evidence['passed']} test cases")
            return 0
        return execute(command + native, environment)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Nexus developer command rejected: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
