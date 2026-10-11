#!/usr/bin/env python3
"""Execute Native CTest once with complete case-level Google evidence."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.ci.tdd_gate import environment, validate_google_report
from scripts.validation.junit import validate_junit


def execute(command, env, log):
    """Retain actual commands, output and exit codes for this CI execution."""
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


def discovered_cases(output):
    """Preserve framework identities, including parameter and disabled names."""
    suite = None
    cases = []
    for line in output.splitlines():
        text = line.split("#", 1)[0].rstrip()
        if text and not text[0].isspace() and text.endswith("."):
            suite = text[:-1]
        elif suite is not None and text.startswith("  ") and text.strip():
            cases.append((suite, text.strip()))
    if not cases or len(cases) != len(set(cases)):
        raise ValueError("GoogleTest discovery is empty or contains duplicates")
    return set(cases)


def selected_build(preset):
    """Use the maintained configure preset without accepting CTest filters."""
    presets = json.loads((ROOT / "CMakePresets.json").read_text())
    selected = next((item for item in presets["configurePresets"]
                     if item["name"] == preset), None)
    if selected is None:
        raise ValueError("Missing maintained Native configure preset")
    build = Path(selected["binaryDir"].replace("${sourceDir}", str(ROOT)))
    build = build.resolve()
    if not build.is_relative_to(ROOT.resolve()):
        raise ValueError("Native build directory is outside the checkout")
    return build


def google_binaries(build):
    """Require a nonempty unique executable plan inside this Native build."""
    manifest = json.loads((build / "google-contracts.json").read_text())
    names = manifest.get("executables")
    if (manifest.get("schema") != 1 or not isinstance(names, list) or not names
            or any(not isinstance(name, str) for name in names)
            or len(names) != len(set(names))):
        raise ValueError("Missing or duplicate GoogleTest executable plan")
    binaries = [Path(name).resolve() for name in names]
    if (len({binary.name for binary in binaries}) != len(binaries)
            or any(not binary.is_file() or not binary.is_relative_to(build)
                   for binary in binaries)):
        raise ValueError("GoogleTest binary is missing, ambiguous "
                         "or outside build")
    return binaries


def ctest_names(plan, binaries):
    """Bind each Google executable to exactly one unfiltered CTest command."""
    tests = plan.get("tests")
    if not isinstance(tests, list) or not tests:
        raise ValueError("CTest plan contains zero tests")
    names = [item.get("name") for item in tests]
    if (any(not isinstance(name, str) or not name for name in names)
            or len(names) != len(set(names))):
        raise ValueError("CTest plan names are missing or duplicated")
    for binary in binaries:
        registrations = [item for item in tests
                         if item.get("command") == [str(binary)]]
        if len(registrations) != 1:
            raise ValueError("GoogleTest binary is not registered exactly once "
                             "with an unfiltered CTest command: " + str(binary))
    return set(names)


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", required=True, choices=(
        "native-debug", "native-release", "native-asan", "native-tsan"))
    args = parser.parse_args(arguments)
    try:
        build = selected_build(args.preset)
        output = build / "tdd"
        output.mkdir(parents=True, exist_ok=True)
        summary = output / "execution.json"
        summary.unlink(missing_ok=True)
        google = output / "google"
        if google.is_symlink():
            raise ValueError("GoogleTest output directory must not "
                             "be a symlink")
        if google.exists():
            shutil.rmtree(google)
        google.mkdir()
        report = build / "ctest-results.xml"
        report.unlink(missing_ok=True)
        env = environment()
        ctest = shutil.which("ctest", path=env["PATH"])
        if not ctest:
            raise ValueError("Missing maintained CTest executable")
        binaries = google_binaries(build)
        plan = json.loads(execute(
            [ctest, "--test-dir", str(build), "--show-only=json-v1"], env,
            output / "ctest-plan.txt"))
        planned = ctest_names(plan, binaries)
        discovered = {}
        for index, binary in enumerate(binaries):
            listing = execute([str(binary), "--gtest_filter=*",
                               "--gtest_list_tests"], env,
                              output / f"discovery-{index}.txt")
            discovered[binary] = discovered_cases(listing)
        # CTest runs the complete plan, including the non-Google contracts.
        # GoogleTest's directory output names each report after its executable.
        env.update({"GTEST_OUTPUT": "xml:" + str(google) + "/",
                    "GTEST_REPEAT": "1", "GTEST_ALSO_RUN_DISABLED_TESTS": "1"})
        started = time.time_ns()
        execute([ctest, "--preset", args.preset, "--output-on-failure",
                 "--no-tests=error", "--parallel", "1", "--output-junit",
                 str(report)], env, output / "ctest-execution.txt")
        ctest_result = validate_junit(report, not_before_ns=started)
        executed = {case["name"] for case in ctest_result["cases"]}
        if (ctest_result["skipped"] or executed != planned
                or ctest_result["tests"] != len(planned)):
            raise ValueError("CTest execution differs from its complete plan")
        expected_reports = {binary.name + ".xml" for binary in binaries}
        if {path.name for path in google.iterdir()} != expected_reports:
            raise ValueError("GoogleTest reports differ from "
                             "the executable plan")
        results = []
        for binary in binaries:
            evidence = validate_google_report(
                google / (binary.name + ".xml"), not_before_ns=started,
                expected_cases=len(discovered[binary]))
            identities = {(case["classname"], case["name"])
                          for case in evidence["cases"]}
            if identities != discovered[binary]:
                raise ValueError("GoogleTest execution differs from discovered "
                                 "case identities: " + binary.name)
            results.append({"binary": binary.name, **evidence})
        summary.write_text(json.dumps({
            "schema": 1, "kind": "development-contract-execution",
            "preset": args.preset, "passed": sum(r["passed"] for r in results),
            "suites": results, "ctest": ctest_result,
            "scope": "Single complete CTest and case-level Google execution; "
                     "not historical TDD order or physical qualification",
        }, indent=2) + "\n")
        print(f"Executed {ctest_result['passed']} CTest entries and "
              f"{sum(r['passed'] for r in results)} GoogleTest cases once; "
              f"evidence: {summary}")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Native CI execution rejected: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
