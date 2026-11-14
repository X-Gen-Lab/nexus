"""Run complete Native modes in independent, explicitly budgeted build trees."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack
import json
import math
import os
from pathlib import Path
import signal
import sys
import threading
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.dev import process
from tools.dev.build_lock import build_lock, child_settings
from tools.dev.presets import Presets
from tools.evidence.common import (atomic_json, file_identity, parse_json,
                                   regular_file, sha256_value)

PRESETS = ("native-debug", "native-release", "native-asan", "native-tsan")


def capacity():
    """Respect process affinity, Linux cgroup limits and available memory."""
    cpus = len(os.sched_getaffinity(0)) if hasattr(
        os, "sched_getaffinity") else os.cpu_count() or 1
    memory = None
    if sys.platform.startswith("linux"):
        try:
            quota, period = Path("/sys/fs/cgroup/cpu.max").read_text().split()
            if quota != "max":
                cpus = min(cpus, max(1, math.floor(int(quota) / int(period))))
        except (OSError, ValueError):
            pass
        try:
            fields = dict(line.split(":", 1) for line in
                          Path("/proc/meminfo").read_text().splitlines())
            memory = int(fields["MemAvailable"].split()[0]) // 1024
        except (OSError, ValueError, KeyError):
            pass
        try:
            limit = Path("/sys/fs/cgroup/memory.max").read_text().strip()
            if limit != "max":
                used = int(Path("/sys/fs/cgroup/memory.current").read_text())
                remaining = max(0, (int(limit) - used) // (1024 * 1024))
                memory = remaining if memory is None else min(memory, remaining)
        except (OSError, ValueError):
            pass
    return cpus, memory


def selected_builds(presets, env):
    """CMake remains the graph; require three preset kinds to share each tree."""
    if len(presets) != len(set(presets)):
        raise ValueError("Native matrix presets must be unique")
    metadata = Presets(ROOT, env)
    builds = []
    for preset in presets:
        configure = metadata.selected("configure", preset)
        for kind in ("build", "test"):
            if metadata.selected(kind, preset)["configurePreset"] != preset:
                raise ValueError("Native matrix requires matching presets")
        build = Path(configure["binaryDir"])
        if (any(part.is_symlink() for part in (build, *build.parents)) or
                not build.resolve().is_relative_to(ROOT.resolve()) or
                build.resolve() == ROOT.resolve()):
            raise ValueError("Native build must be an unaliased checkout tree")
        builds.append(build.resolve())
    if any(left == right or left in right.parents or right in left.parents
           for index, left in enumerate(builds) for right in builds[index + 1:]):
        raise ValueError("Native modes require disjoint build trees")
    return builds


def snapshot_runtime(preset, build, destination):
    """Preserve this mode before another command can replace shared reports."""
    def copy_report(source, target, expected=None):
        contents = regular_file(source).read_bytes()
        with target.open("xb") as stream:
            stream.write(contents)
        identity = file_identity(target)
        if expected is not None and identity["sha256"] != expected:
            raise ValueError("Native runtime report changed: " + str(source))
        return identity

    runtime = copy_report(build / "tdd/execution.json",
                          destination / "contracts.json")
    document = parse_json(Path(runtime["path"]).read_bytes())
    if (document.get("kind") != "development-contract-execution" or
            document.get("preset") != preset):
        raise ValueError("Native runtime report differs from the selected mode")
    suites = document["suites"]
    if not isinstance(suites, list) or not suites:
        raise ValueError("Native runtime Google reports must be nonempty")
    reports = []
    names = set()
    google = destination / "google"
    google.mkdir()
    for suite in suites:
        name = suite["binary"]
        if (not isinstance(name, str) or name in {"", ".", ".."} or
                Path(name).name != name or "\\" in name or name in names):
            raise ValueError("Native runtime Google report name is not unique or local")
        names.add(name)
        filename = name + ".xml"
        reports.append({"binary": name, "report": copy_report(
            build / "tdd/google" / filename, google / filename,
            sha256_value(suite["report_sha256"]))})
    ctest = copy_report(build / "ctest-results.xml",
                        destination / "ctest-results.xml",
                        sha256_value(document["ctest"]["report_sha256"]))
    return {"runtime_report": runtime, "google_reports": reports,
            "ctest_report": ctest}


def execute_mode(preset, build, env, output, cancellation):
    """Configure/build once, then use existing complete runtime admission."""
    result = {"preset": preset, "status": "cancelled", "commands": []}
    commands = (
        ("configure", [sys.executable, "tools/dev/dev.py", "configure",
                       "--preset", preset]),
        ("build", [sys.executable, "tools/dev/dev.py", "build",
                   "--preset", preset]),
        ("test", [sys.executable, "scripts/ci/native_contracts.py",
                  "--preset", preset]),
    )
    for stage, command in commands:
        if cancellation.is_set():
            return result
        try:
            observed = process.run(command, cwd=ROOT, env=env,
                                   log=output / preset / (stage + ".txt"),
                                   stage=stage, cancel_event=cancellation)
        except (OSError, ValueError) as error:
            cancellation.set()
            result.update({"status": "failed", "failed_stage": stage,
                           "error": str(error)})
            return result
        result["commands"].append(str(observed.receipt_path))
        if observed.returncode:
            result.update({"status": "cancelled" if
                           observed.returncode == 130 else "failed",
                           "exit_code":
                           observed.returncode, "failed_stage": stage})
            cancellation.set()
            return result
    try:
        result.update(snapshot_runtime(preset, build, output / preset))
    except (OSError, ValueError, KeyError, TypeError) as error:
        cancellation.set()
        result.update({"status": "failed", "failed_stage": "snapshot",
                       "error": str(error)})
        return result
    result["status"] = "passed"
    return result


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--presets", nargs="+", choices=PRESETS,
                        default=list(PRESETS))
    parser.add_argument("--jobs", type=int)
    parser.add_argument("--parallel-modes", type=int, default=2)
    parser.add_argument("--memory-per-mode-mib", type=int, default=4096)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(arguments)
    cancellation = threading.Event()
    interrupted = threading.Event()
    previous = {}

    def interrupt(signum, frame):
        del signum, frame
        interrupted.set()
        cancellation.set()
    try:
        cpus, memory = capacity()
        jobs = process.positive_integer(
            cpus if args.jobs is None else args.jobs, "total jobs")
        modes = process.positive_integer(args.parallel_modes, "parallel modes")
        reserve = process.positive_integer(args.memory_per_mode_mib,
                                           "memory per mode")
        if jobs > cpus:
            raise ValueError("Total jobs exceed available CPU capacity")
        if memory is None:
            if modes > 1:
                raise ValueError("Unknown memory capacity: select one mode")
        else:
            if memory < reserve:
                raise ValueError("Insufficient available memory for one mode")
            modes = min(modes, memory // reserve)
        modes = min(modes, jobs, len(args.presets))
        env = process.execution_environment(os.environ, jobs=jobs // modes)
        # Sanitizer settings are explicit for every child. Each maintained
        # preset selects which runtime is linked; no test filters are added.
        env.update({"ASAN_OPTIONS": "detect_leaks=1:halt_on_error=1",
                    "UBSAN_OPTIONS": "print_stacktrace=1:halt_on_error=1",
                    "TSAN_OPTIONS": "halt_on_error=1"})
        builds = selected_builds(args.presets, env)
        output = args.output or (ROOT / "build/quality/native-matrix" /
                 (str(time.time_ns()) + "-" + uuid.uuid4().hex))
        if output.exists():
            raise ValueError("Native matrix output must be a fresh directory")
        if any(part.is_symlink() for part in (output, *output.parents)):
            raise ValueError("Native matrix output cannot contain a symlink")
        output = output.resolve()
        if any(output.resolve() == build or build in output.resolve().parents
               or output.resolve() in build.parents for build in builds):
            raise ValueError("Native matrix output must be outside build trees")
        with ExitStack() as locks:
            for build in sorted(builds):
                locks.enter_context(build_lock(build))
            env, _ = child_settings(env)
            output.mkdir(parents=True)
            preflight = process.run(
                [sys.executable, "tools/dev/dev.py", "doctor", "--target",
                 "native"], cwd=ROOT, env=env, log=output / "doctor.txt",
                stage="preflight")
            if preflight.returncode:
                print("Native tools rejected; see " + str(output),
                      file=sys.stderr)
                return preflight.returncode
            if threading.current_thread() is threading.main_thread():
                for signum in (signal.SIGINT, signal.SIGTERM):
                    previous[signum] = signal.getsignal(signum)
                    signal.signal(signum, interrupt)
            with ThreadPoolExecutor(max_workers=modes) as pool:
                futures = [pool.submit(execute_mode, preset, build, env, output,
                                       cancellation)
                           for preset, build in zip(args.presets, builds)]
                results = [future.result() for future in futures]
            passed = all(result["status"] == "passed" for result in results)
            status = ("cancelled" if interrupted.is_set() else
                      "passed" if passed else "failed")
            record = {"schema": 1, "status": status,
                      "parallel_modes": modes, "jobs_per_mode": jobs // modes,
                      "total_jobs": jobs, "available_cpus": cpus,
                      "available_memory_mib": memory,
                      "memory_per_mode_mib": reserve,
                      "preflight_receipt": str(preflight.receipt_path),
                      "modes": results,
                      "scope": "Fresh complete Native modes; tools execute "
                               "separately; not physical qualification"}
            atomic_json(output / "execution.json", record)
            print("Native matrix " + record["status"] + ": " + str(output))
            return 130 if interrupted.is_set() else 0 if passed else 1
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("Native matrix rejected: " + str(error), file=sys.stderr)
        return 2
    finally:
        cancellation.set()
        for signum, handler in previous.items():
            signal.signal(signum, handler)


if __name__ == "__main__":
    raise SystemExit(main())
