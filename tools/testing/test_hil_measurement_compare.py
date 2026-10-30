"""Synthetic before/after records exercise comparison, never physical HIL."""

import copy
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import unittest

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.identity import canonical_digest
from tools.hil import measure
from tools.testing import test_hil_measurements


class HilMeasurementComparisonTests(unittest.TestCase):
    def setUp(self):
        self.fixture = test_hil_measurements.HilMeasurementTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root = self.fixture.root
        self.fixture.protocol.resolved.update(
            soc_family="stm32f4", clock={"profile": "synthetic-fixed"},
            flash={}, memory={},
            source_dependencies=[{"sha256": "a" * 64}])

    def records(self, cycles=False):
        baseline, first = (self.fixture.dwt_record() if cycles else
                           self.fixture.record())
        candidate = copy.deepcopy(baseline)
        second = copy.deepcopy(first)
        directory = self.root / "candidate"
        directory.mkdir()
        elf = directory / "elf.json"
        elf.write_text("synthetic candidate, not executable firmware\n")
        candidate["elf"] = file_identity(elf)
        resolved = json.loads(Path(baseline["resolved"]["path"]).read_text())
        resolved["source_dependencies"] = [{"sha256": "b" * 64}]
        resolved.pop("configuration_sha256")
        resolved["configuration_sha256"] = canonical_digest(resolved)
        config = directory / "resolved.json"
        atomic_json(config, resolved)
        candidate["resolved"] = file_identity(config)
        second.update(elf_sha256=candidate["elf"]["sha256"],
                      resolved_sha256=candidate["resolved"]["sha256"],
                      admission_sha256=canonical_digest(candidate))
        context = json.loads(Path(first["context"]["path"]).read_text())
        context["admission_sha256"] = second["admission_sha256"]
        if cycles:
            conditions = context["measurements"]["cycles"]
            reference = conditions["reference"]
            clock = json.loads(Path(reference["instrument"]["path"]).read_text())
            clock.update(admission_sha256=second["admission_sha256"],
                         epoch=2, cycle_epoch=2)
            clock_path = directory / "reference-clock.json"
            atomic_json(clock_path, clock)
            reference.update(epoch=2, instrument=file_identity(clock_path))
            conditions["cycle_epoch"] = 2
        context_path = directory / "context.json"
        atomic_json(context_path, context)
        second["context"] = file_identity(context_path)
        second["traces"] = []
        for identity in first["traces"]:
            trace = json.loads(Path(identity["path"]).read_text())
            trace.update(admission_sha256=second["admission_sha256"],
                         context_sha256=second["context"]["sha256"])
            if trace["metric"] == "cycles":
                for sample in trace["samples"]:
                    for point in (sample["begin"], sample["end"]):
                        point["cycle_epoch"] = 2
                        point["reference"]["epoch"] = 2
            elif not cycles and trace["metric"] == "irq_latency":
                trace["samples"] = [{"sequence": i, "value": value}
                                    for i, value in enumerate((2, 4, 6))]
            path = directory / Path(identity["path"]).name
            atomic_json(path, trace)
            second["traces"].append(file_identity(path))
        self.fixture.mock_admit.side_effect = lambda *paths: copy.deepcopy(
            candidate if paths[2] == elf else baseline)
        return first, baseline, second, candidate

    def context(self, raw, value=None):
        path = Path(raw["context"]["path"])
        if value is None:
            return json.loads(path.read_text())
        atomic_json(path, value)
        raw["context"] = file_identity(path)
        for index, identity in enumerate(raw["traces"]):
            trace_path = Path(identity["path"])
            trace = json.loads(trace_path.read_text())
            trace["context_sha256"] = raw["context"]["sha256"]
            trace["method"] = value["measurements"][trace["metric"]]["method"]
            atomic_json(trace_path, trace)
            raw["traces"][index] = file_identity(trace_path)
        return None

    def arguments(self, records, report):
        names = ("baseline_raw", "baseline_admission", "candidate_raw",
                 "candidate_admission")
        arguments = ["compare"]
        for name, value in zip(names, records):
            path = self.root / (name + ".json")
            atomic_json(path, value)
            arguments.extend(["--" + name.replace("_", "-"), str(path)])
        return [*arguments, "--report", str(report)]

    def test_changed_firmware_and_source_keep_exact_runtime_conditions(self):
        records = self.records()
        result = measure.compare_measurements(*records)
        self.assertEqual(result["status"], "compared")
        metric = result["metrics"]["irq_latency"]
        self.assertEqual(metric["candidate_minus_baseline"],
                         {"minimum": 1, "maximum": -2, "mean": 0})
        self.assertEqual(metric["samples_per_run"], 3)
        self.assertNotEqual(result["baseline"]["elf"]["sha256"],
                            result["candidate"]["elf"]["sha256"])
        self.assertIn("WCET", result["limits"])
        self.assertEqual(self.fixture.mock_admit.call_count, 2)

    def test_load_clock_executor_priorities_and_acquisition_changes_reject(self):
        records = self.records()
        raw = records[2]
        original = self.context(raw)
        replacements = [
            ("cpu_clock_hz", 84000000), ("load", "different interference"),
            ("executor", "different priority"), ("irq_priorities", {"USART1": 7})]
        for key, value in replacements:
            changed = copy.deepcopy(original)
            changed[key] = value
            self.context(raw, changed)
            with self.subTest(key=key), self.assertRaisesRegex(
                    EvidenceError, "comparison conditions differ"):
                measure.compare_measurements(*records)
        for key, value in (("method", "hardware_timer"), ("resolution", 1),
                           ("observation_window_s", 2)):
            changed = copy.deepcopy(original)
            changed["measurements"]["irq_latency"][key] = value
            self.context(raw, changed)
            with self.subTest(key=key), self.assertRaisesRegex(
                    EvidenceError, "comparison conditions differ"):
                measure.compare_measurements(*records)

    def test_changed_hardware_identity_rejects_even_valid_individual_runs(self):
        records = self.records()
        raw, admission = records[2:]
        raw["chip_uid"] = admission["chip_uid"] = "another-physical-chip"
        raw["admission_sha256"] = canonical_digest(admission)
        context = self.context(raw)
        context["admission_sha256"] = raw["admission_sha256"]
        self.context(raw, context)
        for index, identity in enumerate(raw["traces"]):
            path = Path(identity["path"])
            trace = json.loads(path.read_text())
            trace["admission_sha256"] = raw["admission_sha256"]
            atomic_json(path, trace)
            raw["traces"][index] = file_identity(path)
        with self.assertRaisesRegex(EvidenceError, "comparison conditions differ"):
            measure.compare_measurements(*records)

    def test_rehashed_configuration_mode_change_is_not_an_optimization(self):
        records = self.records()
        raw, admission = records[2:]
        path = Path(admission["resolved"]["path"])
        resolved = json.loads(path.read_text())
        resolved["controllers"][2]["mode"] = "short-poll"
        resolved.pop("configuration_sha256")
        resolved["configuration_sha256"] = canonical_digest(resolved)
        atomic_json(path, resolved)
        admission["resolved"] = file_identity(path)
        raw.update(resolved_sha256=admission["resolved"]["sha256"],
                   admission_sha256=canonical_digest(admission))
        context = self.context(raw)
        context["admission_sha256"] = raw["admission_sha256"]
        self.context(raw, context)
        for index, identity in enumerate(raw["traces"]):
            trace_path = Path(identity["path"])
            trace = json.loads(trace_path.read_text())
            trace["admission_sha256"] = raw["admission_sha256"]
            atomic_json(trace_path, trace)
            raw["traces"][index] = file_identity(trace_path)
        with self.assertRaisesRegex(EvidenceError, "comparison conditions differ"):
            measure.compare_measurements(*records)

    def test_trace_bytes_and_sample_counts_are_revalidated(self):
        records = self.records()
        raw = records[2]
        path = Path(raw["traces"][0]["path"])
        trace = json.loads(path.read_text())
        trace["samples"].pop()
        atomic_json(path, trace)
        with self.assertRaises(EvidenceError):
            measure.compare_measurements(*records)
        raw["traces"][0] = file_identity(path)
        with self.assertRaisesRegex(EvidenceError, "sample count"):
            measure.compare_measurements(*records)

    def test_old_baseline_requires_an_explicit_freshness_window(self):
        records = self.records()
        now = datetime.now(timezone.utc)
        records[0].update(started_utc=(now - timedelta(days=2)).isoformat(),
                          finished_utc=(now - timedelta(days=2,
                                                       seconds=-1)).isoformat())
        with self.assertRaisesRegex(EvidenceError, "stale"):
            measure.compare_measurements(*records, now=now)
        result = measure.compare_measurements(*records, now=now,
                                               maximum_age_s=7 * 86400)
        self.assertEqual(result["status"], "compared")

    def test_independent_dwt_epochs_keep_all_seven_metric_contracts(self):
        records = self.records(cycles=True)
        result = measure.compare_measurements(*records)
        self.assertEqual(len(result["metrics"]), 7)
        self.assertEqual(result["metrics"]["cycles"]["candidate_minus_baseline"],
                         {"minimum": 0, "maximum": 0, "mean": 0})

    def test_cli_retains_original_inputs_and_reports_actual_comparison(self):
        records = self.records()
        report = self.root / "comparison.json"
        arguments = self.arguments(records, report)
        self.assertEqual(measure.main(arguments), 0)
        result = json.loads(report.read_text())
        self.assertEqual(result["kind"], "hil_measurement_comparison")
        self.assertEqual(result["status"], "compared")
        self.assertEqual(result["baseline_raw"], file_identity(
            self.root / "baseline_raw.json"))

    def test_cli_rejects_any_existing_destination_before_reading_inputs(self):
        records = self.records()
        output = Path(records[2]["traces"][0]["path"])
        before = output.read_bytes()
        with self.assertRaises(SystemExit) as failure:
            measure.main(self.arguments(records, output))
        self.assertEqual(failure.exception.code, 1)
        self.assertEqual(output.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
