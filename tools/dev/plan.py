"""Preview existing qualification stages without running their checkers."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import sys

from tools.dev.presets import Presets
from tools.dev.qualification import CHECKERS, validate_plan
from tools.evidence.common import (command, file_identity, parse_json,
                                   separate_outputs)

NATIVE_PRESETS = frozenset(("native-debug", "native-release", "native-asan",
                            "native-tsan"))
MAX_OBSERVED_CASES = 100000


def _option(arguments, name, *, required=False):
    """Read one explicit selector without approximating checker execution."""
    selected = []
    for index, argument in enumerate(arguments):
        if argument == "--":
            break
        if argument == name:
            if (index + 1 == len(arguments)
                    or arguments[index + 1].startswith("-")):
                raise ValueError(name + " requires an explicit value")
            selected.append(arguments[index + 1])
        elif argument.startswith(name + "="):
            selected.append(argument.split("=", 1)[1])
    if len(selected) > 1 or (selected and not selected[0]):
        raise ValueError(name + " requires one unambiguous value")
    if required and not selected:
        raise ValueError(name + " is required for preview metadata")
    return selected[0] if selected else None


def _preset(presets, checker, arguments):
    """Resolve only selectors owned by known preset-consuming checkers."""
    kinds = {"configure": "configure", "build": "build",
             "native_contracts": "test", "tdd": "configure"}
    if checker not in kinds:
        return None
    name = _option(arguments, "--preset", required=checker != "tdd")
    if name is None:
        # This is the existing TDD checker's declared default.
        name = "native-debug"
    kind = kinds[checker]
    selected = presets.selected(kind, name)
    configure_name = (name if kind == "configure"
                      else selected["configurePreset"])
    configure = presets.selected("configure", configure_name)
    return {"kind": kind, "name": name, "configure": configure_name,
            "binary_directory": configure["binaryDir"],
            "filter": selected.get("filter") if kind == "test" else None}


def _retained_json(path, retain, *, max_bytes=4 * 1024 * 1024):
    """Parse the exact bounded bytes admitted by the retained-file identity."""
    identity = retain(path)
    if identity["size"] > max_bytes:
        raise ValueError("JSON observation exceeds its bounded input size")
    with Path(path).open("rb") as stream:
        raw = stream.read(max_bytes + 1)
    if (len(raw) != identity["size"] or len(raw) > max_bytes or
            hashlib.sha256(raw).hexdigest() != identity["sha256"]):
        raise ValueError("JSON observation bytes changed during reading")
    return identity, parse_json(raw)


def _observation(root, preset, build, snapshot, retain):
    """Read prior case-list evidence; never execute the binary or trust XML."""
    from scripts.ci import io_lifecycle_matrix as matrix, native_contracts
    from tools.dev.qualification import completed_command_inputs

    snapshot = (root / snapshot).absolute()
    if snapshot != build / "tdd/io-property-inputs.json":
        raise ValueError("observation does not match the selected build")
    identity, raw = _retained_json(
        snapshot, retain, max_bytes=32 * 1024 * 1024)
    if raw.get("compiler_inputs", {}).get("scope") != (
            "current_ninja_materialized_context_superset"):
        raise ValueError("observation lacks complete compiler input binding")
    before = matrix.verify_snapshot(
        root, build, root / "tools/testing/io_lifecycle_matrix.json", snapshot)
    if before != raw:
        raise ValueError("Verified snapshot differs from retained JSON bytes")
    if (type(before.get("not_before_ns")) is not int or
            before["not_before_ns"] <= 0):
        raise ValueError("observation has no pre-discovery time boundary")
    catalogue = root / "tools/testing/io_lifecycle_matrix.json"
    _, admitted_catalogue = _retained_json(catalogue, retain)
    data, _, _ = matrix.catalogue_inputs(root, catalogue)
    if data != admitted_catalogue:
        raise ValueError("Validated catalogue differs from retained JSON bytes")
    properties = {}
    for row in data["properties"]:
        properties.setdefault((row["target"], row["case"]), []).append({
            key: row[key] for key in ("id", "family", "peripheral", "mode",
                                     "invariant", "limitations")})
    references = {build / "google-contracts.json",
                  build / "compile_commands.json"}
    references.update(root / name for name in before["sources"])
    references.update(Path(name) for name in before["complete_sources"])
    references.update(Path(name) for name in before["objects"])
    references.update(Path(row["path"]) for row in before["binaries"].values())
    references.update(Path(name) for name in before["compiler_inputs"]["files"])
    cases = []
    for index, binary in enumerate(native_contracts.google_binaries(build)):
        log = build / "tdd" / ("discovery-" + str(index) + ".txt")
        if log.stat().st_size > matrix.MAX_METADATA_BYTES:
            raise ValueError("case-list observation exceeds its input bound")
        argv = [str(binary), "--gtest_filter=*", "--gtest_list_tests"]
        selected = completed_command_inputs(log, expected_argv=argv)
        references.update(selected)
        for path in selected:
            retain(path)
        listing_identity = retain(log)
        listing = log.read_bytes()
        if (len(listing) > matrix.MAX_METADATA_BYTES or
                hashlib.sha256(listing).hexdigest() !=
                listing_identity["sha256"]):
            raise ValueError("case-list bytes changed during observation")
        _, receipt = _retained_json(log.with_suffix(".command.json"), retain)
        if (receipt.get("cwd") != str(root) or
                type(receipt.get("started_ns")) is not int or
                receipt["started_ns"] < before["not_before_ns"]):
            raise ValueError("case-list receipt is stale or from another root")
        for suite, name in sorted(native_contracts.discovered_cases(
                listing.decode("utf-8"))):
            reviewed = properties.get((binary.name, suite + "." + name), [])
            cases.append({"target": binary.name, "suite": suite, "name": name,
                          "catalogue_status": "reviewed" if reviewed else "unknown",
                          "properties": reviewed})
        if len(cases) > MAX_OBSERVED_CASES:
            raise ValueError("case observation exceeds its bounded count")
        if completed_command_inputs(log, expected_argv=argv) != selected:
            raise ValueError("case-list original command closure changed")
    for path in sorted(references):
        retain(path)
    if matrix.verify_snapshot(root, build, catalogue, snapshot) != before:
        raise ValueError("Snapshot changed during observation projection")
    return {"preset": preset, "snapshot": identity,
            "status": "previously_observed_inputs_match",
            "execution_status": "not_assessed", "case_count": len(cases),
            "cases": cases, "unproven": data["unproven"],
            "scope": ("Prior unfiltered case-list processes only; current bound "
                      "inputs match. No case execution, pass or coverage "
                      "percentage is inferred by this preview.")}


def _hardware(root, checker, arguments, observe):
    """Project known checker duties and explicit HIL facts without acquisition."""
    unknown = {"requires_equipment": None, "purpose": "not_assessed",
               "assessment": "unknown"}
    if checker in {"native_contracts", "tdd", "tool_tests"}:
        return {"requires_equipment": False, "purpose": "host_contracts",
                "assessment": "not_required"}
    if checker != "hil_prepare":
        return unknown
    result = {"requires_equipment": False,
              "purpose": "station_preparation_only", "assessment": "not_required",
              "requirements": {"status": "configuration_pending"}}
    fixture = _option(arguments, "--fixture")
    resolved = _option(arguments, "--resolved")
    if fixture is None or not (root / fixture).is_file():
        result["requirements"]["reason"] = "fixture_unavailable"
        return result
    from tools.hil.admit import validate_fixture
    from tools.hil.prepare import preparation_plan
    from tools.evidence.identity import resolved_identity

    _, authored = _retained_json(root / fixture, observe)
    validate_fixture(authored)
    result["requirements"].update(
        fixture=authored["board"], part=authored["part"],
        required_tests=list(authored["required_tests"]),
        pending_qualification=[{**row, "status": "not_executed"}
                               for row in authored["pending_qualification"]],
        station_prerequisites=None)
    if resolved is None or not (root / resolved).is_file():
        result["requirements"]["reason"] = "resolved_configuration_unavailable"
        return result
    resolved_file, admitted_configuration = _retained_json(
        root / resolved, observe)
    configuration, verified_file = resolved_identity(root / resolved)
    if (configuration != admitted_configuration or
            verified_file != resolved_file):
        raise ValueError("Validated HIL configuration differs from retained JSON")
    prepared = preparation_plan(authored, configuration)
    result["requirements"].update(
        status="declared_configuration_matches",
        controllers=prepared["controllers"], backend=prepared["backend"],
        station_prerequisites=prepared["station_prerequisites"],
        unreviewed_budgets=prepared["unreviewed_budgets"])
    return result


def preview(root, plan_path, output_root, *, environment=None, observations=None):
    """Project authored stage order and input identities; never discover tests.

    The returned command arguments match the existing plan executor. Checkers
    retain complete argument validation and execution ownership. Observing a
    formal plan or source bytes does not establish any execution qualification.
    """
    root = Path(root).absolute()
    plan_path = root / plan_path
    output_root = root / output_root
    separate_outputs([output_root], inputs=[plan_path])
    if output_root.exists():
        raise ValueError("future execution output must be a new directory")
    from tools.dev.qualification import _retained_identity, _recheck_retained

    inputs = {}

    def observe(path):
        identity = file_identity(path)
        previous = inputs.setdefault(identity["path"], identity)
        if previous != identity:
            raise ValueError("preview input changed during observation")
        return identity

    def retain(path):
        identity = _retained_identity(Path(path))
        previous = inputs.setdefault(identity["path"], identity)
        if previous != identity:
            raise ValueError("observation input changed during reading")
        return identity

    plan_identity, authored = _retained_json(plan_path, observe)
    plan = validate_plan(authored)
    stages = []
    presets = None
    observations = {} if observations is None else dict(observations)
    if not set(observations).issubset(NATIVE_PRESETS):
        raise ValueError("observation requires a maintained Native preset")
    observed = {}
    for index, stage in enumerate(plan["stages"]):
        checker = stage["checker"]
        script, *prefix = CHECKERS[checker]
        checker_identity = observe(root / script)
        arguments = command(["checker", *stage["args"]], {
            "root": str(root), "output": str(output_root)})[1:]
        argv = command([sys.executable, "-B", str(root / script),
                        *prefix, *arguments])
        coverage = {"checker": checker, "discovery": "not_executed",
                    "case_count": None, "tool_suite": None,
                    "tool_directory": None, "preset": None,
                    "scope": "declared_checker_arguments_only"}
        hardware = _hardware(root, checker, arguments, observe)
        if checker == "tool_tests":
            from tools.testing.run_tool_tests import CI_SUITES
            suite = _option(arguments, "--suite", required=True)
            if suite not in CI_SUITES:
                raise ValueError("Unknown maintained tool scope: " + suite)
            coverage.update(tool_suite=suite, tool_directory=CI_SUITES[suite])
        if checker in ("configure", "build", "native_contracts", "tdd"):
            if presets is None:
                presets = Presets(root, environment)
                for path, document in presets.files.items():
                    _, admitted = _retained_json(path, observe)
                    if admitted != document:
                        raise ValueError("preset input changed during reading")
            coverage["preset"] = _preset(presets, checker, arguments)
        selection = {"reason": "explicit_plan_stage", "stage_index": index}
        if checker in {"native_contracts", "tdd"}:
            preset = coverage["preset"]["configure"]
            if preset in observations:
                if preset not in observed:
                    observed[preset] = _observation(
                        root, preset,
                        Path(coverage["preset"]["binary_directory"]),
                        Path(observations[preset]), retain)
                previous = observed[preset]
                coverage.update(discovery="previously_observed_inputs_match",
                                case_count=previous["case_count"],
                                observed_cases=previous["cases"],
                                observation=previous["snapshot"],
                                execution_status="not_assessed")
                selection["observation_reason"] = (
                    "complete_current_inputs_and_unfiltered_discovery_match")
        stages.append({
            "name": stage["name"], "checker": checker,
            "checker_source": checker_identity, "argv": argv,
            "timeout_s": stage.get("timeout_s", 1800),
            "selection": selection,
            "coverage": coverage, "hardware": hardware,
            "status": "not_executed",
        })
    if set(observed) != set(observations):
        raise ValueError("observation has no matching selected plan stage")
    _recheck_retained(list(inputs.values()))
    return {
        "schema": 1, "kind": "test_plan_preview", "status": "not_executed",
        "qualification": False, "software_qualified": False,
        "physical_status": "not_executed", "plan": plan_identity,
        "formal_requested": plan.get("formal", False),
        "root": str(root), "future_output_root": str(output_root),
        "stages": stages, "inputs": list(inputs.values()),
        "observations": list(observed.values()),
        "scope": ("No checker or process executed by preview. Explicit prior "
                  "discovery may be projected only while all bound inputs "
                  "match. Unknown catalogue/hardware facts stay unknown; "
                  "no test pass, coverage percentage or qualification inferred."),
    }


def main(argv=None, *, root):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument("--plan", type=Path,
                        default=Path("tools/dev/plans/software.json"))
    parser.add_argument("--output-root", type=Path,
                        default=Path("build/quality/planned-execution"))
    parser.add_argument("--json", type=Path)
    parser.add_argument("--observation", action="append", default=[],
                        metavar="PRESET=SNAPSHOT",
                        help="Read an existing complete case-list observation")
    args = parser.parse_args(argv)
    try:
        observations = {}
        for value in args.observation:
            preset, separator, path = value.partition("=")
            if (not separator or not path or preset in observations or
                    preset not in NATIVE_PRESETS):
                raise ValueError("observation requires unique PRESET=SNAPSHOT")
            observations[preset] = Path(path)
        result = preview(root, args.plan, args.output_root,
                         environment=os.environ, observations=observations)
        if args.json:
            destination = Path(root) / args.json
            separate_outputs(
                [destination],
                inputs=[Path(row["path"]) for row in result["inputs"]],
                input_trees=[Path(result["future_output_root"])])
            with destination.open("x", encoding="utf-8") as stream:
                json.dump(result, stream, indent=2, allow_nan=False)
                stream.write("\n")
        print(f"Plan preview: {len(result['stages'])} stages; "
              "not executed; qualification=false")
        for stage in result["stages"]:
            equipment = stage["hardware"]["purpose"]
            print(f"{stage['selection']['stage_index'] + 1:3}. "
                  f"{stage['name']} [{stage['checker']}; "
                  f"{stage['timeout_s']}s; hardware={equipment}]")
            print("     " + shlex.join(stage["argv"]))
        print("No process executed by preview; absent observations have unknown "
              "case counts. Prior observations never grant execution status. "
              "Future output was not created.")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("Nexus plan preview rejected: " + str(error), file=sys.stderr)
        return 2
