#!/usr/bin/env python3
"""Check the required jobs of ci.yml, including its conditional skips.

GitHub Actions supplies CI_NEEDS_JSON and CI_EVENT_NAME through environment
variables. Keep required_jobs() aligned with the workflow's job conditions;
test_check_required_jobs.py checks that contract without external dependencies.
"""

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import sys
from typing import Any, Dict, List, Optional


GATE_JOBS = ("build-test", "code-quality", "docs")
OUTPUT_NAMES = ("code", "docs", "workflows")
EVENT_NAMES = ("push", "pull_request", "schedule", "workflow_dispatch")


@dataclass
class CheckResult:
    required: Dict[str, bool]
    actual: Dict[str, str]
    errors: List[str]

    @property
    def passed(self) -> bool:
        return not self.errors


def required_jobs(outputs: Dict[str, str], event_name: str) -> Dict[str, bool]:
    """Mirror the explicit ci.yml conditions after input validation."""
    manual = event_name == "workflow_dispatch"
    scheduled = event_name == "schedule"
    code_or_workflows = (
        outputs["code"] == "true" or outputs["workflows"] == "true"
    )
    return {
        "build-test": code_or_workflows or scheduled or manual,
        "code-quality": code_or_workflows or manual,
        "docs": (
            outputs["docs"] == "true"
            or outputs["code"] == "true"
            or scheduled
            or manual
        ),
    }


def check_required_jobs(needs: Any, event_name: str) -> CheckResult:
    """Require success for triggered jobs; accept only justified skips.

    Missing or malformed change detection cannot justify skipping any gate.
    Failed or cancelled jobs never satisfy the aggregate check, even if their
    conditions did not require them to run.
    """
    result = CheckResult(required={"changes": True}, actual={}, errors=[])
    if event_name not in EVENT_NAMES:
        result.errors.append("Unsupported or missing event_name: {!r}".format(event_name))
    if not isinstance(needs, dict):
        result.errors.append("needs must be a JSON object")
        return result

    for job in ("changes",) + GATE_JOBS:
        entry = needs.get(job)
        if not isinstance(entry, dict):
            result.actual[job] = "missing"
            result.errors.append("{}: missing or malformed needs entry".format(job))
            continue
        status = entry.get("result")
        result.actual[job] = status if isinstance(status, str) else "missing"
        if status not in ("success", "failure", "cancelled", "skipped"):
            result.errors.append("{}: missing or invalid result {!r}".format(job, status))

    changes = needs.get("changes")
    if not isinstance(changes, dict):
        return result
    if changes.get("result") != "success":
        result.errors.append("changes: change detection must succeed")

    outputs = changes.get("outputs")
    valid_outputs = isinstance(outputs, dict)
    if not valid_outputs:
        result.errors.append("changes: missing or malformed outputs")
    else:
        for name in OUTPUT_NAMES:
            value = outputs.get(name)
            if not isinstance(value, str) or value not in ("true", "false"):
                valid_outputs = False
                result.errors.append(
                    "changes: output {!r} must be 'true' or 'false', got {!r}".format(
                        name, value
                    )
                )

    if not valid_outputs or event_name not in EVENT_NAMES:
        return result

    result.required.update(required_jobs(outputs, event_name))
    for job in GATE_JOBS:
        status = result.actual[job]
        if status == "success":
            continue
        if status == "skipped" and not result.required[job]:
            continue
        result.errors.append(
            "{}: {} job requires {}, got {!r}".format(
                job,
                "required" if result.required[job] else "optional",
                "success" if result.required[job] else "success or skipped",
                status,
            )
        )
    return result


def _cell(value: str) -> str:
    return value.replace("|", "\\|").replace("\r", " ").replace("\n", " ")


def summary(result: CheckResult) -> str:
    lines = [
        "## CI Results",
        "",
        "| Job | Required | Status |",
        "|-----|----------|--------|",
    ]
    for job in ("changes",) + GATE_JOBS:
        required = result.required.get(job)
        requirement = "unknown" if required is None else ("yes" if required else "no")
        lines.append(
            "| {} | {} | {} |".format(
                job, requirement, _cell(result.actual.get(job, "missing"))
            )
        )
    if result.errors:
        lines.extend(["", "The aggregate check failed:", ""])
        lines.extend("- " + _cell(error) for error in result.errors)
    else:
        lines.extend(["", "All required jobs succeeded; conditional skips are valid."])
    return "\n".join(lines) + "\n"


def _workflow_error(message: str) -> str:
    # Prevent data containing a newline from creating another workflow command.
    escaped = message.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
    return "::error::" + escaped


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--needs-json", default=os.environ.get("CI_NEEDS_JSON"))
    parser.add_argument("--event-name", default=os.environ.get("CI_EVENT_NAME", ""))
    parser.add_argument("--summary-file", default=os.environ.get("GITHUB_STEP_SUMMARY"))
    args = parser.parse_args(argv)
    try:
        needs = json.loads(args.needs_json) if args.needs_json is not None else None
    except (ValueError, TypeError) as error:
        result = CheckResult({}, {}, ["Invalid needs JSON: {}".format(error)])
    else:
        result = check_required_jobs(needs, args.event_name)

    report = summary(result)
    print(report, end="")
    if args.summary_file:
        try:
            with Path(args.summary_file).open("a", encoding="utf-8") as stream:
                stream.write(report)
        except OSError as error:
            result.errors.append("Unable to write CI summary: {}".format(error))
    for error in result.errors:
        print(_workflow_error(error), file=sys.stderr)
    return 0 if result.passed else 1


if __name__ == "__main__":
    sys.exit(main())
