"""Rehashed malformed protocols cannot authorize capture or qualify traces."""

import copy
import json
from pathlib import Path
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.hil.acquire import acquisition_plan
from tools.hil.measure import validate_measurements
from tools.testing import (
    test_hil_acquire, test_hil_measurements, test_os_acceptance,
)


class HilProtocolAdmissionTests(unittest.TestCase):
    def mutated(self, document, invalid_budget):
        if not invalid_budget:
            return {"status": "passed",
                    "scope": "opaque, not a reviewed protocol"}
        result = copy.deepcopy(document)
        metric = next(iter(result["cases"][0]["limits"]))
        result["cases"][0]["limits"][metric]["maximum"] = None
        return result

    def acquire(self, invalid_budget):
        fixture = test_hil_acquire.HilAcquireTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        plan = acquisition_plan(fixture.recipe_path, fixture.admission_path)
        self.assertEqual(plan["status"], "prepared_bound")
        document = json.loads(fixture.workload.read_text())
        atomic_json(fixture.workload, self.mutated(document, invalid_budget))
        fixture.recipe["workload"] = file_identity(fixture.workload)
        fixture.write_recipe()
        with patch("subprocess.Popen") as process, self.assertRaises(EvidenceError):
            acquisition_plan(fixture.recipe_path, fixture.admission_path)
        process.assert_not_called()
        self.assertFalse(Path(fixture.recipe["station_lock"]).exists())

    def measure(self, invalid_budget):
        fixture = test_hil_measurements.HilMeasurementTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        admission, raw = fixture.record()
        report = validate_measurements(raw, admission)
        self.assertEqual(report["status"], "passed")
        context_path = Path(raw["context"]["path"])
        context = json.loads(context_path.read_text())
        workload = Path(context["workload"]["path"])
        document = json.loads(workload.read_text())
        atomic_json(workload, self.mutated(document, invalid_budget))
        context["workload"] = file_identity(workload)
        atomic_json(context_path, context)
        raw["context"] = file_identity(context_path)
        for index, identity in enumerate(raw["traces"]):
            path = Path(identity["path"])
            trace = json.loads(path.read_text())
            trace["context_sha256"] = raw["context"]["sha256"]
            atomic_json(path, trace)
            raw["traces"][index] = file_identity(path)
        with self.assertRaises(EvidenceError):
            validate_measurements(raw, admission)

    def os(self, invalid_budget):
        fixture = test_os_acceptance.OSAcceptanceTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        self.assertEqual(fixture.validate()["status"], "passed")
        workload = Path(fixture.context["workload"]["path"])
        document = json.loads(workload.read_text())
        atomic_json(workload, self.mutated(document, invalid_budget))
        fixture.context["workload"] = file_identity(workload)
        atomic_json(fixture.context_path, fixture.context)
        fixture.record["context"] = file_identity(fixture.context_path)
        for name, trace in fixture.traces.items():
            fixture.write_trace(name, trace)
        with self.assertRaises(EvidenceError):
            fixture.validate()

    def test_acquisition_rejects_rehashed_opaque_protocol_before_equipment(self):
        self.acquire(False)

    def test_acquisition_rejects_unreviewed_budget_before_equipment(self):
        self.acquire(True)

    def test_measurement_rejects_rehashed_opaque_protocol(self):
        self.measure(False)

    def test_measurement_rejects_unreviewed_budget_despite_valid_trace(self):
        self.measure(True)

    def test_os_trace_rejects_rehashed_opaque_protocol(self):
        self.os(False)

    def test_os_trace_rejects_unreviewed_budget_despite_complete_cases(self):
        self.os(True)


if __name__ == "__main__":
    unittest.main()
