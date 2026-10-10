"""Synthetic measurement fixtures test validators, never qualify real boards."""

import copy
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.identity import canonical_digest
from tools.hil.admit import validate_raw, validate_station
from tools.hil.measure import measurement_plan, validate_measurements
from tests.contracts import test_hil_admission


class HilMeasurementTests(unittest.TestCase):
    def setUp(self):
        self.fixture = test_hil_admission.HilAdmissionTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root = self.fixture.root
        self.fixture.station["budgets"] = {
            "irq_latency": {"maximum": 100, "unit": "us"},
            "throughput": {"minimum": 1000, "maximum": 1000000,
                           "unit": "bytes_per_s"},
        }

    def record(self):
        # The admission service is an external ELF/toolchain dependency. Real
        # hashes and measurement validation remain active on synthetic inputs.
        files = {}
        for key in ("station", "fixture", "elf", "resolved", "resources"):
            path = self.root / (key + ".json")
            value = {"synthetic": "validator-only; no equipment executed"}
            if key == "station":
                value = self.fixture.station
            elif key == "fixture":
                value = self.fixture.fixture
            atomic_json(path, value)
            files[key] = file_identity(path)
        admission = {"schema_version": 1, "kind": "hil_admission",
                     "status": "ready", "physical_status": "not_executed",
                     "station_id": self.fixture.station["station_id"],
                     "board": self.fixture.fixture["board"],
                     "part": self.fixture.fixture["part"],
                     "chip_uid": self.fixture.station["chip_uid"],
                     "budgets": copy.deepcopy(self.fixture.station["budgets"]),
                     **files}
        self.admission = copy.deepcopy(admission)
        service = patch("tools.hil.measure.admit", return_value=self.admission)
        self.mock_admit = service.start()
        self.addCleanup(service.stop)
        now = datetime.now(timezone.utc)
        raw = {
            "schema_version": 1, "kind": "physical_hil_measurements",
            "started_utc": (now - timedelta(seconds=2)).isoformat(),
            "finished_utc": (now - timedelta(seconds=1)).isoformat(),
            "station_id": admission["station_id"],
            "board": admission["board"], "part": admission["part"],
            "chip_uid": admission["chip_uid"],
            "elf_sha256": admission["elf"]["sha256"],
            "resolved_sha256": admission["resolved"]["sha256"],
            "admission_sha256": canonical_digest(admission),
            "traces": [],
        }
        workload = self.root / "workload.txt"
        workload.write_text("synthetic acquisition; no physical workload")
        instrument = self.root / "instrument.txt"
        instrument.write_text("synthetic calibration model, not an instrument")
        context = {
            "schema_version": 1, "kind": "hil_acquisition_context",
            "admission_sha256": raw["admission_sha256"],
            "workload": file_identity(workload), "cpu_clock_hz": 168000000,
            "load": "synthetic validator-only load", "executor": "model loop",
            "irq_priorities": {"USART1": 6},
            "measurements": {
                "irq_latency": {
                    "method": "logic_analyzer",
                    "instrument": file_identity(instrument),
                    "resolution": 0.1, "observation_window_s": 1},
                "throughput": {
                    "method": "wire_byte_counter",
                    "instrument": file_identity(instrument),
                    "resolution": 1, "observation_window_s": 1},
            },
        }
        context_path = self.root / "context.json"
        atomic_json(context_path, context)
        raw["context"] = file_identity(context_path)
        for metric, unit, method, values in (
                ("irq_latency", "us", "logic_analyzer", [1, 8, 3]),
                ("throughput", "bytes_per_s", "wire_byte_counter",
                 [3000, 2000, 4000])):
            trace = {"schema_version": 1, "kind": "physical_hil_trace",
                     "metric": metric, "unit": unit, "method": method,
                     "admission_sha256": raw["admission_sha256"],
                     "context_sha256": raw["context"]["sha256"],
                     "samples": [{"sequence": i, "value": value}
                                 for i, value in enumerate(values)]}
            path = self.root / (metric + ".json")
            atomic_json(path, trace)
            raw["traces"].append(file_identity(path))
        return admission, raw

    def test_unbound_plan_covers_measurements_without_claiming_execution(self):
        fixture = json.loads(Path(
            "tools/hil/fixtures/stm32f407_qiming_v31.json").read_text())
        resolved = {"board": fixture["board"], "part": fixture["part"],
                    "backend": "baremetal", "controllers": []}
        plan = measurement_plan(fixture, resolved)
        self.assertEqual(plan["physical_status"], "not_executed")
        self.assertEqual({item["id"] for item in plan["measurements"]}, {
            "cycles", "irq_latency", "service_interval", "stack_used_peak",
            "throughput", "loss", "duration"})
        self.assertTrue(all(item["budget"] is None
                            for item in plan["measurements"]))
        with self.assertRaises(EvidenceError):
            measurement_plan(fixture, {**resolved, "part": "wrong-chip"})

    def test_observed_statistics_and_lower_bound_are_derived_from_trace(self):
        admission, raw = self.record()
        result = validate_measurements(raw, admission)
        self.assertEqual(result["metrics"]["irq_latency"]["maximum"], 8)
        self.assertEqual(result["metrics"]["throughput"]["minimum"], 2000)
        self.assertEqual(result["metrics"]["irq_latency"]["samples"], 3)
        self.mock_admit.assert_called_once()
        self.assertEqual(result["physical_status"],
                         "qualified_for_recorded_measurements")

    def test_missing_duplicate_unit_method_and_sequence_are_rejected(self):
        admission, original = self.record()
        for traces in ([], original["traces"][:1],
                       original["traces"] + original["traces"][:1]):
            with self.subTest(traces=len(traces)), \
                    self.assertRaises(EvidenceError):
                validate_measurements({**original, "traces": traces}, admission)
        path = Path(original["traces"][0]["path"])
        trace = json.loads(path.read_text())
        invalid = [{**trace, "unit": "cycles"},
                   {**trace, "method": "host_clock"},
                   {**trace, "samples": []},
                   {**trace, "samples": [{"sequence": 1, "value": 2}]},
                   {**trace, "samples": [{"sequence": 0, "value": True}]},
                   {**trace, "samples": [{"sequence": 0, "value": -1}]},
                   {**trace, "admission_sha256": "0" * 64}]
        for replacement in invalid:
            atomic_json(path, replacement)
            raw = {**original, "traces": [file_identity(path),
                                          original["traces"][1]]}
            with self.subTest(replacement=replacement), \
                    self.assertRaises(EvidenceError):
                validate_measurements(raw, admission)

    def test_changed_trace_stale_report_and_wrong_identity_are_rejected(self):
        admission, original = self.record()
        for key, value in (("kind", "host_model"), ("chip_uid", "wrong-uid"),
                           ("elf_sha256", "0" * 64),
                           ("admission_sha256", "0" * 64),
                           ("started_utc", "2020-01-01T00:00:00Z"),
                           ("finished_utc", "2099-01-01T00:00:00Z")):
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                validate_measurements({**original, key: value}, admission)
        Path(original["traces"][0]["path"]).write_text("modified")
        with self.assertRaises(EvidenceError):
            validate_measurements(original, admission)

    def test_upper_and_lower_measurement_limits_both_apply(self):
        admission, original = self.record()
        for index, value in ((0, 101), (1, 999)):
            path = Path(original["traces"][index]["path"])
            trace = json.loads(path.read_text())
            trace["samples"][0]["value"] = value
            atomic_json(path, trace)
            raw = copy.deepcopy(original)
            raw["traces"][index] = file_identity(path)
            with self.subTest(index=index), self.assertRaises(EvidenceError):
                validate_measurements(raw, admission)
            trace["samples"][0]["value"] = 1 if index == 0 else 3000
            atomic_json(path, trace)
            original["traces"][index] = file_identity(path)

    def test_unreviewed_or_invalid_budget_cannot_admit_measurements(self):
        for minimum in (True, -1, float("nan"), 1000001):
            self.fixture.station["budgets"]["throughput"]["minimum"] = minimum
            with self.subTest(minimum=minimum), \
                    self.assertRaises(EvidenceError):
                validate_station(self.fixture.station, self.fixture.fixture)

    def test_unrepresentable_samples_raise_evidence_errors(self):
        admission, raw = self.record()
        path = Path(raw["traces"][0]["path"])
        trace = json.loads(path.read_text())
        trace["samples"][0]["value"] = 10 ** 400
        atomic_json(path, trace)
        raw["traces"][0] = file_identity(path)
        with self.assertRaises(EvidenceError):
            validate_measurements(raw, admission)

    def test_changed_admission_and_unbound_context_are_rejected(self):
        admission, original = self.record()
        changed = copy.deepcopy(self.admission)
        changed["chip_uid"] = "changed-observation"
        self.mock_admit.return_value = changed
        with self.assertRaises(EvidenceError):
            validate_measurements(original, admission)
        self.mock_admit.return_value = self.admission
        path = Path(original["context"]["path"])
        context = json.loads(path.read_text())
        invalid = [{**context, "cpu_clock_hz": True},
                   {**context, "cpu_clock_hz": 0},
                   {**context, "load": ""},
                   {**context, "executor": ""},
                   {**context, "irq_priorities": {}},
                   {**context, "irq_priorities": {"USART1": True}},
                   {**context, "measurements": {}},
                   {**context, "admission_sha256": "0" * 64}]
        for replacement in invalid:
            atomic_json(path, replacement)
            raw = {**original, "context": file_identity(path)}
            with self.subTest(context=replacement), \
                    self.assertRaises(EvidenceError):
                validate_measurements(raw, admission)
        atomic_json(path, context)
        trace_path = Path(original["traces"][0]["path"])
        trace = json.loads(trace_path.read_text())
        trace["context_sha256"] = "0" * 64
        atomic_json(trace_path, trace)
        raw = copy.deepcopy(original)
        raw["traces"][0] = file_identity(trace_path)
        with self.assertRaises(EvidenceError):
            validate_measurements(raw, admission)

    def test_acquisition_resolution_method_and_bounds_are_strict(self):
        admission, original = self.record()
        path = Path(original["context"]["path"])
        context = json.loads(path.read_text())
        for key, value in (("resolution", 0), ("resolution", True),
                           ("observation_window_s", -1),
                           ("method", "host_clock")):
            changed = copy.deepcopy(context)
            changed["measurements"]["irq_latency"][key] = value
            atomic_json(path, changed)
            raw = {**original, "context": file_identity(path)}
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                validate_measurements(raw, admission)
        atomic_json(path, context)
        Path(context["workload"]["path"]).write_text("changed workload")
        with self.assertRaises(EvidenceError):
            validate_measurements(original, admission)

    def all_metrics_record(self):
        admission, raw = self.record()
        methods = {
            "cycles": ("cycles", "dwt_snapshot", 100),
            "irq_latency": ("us", "logic_analyzer", 1),
            "service_interval": ("us", "monotonic_timestamp", 10),
            "stack_used_peak": ("bytes", "stack_watermark", 64),
            "throughput": ("bytes_per_s", "wire_byte_counter", 2000),
            "loss": ("count", "receiver_event_counter", 0),
            "duration": ("s", "monotonic_timestamp", 1),
        }
        admission["budgets"] = {
            name: {"minimum": 0, "maximum": 10000, "unit": unit}
            for name, (unit, _, _) in methods.items()}
        self.admission.clear()
        self.admission.update(copy.deepcopy(admission))
        raw["admission_sha256"] = canonical_digest(admission)
        path = Path(raw["context"]["path"])
        context = json.loads(path.read_text())
        instrument = context["measurements"]["irq_latency"]["instrument"]
        context["admission_sha256"] = raw["admission_sha256"]
        context["measurements"] = {
            name: {"method": method, "instrument": instrument,
                   "resolution": 1, "observation_window_s": 1}
            for name, (_, method, _) in methods.items()}
        context["measurements"]["cycles"].update(
            counter_bits=32, maximum_interval_cycles=1000)
        context["measurements"]["stack_used_peak"]["stack_capacity_bytes"] = 128
        context["measurements"]["loss"]["expected_events"] = 100
        atomic_json(path, context)
        raw["context"] = file_identity(path)
        raw["traces"] = []
        for name, (unit, method, value) in methods.items():
            trace = {"schema_version": 1, "kind": "physical_hil_trace",
                     "metric": name, "unit": unit, "method": method,
                     "admission_sha256": raw["admission_sha256"],
                     "context_sha256": raw["context"]["sha256"],
                     "samples": [{"sequence": 0, "value": value}]}
            trace_path = self.root / (name + ".json")
            atomic_json(trace_path, trace)
            raw["traces"].append(file_identity(trace_path))
        return admission, raw

    def test_seven_metrics_derive_values_with_discrete_capacity_limits(self):
        admission, original = self.all_metrics_record()
        result = validate_measurements(original, admission)
        self.assertEqual(len(result["metrics"]), 7)
        for name, invalid in (("cycles", 1001), ("cycles", 1.5),
                              ("stack_used_peak", 129), ("loss", 101)):
            raw = copy.deepcopy(original)
            index = list(result["metrics"]).index(name)
            path = Path(raw["traces"][index]["path"])
            trace = json.loads(path.read_text())
            before = copy.deepcopy(trace)
            trace["samples"][0]["value"] = invalid
            atomic_json(path, trace)
            raw["traces"][index] = file_identity(path)
            with self.subTest(metric=name), self.assertRaises(EvidenceError):
                validate_measurements(raw, admission)
            atomic_json(path, before)
        path = Path(original["context"]["path"])
        context = json.loads(path.read_text())
        for name, key, value in (("cycles", "counter_bits", 64),
                                 ("cycles", "maximum_interval_cycles", 2 ** 32),
                                 ("stack_used_peak", "stack_capacity_bytes", 0),
                                 ("loss", "expected_events", True)):
            changed = copy.deepcopy(context)
            changed["measurements"][name][key] = value
            atomic_json(path, changed)
            raw = {**original, "context": file_identity(path)}
            with self.subTest(metric=name), self.assertRaises(EvidenceError):
                validate_measurements(raw, admission)

    def test_extreme_smoke_metrics_raise_evidence_errors(self):
        admission, _ = self.record()
        admission["required_tests"] = self.fixture.fixture["required_tests"]
        raw = self.fixture.raw_record(admission)
        raw["metrics"] = {
            name: {"value": 2000 if name == "throughput" else 1,
                   "unit": budget["unit"], "raw": self.fixture.identity}
            for name, budget in admission["budgets"].items()}
        self.assertEqual(validate_raw(raw, admission)["status"], "passed")
        for value in (10 ** 400, -(10 ** 400), True):
            changed = copy.deepcopy(raw)
            changed["metrics"]["irq_latency"]["value"] = value
            with self.subTest(value=value), self.assertRaises(EvidenceError):
                validate_raw(changed, admission)

    def test_extreme_station_bounds_raise_evidence_errors(self):
        for key in ("minimum", "maximum"):
            station = copy.deepcopy(self.fixture.station)
            station["budgets"]["throughput"][key] = 10 ** 400
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                validate_station(station, self.fixture.fixture)

    def test_cli_preserves_retained_inputs_even_with_tampered_context(self):
        admission, raw = self.record()
        admission_path = self.root / "admission.json"
        raw_path = self.root / "measurements.json"
        atomic_json(admission_path, admission)
        atomic_json(raw_path, raw)
        context_path = Path(raw["context"]["path"])
        context = json.loads(context_path.read_text())
        context["load"] = "changed acquisition load"
        atomic_json(context_path, context)
        for path in (Path(raw["traces"][0]["path"]),
                     self.root / "workload.txt",
                     self.root / "instrument.txt"):
            original = path.read_bytes()
            result = subprocess.run(
                [sys.executable, "tools/hil/measure.py", "validate",
                 "--admission", str(admission_path), "--raw-evidence",
                 str(raw_path), "--report", str(path)],
                capture_output=True, text=True)
            with self.subTest(path=path.name):
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(path.read_bytes(), original)

    def test_cli_cannot_overwrite_trace_or_any_admitted_input(self):
        admission, raw = self.record()
        admission_path = self.root / "admission.json"
        raw_path = self.root / "measurements.json"
        atomic_json(admission_path, admission)
        atomic_json(raw_path, raw)
        for path in (Path(raw["traces"][0]["path"]),
                     Path(admission["elf"]["path"]),
                     Path(admission["station"]["path"]),
                     Path(raw["context"]["path"]),
                     self.root / "workload.txt",
                     self.root / "instrument.txt", raw_path):
            original = path.read_bytes()
            command = [sys.executable, "tools/hil/measure.py", "validate",
                       "--admission", str(admission_path),
                       "--raw-evidence", str(raw_path), "--report", str(path)]
            result = subprocess.run(command, capture_output=True, text=True)
            with self.subTest(path=path.name):
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(path.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
