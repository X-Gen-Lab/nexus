"""Link explicit software fixtures through the maintained platform CMake graph.

The authored TOML and its single resolver own selection. This gate neither
rewrites configuration nor registers providers, target names or hardware facts.
Actual ELF budgets and selected face/table/method bytes are software evidence;
physical operation, electrical behavior and timing remain unexecuted.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
try:
    from . import arch_compile
except ImportError:
    import arch_compile
from tools.configure.configure import resolve_ir
from tools.evidence.common import (EvidenceError, atomic_json, load_json,
                                   regular_file)
from tools.evidence.identity import (git_source, resolved_identity,
                                     resolved_input_files)
from tools.measurement import resources, retention
from tools.measurement.elf import Elf32


def source_path(root, value):
    """Reject lexical aliases before resolving an explicit owned input."""
    path = Path(value)
    if ".." in path.parts:
        raise EvidenceError("source path cannot contain parent aliases")
    return path if path.is_absolute() else root / path



def owned_file(path, *, nonempty=True):
    """Keep authored selections and owned project files free of aliases."""
    path = regular_file(path, nonempty=nonempty)
    if path.stat().st_nlink != 1:
        raise EvidenceError("owned input hardlinks are not accepted")
    return path

def validate_fixtures(root, assemblies, *, family=None, project=None):
    """Resolve each explicit, owned firmware fixture once, without mutation."""
    root = root.resolve()
    if not assemblies:
        raise EvidenceError("provider fixture matrix must be nonempty")
    selected_project = None
    if project is not None:
        selected_project = source_path(root, project)
        if not selected_project.is_relative_to(root / "tests/contracts"):
            raise EvidenceError("project must be an owned tests/contracts directory")
        owned_file(selected_project / "CMakeLists.txt")
    rows, seen = [], set()
    for value in assemblies:
        path = owned_file(source_path(root, value))
        contract = path.is_relative_to(root / "tests/contracts")
        canonical = path.parent == root / "tools/configure/assemblies"
        if path.suffix != ".toml" or not (
                contract or (selected_project is not None and canonical)):
            raise EvidenceError("fixture must be an owned tests/contracts TOML "
                                "or explicit canonical input")
        if path in seen:
            raise EvidenceError("duplicate fixture cannot count as another execution")
        seen.add(path)
        actual_project = selected_project or path.parent
        owned_file(actual_project / "CMakeLists.txt")
        if selected_project is not None:
            authored = tomllib.loads(path.read_text(encoding="utf-8"))
            if type(authored.get("schema")) is not int or authored["schema"] != 2:
                raise EvidenceError("explicit project requires assembly schema 2")
        navigation = {}
        resolved = resolve_ir(path, root, input_paths=navigation).to_dict()
        if resolved["cpu"]["arch"] == "native" or resolved["backend"] == "native":
            raise EvidenceError("host model is not an ARM assembly fixture")
        if family is not None and resolved["soc_family"] != family:
            raise EvidenceError("fixture family differs from requested family")
        inputs = [arch_compile.digest(Path(file)) for file in navigation.values()]
        rows.append({"assembly": str(path), "project": str(actual_project),
                     "resolved": resolved, "inputs": inputs})
    return rows


def fresh_output(path, root):
    path = Path(path).absolute()
    if ".." in path.parts:
        raise EvidenceError("output path cannot contain parent aliases")
    if any(item.is_symlink() for item in [path, *path.parents]):
        raise EvidenceError("output path must not be a symlink")
    root = root.resolve()
    if path.is_relative_to(root) and not path.is_relative_to(root / "build"):
        raise EvidenceError("assembly output must not modify source directories")
    if path.exists():
        raise EvidenceError("assembly output must be fresh and absent")
    path.mkdir(parents=True)
    return path


def verify_inputs(identities):
    for identity in identities:
        if arch_compile.digest(Path(identity["path"])) != identity:
            raise EvidenceError("assembly input changed: " + identity["path"])


def require_configuration(expected, actual):
    if actual.get("soc_family") != expected["soc_family"]:
        raise EvidenceError("built family differs from authored fixture")
    if actual.get("configuration_sha256") != expected["configuration_sha256"]:
        raise EvidenceError("built configuration differs from authored fixture")


def require_budget(report):
    if report.get("status") != "passed" or report.get("violations") != []:
        raise EvidenceError("actual ELF resource budget failed")


class Recorder:
    """Keep actual status and raw streams, including failed or timed-out tools."""

    def __init__(self, output, root, *, timeout=300, env=None):
        self.output, self.root = Path(output), Path(root)
        self.output.mkdir(parents=True, exist_ok=True)
        self.timeout = timeout
        self.commands = []
        self.env = dict(os.environ if env is None else env)
        for name in list(self.env):
            if name.startswith(("GTEST_", "CTEST_", "GIT_")):
                del self.env[name]

    def execute(self, argv, label):
        if not re.fullmatch(r"[a-zA-Z0-9_.-]+", label):
            raise EvidenceError("invalid command log label")
        prefix = self.output / f"{len(self.commands):03d}-{label}"
        stdout, stderr = prefix.with_suffix(".stdout"), prefix.with_suffix(".stderr")
        record = {"argv": argv, "cwd": str(self.root), "returncode": None,
                  "started_at": datetime.now(timezone.utc).isoformat()}
        self.commands.append(record)
        error = None
        with stdout.open("xb") as out, stderr.open("xb") as err:
            try:
                process = subprocess.Popen(argv, cwd=self.root, env=self.env,
                    stdin=subprocess.DEVNULL, stdout=out, stderr=err,
                    start_new_session=os.name == "posix")
                try:
                    record["returncode"] = process.wait(timeout=self.timeout)
                except subprocess.TimeoutExpired:
                    if os.name == "posix":
                        os.killpg(process.pid, signal.SIGKILL)
                    else:
                        process.kill()
                    record["returncode"] = process.wait()
                    error = "command timed out"
                if os.name == "posix":
                    try:
                        os.killpg(process.pid, 0)
                    except ProcessLookupError:
                        pass
                    else:
                        os.killpg(process.pid, signal.SIGKILL)
                        error = error or "command left background artifact writers"
            except OSError as exception:
                error = "command execution failed: " + str(exception)
        record["ended_at"] = datetime.now(timezone.utc).isoformat()
        record["stdout"] = arch_compile.digest(stdout)
        record["stderr"] = arch_compile.digest(stderr)
        if error:
            record["error"] = error
            raise EvidenceError(error)
        if record["returncode"] != 0:
            raise EvidenceError(f"command failed with exit {record['returncode']}")
        return stdout.read_text(encoding="utf-8", errors="strict")


def dependency_paths(text, build):
    """Read the actual Ninja compiler dependency log, including vendor headers."""
    files, objects = set(), 0
    for line in text.splitlines():
        if not line.strip():
            continue
        if not line.startswith("    "):
            if not re.fullmatch(r".+: #deps [0-9]+, deps mtime [0-9]+ \(VALID\)", line):
                raise EvidenceError("stale or malformed actual compiler dependencies")
            objects += 1
        else:
            if objects == 0:
                raise EvidenceError("compiler dependencies have no object identity")
            path = Path(line.strip())
            files.add(regular_file(path if path.is_absolute() else build / path,
                                   nonempty=False))
    if not objects or not files:
        raise EvidenceError("actual compiler dependencies missing")
    return sorted(files)


def empty_retention(elf, bindings, resolved, *, root=ROOT):
    """Prove an explicit empty selection from actual ELF, never a skipped pass."""
    if (resolved["controllers"] or resolved["devices"] or
            retention.FACE_DECLARATION.search(bindings)):
        raise EvidenceError("empty retention has selected provider declarations")
    _, _, providers, public = retention.source_contracts(root)
    symbols = {item["name"] for item in elf.defined_symbols}
    if symbols & providers.keys() or any(name.startswith((
            "s_nx_face_", "s_nx_device_face_")) for name in symbols):
        raise EvidenceError("unselected concrete provider or face survived GC")
    return {"schema_version": 1, "kind": "empty_provider_gc",
            "status": "passed", "face_count": 0, "operations_table_count": 0,
            "method_count": 0, "public_contracts": public,
            "physical_status": "not_executed"}


def one(paths, label):
    selected = list(paths)
    if len(selected) != 1:
        raise EvidenceError(f"expected one actual {label}, found {len(selected)}")
    return regular_file(selected[0])


def inspect(build, fixture, compiler, *, root=ROOT):
    """Recompute budget and retention, binding the real generated bundle/ELF."""
    elf_path = one((build / "bin").glob("*.elf"), "firmware ELF")
    resolved_path = one(build.glob("nexus-sdk/generated/resolved.json"), "resolved bundle")
    resolved, _ = resolved_identity(resolved_path)
    require_configuration(fixture["resolved"], resolved)
    inputs = resolved_input_files(resolved_path, resolved)
    generated = resolved_path.parent
    marker = regular_file(generated / ".nexus-config-bundle")
    if marker.read_text().strip() != resolved["configuration_sha256"]:
        raise EvidenceError("generated bundle completion marker is stale")
    budget_path = generated / "resource-budget.json"
    measured = resources.measure(elf_path, resolved_path, budget_path,
                                 require_cpu=True)
    require_budget(measured)
    original = load_json(elf_path.with_suffix(".resources.json"))
    if original != measured:
        raise EvidenceError("post-link budget report is stale or differs from actual ELF")
    abi = resources.resolved_float_abi(resolved, require_cpu=True)
    elf = Elf32(elf_path.read_bytes(), float_abi=abi)
    bindings = regular_file(generated / "bindings.c")
    source = bindings.read_text()
    if retention.FACE_DECLARATION.search(source):
        original_retention = load_json(build / "bin/provider-retention.json")
        if abi == "soft" or "resolved" in original_retention:
            retained = retention.inspect_files(
                elf_path, bindings, resolved_path=resolved_path, root=root)
        else:
            retained = retention.inspect_provider_retention(elf, source, root=root)
            retained["elf"] = retention.file_identity(elf_path)
            retained["bindings"] = retention.file_identity(bindings)
        if original_retention != retained:
            raise EvidenceError("post-link retention report is stale")
    else:
        retained = empty_retention(elf, source, resolved, root=root)
        retained["elf"] = retention.file_identity(elf_path)
        retained["bindings"] = retention.file_identity(bindings)
    audit = build / "assembly-audit"
    audit.mkdir()
    atomic_json(audit / "resources.json", measured)
    atomic_json(audit / "retention.json", retained)
    database = regular_file(build / "compile_commands.json")
    entries = json.loads(database.read_text())
    if not isinstance(entries, list) or not entries:
        raise EvidenceError("actual compile database is empty")
    compile_sources = []
    for entry in entries:
        argv = entry.get("arguments") or shlex.split(entry["command"])
        if Path(argv[0]).resolve() != compiler.resolve():
            raise EvidenceError("compile graph used a different actual compiler")
        path = Path(entry["file"])
        if not path.is_absolute():
            path = Path(entry["directory"]) / path
        compile_sources.append(arch_compile.digest(
            regular_file(path, nonempty=False)))
    row = {"status": "passed", "family": resolved["soc_family"],
           "part": resolved["part"], "backend": resolved["backend"],
           "configuration_sha256": resolved["configuration_sha256"],
           "configuration_inputs": inputs, "compile_sources": compile_sources,
           "flash_loaded_bytes": measured["metrics"]["flash"]["loaded_bytes"],
           "ram_reserved_bytes": {name: facts["reserved_bytes"] for name, facts in
                                  measured["metrics"]["ram"].items()},
           "face_count": retained["face_count"],
           "method_count": retained["method_count"], "artifacts": {}}
    for label, path in {"elf": elf_path, "bin": elf_path.with_suffix(".bin"),
            "map": elf_path.with_suffix(".map"), "resolved": resolved_path,
            "budget": budget_path, "bindings": bindings, "bundle_marker": marker,
            "compile_commands": database, "resources": audit / "resources.json",
            "retention": audit / "retention.json"}.items():
        row["artifacts"][label] = arch_compile.digest(regular_file(path))
    return row


def source_inputs(root, fixtures):
    """Snapshot maintained graph sources and selected locked SDK input bytes."""
    paths = {root / "CMakeLists.txt", Path(__file__).resolve(),
             Path(arch_compile.__file__).resolve()}
    directories = {root / name for name in (
        "arch", "core", "io", "os", "soc", "boards", "components", "cmake",
        "tools/configure", "tools/measurement", "tools/evidence", "dependencies")}
    projects = set()
    for fixture in fixtures:
        directories.add(Path(fixture["assembly"]).parent)
        projects.add(Path(fixture.get("project",
                                      Path(fixture["assembly"]).parent)))
        directories.update(root / item["path"] for item in
                           fixture["resolved"]["source_dependencies"])
        paths.update(Path(item["path"]) for item in fixture["inputs"])
    for directory in directories:
        paths.update(path for path in directory.rglob("*") if path.is_file()
            and (path.suffix in {".c", ".h", ".S", ".s", ".cmake", ".py", ".json", ".toml", ".ld"}
                 or path.name == "CMakeLists.txt") and ".git" not in path.parts)
    # An owned CMake project may consume templates or extensionless inputs.
    # Snapshot its complete file tree rather than infer a second source graph.
    for project in sorted(projects):
        for path in project.rglob("*"):
            if ".git" in path.parts:
                continue
            if path.is_symlink():
                raise EvidenceError("owned project symlinks are not accepted")
            if path.is_file():
                paths.add(owned_file(path, nonempty=False))
    # Empty C/header inputs are valid, including locked vendor placeholders.
    # Keep their zero-byte identities in the closure; artifacts stay nonempty.
    return [arch_compile.digest(regular_file(path, nonempty=False))
            for path in sorted(paths)]


def verify_snapshot(root, fixtures, original):
    """Refuse added, removed or changed graph inputs, including dirty sources."""
    if source_inputs(root, fixtures) != original:
        raise EvidenceError("maintained source input snapshot changed during matrix")


def run(root, assemblies, compiler, output, *, family=None, parallel=2,
        project=None):
    root = root.resolve()
    output = fresh_output(output, root)
    report = {"schema_version": 1, "kind": "selected_provider_assembly_matrix",
              "status": "failed", "scope": "actual_platform_cmake_arm_elf",
              "physical_status": "not_executed", "cycles_status": "not_measured",
              "fixtures": []}
    recorder = Recorder(output / "commands", root)
    report["commands"] = recorder.commands
    try:
        if root != ROOT.resolve():
            raise EvidenceError("selected source root differs from actual tool source")
        if any(name.startswith("GIT_") for name in os.environ):
            raise EvidenceError("Git environment overrides cannot qualify source identity")
        if type(parallel) is not int or not 1 <= parallel <= 16:
            raise EvidenceError("parallel count must be 1..16")
        fixtures = validate_fixtures(root, assemblies, family=family,
                                     project=project)
        report["source_before"] = git_source(root)
        report["source_inputs"] = source_inputs(root, fixtures)
        tools = arch_compile.tools(compiler)
        for name in ("cmake", "ninja"):
            tools[name] = shutil.which(name)
            if not tools[name]:
                raise EvidenceError("missing actual build tool: " + name)
        tools["python"] = sys.executable
        report["tools"] = {name: arch_compile.digest(Path(path).resolve())
                           for name, path in tools.items()}
        recorder.env["PATH"] = str(Path(tools["gcc"]).parent) + os.pathsep + recorder.env["PATH"]
        for name, tool in tools.items():
            recorder.execute([tool, "--version"], name + "-version")
        for index, fixture in enumerate(fixtures):
            path = Path(fixture["assembly"])
            build = output / f"{index:03d}-{path.parent.name}-{path.stem}"
            recorder.execute([tools["cmake"], "-S", fixture["project"], "-B", str(build),
                "-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=" + tools["ninja"],
                "-DCMAKE_TOOLCHAIN_FILE=" + str(root / "cmake/toolchains/arm-gcc.cmake"),
                "-DPython3_EXECUTABLE=" + tools["python"],
                "-DNEXUS_SOURCE_ROOT=" + str(root),
                "-DNEXUS_ASSEMBLY_FILE=" + str(path)], f"fixture-{index}-configure")
            recorder.execute([tools["cmake"], "--build", str(build), "--parallel", str(parallel)],
                             f"fixture-{index}-build")
            dependencies = recorder.execute([tools["ninja"], "-C", str(build), "-t", "deps"],
                                            f"fixture-{index}-dependencies")
            row = inspect(build, fixture, Path(tools["gcc"]), root=root)
            row["assembly"] = arch_compile.digest(path)
            row["project"] = arch_compile.digest(
                Path(fixture["project"]) / "CMakeLists.txt")
            row["compiler_dependencies"] = [arch_compile.digest(path) for path in
                                            dependency_paths(dependencies, build)]
            report["fixtures"].append(row)
            verify_inputs(report["source_inputs"])
            atomic_json(output / "report.json", report)
        report["source_after"] = git_source(root)
        if report["source_after"] != report["source_before"]:
            raise EvidenceError("Git source identity changed during matrix")
        verify_snapshot(root, fixtures, report["source_inputs"])
        verify_inputs(list(report["tools"].values()))
        for row in report["fixtures"]:
            verify_inputs(row["compiler_dependencies"])
            verify_inputs(list(row["artifacts"].values()))
        report["status"] = "passed"
        report["source_inputs_stable"] = True
        report["tools_stable"] = True
        report["fixture_count"] = len(report["fixtures"])
    except (OSError, ValueError, KeyError, TypeError, UnicodeError) as error:
        report["error"] = str(error)
    atomic_json(output / "report.json", report)
    return 0 if report["status"] == "passed" else 1


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--fixture", type=Path, action="append", required=True)
    parser.add_argument("--project", type=Path, action="append")
    parser.add_argument("--family")
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--parallel", type=int, default=2)
    args = parser.parse_args(arguments)
    if args.project is not None and len(args.project) != 1:
        parser.error("project must be supplied exactly once")
    try:
        return run(args.root, args.fixture, args.compiler, args.output,
                   family=args.family, parallel=args.parallel,
                   project=args.project[0] if args.project else None)
    except (OSError, ValueError) as error:
        print("Provider assembly gate rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
