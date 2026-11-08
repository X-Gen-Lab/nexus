#!/usr/bin/env python3
"""Bind a bounded reviewed I/O catalogue to existing Google execution evidence.

This read-only consumer adds no test runner or production instrumentation.
Source assertion anchors are review navigation, not automatic semantic proof.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.ci.tdd_gate import checked_binary, validate_google_report

MAX_CONTEXTS = 50000
MAX_METADATA_BYTES = 32 * 1024 * 1024

# Reviewed register models compile these shared engines once. Concrete SoC
# wrappers and all other hardware TUs still require the property target itself.
# These are evidence-owner constraints, never a source selection/build graph.
SHARED_ENGINE_OWNERS = {
    "soc/stm32/stm32f4/drivers/ip/classic_can/can_controller.c": "nexus_can_native_engine",
    "soc/stm32/stm32f4/drivers/ip/fs_device_pio/usb_device_controller.c": "nexus_usb_native_engine",
    "soc/stm32/stm32f4/drivers/ip/legacy_mac/eth_mac.c": "nexus_eth_native_engine",
}


def digest(path):
    """Hash existing regular bytes; do not traverse an evidence symlink."""
    path = Path(path)
    if not path.is_file() or any(p.is_symlink() for p in (path, *path.parents)):
        raise ValueError(f"Missing or symlink input: {path}")
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path):
    """Malformed or missing evidence never becomes an empty successful report."""
    digest(path)
    return json.loads(Path(path).read_text())


def owned(root, name):
    """Catalogue inputs stay in the checkout and cannot borrow external bytes."""
    path = root / name
    digest(path)
    if not path.resolve().is_relative_to(root.resolve()):
        raise ValueError("Catalogue source is outside the checkout")
    return path


def uncomment(text):
    """Remove comments while preserving literals and their escaped quotes."""
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*', re.S)
    return tokens.sub(lambda m: " " if m.group().startswith(("/*", "//")) else m.group(), text)


def test_body(text, identity):
    """Locate one ordinary TEST/TEST_F body, including nested assertion scopes."""
    suite, case = identity.split(".", 1)
    text = uncomment(text)
    expression = r'\bTEST(?:_F)?\s*\(\s*' + re.escape(suite) + r'\s*,\s*' + re.escape(case) + r'\s*\)\s*\{'
    matches = list(re.finditer(expression, text))
    if len(matches) != 1:
        raise ValueError("Reviewed case must have one ordinary test body")
    start = matches[0].end()
    depth = 1
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]')
    for token in tokens.finditer(text, start):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return re.sub(r"\s+", "", text[start:token.start()])
    raise ValueError("Reviewed test body is incomplete")


def anchor_present(body, anchor):
    """A quoted assertion spelling is not executable assertion navigation."""
    anchor = re.sub(r"\s+", "", anchor)
    literals = [m.span() for m in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', body)]
    position = body.find(anchor)
    while position != -1:
        if not any(start <= position < end for start, end in literals):
            return True
        position = body.find(anchor, position + 1)
    return False


def catalogue_inputs(root, catalogue):
    """Require explicit reviewed predicates and invariant limits for each row."""
    data = read_json(catalogue)
    if data.get("schema") != 1 or not data.get("scope") or not data.get("properties"):
        raise ValueError("Missing bounded catalogue")
    identities = set()
    sources = set()
    compiled = set()
    required = {"id", "family", "peripheral", "mode", "target", "case",
                "test_source", "production_sources", "model_sources",
                "assertions", "calls", "invariant", "limitations"}
    for row in data["properties"]:
        if set(row) != required or row["id"] in identities:
            raise ValueError("Unknown fields or duplicate property identity")
        identities.add(row["id"])
        if not all(row[k] for k in required - {"model_sources"}):
            raise ValueError("Empty reviewed property field")
        if not row["target"].startswith("nexus_") or "/" in row["target"]:
            raise ValueError("Catalogue target must be a Google executable name")
        body = test_body(owned(root, row["test_source"]).read_text(), row["case"])
        if len(row["assertions"]) < 2:
            raise ValueError("A name alone cannot establish a property")
        for assertion in row["assertions"]:
            if not assertion.lstrip().startswith(("EXPECT_", "ASSERT_")):
                raise ValueError("Catalogue predicate must navigate a real assertion")
        for anchor in row["assertions"] + row["calls"]:
            if not anchor_present(body, anchor):
                raise ValueError("Reviewed predicate or production call is absent from this case")
        files = [row["test_source"], *row["production_sources"], *row["model_sources"]]
        for name in files:
            owned(root, name)
        sources.update(files)
        compiled.update([row["test_source"], *row["production_sources"]])
    if not data.get("unproven") or any(not r.get("scope") or not r.get("reason") for r in data["unproven"]):
        raise ValueError("Catalogue must state unproven scopes explicitly")
    sources.update(data.get("contract_sources", []))
    for name in data.get("contract_sources", []):
        owned(root, name)
    return data, sources, compiled


def build_inputs(root, build, catalogue, *, complete=False):
    """Observe actual compiled contexts, objects and executable identities."""
    data, sources, compiled = catalogue_inputs(root, catalogue)
    identities = {}

    def observe_digest(path):
        name = str(Path(path).absolute())
        if name not in identities:
            identities[name] = digest(path)
        return identities[name]

    plan_path = build / "google-contracts.json"
    plan = read_json(plan_path)
    names = plan.get("executables", [])
    if plan.get("schema") != 1 or not names or len(set(names)) != len(names):
        raise ValueError("Missing actual Google target plan")
    binaries = {Path(n).name: Path(n) for n in names}
    if len(binaries) != len(names):
        raise ValueError("Duplicate Google executable basename")
    targets = {row["target"] for row in data["properties"]}
    if not targets.issubset(binaries):
        raise ValueError("Catalogue target is not in the actual CMake graph: " +
                         ", ".join(sorted(targets - binaries.keys())))
    binary_ids = {}
    for name in sorted(binaries):
        path = checked_binary(binaries[name], build)
        binary_ids[name] = {"path": str(path), "sha256": observe_digest(path)}
    database_path = build / "compile_commands.json"
    entries = read_json(database_path)
    if not isinstance(entries, list) or len(entries) > MAX_CONTEXTS:
        raise ValueError("Compiler context input exceeds its bound")
    contexts = {name: [] for name in compiled}
    target_contexts = {name: [] for name in sorted(targets)}
    all_sources = {}
    all_objects = {}
    complete_contexts = {name: [] for name in sorted(binaries)}
    for entry in entries:
        directory = Path(entry["directory"])
        source = Path(entry["file"])
        source = source if source.is_absolute() else directory / source
        command = entry.get("arguments") or shlex.split(entry["command"])
        if "-o" not in command or command.index("-o") + 1 >= len(command):
            if source.is_relative_to(root) and str(source.relative_to(root)) in contexts:
                raise ValueError("Reviewed compiled context has no actual output")
            continue
        output = Path(command[command.index("-o") + 1])
        output = output if output.is_absolute() else directory / output
        if output.is_file():
            canonical_source, canonical_output = source.resolve(), output.resolve()
            if (not canonical_output.is_relative_to(build.resolve()) or
                    not (canonical_source.is_relative_to(root.resolve()) or
                         canonical_source.is_relative_to(build.resolve()))):
                raise ValueError("Materialized compiled context escapes checkout/build")
            if command.count("-c") != 1 or command.index("-c") + 1 >= len(command):
                raise ValueError("Materialized context lacks one compile source")
            selected = Path(command[command.index("-c") + 1])
            selected = selected if selected.is_absolute() else directory / selected
            if selected.resolve() != canonical_source:
                raise ValueError("Compiled source and compile command disagree")
            source_name, object_name = str(source), str(output)
            all_sources[source_name] = observe_digest(source)
            context = {"source": source_name, "directory": str(directory),
                       "command": command, "sha256": observe_digest(output)}
            if object_name in all_objects:
                raise ValueError("Duplicate materialized object context")
            all_objects[object_name] = context
            owners = [name for name in complete_contexts if
                      "/CMakeFiles/" + name + ".dir/" in output.as_posix()]
            if len(owners) > 1:
                raise ValueError("Ambiguous complete target context")
            if owners:
                complete_contexts[owners[0]].append(object_name)
        local = [name for name in target_contexts if
                 "/CMakeFiles/" + name + ".dir/" in output.as_posix()]
        if local:
            if len(local) != 1:
                raise ValueError("Ambiguous target-local compiled context")
            target = local[0]
            canonical_source, canonical_output = source.resolve(), output.resolve()
            if not (canonical_source.is_relative_to(root.resolve()) or
                    canonical_source.is_relative_to(build.resolve())):
                raise ValueError("Target-local source is outside checkout/build")
            marker = "/CMakeFiles/" + target + ".dir/"
            if (not canonical_output.is_relative_to(build.resolve()) or
                    marker not in canonical_output.as_posix()):
                raise ValueError("Target-local object escapes its build/target")
            target_contexts[target].append({
                "source": str(source), "source_sha256": observe_digest(source),
                "directory": str(directory), "command": command,
                "object": str(output), "object_sha256": observe_digest(output),
            })
        if not source.is_relative_to(root):
            continue
        name = str(source.relative_to(root))
        if name not in contexts:
            continue
        if not output.resolve().is_relative_to(build.resolve()):
            raise ValueError("Compiled object is outside selected build")
        if output.is_file():
            contexts[name].append({"directory": str(directory),
                                   "command": command, "object": str(output),
                                   "object_sha256": observe_digest(output)})
    if any(not values for values in contexts.values()):
        raise ValueError("A reviewed production/test TU has no compiled object")
    if any(not values for values in complete_contexts.values()):
        raise ValueError("A planned Google target has no materialized context")
    for row in data["properties"]:
        for source in [row["test_source"], *row["production_sources"]]:
            source_name = (root / source).resolve().relative_to(
                root.resolve()).as_posix()
            if (source == row["test_source"] or
                    source_name.startswith(("soc/stm32/stm32f4/",
                                            "tests/support/native/drivers/"))):
                # Native models and explicitly reviewed engines may be shared;
                # concrete providers and test bodies retain target-local builds.
                owner = ("nexus_io_native" if source != row["test_source"] and
                         source_name.startswith("tests/support/native/drivers/") else row["target"])
                owners = {owner}
                if source != row["test_source"] and source_name in SHARED_ENGINE_OWNERS:
                    owners.add(SHARED_ENGINE_OWNERS[source_name])
                if not any("/CMakeFiles/" + name + ".dir/" in
                           Path(item["object"]).resolve().as_posix()
                           for name in owners for item in contexts[source]):
                    raise ValueError(
                        "Reviewed test/provider compiled only for another target: "
                        + row["id"] + ": " + source_name + " requires "
                        + ", ".join(sorted(owners)))
    result = {"catalogue_sha256": observe_digest(catalogue),
              "plan_sha256": observe_digest(plan_path),
              "compile_commands_sha256": observe_digest(database_path),
              "sources": {name: observe_digest(root / name)
                          for name in sorted(sources)},
              "compiled": contexts, "target_compiled": target_contexts,
              "complete_sources": all_sources, "objects": all_objects,
              "target_contexts": complete_contexts,
              "binaries": binary_ids}
    if complete:
        result["compiler_inputs"] = compiler_inputs(root, build, all_objects)
    if any(digest(name) != value for name, value in identities.items()):
        raise ValueError("Compiled inputs changed during observation")
    return result


def ninja_tool(build):
    """Use the selected build's tool, never guess a host Ninja executable."""
    cache = build / "CMakeCache.txt"
    digest(cache)
    text = cache.read_text()
    generator = re.findall(r"^CMAKE_GENERATOR:INTERNAL=(.+)$", text, re.M)
    programs = re.findall(r"^CMAKE_MAKE_PROGRAM:FILEPATH=(.+)$", text, re.M)
    if generator != ["Ninja"] or len(programs) != 1:
        raise ValueError("Complete observation requires the Native Ninja build")
    tool = Path(programs[0]).resolve()
    digest(tool)
    return str(tool)


