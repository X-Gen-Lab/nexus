#!/usr/bin/env python3
"""Run real tool tests and reject empty, incomplete or skipped execution."""

import argparse
import json
import os
from pathlib import Path
import sys
from time import perf_counter
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
SUITES = {"testing": "tools/testing", "configure": "tools/configure/tests",
          "ci": "scripts/ci", "workflows": ".github/workflows"}
# Full CI additionally owns standalone SDK/build contracts and the kernel overlay.
CI_SUITES = {**SUITES, "contracts": "tests/contracts",
             "freertos": "os/freertos"}
SCOPE_POLICY = {"tools/testing/run_tool_tests.py", "tools/dev/change_paths.py",
                "scripts/ci/tdd_gate.py"}
SDK_TOOLS = {"cmake/package/package_source_sdk.py",
             "cmake/package/verify_consumers.py"}
SDK_FIXTURE_SUFFIXES = {".c", ".h", ".toml", ".json"}
FIRMWARE_ROOTS = ("core/", "arch/", "os/", "io/", "soc/", "boards/",
                  "components/", "tests/")
POLICY_TESTS = ("tools/testing/test_tool_runner.py",
                "tools/testing/test_tdd_gate.py",
                "tools/testing/test_tdd_preflight.py",
                "tools/testing/test_staged_execution.py")
INSTALLER_TESTS = ("scripts/ci/test_install_dev_tools.py",
                   "scripts/ci/test_cmake_format.py")
CMAKE_TESTS = ("tools/testing/test_cmake_helpers.py",
               "tools/testing/test_io_source_selection.py",
               "tools/configure/tests/test_incremental_consumer.py")
FIRMWARE_RESOURCE_TESTS = ("tests/contracts/test_build_artifacts.py",
                           "tests/contracts/test_measurement_resources.py")
FACT_TESTS = ("tools/testing/test_build_impact.py",
              "tools/configure/tests/test_resource_graph.py")
DEVELOPMENT_INPUTS = {
    "dependencies/environment.lock.json", "dependencies/environment-tools.txt",
    "dependencies/development-tools.txt",
    "dependencies/python-tools-linux-cp311.lock.json",
    "dependencies/python-tools-linux-cp311.txt",
    "dependencies/python-tools-linux-cp312.lock.json",
    "dependencies/python-tools-linux-cp312.txt",
}
NAMESPACE_TESTS = {
    "tools/configure": "tools/configure/tests",
    "tools/dev": "tools/testing", "tools/testing": "tools/testing",
    "tools/maintenance": "tools/testing", "tools/evidence": "tools/testing",
    "tools/measurement": "tools/testing", "tools/hil": "tools/testing",
    "scripts/ci": "scripts/ci", "scripts/tools": "scripts/ci",
    "scripts/setup": "scripts/ci",
}
COMMIT_ALIASES = {
    **dict.fromkeys(SCOPE_POLICY, POLICY_TESTS),
    "AGENTS.md": POLICY_TESTS + INSTALLER_TESTS,
    ".pre-commit-config.yaml": POLICY_TESTS + INSTALLER_TESTS,
    ".gersemirc": INSTALLER_TESTS,
    "scripts/ci/check_commit_message.py": ("scripts/ci/test_commit_message.py",),
    ".editorconfig": ("scripts/ci/test_format_tool.py",
                       "scripts/ci/test_cmake_format.py"),
    ".github/security/trufflehog.json": ("tools/testing/test_secret_scan.py",),
    "docs/development/coding-standards.md": (
        "scripts/ci/test_comment_style.py", "scripts/ci/test_cmake_format.py"),
    "tools/dev/dev.py": ("tools/testing/test_dev.py",
                         "tools/testing/test_dev_efficiency.py",
                         "tools/testing/test_build_workflow.py"),
    "tools/dev/presets.py": ("tools/testing/test_build_workflow.py",
                             "tools/testing/test_tdd_gate.py",
                             "tools/testing/test_sdk_verification_call.py"),
    "tools/dev/process.py": ("tools/testing/test_build_workflow.py",
                             "tools/testing/test_process_resources.py",
                             "tools/testing/test_module_workers.py"),
    "tools/testing/run_tool_tests.py": POLICY_TESTS + ("tools/testing/test_module_workers.py",),
    "cmake/package/verify_consumers.py": ("tools/testing/test_sdk_verification_call.py",),
    "tools/dev/timings.py": ("tools/testing/test_dev_timings.py",),
    "tools/dev/projects.py": ("tools/testing/test_dev_projects.py",),
    **dict.fromkeys(("tools/measurement/resources.py", "tools/measurement/build_resources.py"),
                    FIRMWARE_RESOURCE_TESTS),
    "tools/measurement/elf.py": FIRMWARE_RESOURCE_TESTS + ("tests/contracts/test_image_artifacts.py",),
    "tools/testing/arm_links.py": ("tools/testing/test_affected_arm_links.py",
                                  "tools/testing/test_tdd_gate.py"),
    "scripts/ci/reference_matrix.py": (".github/workflows/test_platform_ci.py",),
    "tools/measurement/compare.py": ("tests/contracts/test_measurement_compare.py",),
    "tools/maintenance/capabilities.py": ("tools/testing/test_maintenance.py",),
    "tests/support/external_provider/provider/nexus-soc.json": (
        "tools/configure/tests/test_external_package.py",),
    "cmake/package/package_source_sdk.py": (
        "tools/testing/test_sdk_profile_policy.py",
        "tools/testing/test_sdk_export_ownership.py"),
    "tools/configure/coupled_assembly.py": (
        "tools/configure/tests/test_power_stage_domains.py",
        "tools/configure/tests/test_power_stage.py",
        "tools/configure/tests/test_bindings.py",
        "tools/configure/tests/test_controller_integration.py",
        "tools/configure/tests/test_device_graph_stage.py",
        "tools/configure/tests/test_static_ir.py",
        "tools/configure/tests/test_static_calls.py",
        "tools/configure/tests/test_platform_lifecycle.py",
        "tools/configure/tests/test_factory_includes.py",
        "tools/configure/tests/test_factory_features.py",
        "tools/configure/tests/test_ir_unification.py",
        "tools/configure/tests/test_runtime.py",
        "tools/configure/tests/test_header_projection.py"),
}


