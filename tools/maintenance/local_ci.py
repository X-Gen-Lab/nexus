#!/usr/bin/env python3
"""Verify the maintained local software plan and optionally publish its status."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.dev.qualification import verify_execution
from tools.evidence.common import (EvidenceError, atomic_json, file_identity,
                                   load_json, separate_outputs)
from tools.evidence.identity import observe_git_source
from tools.release.delivery import GitHub

CONTEXT = "nexus/local-software-plan"
PLAN = "tools/dev/plans/software.json"


def verify(report_path, root=ROOT):
    """Only the checked-in complete software plan can produce this status."""
    root = Path(root).resolve()
    plan_path = root / PLAN
    before = file_identity(plan_path)
    report = verify_execution(load_json(plan_path), Path(report_path), root)
    if file_identity(plan_path) != before:
        raise EvidenceError("maintained software plan changed during verification")
    return {**report, "plan": before, "context": CONTEXT}


def _pull(api, number):
    value = api.get("/pulls/" + str(number))
    if (not isinstance(value, dict) or value.get("state") != "open" or
            value.get("merged") is not False or value.get("number") != number):
        raise EvidenceError("an existing open pull request is required")
    if (value["base"]["repo"]["full_name"] != api.repository or
            value["base"]["ref"] != "main"):
        raise EvidenceError("pull request must target this repository main")
    for key in ("head", "base"):
        if not re.fullmatch(r"[0-9a-f]{40}", str(value[key].get("sha", ""))):
            raise EvidenceError("pull request requires exact head and base commits")
    return {"head": value["head"]["sha"], "base": value["base"]["sha"]}


def publish(report_path, root, api, pull_number, output):
    """Publish only to the verified SHA; a later PR head receives no old pass.

    Two PR observations detect ordinary head/base drift. They do not claim an
    atomic transaction with GitHub's subsequent status creation or merge rules.
    """
    root, report_path, output = (Path(root).resolve(), Path(report_path),
                                 Path(output).absolute())
    if type(pull_number) is not int or pull_number <= 0:
        raise EvidenceError("pull request number must be positive")
    separate_outputs([output], [report_path], [root, report_path.parent])
    if output.exists():
        raise EvidenceError("publication receipt must be a new path")
    initial = _pull(api, pull_number)
    gate = verify(report_path, root)
    if initial["head"] != gate["source"]["commit"]:
        raise EvidenceError("pull request head differs from verified source")
    if _pull(api, pull_number) != initial:
        raise EvidenceError("pull request head or base changed during verification")
    if observe_git_source(root) != gate["source"]:
        raise EvidenceError("source changed before status publication")
    payload = {"state": "success", "context": CONTEXT,
               "description": "Local software plan: " + str(gate["stage_count"]) +
                              " stages; observed base " + initial["base"][:12]}
    endpoint = api.base + "/statuses/" + initial["head"]
    result = {"schema_version": 1, "kind": "local_software_plan_status",
              "repository": api.repository, "pull_request": pull_number,
              "observed_refs": initial, "verification": gate,
              "request": {"method": "POST", "endpoint": endpoint,
                          "payload": payload},
              "publication": "pending", "software_qualified": False,
              "scope": "maintained local software plan; no Actions or release claim"}
    atomic_json(output, result)
    try:
        response = api.request("POST", endpoint, payload)
        if (not isinstance(response, dict) or
                response.get("state") != payload["state"] or
                response.get("context") != CONTEXT or
                type(response.get("id")) is not int or response["id"] <= 0):
            raise EvidenceError("GitHub did not confirm the requested status")
        result.update(publication="confirmed", response={key: response.get(key)
                      for key in ("id", "url", "state", "context", "description")})
        atomic_json(output, result)
    except (OSError, ValueError, KeyError, TypeError) as error:
        result.update(publication="unconfirmed", failure=str(error))
        atomic_json(output, result)
        raise
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--publish", action="store_true")
    parser.add_argument("--repository")
    parser.add_argument("--pull-request", type=int)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    if args.publish and not all((args.repository, args.pull_request, args.output)):
        parser.error("--publish requires --repository, --pull-request and --output")
    if not args.publish and any((args.repository, args.pull_request, args.output)):
        parser.error("publication options require --publish")
    try:
        if args.publish:
            if not os.environ.get("GH_TOKEN"):
                raise EvidenceError("GitHub publication requires configured GH_TOKEN")
            report = publish(args.report, args.root, GitHub(args.repository),
                             args.pull_request, args.output)
        else:
            report = verify(args.report, args.root)
        print(json.dumps(report, indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("Local software plan rejected: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