def metadata_commands(build):
    """Query the existing aggregate once; no per-binary link graph is built."""
    prefix = [ninja_tool(build), "-C", str(build), "-t"]
    return {
        "input-graph": prefix + ["graph", "nexus_google_contracts"],
        "input-dependencies": prefix + ["deps"],
    }


def compiler_inputs(root, build, objects):
    """Deduplicate actual graph/dependency facts into the existing snapshot.

    This conservatively binds materialized compiler contexts and shared files;
    it is not another target/link resolver or a compilation execution proof.
    """
    from tools.dev.qualification import completed_command_inputs

    files = {}
    archives = set()
    headers = set()

    def observe(path):
        path = Path(path).resolve()
        name = str(path)
        if name in files:
            return path
        identity = {"sha256": digest(path), "size": path.stat().st_size}
        files[name] = identity
        if len(files) > MAX_CONTEXTS:
            raise ValueError("Compiler file observation exceeds its bound")
        return path

    for name in ("CMakeCache.txt", "build.ninja", "CMakeFiles/rules.ninja",
                 ".ninja_deps"):
        observe(build / name)
    observe(Path(ninja_tool(build)))
    logs = {}
    for stem, argv in metadata_commands(build).items():
        log = build / "tdd" / (stem + ".txt")
        if log.stat().st_size > MAX_METADATA_BYTES:
            raise ValueError("Compiler metadata output exceeds its bound")
        selected = completed_command_inputs(log, expected_argv=argv)
        receipt = read_json(log.with_suffix(".command.json"))
        if receipt.get("cwd") != str(root):
            raise ValueError("Compiler observation used a different source")
        for path in selected:
            observe(path)
        raw = log.read_bytes()
        if (len(raw) > MAX_METADATA_BYTES or
                hashlib.sha256(raw).hexdigest() !=
                files[str(log.resolve())]["sha256"]):
            raise ValueError("Compiler metadata bytes changed during reading")
        logs[stem] = raw.decode("utf-8")
        if completed_command_inputs(log, expected_argv=argv) != selected:
            raise ValueError("Compiler metadata original closure changed")

    graph = logs["input-graph"].strip()
    if not graph.startswith("digraph ninja {\n") or not graph.endswith("}"):
        raise ValueError("Ninja graph observation is missing or malformed")
    node = re.compile(r'^"[^"\n]+" \[label=("(?:\\.|[^"\\])*")')
    nodes = 0
    for line in graph.splitlines():
        match = node.match(line)
        if match is None:
            continue
        nodes += 1
        if nodes > MAX_CONTEXTS:
            raise ValueError("Ninja graph observation exceeds its bound")
        name = json.loads(match[1])
        path = Path(name)
        path = path if path.is_absolute() else build / path
        if path.is_file():
            if (path.suffix in {".a", ".lib", ".o", ".obj"} and
                    any(part.is_symlink() for part in (path, *path.parents))):
                raise ValueError("Ninja linked input must be materialized, "
                                 "not a cached symlink")
            observed = observe(path)
            if path.suffix in {".a", ".lib", ".so"}:
                archives.add(str(observed))
        elif path.suffix in {".a", ".lib", ".o", ".obj"}:
            raise ValueError("Ninja linked input is not materialized")
    if not nodes:
        raise ValueError("Ninja graph contains no observed nodes")

    expected = {str(Path(name).resolve()) for name in objects}
    dependencies = {}
    current = None
    declared = 0
    lines = logs["input-dependencies"].splitlines()
    pattern = re.compile(
        r"(.+): #deps ([0-9]+), deps mtime ([0-9]+) \((VALID|STALE)\)")

    def finish():
        if current is not None and len(dependencies[current]) != declared:
            raise ValueError("Compiler dependency count differs from output")

    for line in lines:
        if not line.strip():
            continue
        if not line.startswith("    "):
            finish()
            match = pattern.fullmatch(line)
            if match is None:
                raise ValueError("Compiler dependencies are malformed")
            path = Path(match[1])
            path = path if path.is_absolute() else build / path
            name = str(path.resolve())
            current = name if name in expected else None
            if current is not None:
                if (match[4] != "VALID" or current in dependencies or
                        int(match[3]) != path.stat().st_mtime_ns):
                    raise ValueError("Compiler dependencies are stale")
                declared = int(match[2])
                dependencies[current] = []
                observe(path)
        elif current is not None:
            path = Path(line.strip())
            path = path if path.is_absolute() else build / path
            observed = observe(path)
            dependencies[current].append(str(observed))
            headers.add(str(observed))
    finish()
    if set(dependencies) != expected or any(not row for row in dependencies.values()):
        raise ValueError("A materialized object lacks compiler dependencies")
    for name, identity in files.items():
        if (digest(name) != identity["sha256"] or
                Path(name).stat().st_size != identity["size"]):
            raise ValueError("Compiler inputs changed during observation")
    return {"scope": "current_ninja_materialized_context_superset",
            "files": files, "archives": sorted(archives),
            "headers": sorted(headers)}


