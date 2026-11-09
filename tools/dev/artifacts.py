"""Navigate verified build observations without promoting their qualification."""

import argparse
import json
from pathlib import Path

from tools.dev.provenance import verify_provenance
from tools.evidence.common import (
    file_identity, load_json, separate_outputs, verify_file_identity,
)
from tools.evidence.identity import resolved_identity


def _role(path):
    name = Path(path)
    if name.name.endswith(".resources.json"):
        return "linked_resource_report"
    return {".elf": "elf", ".bin": "binary", ".map": "link_map"}.get(
        name.suffix, "observed_output")


def inspect(path):
    """Recheck the existing complete closure; do not run builds or equipment.

    This view retains the producer's actual execution association. It does not
    prove the checkout still matches that source or that every incremental
    object was rebuilt. Resource reports remain observed producer outputs.
    """
    identity = file_identity(path)
    record = load_json(path)
    verify_provenance(record)
    if (record.get("action") != "build"
            or not record.get("actual_execution")):
        raise ValueError("An actual completed build observation is required")
    configuration = None
    if record["configuration"] is not None:
        resolved, _ = resolved_identity(
            verify_file_identity(record["configuration"]))
        configuration = {key: resolved.get(key) for key in
                         ("board", "part", "soc_family", "backend",
                          "configuration_sha256")}
    result = {
        "schema": 1, "kind": "build_artifact_navigation",
        "status": "identities_verified", "provenance": identity,
        "source": record["source"], "configuration": configuration,
        "artifacts": [{**item, "role": _role(item["path"])}
                      for item in record["artifacts"]],
        "actual_execution": record["actual_execution"],
        "software_qualified": False, "physical_status": "not_assessed",
        "scope": ("Verified recorded build inputs, outputs and command receipt; "
                  "no current-checkout comparison, new execution, resource "
                  "measurement, software candidate or physical qualification."),
    }
    verify_file_identity(identity)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument("--provenance", type=Path, required=True)
    parser.add_argument("--json", type=Path)
    arguments = parser.parse_args(argv)
    result = inspect(arguments.provenance)
    encoded = json.dumps(result, indent=2) + "\n"
    if arguments.json is not None:
        separate_outputs([arguments.json], inputs=[
            arguments.provenance,
            *(item["path"] for item in result["artifacts"]),
        ])
        with arguments.json.open("x", encoding="utf-8") as stream:
            stream.write(encoded)
    else:
        print(encoded, end="")
    return 0
