"""Exercise OS evidence boundaries with synthetic bytes, never real hardware."""

import copy
from datetime import datetime, timedelta, timezone
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.identity import canonical_digest
from tools.hil.os_acceptance import main, os_plan, validate_os_run


class OSAcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.resolved = {"board": "fixture_board", "part": "fixture_part",
                         "backend": "freertos"}
        self.admission = {"kind": "hil_admission", "status": "ready",
                          "station_id": "fixture", "board": "fixture_board",
                          "part": "fixture_part", "chip_uid": "fixture_uid",
                          "budgets": {
                              "os_irq_latency": {"maximum": 100, "unit": "us"},
                              "os_deadline_lateness": {"maximum": 100, "unit": "us"},
                              "os_low_power_lateness": {"maximum": 100, "unit": "us"},
                              "os_lifecycle": {"minimum": 2, "maximum": 100,
                                               "unit": "count"},
                              "os_fail_response": {"maximum": 100, "unit": "us"}}}
        for key in ("station", "fixture", "elf", "resolved", "resources"):
            path = self.root / (key + ".json")
            atomic_json(path, {"cpu": {"arch": "cortex-m4", "fpu": "none",
                                       "mve": "none"}} if key == "resolved" else
                        {"synthetic": "validator-only"})
            self.admission[key] = file_identity(path)
        self.now = datetime.now(timezone.utc)
        context = {"schema_version": 1, "kind": "os_hil_context",
                   "admission_sha256": canonical_digest(self.admission),
                   "features": {"fpu": False, "mve": False},
                   "load": "validator fixture", "executor": "fixture-owner",
                   "stack_names": ["MSP", "owner"],
                   "irq_priorities": {"UART": 5}, "cpu_clock_hz": 1000000,
                   "requirements": {
                       "irq_wake": {"maximum_us": 100},
                       "deadline": {"maximum_lateness_us": 100},
                       "low_power": {"maximum_lateness_us": 100},
                       "lifecycle": {"minimum_iterations": 2},
                       "fail_stop": {"maximum_response_us": 100}}}
        for key in ("workload", "instrument"):
            path = self.root / (key + ".json")
            atomic_json(path, {"synthetic": key})
            context[key] = file_identity(path)
        self.context_path = self.root / "acquisition-context.json"
        atomic_json(self.context_path, context)
        self.context = context
        self.record = {"schema_version": 1, "kind": "physical_os_hil_run",
                       "started_utc": (self.now - timedelta(seconds=2)).isoformat(),
                       "finished_utc": (self.now - timedelta(seconds=1)).isoformat(),
                       "station_id": "fixture", "board": "fixture_board",
                       "part": "fixture_part", "chip_uid": "fixture_uid",
                       "admission_sha256": canonical_digest(self.admission),
                       "elf_sha256": self.admission["elf"]["sha256"],
                       "resolved_sha256": self.admission["resolved"]["sha256"],
                       "context": file_identity(self.context_path), "traces": []}
        self.traces = {
            "irq_wake": {"samples": [{"sequence": 0, "irq_us": 1,
                                        "task_us": 3}]},
            "context": {"registers": [
                {"name": "r" + str(index), "expected": index,
                 "observed": index} for index in range(4, 12)]},
            "stack": {"stacks": [{"name": "MSP", "capacity_bytes": 512,
                                     "used_bytes": 128},
                                    {"name": "owner", "capacity_bytes": 1024,
                                     "used_bytes": 256}]},
            "deadline": {"samples": [{"sequence": 0, "deadline_us": 100,
                                        "return_us": 101}]},
            "lifecycle": {"iterations": 2, "admitted": 2, "settled": 2,
                            "joined": 2, "retained": 0, "stale_accesses": 0},
            "low_power": {"samples": [{"sequence": 0, "before_us": 1,
                                         "after_us": 3, "wake_deadline_us": 2,
                                         "latched_events": 1,
                                         "observed_events": 1}]},
            "fail_stop": {"scenarios": [
                {"id": "assert", "trigger_us": 1, "safe_us": 2,
                 "expected_output": 0, "observed_output": 0},
                {"id": "stack_overflow", "trigger_us": 3, "safe_us": 4,
                 "expected_output": 0, "observed_output": 0}]}}
        for name, trace in self.traces.items():
            self.write_trace(name, trace)
        self.rebuild = patch("tools.hil.os_acceptance.admit",
                             return_value=copy.deepcopy(self.admission))
        self.mock_admit = self.rebuild.start()
        self.addCleanup(self.rebuild.stop)

    def write_trace(self, name, fields):
        path = self.root / (name + ".json")
        atomic_json(path, {"schema_version": 1, "kind": "physical_os_trace",
                          "case": name,
                          "context_sha256": self.record["context"]["sha256"],
                          **fields})
        self.record["traces"] = [identity for identity in self.record["traces"]
                                 if Path(identity["path"]).stem != name]
        self.record["traces"].append(file_identity(path))

    def validate(self):
        return validate_os_run(self.record, self.admission, now=self.now)

    def test_plan_keeps_seven_hardware_cases_unexecuted(self):
        plan = os_plan(self.resolved)
        self.assertEqual(plan["physical_status"], "not_executed")
        self.assertEqual(plan["status"], "prepared_unbound")
        self.assertEqual(len(plan["cases"]), 7)
        self.assertTrue(all(case["budget"] is None for case in plan["cases"]))
        self.assertTrue(all(case["status"] == "not_executed"
                            for case in plan["cases"]))

    def test_plan_rejects_unknown_or_missing_backend(self):
        for backend in (None, "other", True):
            with self.subTest(backend=backend), self.assertRaises(EvidenceError):
                os_plan({**self.resolved, "backend": backend})

    def test_retained_seven_case_record_qualifies_only_recorded_scope(self):
        result = self.validate()
        self.assertEqual(result["status"], "passed")
        self.assertEqual(len(result["cases"]), 7)
        self.assertEqual(result["physical_status"],
                         "qualified_for_recorded_os_cases")

    def test_missing_duplicate_or_unknown_trace_rejected(self):
        for traces in (self.record["traces"][:-1],
                       self.record["traces"] + self.record["traces"][:1], []):
            with self.subTest(traces=len(traces)), self.assertRaises(EvidenceError):
                validate_os_run({**self.record, "traces": traces},
                                self.admission, now=self.now)
        self.write_trace("unknown", {})
        with self.assertRaises(EvidenceError):
            self.validate()

    def test_changed_trace_bytes_context_or_admission_rejected(self):
        path = self.root / "irq_wake.json"
        path.write_text("changed")
        with self.assertRaises(EvidenceError):
            self.validate()
        self.write_trace("irq_wake", self.traces["irq_wake"])
        atomic_json(self.context_path, {**self.context, "load": "changed"})
        with self.assertRaises(EvidenceError):
            self.validate()
        atomic_json(self.context_path, self.context)
        changed = copy.deepcopy(self.admission)
        changed["chip_uid"] = "another"
        with self.assertRaises(EvidenceError):
            validate_os_run(self.record, changed, now=self.now)

    def test_empty_boolean_negative_and_out_of_order_timing_rejected(self):
        for samples in ([], [{"sequence": 0, "irq_us": True, "task_us": 3}],
                        [{"sequence": 0, "irq_us": 3, "task_us": 1}],
                        [{"sequence": 1, "irq_us": 1, "task_us": 3}],
                        [{"sequence": 0, "irq_us": 1, "task_us": 102}]):
            self.write_trace("irq_wake", {"samples": samples})
            with self.subTest(samples=samples), self.assertRaises(EvidenceError):
                self.validate()

    def test_context_requires_integer_registers_and_real_comparison(self):
        for registers in ([], [{"name": "r4", "expected": 1, "observed": 2}],
                          [{"name": "r4", "expected": True, "observed": 1}],
                          [{"name": "s0", "expected": 1, "observed": 1}]):
            self.write_trace("context", {"registers": registers})
            with self.subTest(registers=registers), self.assertRaises(EvidenceError):
                self.validate()

    def test_fp_or_mve_enabled_requires_coprocessor_and_vpr_samples(self):
        for feature, expected in (("fpu", "s0"), ("mve", "vpr")):
            context = copy.deepcopy(self.context)
            context["features"][feature] = True
            atomic_json(self.context_path, context)
            self.record["context"] = file_identity(self.context_path)
            for name, trace in self.traces.items():
                self.write_trace(name, trace)
            with self.subTest(feature=feature), self.assertRaises(EvidenceError):
                self.validate()
        self.assertEqual(expected, "vpr")

    def test_stack_requires_msp_capacity_and_task_observations(self):
        for stacks in ([], [{"name": "owner", "capacity_bytes": 10,
                             "used_bytes": 1}],
                       [{"name": "MSP", "capacity_bytes": 10,
                         "used_bytes": 11}],
                       [{"name": "MSP", "capacity_bytes": True,
                         "used_bytes": 1}]):
            self.write_trace("stack", {"stacks": stacks})
            with self.subTest(stacks=stacks), self.assertRaises(EvidenceError):
                self.validate()

    def test_stack_measurement_covers_complete_declared_workload_inventory(self):
        from tools.hil.os_acceptance import _stacks
        trace = copy.deepcopy(self.traces["stack"])
        context = {"stack_names": ["MSP", "owner", "producer"]}
        with self.assertRaises(EvidenceError):
            _stacks(trace, context)
        trace["stacks"].append({"name": "producer", "capacity_bytes": 512,
                                 "used_bytes": 128})
        self.assertEqual(_stacks(trace, context)["stacks"], 3)

    def test_deadline_overshoot_does_not_accept_average(self):
        self.write_trace("deadline", {"samples": [
            {"sequence": 0, "deadline_us": 100, "return_us": 101},
            {"sequence": 1, "deadline_us": 100, "return_us": 1000}]})
        with self.assertRaises(EvidenceError):
            self.validate()

    def test_lifecycle_retained_borrows_or_missing_joins_fail(self):
        for key, value in (("settled", 1), ("joined", 1), ("retained", 1),
                           ("stale_accesses", 1), ("iterations", 0)):
            self.write_trace("lifecycle", {**self.traces["lifecycle"], key: value})
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                self.validate()

    def test_low_power_clock_regression_or_event_loss_rejected(self):
        before = self.traces["low_power"]["samples"][0]
        for change in ({"after_us": 0}, {"observed_events": 0},
                       {"after_us": 1000}, {"latched_events": False}):
            self.write_trace("low_power", {"samples": [{**before, **change}]})
            with self.subTest(change=change), self.assertRaises(EvidenceError):
                self.validate()

    def test_fail_stop_needs_assert_and_overflow_observed_outputs(self):
        for scenarios in ([], self.traces["fail_stop"]["scenarios"][:1],
                          [{**row, "observed_output": 1}
                           for row in self.traces["fail_stop"]["scenarios"]]):
            self.write_trace("fail_stop", {"scenarios": scenarios})
            with self.subTest(scenarios=scenarios), self.assertRaises(EvidenceError):
                self.validate()

    def test_freshness_and_strict_context_reject_self_asserted_success(self):
        for change in ({"status": "passed"}, {"started_utc": "2000-01-01"},
                       {"finished_utc": (self.now + timedelta(seconds=1)).isoformat()}):
            with self.subTest(change=change), self.assertRaises(EvidenceError):
                validate_os_run({**self.record, **change}, self.admission,
                                now=self.now)
        atomic_json(self.context_path, {**self.context, "synthetic": True})
        self.record["context"] = file_identity(self.context_path)
        with self.assertRaises(EvidenceError):
            self.validate()


    def test_fp_and_mve_full_shared_bank_is_checked(self):
        for fpu, mve in ((True, False), (False, True), (True, True)):
            resolved_path = Path(self.admission["resolved"]["path"])
            atomic_json(resolved_path, {"cpu": {
                "arch": "cortex-m55", "fpu": "fpv5-sp-d16" if fpu else "none",
                "mve": "int" if mve else "none"}})
            self.admission["resolved"] = file_identity(resolved_path)
            self.mock_admit.return_value = copy.deepcopy(self.admission)
            self.context["features"] = {"fpu": fpu, "mve": mve}
            digest = canonical_digest(self.admission)
            self.context["admission_sha256"] = digest
            atomic_json(self.context_path, self.context)
            self.record["context"] = file_identity(self.context_path)
            self.record["admission_sha256"] = digest
            self.record["resolved_sha256"] = self.admission["resolved"]["sha256"]
            registers = copy.deepcopy(self.traces["context"]["registers"])
            registers += [{"name": "s" + str(index), "expected": index + 17,
                           "observed": index + 17} for index in range(32)]
            if mve:
                registers.append({"name": "vpr", "expected": 7, "observed": 7})
            for name, trace in self.traces.items():
                self.write_trace(name, {"registers": registers}
                                 if name == "context" else trace)
            with self.subTest(fpu=fpu, mve=mve):
                self.assertEqual(self.validate()["status"], "passed")
            registers[-1]["observed"] ^= 1
            self.write_trace("context", {"registers": registers})
            with self.assertRaises(EvidenceError):
                self.validate()

    def test_station_budgets_cannot_be_relaxed_in_context(self):
        for case, key in (("irq_wake", "maximum_us"),
                          ("lifecycle", "minimum_iterations")):
            context = copy.deepcopy(self.context)
            context["requirements"][case][key] += 1
            atomic_json(self.context_path, context)
            self.record["context"] = file_identity(self.context_path)
            for name, trace in self.traces.items():
                self.write_trace(name, trace)
            with self.subTest(case=case), self.assertRaises(EvidenceError):
                self.validate()

    def test_cli_preserves_retained_inputs_even_when_hashes_are_bad(self):
        admission_path = self.root / "admission.json"
        raw_path = self.root / "run.json"
        atomic_json(admission_path, self.admission)
        atomic_json(raw_path, self.record)
        for path in (admission_path, raw_path, self.context_path,
                     self.root / "irq_wake.json", self.root / "workload.json",
                     self.root / "instrument.json"):
            before = path.read_bytes()
            arguments = ["validate", "--admission", str(admission_path),
                         "--raw-evidence", str(raw_path), "--report", str(path)]
            with self.subTest(path=path.name), self.assertRaises(SystemExit):
                main(arguments)
            self.assertEqual(path.read_bytes(), before)
        atomic_json(self.context_path, {**self.context, "load": "changed"})
        before = (self.root / "workload.json").read_bytes()
        with self.assertRaises(SystemExit):
            main(["validate", "--admission", str(admission_path),
                  "--raw-evidence", str(raw_path), "--report",
                  str(self.root / "workload.json")])
        self.assertEqual((self.root / "workload.json").read_bytes(), before)

    def test_cli_validation_writes_derived_scope_and_never_operates_equipment(self):
        admission_path = self.root / "admission.json"
        raw_path = self.root / "run.json"
        atomic_json(admission_path, self.admission)
        atomic_json(raw_path, self.record)
        output = self.root / "validation.json"
        self.assertEqual(main(["validate", "--admission", str(admission_path),
                               "--raw-evidence", str(raw_path), "--report",
                               str(output)]), 0)
        self.assertTrue(output.is_file())


    def test_cli_requires_fresh_destination_even_for_unrelated_existing_file(self):
        admission_path = self.root / "admission.json"
        raw_path = self.root / "run.json"
        atomic_json(admission_path, self.admission)
        atomic_json(raw_path, self.record)
        output = self.root / "prior-report.json"
        output.write_text("retained prior evidence\n")
        before = output.read_bytes()
        with self.assertRaises(SystemExit):
            main(["validate", "--admission", str(admission_path),
                  "--raw-evidence", str(raw_path), "--report", str(output)])
        self.assertEqual(output.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