def capture_complete(root, build, catalogue, output, execute, environment):
    """Observe one actual Ninja build before the existing producer lists cases."""
    root, build = Path(root).absolute(), Path(build).absolute()
    for stem, argv in metadata_commands(build).items():
        text = execute(argv, environment, build / "tdd" / (stem + ".txt"))
        if len(text.encode()) > MAX_METADATA_BYTES:
            raise ValueError("Compiler metadata output exceeds its bound")
    return capture(root, build, catalogue, output, complete=True)


def verify_snapshot(root, build, catalogue, snapshot):
    """Recheck current bytes without running a compiler, Ninja or test process."""
    before = read_json(snapshot)
    if (before.get("schema") != 1 or
            before.get("kind") != "io-property-preexecution-inputs"):
        raise ValueError("Missing preexecution compiled/source identity")
    current = build_inputs(
        Path(root).absolute(), Path(build).absolute(), catalogue,
        complete="compiler_inputs" in before)
    if any(before.get(key) != value for key, value in current.items()):
        raise ValueError("Source, binary or compiled inputs changed")
    return before


def capture(root, build, catalogue, output, *, complete=False):
    """Capture after real build and before executing the existing test gate."""
    root, build = Path(root).absolute(), Path(build).absolute()
    binding = build_inputs(root, build, catalogue, complete=complete)
    report_directory = build / "tdd"
    report_directory.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=report_directory, prefix=".io-matrix-") as anchor:
        anchor.write(b"before execution\n")
        anchor.flush()
        binding["not_before_ns"] = Path(anchor.name).stat().st_mtime_ns
    binding.update({"schema": 1, "kind": "io-property-preexecution-inputs"})
    Path(output).parent.mkdir(parents=True, exist_ok=True)
    Path(output).write_text(json.dumps(binding, indent=2) + "\n")
    return binding


