"""Preview existing qualification stages without running their checkers."""

import argparse
import json
import os
from pathlib import Path
import shlex
import sys

from tools.dev.presets import Presets
from tools.dev.qualification import CHECKERS, validate_plan
from tools.evidence.common import (command, file_identity, load_json,
                                   separate_outputs, verify_file_identity)


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


def preview(root, plan_path, output_root, *, environment=None):
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
    inputs = {}

    def observe(path):
        identity = file_identity(path)
        previous = inputs.setdefault(identity["path"], identity)
        if previous != identity:
            raise ValueError("preview input changed during observation")
        return identity

    plan_identity = observe(plan_path)
    plan = validate_plan(load_json(plan_path))
    stages = []
    presets = None
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
        hardware = {"requires_equipment": None,
                    "purpose": "not_assessed"}
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
                    observe(path)
                    if load_json(path) != document:
                        raise ValueError("preset input changed during reading")
            coverage["preset"] = _preset(presets, checker, arguments)
        if checker == "hil_prepare":
            hardware = {"requires_equipment": False,
                        "purpose": "station_preparation_only"}
        stages.append({
            "name": stage["name"], "checker": checker,
            "checker_source": checker_identity, "argv": argv,
            "timeout_s": stage.get("timeout_s", 1800),
            "selection": {"reason": "explicit_plan_stage",
                          "stage_index": index},
            "coverage": coverage, "hardware": hardware,
            "status": "not_executed",
        })
    for identity in inputs.values():
        verify_file_identity(identity)
    return {
        "schema": 1, "kind": "test_plan_preview", "status": "not_executed",
        "qualification": False, "software_qualified": False,
        "physical_status": "not_executed", "plan": plan_identity,
        "formal_requested": plan.get("formal", False),
        "root": str(root), "future_output_root": str(output_root),
        "stages": stages, "inputs": list(inputs.values()),
        "scope": ("Authored stages and observed preset metadata only; "
                  "checker argument validation, test discovery, tool "
                  "availability and physical requirements remain unassessed "
                  "unless explicitly identified. No checker was executed."),
    }


def main(argv=None, *, root):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument("--plan", type=Path,
                        default=Path("tools/dev/plans/software.json"))
    parser.add_argument("--output-root", type=Path,
                        default=Path("build/quality/planned-execution"))
    parser.add_argument("--json", type=Path)
    args = parser.parse_args(argv)
    try:
        result = preview(root, args.plan, args.output_root,
                         environment=os.environ)
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
        print("Case discovery: not executed; case counts unknown. "
              "Future output was not created.")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("Nexus plan preview rejected: " + str(error), file=sys.stderr)
        return 2