def changed_suites(names):
    """Conservatively select affected complete tool domains, including deletes."""
    from tools.dev.change_paths import behavioral
    selected = set()
    for name in names:
        if not behavioral(name):
            continue
        if name in SCOPE_POLICY:
            return tuple(SUITES)
        if (name in SDK_TOOLS or
                (name.startswith("cmake/package/verification/")
                 and Path(name).suffix in SDK_FIXTURE_SUFFIXES)):
            # Both domains import canonical SDK consumers. Build rules and
            # unknown fixture kinds retain the broad CMake policy below.
            selected.update(("testing", "configure"))
        elif name.startswith((".github/", "scripts/ci/")):
            selected.update(("ci", "workflows", "testing"))
        elif name.startswith("tools/configure/"):
            selected.update(("configure", "testing"))
        elif (name.startswith(("cmake/", "dependencies/", "scripts/setup/"))
              or name in {"CMakeLists.txt", "CMakePresets.json", "AGENTS.md",
                          ".pre-commit-config.yaml"}):
            return tuple(SUITES)
        elif name.startswith(FIRMWARE_ROOTS):
            suffix = Path(name).suffix.lower()
            if suffix in {".c", ".s", ".cpp", ".cc"}:
                # Native Google contracts always execute. MCU compilation uses
                # the explicit ARM fixtures, not an inferred path matrix.
                continue
            if suffix in {".h", ".hpp", ".inc", ".json", ".toml"}:
                selected.update(("testing", "configure"))
            elif name.startswith("tests/") and suffix == ".py":
                selected.add("testing")
            else:
                return tuple(SUITES)
        elif name.startswith("tools/"):
            selected.add("testing")
        elif behavioral(name):
            return tuple(SUITES)
    return tuple(suite for suite in SUITES if suite in selected)


def module_domain(name):
    path = Path(name)
    if (path.name.startswith("test_") and path.suffix == ".py"
            and path.stem.isidentifier()):
        return next((suite for suite, directory in CI_SUITES.items()
                     if path.parent == Path(directory)), None)
    return None


def commit_tool_plan(names):
    """Select complete modules; absent coverage keeps its full-domain fallback."""
    from tools.dev.change_paths import behavioral
    plan = {}

    def add(files):
        if any(module_domain(name) is None or not (ROOT / name).is_file()
               for name in files):
            return False
        for name in files:
            suite = module_domain(name)
            if suite not in plan:
                plan[suite] = set()
            if plan[suite] is not None:
                plan[suite].add(name)
        return True

    def fallback(name, files=()):
        domains = set(changed_suites([name]))
        domains.update(domain for path in files
                       if (domain := module_domain(path)) is not None)
        for suite in domains:
            plan[suite] = None

    for name in names:
        if not behavioral(name):
            continue
        files = COMMIT_ALIASES.get(name)
        if not (ROOT / name).is_file():
            domain = module_domain(name)
            if domain is not None:
                plan[domain] = None
            else:
                fallback(name, files or ())
            continue
        domain = module_domain(name)
        if files is None and name.startswith((
                "components/devices/tmp117/src/", "cmake/package/verification/device_tmp117/",
                "cmake/package/verification/image_window/")):
            files = ("tools/testing/test_sdk_device_image_policy.py",)
        if files is None and domain is not None:
            if add((name,)):
                continue
            plan[domain] = None
            continue
        if files is None and name in DEVELOPMENT_INPUTS:
            files = INSTALLER_TESTS
        cmake_input = (Path(name).name == "CMakeLists.txt" or
                       Path(name).suffix == ".cmake" or name.endswith(".cmake.in"))
        if files is None and ((cmake_input and
                name.startswith(("cmake/", *FIRMWARE_ROOTS))) or
                name in {"CMakeLists.txt", "CMakePresets.json", "CMakeUserPresets.json"}
                or name.startswith("cmake/presets/")):
            files = CMAKE_TESTS
        if files is None and name.startswith(FIRMWARE_ROOTS):
            suffix = Path(name).suffix.lower()
            if suffix in {".h", ".hpp", ".inc"} or (
                    suffix in {".json", ".toml"} and
                    name.startswith(("soc/", "boards/", "tests/support/native/"))):
                files = FACT_TESTS
                if name.startswith("boards/"):
                    from tools.configure.tests import BOARD_FACT_CONTRACTS
                    files += tuple(f"{SUITES['configure']}/{module}"
                                   for module, inputs in BOARD_FACT_CONTRACTS.items()
                                   if name in inputs)
        if (files is None and name.startswith(".github/workflows/")
                and Path(name).suffix in {".yml", ".yaml"}):
            files = (".github/workflows/test_platform_ci.py",
                     "scripts/ci/test_lint_workflows.py",
                     "scripts/ci/test_action_pins.py")
        directory = NAMESPACE_TESTS.get(str(Path(name).parent))
        if files is None and directory is not None and Path(name).suffix == ".py":
            candidate = str(Path(directory) / ("test_" + Path(name).stem + ".py"))
            files = (candidate,) if (ROOT / candidate).is_file() else ()
        if files and add(files):
            continue
        fallback(name, files or ())
    return {suite: None if plan[suite] is None else tuple(sorted(plan[suite]))
            for suite in CI_SUITES if suite in plan}


