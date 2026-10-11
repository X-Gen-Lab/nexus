"""Keep the OS work ledger exact; prepared plans cannot qualify hardware."""

import copy
import csv
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
LEDGER = ROOT / "docs/design/os-execution.csv"
FIELDS = {"task_id", "priority", "task", "implementation_paths", "evidence_paths",
          "implementation_status", "software_status", "hardware_status",
          "qualification_authority"}
IMPLEMENTATION = {"implemented", "implemented_tooling", "implemented_contract",
                  "in_progress", "not_implemented"}
SOFTWARE = {"development_passed", "pending_exact_head_qualification",
            "documentation_review_pending", "not_qualified",
            "no_additional_kernel_selected", "qualification_record_authority"}
EXTERNAL = "X-Gen-Lab/nexus-examples/"


def validate_rows(rows):
    expected = {"OS" + str(index).zfill(2) for index in range(1, 56)}
    seen = set()
    for row in rows:
        if set(row) != FIELDS or any(not value.strip() for value in row.values()):
            raise ValueError("Incomplete OS ledger row")
        identity = row["task_id"]
        if identity not in expected or identity in seen:
            raise ValueError("Missing/repeated OS work item")
        seen.add(identity)
        if row["priority"] not in {"P1", "P2", "optional", "physical"}:
            raise ValueError("Unknown OS priority/scope")
        if (row["implementation_status"] not in IMPLEMENTATION or
                row["software_status"] not in SOFTWARE):
            raise ValueError("Unknown implementation/software state")
        if row["hardware_status"] not in {"not_executed", "not_applicable"}:
            raise ValueError("No physical equipment ran; plans cannot qualify it")
        if row["priority"] == "physical" and (
                row["implementation_status"] != "implemented_tooling" or
                row["hardware_status"] != "not_executed"):
            raise ValueError("OS49-OS55 tooling is separate from physical execution")
        for field in ("implementation_paths", "evidence_paths"):
            if field == "evidence_paths" and not row["implementation_status"].startswith(
                    "implemented"):
                continue
            for name in row[field].split(";"):
                path = Path(name.removeprefix(EXTERNAL))
                if path.is_absolute() or ".." in path.parts or not path.parts:
                    raise ValueError("Invalid OS source/evidence reference")
                if name.startswith(EXTERNAL):
                    continue
                if row["implementation_status"].startswith("implemented") and not (
                        ROOT / name).exists():
                    raise ValueError("Missing implemented OS source/evidence: " + name)
        if "exact source" not in row["qualification_authority"]:
            raise ValueError("OS qualification must bind current source")
    if seen != expected:
        raise ValueError("OS ledger must enumerate exactly OS01 through OS55")


class OSLedgerTests(unittest.TestCase):
    def setUp(self):
        with LEDGER.open(newline="") as stream:
            self.rows = list(csv.DictReader(stream))

    def test_current_ledger_has_exact_items_real_paths_and_honest_states(self):
        validate_rows(self.rows)

    def test_missing_duplicate_and_unknown_items_are_rejected(self):
        for rows in (self.rows[:-1], self.rows + self.rows[:1],
                     [{**self.rows[0], "task_id": "OS56"}, *self.rows[1:]]):
            with self.subTest(rows=len(rows)), self.assertRaises(ValueError):
                validate_rows(rows)

    def test_prepared_plan_cannot_change_physical_to_passed(self):
        rows = copy.deepcopy(self.rows)
        rows[-1]["hardware_status"] = "passed"
        rows[-1]["evidence_paths"] = "tools/hil/os_acceptance.py plan"
        with self.assertRaises(ValueError):
            validate_rows(rows)

    def test_impl_missing_path_unknown_state_and_escaping_path_rejected(self):
        for change in ({"implementation_paths": "os/missing-production.c"},
                       {"evidence_paths": "tests/missing-contract.cpp"},
                       {"implementation_paths": "../other-repository/code.c"},
                       {"implementation_status": "best_platform_complete"},
                       {"software_status": "all_done"},
                       {"qualification_authority": "old candidate inherited"}):
            rows = copy.deepcopy(self.rows)
            rows[0].update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                validate_rows(rows)


if __name__ == "__main__":
    unittest.main()