def discovery_names(text):
    """Framework discovery identities must match execution, not merely count."""
    suite = None
    cases = set()
    for line in text.splitlines():
        token = line.split("#", 1)[0].rstrip()
        if token and not token[0].isspace() and token.endswith("."):
            suite = token[:-1]
        elif suite is not None and token.startswith("  ") and token.strip():
            identity = suite + "." + token.strip()
            if identity in cases:
                raise ValueError("Duplicate discovered Google case")
            cases.add(identity)
    if not cases:
        raise ValueError("Google discovery is empty")
    return cases


def audit(root, build, catalogue, snapshot, execution, *, allow_subset=False):
    """Consume existing gate outputs; never execute tests or infer HIL results."""
    root, build, execution = map(lambda p: Path(p).absolute(), (root, build, execution))
    before = verify_snapshot(root, build, catalogue, snapshot)
    current = before
    summary = read_json(execution / "execution.json")
    if summary.get("schema") != 1 or summary.get("kind") != "development-contract-execution" or not summary.get("suites"):
        raise ValueError("Missing actual existing Google execution summary")
    names = [r["binary"] for r in summary["suites"]]
    plan = read_json(build / "google-contracts.json")
    full_names = [Path(p).name for p in plan["executables"]]
    if len(names) != len(set(names)) or (not allow_subset and names != full_names):
        raise ValueError("Execution differs from the actual complete Google plan")
    reviewed_targets = {row["target"] for row in read_json(catalogue)["properties"]}
    if not reviewed_targets.issubset(names):
        raise ValueError("A reviewed target was not executed")
    passed = {}
    for index, suite in enumerate(summary["suites"]):
        target = suite["binary"]
        if target not in current["binaries"]:
            continue
        binary = current["binaries"][target]["path"]
        xml = execution / f"google-{index}.xml"
        expected = {"discovery": [binary, "--gtest_filter=*", "--gtest_list_tests"],
                    "execution": [binary, "--gtest_filter=*", "--gtest_repeat=1",
                                  "--gtest_also_run_disabled_tests", "--gtest_output=xml:" + str(xml)]}
        for kind, command in expected.items():
            actual = read_json(execution / f"{kind}-{index}.command.json")
            if actual.get("exit_code") != 0 or actual.get("command") != command or actual.get("cwd") != str(root):
                raise ValueError("Google subprocess failed, was filtered or used different inputs")
        discovered = discovery_names((execution / f"discovery-{index}.txt").read_text())
        result = validate_google_report(xml, not_before_ns=before["not_before_ns"], expected_cases=len(discovered))
        executed = {case["classname"] + "." + case["name"] for case in result["cases"]}
        if executed != discovered or suite.get("report_sha256") != result["report_sha256"] or suite.get("passed") != result["passed"] or suite.get("cases") != result["cases"]:
            raise ValueError("Summary/discovery/XML case identities differ")
        passed[target] = executed
    data = read_json(catalogue)
    properties = []
    for row in data["properties"]:
        if row["case"] not in passed[row["target"]]:
            raise ValueError("Reviewed property case did not execute")
        properties.append({**row, "status": "executed-bounded",
                           "compiled_inputs": {p: current["compiled"][p] for p in [row["test_source"], *row["production_sources"]]}})
    return {"schema": 1, "kind": "bounded-io-property-execution",
            "execution_scope": "development-subset" if allow_subset else "complete-existing-gate",
            "hardware_status": "not_executed", "formal_proof": False,
            "properties": properties, "unproven": data["unproven"],
            "inputs": before, "execution_summary_sha256": digest(execution / "execution.json"),
            "limits": "Reviewed assertion anchors plus completed actual model execution; not semantic/formal proof, exhaustive interleavings, physical timing or electrical qualification."}


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("capture", "audit"))
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--catalogue", type=Path, default=ROOT / "tools/testing/io_lifecycle_matrix.json")
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--execution", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--allow-subset", action="store_true")
    args = parser.parse_args(arguments)
    try:
        if args.operation == "capture":
            capture(args.root, args.build, args.catalogue, args.snapshot)
        else:
            if args.execution is None or args.report is None:
                raise ValueError("Audit requires actual execution and report paths")
            args.report.unlink(missing_ok=True)
            report = audit(args.root, args.build, args.catalogue, args.snapshot, args.execution, allow_subset=args.allow_subset)
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2) + "\n")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"I/O property evidence rejected: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