class TimedResult(unittest.TextTestResult):
    """Retain ten observed case durations in the existing unittest result."""

    def __init__(self, *arguments, record_cases=False, **keywords):
        super().__init__(*arguments, **keywords)
        self.slowest_tests = []
        self.case_ids = [] if record_cases else None

    def startTest(self, test):
        if self.case_ids is not None:
            self.case_ids.append(test.id())
        super().startTest(test)
        self._started = perf_counter()

    def stopTest(self, test):
        self.slowest_tests.append({
            "test": str(test), "duration_seconds": perf_counter() - self._started})
        self.slowest_tests.sort(key=lambda item: item["duration_seconds"],
                                reverse=True)
        del self.slowest_tests[10:]
        super().stopTest(test)


def execute(suite, files=None):
    """Return actual runner counts; loading or skipping tests is not a pass."""
    if suite not in CI_SUITES:
        raise ValueError("Unknown maintained tool scope")
    return execute_directory(ROOT / CI_SUITES[suite], suite, files=files)


def execute_directory(directory, suite, *, files=None, record_cases=False,
                      allow_empty_module=False):
    """Use the same admission for documentation's separate locked environment."""
    loader = unittest.TestLoader()
    if files is None:
        tests = loader.discover(str(directory), pattern="test_*.py")
    else:
        selected = sorted(set(files))
        if not selected or any(module_domain(name) != suite or
                (ROOT / name).is_symlink() or not (ROOT / name).is_file()
                for name in selected):
            raise ValueError("Select existing test modules from one tool domain")
        tests = unittest.TestSuite(loader.discover(
            str(directory), pattern=Path(name).name) for name in selected)
    expected = tests.countTestCases()
    if expected == 0 and not allow_empty_module:
        raise ValueError("Tool suite discovers zero tests")
    def result_class(*args, **kwargs):
        return TimedResult(*args, record_cases=record_cases, **kwargs)

    result = unittest.TextTestRunner(verbosity=1, failfast=True,
                                     resultclass=result_class).run(tests)
    record = {"suite": suite, "discovered": expected,
              "executed": result.testsRun, "skipped": len(result.skipped),
              "failures": len(result.failures), "errors": len(result.errors),
              "expected_failures": len(result.expectedFailures),
              "unexpected_successes": len(result.unexpectedSuccesses),
              "slowest_tests": result.slowest_tests,
              "scope": "full-tool-domain" if files is None else "commit-tools",
              "qualification": False}
    if files is not None:
        record["files"] = selected
    if (not result.wasSuccessful() or result.testsRun != expected
            or result.skipped or result.expectedFailures):
        raise ValueError("Tool suite did not completely pass: " + json.dumps(record))
    if record_cases:
        record["case_ids"] = result.case_ids
    return {**record, "status": "passed"}


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=tuple(CI_SUITES), required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--files", nargs="+",
                        help="run complete named modules as commit-tools")
    parser.add_argument("--jobs", type=int, default=os.getenv("NEXUS_TOOL_JOBS"),
                        help="maximum isolated module workers within the build budget")
    args = parser.parse_args(arguments)
    args.report.unlink(missing_ok=True)
    try:
        from tools.dev.process import parallel_jobs
        from tools.testing.module_workers import run
        jobs = parallel_jobs(os.environ, requested=args.jobs)
        record = run(args.suite, args.files, jobs)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(record, indent=2) + "\n")
    except (ValueError, OSError) as error:
        print(f"Tool test execution rejected: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
