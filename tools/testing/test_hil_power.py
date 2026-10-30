"""Synthetic paired V/I captures test validators, never qualify real boards."""

import copy
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import unittest

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.hil import measure
from tools.testing import test_hil_measurements


class HilPowerTests(unittest.TestCase):
    def setUp(self):
        self.fixture = test_hil_measurements.HilMeasurementTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root = self.fixture.root
        self.fixture.protocol.resolved.update(
            soc_family="stm32f4", clock={"profile": "synthetic-fixed"},
            flash={}, memory={}, source_dependencies=[{"sha256": "a" * 64}])

    def records(self, *, energy_maximum=1, power_maximum=1,
                current_gain=1, current_offset=0):
        self.fixture.fixture.station["budgets"] = {
            "power_mean": {"maximum": power_maximum, "unit": "W"},
            "power_peak": {"maximum": power_maximum, "unit": "W"},
            "energy": {"maximum": energy_maximum, "unit": "J"},
        }
        admission, raw = self.fixture.record()
        now = datetime.now(timezone.utc)
        self.calibration = self.root / "power-calibration.json"
        atomic_json(self.calibration, {
            "schema_version": 1, "kind": "physical_hil_power_calibration",
            "station_id": admission["station_id"],
            "instrument_id": "synthetic-meter", "calibration_id": "model-1",
            "calibrated_utc": (now - timedelta(days=1)).isoformat(),
            "valid_until_utc": (now + timedelta(days=1)).isoformat(),
            "timebase": {"unit": "ns", "resolution_ns": 1,
                         "accuracy_ppm": 50},
            "voltage": {"unit": "mV", "gain": 1, "offset": 0,
                        "minimum": 0, "maximum": 5000, "resolution": 1},
            "current": {"unit": "mA", "gain": current_gain,
                        "offset": current_offset,
                        "minimum": 0, "maximum": 1000, "resolution": 1}})
        power = {
            "supply_domain": "mcu-vdd", "included_loads": ["mcu"],
            "probe_influence": "synthetic fixture; no instrument operated",
            "anti_alias_filter": "synthetic bounded-bandwidth input",
            "bandwidth_hz": 1, "sample_period_ns": 500000000,
            "interval_tolerance_ns": 1, "maximum_pair_skew_ns": 1,
            "window_start_ns": 0, "window_end_ns": 1000000000}
        context_path = Path(raw["context"]["path"])
        context = json.loads(context_path.read_text())
        context["measurements"] = {
            name: {"method": "paired_voltage_current",
                   "instrument": file_identity(self.calibration),
                   "resolution": 0.001, "observation_window_s": 1,
                   "power": copy.deepcopy(power)}
            for name in admission["budgets"]}
        atomic_json(context_path, context)
        raw["context"] = file_identity(context_path)
        self.capture = self.root / "power-capture.json"
        atomic_json(self.capture, {
            "schema_version": 1, "kind": "physical_hil_power_capture",
            "admission_sha256": raw["admission_sha256"],
            "context_sha256": raw["context"]["sha256"],
            "dropped_samples": 0, "overrange": False,
            "samples": [{"sequence": index,
                         "voltage_time_ns": index * 500000000,
                         "current_time_ns": index * 500000000,
                         "voltage": 3300, "current": current}
                        for index, current in enumerate((100, 200, 300))]})
        raw["traces"] = []
        for name, budget in admission["budgets"].items():
            path = self.root / (name + ".json")
            atomic_json(path, {
                "schema_version": 2, "kind": "physical_hil_trace",
                "metric": name, "unit": budget["unit"],
                "method": "paired_voltage_current",
                "admission_sha256": raw["admission_sha256"],
                "context_sha256": raw["context"]["sha256"],
                "samples": [{"sequence": 0,
                             "capture": file_identity(self.capture)}]})
            raw["traces"].append(file_identity(path))
        return admission, raw

    def rewrite_capture(self, raw, capture):
        atomic_json(self.capture, capture)
        for index, identity in enumerate(raw["traces"]):
            path = Path(identity["path"])
            trace = json.loads(path.read_text())
            trace["samples"][0]["capture"] = file_identity(self.capture)
            atomic_json(path, trace)
            raw["traces"][index] = file_identity(path)

    def rewrite_context(self, raw, context):
        path = Path(raw["context"]["path"])
        atomic_json(path, context)
        raw["context"] = file_identity(path)
        capture = json.loads(self.capture.read_text())
        capture["context_sha256"] = raw["context"]["sha256"]
        self.rewrite_capture(raw, capture)
        for index, identity in enumerate(raw["traces"]):
            path = Path(identity["path"])
            trace = json.loads(path.read_text())
            trace["context_sha256"] = raw["context"]["sha256"]
            atomic_json(path, trace)
            raw["traces"][index] = file_identity(path)

    def test_opt_in_plan_does_not_claim_equipment_execution(self):
        fixture = self.fixture.fixture.fixture
        resolved = self.fixture.protocol.resolved
        regular = measure.measurement_plan(fixture, resolved)
        plan = measure.measurement_plan(fixture, resolved, include_power=True)
        self.assertEqual(len(regular["measurements"]), 7)
        self.assertEqual(len(plan["measurements"]), 10)
        self.assertEqual(plan["physical_status"], "not_executed")
        self.assertTrue(all(item["budget"] is None
                            for item in plan["measurements"]))

    def test_raw_units_pairs_and_trapezoids_derive_all_three_metrics(self):
        admission, raw = self.records()
        metrics = measure.validate_measurements(raw, admission)["metrics"]
        self.assertAlmostEqual(metrics["power_mean"]["mean"], 0.66)
        self.assertAlmostEqual(metrics["power_peak"]["maximum"], 0.99)
        self.assertAlmostEqual(metrics["energy"]["mean"], 0.66)
        capture = metrics["energy"]["captures"][0]
        self.assertEqual(capture["samples"], 3)
        self.assertEqual(capture["raw"], file_identity(self.capture))
        self.assertEqual(capture["calibration"],
                         file_identity(self.calibration))

    def test_calibration_gain_and_offset_are_applied_in_input_units(self):
        admission, raw = self.records(energy_maximum=3, power_maximum=3,
                                      current_gain=2, current_offset=100)
        metrics = measure.validate_measurements(raw, admission)["metrics"]
        self.assertAlmostEqual(metrics["power_mean"]["mean"], 1.65)
        self.assertAlmostEqual(metrics["power_peak"]["maximum"], 2.31)
        self.assertAlmostEqual(metrics["energy"]["mean"], 1.65)

    def test_scalar_power_summaries_cannot_replace_raw_capture(self):
        admission, raw = self.records()
        path = Path(raw["traces"][0]["path"])
        trace = json.loads(path.read_text())
        trace["samples"] = [{"sequence": 0, "value": 0.01}]
        atomic_json(path, trace)
        raw["traces"][0] = file_identity(path)
        with self.assertRaises(EvidenceError):
            measure.validate_measurements(raw, admission)

    def test_missing_repeated_reordered_skewed_samples_reject(self):
        admission, raw = self.records()
        original = json.loads(self.capture.read_text())
        for change in ("missing", "duplicate", "reorder", "gap", "skew"):
            capture = copy.deepcopy(original)
            if change == "missing":
                capture["samples"].pop(1)
            elif change == "duplicate":
                capture["samples"][1]["sequence"] = 0
            elif change == "reorder":
                capture["samples"].reverse()
            elif change == "gap":
                capture["samples"][1]["voltage_time_ns"] += 20
            else:
                capture["samples"][1]["current_time_ns"] += 20
            self.rewrite_capture(raw, capture)
            with self.subTest(change=change), self.assertRaises(EvidenceError):
                measure.validate_measurements(raw, admission)

    def test_wrong_range_type_timestamps_and_binding_reject(self):
        admission, raw = self.records()
        original = json.loads(self.capture.read_text())
        for key, value in (("voltage", True), ("voltage", -1),
                           ("voltage", 5001), ("current", 1001),
                           ("voltage_time_ns", -1)):
            capture = copy.deepcopy(original)
            capture["samples"][0][key] = value
            self.rewrite_capture(raw, capture)
            with self.subTest(key=key, value=value), \
                    self.assertRaises(EvidenceError):
                measure.validate_measurements(raw, admission)
        for key in ("admission_sha256", "context_sha256"):
            capture = copy.deepcopy(original)
            capture[key] = "0" * 64
            self.rewrite_capture(raw, capture)
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                measure.validate_measurements(raw, admission)

    def test_changed_or_expired_calibration_rejects_even_after_rehash(self):
        admission, raw = self.records()
        original = json.loads(self.calibration.read_text())
        for change in ("station", "expired", "unit", "gain", "resolution"):
            calibration = copy.deepcopy(original)
            if change == "station":
                calibration["station_id"] = "another-station"
            elif change == "expired":
                calibration["valid_until_utc"] = "2020-01-01T00:00:00Z"
            else:
                key, value = {"unit": ("unit", "watts"),
                              "gain": ("gain", 0),
                              "resolution": ("resolution", 0)}[change]
                calibration["current"][key] = value
            atomic_json(self.calibration, calibration)
            context = json.loads(Path(raw["context"]["path"]).read_text())
            for conditions in context["measurements"].values():
                conditions["instrument"] = file_identity(self.calibration)
            self.rewrite_context(raw, context)
            with self.subTest(change=change), self.assertRaises(EvidenceError):
                measure.validate_measurements(raw, admission)

    def test_rehashed_window_domain_bandwidth_reject(self):
        admission, raw = self.records()
        original = json.loads(Path(raw["context"]["path"]).read_text())
        for key, value in (("supply_domain", ""), ("included_loads", []),
                           ("window_end_ns", 2000000000),
                           ("sample_period_ns", 0), ("bandwidth_hz", 3),
                           ("maximum_pair_skew_ns", 500000000)):
            context = copy.deepcopy(original)
            context["measurements"]["energy"]["power"][key] = value
            self.rewrite_context(raw, context)
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                measure.validate_measurements(raw, admission)

    def test_derived_energy_must_meet_reviewed_budget(self):
        admission, raw = self.records(energy_maximum=0.5)
        with self.assertRaisesRegex(EvidenceError, "budget failed: energy"):
            measure.validate_measurements(raw, admission)

    def test_overrange_dropped_samples_and_changed_clock_are_rejected(self):
        admission, raw = self.records()
        original = json.loads(self.capture.read_text())
        for key, value in (("overrange", True), ("dropped_samples", 1)):
            capture = copy.deepcopy(original)
            capture[key] = value
            self.rewrite_capture(raw, capture)
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                measure.validate_measurements(raw, admission)
        self.rewrite_capture(raw, original)
        calibration = json.loads(self.calibration.read_text())
        calibration["timebase"]["unit"] = "us"
        atomic_json(self.calibration, calibration)
        context = json.loads(Path(raw["context"]["path"]).read_text())
        for conditions in context["measurements"].values():
            conditions["instrument"] = file_identity(self.calibration)
        self.rewrite_context(raw, context)
        with self.assertRaises(EvidenceError):
            measure.validate_measurements(raw, admission)

    def test_comparison_keeps_domain_window_load_and_calibration_equal(self):
        admission, raw = self.records()
        result = measure.compare_measurements(raw, admission, raw, admission)
        self.assertEqual(result["status"], "compared")
        self.assertEqual(result["metrics"]["energy"]
                         ["candidate_minus_baseline"]["mean"], 0)
        candidate = copy.deepcopy(raw)
        path = self.root / "candidate-context.json"
        context = json.loads(Path(raw["context"]["path"]).read_text())
        context["measurements"]["energy"]["power"]["supply_domain"] = "board"
        atomic_json(path, context)
        candidate["context"] = file_identity(path)
        capture = json.loads(self.capture.read_text())
        capture["context_sha256"] = candidate["context"]["sha256"]
        capture_path = self.root / "candidate-capture.json"
        atomic_json(capture_path, capture)
        candidate["traces"] = []
        for identity in raw["traces"]:
            path = Path(identity["path"])
            trace = json.loads(path.read_text())
            trace["context_sha256"] = candidate["context"]["sha256"]
            trace["samples"][0]["capture"] = file_identity(capture_path)
            output = self.root / ("candidate-" + path.name)
            atomic_json(output, trace)
            candidate["traces"].append(file_identity(output))
        with self.assertRaisesRegex(EvidenceError,
                                    "comparison conditions differ"):
            measure.compare_measurements(raw, admission, candidate, admission)

    def test_cli_report_cannot_overwrite_nested_capture(self):
        admission, raw = self.records()
        admission_path = self.root / "admission.json"
        raw_path = self.root / "measurements.json"
        atomic_json(admission_path, admission)
        atomic_json(raw_path, raw)
        original = self.capture.read_bytes()
        with self.assertRaises(SystemExit):
            measure.main(["validate", "--admission", str(admission_path),
                          "--raw-evidence", str(raw_path),
                          "--report", str(self.capture)])
        self.assertEqual(self.capture.read_bytes(), original)

    def test_capture_identity_and_nonfinite_raw_values_are_rejected(self):
        admission, raw = self.records()
        capture = json.loads(self.capture.read_text())
        self.capture.write_text("changed raw capture")
        with self.assertRaises(EvidenceError):
            measure.validate_measurements(raw, admission)

        capture["samples"][1]["current"] = float("nan")
        self.capture.write_text(json.dumps(capture))
        for index, identity in enumerate(raw["traces"]):
            path = Path(identity["path"])
            trace = json.loads(path.read_text())
            trace["samples"][0]["capture"] = file_identity(self.capture)
            atomic_json(path, trace)
            raw["traces"][index] = file_identity(path)
        with self.assertRaises(EvidenceError):
            measure.validate_measurements(raw, admission)

    def test_power_capture_enumeration_has_a_separate_processing_bound(self):
        admission, raw = self.records()
        path = Path(raw["traces"][0]["path"])
        trace = json.loads(path.read_text())
        capture = trace["samples"][0]["capture"]
        trace["samples"] = [{"sequence": sequence, "capture": capture}
                            for sequence in range(129)]
        atomic_json(path, trace)
        raw["traces"][0] = file_identity(path)
        with self.assertRaisesRegex(EvidenceError, "bounded sample count"):
            measure.validate_measurements(raw, admission)

    def test_duplicate_capture_bytes_cannot_inflate_sample_count(self):
        admission, raw = self.records()
        alias = self.root / "same-capture-other-path.json"
        alias.write_bytes(self.capture.read_bytes())
        path = Path(raw["traces"][0]["path"])
        trace = json.loads(path.read_text())
        trace["samples"].append({"sequence": 1,
                                 "capture": file_identity(alias)})
        atomic_json(path, trace)
        raw["traces"][0] = file_identity(path)
        with self.assertRaisesRegex(EvidenceError, "duplicate power capture"):
            measure.validate_measurements(raw, admission)

    def test_oversized_numeric_calibration_raises_evidence_error(self):
        admission, raw = self.records()
        calibration = json.loads(self.calibration.read_text())
        calibration["current"]["gain"] = 10 ** 400
        atomic_json(self.calibration, calibration)
        context = json.loads(Path(raw["context"]["path"]).read_text())
        for conditions in context["measurements"].values():
            conditions["instrument"] = file_identity(self.calibration)
        self.rewrite_context(raw, context)
        with self.assertRaises(EvidenceError):
            measure.validate_measurements(raw, admission)

    def test_positive_channels_cannot_silently_underflow_to_zero_power(self):
        admission, raw = self.records(current_gain=1e-200)
        calibration = json.loads(self.calibration.read_text())
        calibration["voltage"]["gain"] = 1e-200
        atomic_json(self.calibration, calibration)
        context = json.loads(Path(raw["context"]["path"]).read_text())
        for conditions in context["measurements"].values():
            conditions["instrument"] = file_identity(self.calibration)
        self.rewrite_context(raw, context)
        with self.assertRaises(EvidenceError):
            measure.validate_measurements(raw, admission)


if __name__ == "__main__":
    unittest.main()
