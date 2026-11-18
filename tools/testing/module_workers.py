"""Execute independent unittest modules in isolated interpreter processes."""

import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import signal
import sys
import tempfile
import threading

from tools.dev import build_lock, process
from tools.testing import run_tool_tests as runner


def run(suite, files, jobs):
    requested = process.positive_integer(jobs, "module workers")
    directory = runner.ROOT / runner.CI_SUITES[suite]
    if files is None:
        # Discovery may delegate to nested packages and their load_tests hooks.
        # Let unittest perform that whole discovery when a domain gains one.
        if any(p.is_dir() and (p / "__init__.py").exists()
               for p in directory.iterdir()):
            return runner.execute(suite)
        selected = sorted(p.relative_to(runner.ROOT).as_posix()
                          for p in directory.glob("test_*.py"))
    else:
        selected = sorted(set(files))
    if not selected or any(runner.module_domain(name) != suite or
            (runner.ROOT / name).is_symlink() or
            not (runner.ROOT / name).is_file() for name in selected):
        raise ValueError("Select existing test modules from one tool domain")

    allocation = process.parallel_jobs(os.environ, default=jobs)
    count = min(requested, allocation, len(selected))
    env = process.execution_environment(os.environ, jobs=allocation // count)
    env["CTEST_PARALLEL_LEVEL"] = env["NEXUS_BUILD_JOBS"]
    env.pop("NEXUS_TOOL_JOBS", None)
    # Sibling processes must acquire their own build locks before writing.
    env.pop(build_lock.LEASES, None)
    cancel = threading.Event()
    interrupted = False
    previous = {}

    def interrupt(signum, frame):
        nonlocal interrupted
        interrupted = True
        cancel.set()

    def module(name):
        if cancel.is_set():
            return None
        path = scratch / (Path(name).stem + ".json")
        completed = process.run(
            [sys.executable, "-B", "-m", "tools.testing.module_workers",
             suite, name, str(path), "full" if files is None else "selected"],
            cwd=runner.ROOT, env=env,
            stage=name, cancel_event=cancel, reject_background=True)
        if completed.returncode:
            if completed.returncode == 130 and cancel.is_set():
                return None
            cancel.set()
            raise ValueError(f"{name} exited {completed.returncode}\n" +
                             completed.stdout + completed.stderr)
        return json.loads(path.read_text())

    with tempfile.TemporaryDirectory(prefix="nexus-tool-modules-") as temp:
        scratch = Path(temp)
        if threading.current_thread() is threading.main_thread():
            for sig in (signal.SIGINT, signal.SIGTERM):
                previous[sig] = signal.signal(sig, interrupt)
        try:
            # Every module executes complete discovery with its own process
            # state, including class fixtures and unittest fail-fast handling.
            with concurrent.futures.ThreadPoolExecutor(max_workers=count) as pool:
                futures = {pool.submit(module, name): i
                           for i, name in enumerate(selected)}
                records = [None] * len(selected)
                try:
                    for future in concurrent.futures.as_completed(futures):
                        records[futures[future]] = future.result()
                finally:
                    cancel.set()
        finally:
            for sig, handler in previous.items():
                signal.signal(sig, handler)
    if interrupted or any(record is None for record in records):
        raise ValueError("Tool module execution cancelled")
    summary = {field: sum(record[field] for record in records) for field in (
        "discovered", "executed", "skipped", "failures", "errors",
        "expected_failures", "unexpected_successes")}
    if not summary["discovered"]:
        raise ValueError("Tool suite discovers zero tests")
    slowest = sorted((item for record in records for item in
                      record["slowest_tests"]),
                     key=lambda item: item["duration_seconds"], reverse=True)
    summary.update(suite=suite, qualification=False, status="passed",
                   scope="full-tool-domain" if files is None else "commit-tools",
                   slowest_tests=slowest[:10], workers=count,
                   modules={name: r["case_set_sha256"]
                            for name, r in zip(selected, records)})
    if files is not None:
        summary["files"] = selected
    return summary


if __name__ == "__main__":
    scope, filename, output, discovery = sys.argv[1:]
    result = runner.execute_directory(
        runner.ROOT / runner.CI_SUITES[scope], scope, files=[filename],
        record_cases=True, allow_empty_module=discovery == "full")
    result["case_set_sha256"] = hashlib.sha256(
        json.dumps(sorted(result.pop("case_ids"))).encode()).hexdigest()
    Path(output).write_text(json.dumps(result))
