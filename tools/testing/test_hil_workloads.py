"""Synthetic workload protocols test preparation, never physical equipment."""

import copy
import importlib
import json
import math
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.identity import canonical_digest


class HilWorkloadTests(unittest.TestCase):
    def setUp(self):
        self.module = importlib.import_module("tools.hil.workloads")
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.fixture = json.loads(Path(
            "tools/hil/fixtures/stm32f407_qiming_v31.json").read_text())
        self.resolved = {
            "schema_version": 1, "board": self.fixture["board"],
            "part": self.fixture["part"], "backend": "freertos",
            "cpu": {"arch": "cortex-m4", "fpu": "fpv4-sp-d16"},
            "kernel_profile": {"tickless": True},
            "controllers": [],
        }
        modes = {"gpio": "output", "uart": "dma-duplex-continuous",
                 "spi": "dma", "watchdog": "independent",
                 "i2c": "poll-messages", "adc": "trigger-dma-continuous",
                 "pwm": "fixed-pwm", "exti": "edge-event", "flash": "poll-byte",
                 "can": "classic-irq", "eth": "raw-rmii",
                 "usb_device": "device-fs", "timer_capture": "edge-capture",
                 "advanced_pwm": "complementary-pwm"}
        for kind, mode in modes.items():
            self.resolved["controllers"].append({
                "id": kind + "0", "kind": kind,
                "controller": "USART1" if kind == "uart" else kind.upper(),
                "mode": mode,
                "route": "synthetic-validator-route-" + kind, "pins": [],
            })
        uart = self.resolved["controllers"][1]
        uart.update(baud=115200, pins=[
            {"function": "tx", "pin": "PA9"},
            {"function": "rx", "pin": "PA10"},
        ])
        self.instrument = self.root / "instrument.json"
        self.instrument.write_text("synthetic config; no equipment run\n")

    def plan(self, workload=None):
        return self.module.workload_plan(
            self.fixture, self.resolved, workload)

    def case(self, name, plan=None):
        return next(item for item in (plan or self.plan())["cases"]
                    if item["id"] == name)

    def reviewed(self, names=("station_binding",)):
        cases = []
        for name in names:
            item = self.case(name)
            cases.append({
                "id": name,
                "bindings": [controller["id"] for controller in
                             self.resolved["controllers"] if
                             controller["kind"] in item["required_kinds"]],
                "stimulus": "Synthetic protocol description, never executed",
                "raw_observations": {key: "Retain observed " + key for
                                     key in item["raw_observations"]},
                "instruments": {role: file_identity(self.instrument) for
                                role in item["instruments"]},
                "safety": {key: "Review and isolate " + key for
                           key in item["safety"]},
                "limits": {key: {"minimum": 1 if key in
                                 self.module.WORK_AMOUNTS
                                 else 0, "maximum": 100,
                                 "unit": unit} for key, unit in
                           item["metric_units"].items()},
            })
        return {"schema_version": 1, "kind": "hil_workload",
                "board": self.fixture["board"], "part": self.fixture["part"],
                "cases": cases}

    def validate(self, workload):
        return self.module.validate_workload(
            workload, self.fixture, self.resolved)

    def input_files(self):
        fixture = self.root / "fixture.json"
        resolved = self.root / "resolved.json"
        atomic_json(fixture, self.fixture)
        value = copy.deepcopy(self.resolved)
        value["configuration_sha256"] = canonical_digest(value)
        atomic_json(resolved, value)
        return fixture, resolved

    def cli(self, fixture, resolved, report, *extra):
        return self.module.main([
            "plan", "--fixture", str(fixture), "--resolved", str(resolved),
            "--report", str(report), *map(str, extra)])

    def test_twenty_five_cases_cover_requested_items_without_measurements(self):
        report = self.plan()
        self.assertEqual(len(report["cases"]), 25)
        self.assertEqual({item["work_item"] for item in report["cases"]},
                         set(range(47, 58)) | set(range(25, 31)) | {7, 12, 14, 15})
        self.assertEqual(report["status"], "prepared_unbound")
        self.assertEqual(report["physical_status"], "not_executed")
        for item in report["cases"]:
            self.assertEqual(item["status"], "not_executed")
            self.assertIsNone(item["reviewed_protocol"])
            self.assertTrue(all(value is None for value in
                                item["limits"].values()))
            for key in ("stimulus", "raw_observations", "instruments",
                        "safety"):
                self.assertTrue(item[key])

    def test_preparation_never_operates_subprocess_or_devices(self):
        with patch("subprocess.run", side_effect=AssertionError("equipment")), \
                patch("subprocess.Popen",
                      side_effect=AssertionError("equipment")):
            self.assertEqual(self.plan()["physical_status"], "not_executed")

    def test_plan_preserves_only_selected_routes_and_modes(self):
        report = self.plan()
        self.assertEqual(report["controllers"], self.resolved["controllers"])
        self.assertEqual(self.case("uart_spi_dma")["availability"], "selected")

    def test_chip_capability_is_not_selected_board_route(self):
        self.resolved["controllers"] = self.resolved["controllers"][:2]
        self.resolved["routes"] = {"spi-extra": {"kind": "spi", "mode": "dma"}}
        item = self.case("uart_spi_dma")
        self.assertEqual(item["availability"], "requires_selection")
        self.assertIn("spi", item["selection_gaps"])
        self.assertEqual(len(self.plan()["controllers"]), 2)

    def test_polling_modes_do_not_establish_dma_selection(self):
        self.resolved["controllers"][1]["mode"] = "irq-byte-event"
        self.resolved["controllers"][2]["mode"] = "short-poll"
        item = self.case("uart_spi_dma")
        self.assertEqual(item["availability"], "requires_selection")
        self.assertIn("uart_or_spi_dma", item["selection_gaps"])

    def test_context_requires_selected_m4f_freertos_tuple(self):
        for change in ({"arch": "cortex-m3", "fpu": "none"},
                       {"arch": "cortex-m4", "fpu": "none"}):
            self.resolved["cpu"] = change
            self.assertEqual(self.case("m4f_context")["availability"],
                             "unsupported")

    def test_tickless_plan_does_not_install_timer_port(self):
        self.resolved["kernel_profile"]["tickless"] = False
        self.assertEqual(self.case("tickless_timer")["availability"],
                         "requires_selection")

    def test_baremetal_does_not_claim_freertos_task_cases(self):
        self.resolved["backend"] = "baremetal"
        for name in ("m4f_context", "stack_peaks", "lifecycle_endurance",
                     "tickless_timer", "fail_stop"):
            self.assertEqual(self.case(name)["availability"], "unsupported")

    def test_reviewed_protocol_is_still_unbound_and_input_is_unchanged(self):
        selected = tuple(item["id"] for item in self.plan()["cases"]
                         if item["availability"] == "selected")
        self.assertEqual(len(selected), 21)
        workload = self.reviewed(selected)
        before = copy.deepcopy(workload)
        report = self.plan(workload)
        self.assertEqual(workload, before)
        self.assertEqual(report["physical_status"], "not_executed")
        self.assertEqual({item["id"] for item in report["cases"]
                          if item["reviewed_protocol"] is not None}, set(selected))
        report["cases"][0]["reviewed_protocol"]["stimulus"] = "changed"
        self.assertEqual(workload, before)

    def test_authored_success_flags_and_unknown_fields_are_rejected(self):
        workload = self.reviewed()
        for key, value in (("physical_status", "passed"), ("status", "passed"),
                           ("unknown", True)):
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                self.validate({**workload, key: value})
        workload["cases"][0]["status"] = "passed"
        with self.assertRaises(EvidenceError):
            self.validate(workload)

    def test_exact_fixture_build_and_workload_board_part_must_match(self):
        workload = self.reviewed()
        for key in ("board", "part"):
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                self.validate({**workload, key: "other"})
            old = self.resolved[key]
            self.resolved[key] = "other"
            with self.assertRaises(EvidenceError):
                self.plan()
            self.resolved[key] = old

    def test_missing_raw_definition_or_unreviewed_extra_is_rejected(self):
        for missing in (True, False):
            workload = self.reviewed()
            observations = workload["cases"][0]["raw_observations"]
            if missing:
                observations.pop(next(iter(observations)))
            else:
                observations["self_asserted_success"] = "passed"
            with self.assertRaises(EvidenceError):
                self.validate(workload)

    def test_instrument_and_safety_inventory_cannot_be_weakened(self):
        for key in ("instruments", "safety"):
            workload = self.reviewed()
            workload["cases"][0][key] = {}
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                self.validate(workload)

    def test_instrument_bytes_are_verified_and_cannot_be_symlink(self):
        workload = self.reviewed()
        self.instrument.write_text("changed")
        with self.assertRaises(EvidenceError):
            self.validate(workload)
        link = self.root / "instrument-link"
        link.symlink_to(self.instrument)
        role = next(iter(workload["cases"][0]["instruments"]))
        identity = file_identity(self.instrument)
        workload["cases"][0]["instruments"][role] = {
            **identity, "path": str(link)}
        with self.assertRaises(EvidenceError):
            self.validate(workload)

    def test_unselected_duplicate_and_insufficient_bindings_fail(self):
        workload = self.reviewed(("i2c_electrical",))
        for bindings in (["not-selected"], ["i2c0", "i2c0"], ["gpio0"]):
            workload["cases"][0]["bindings"] = bindings
            with self.subTest(bindings=bindings), \
                    self.assertRaises(EvidenceError):
                self.validate(workload)

    def test_authored_case_cannot_promote_unselected_or_unsupported_mode(self):
        workload = self.reviewed(("uart_spi_dma",))
        self.resolved["controllers"][2]["mode"] = "short-poll"
        self.resolved["controllers"][1]["mode"] = "irq-byte-event"
        with self.assertRaises(EvidenceError):
            self.validate(workload)
        self.resolved["backend"] = "baremetal"
        with self.assertRaises(EvidenceError):
            self.validate(self.reviewed(("stack_peaks",)))

    def test_limits_require_reviewed_numbers_units_and_exact_metric_names(self):
        workload = self.reviewed()
        limits = workload["cases"][0]["limits"]
        metric = next(iter(limits))
        for bad in (None, {}, {"maximum": None, "unit": "count"},
                    {"maximum": True, "unit": "count"},
                    {"maximum": -1, "unit": "count"},
                    {"maximum": math.inf, "unit": "count"},
                    {"maximum": math.nan, "unit": "count"},
                    {"maximum": 10 ** 400, "unit": "count"},
                    {"minimum": 2, "maximum": 1, "unit": "count"},
                    {"maximum": 1, "unit": "us"},
                    {"maximum": 1, "unit": "count", "unknown": 1}):
            limits[metric] = bad
            with self.subTest(budget=bad), self.assertRaises(EvidenceError):
                self.validate(workload)
        workload = self.reviewed()
        workload["cases"][0]["limits"]["extra"] = {
            "maximum": 1, "unit": "count"}
        with self.assertRaises(EvidenceError):
            self.validate(workload)

    def test_endurance_and_power_cut_require_reviewed_lower_bounds(self):
        for name, metric in (("concurrent_load", "duration"),
                             ("lifecycle_endurance", "iterations"),
                             ("flash_power_loss", "power_cut_cases")):
            for minimum in (None, 0):
                workload = self.reviewed((name,))
                bound = workload["cases"][0]["limits"][metric]
                if minimum is None:
                    bound.pop("minimum")
                else:
                    bound["minimum"] = minimum
                with self.subTest(name=name, minimum=minimum), \
                        self.assertRaises(EvidenceError):
                    self.validate(workload)

    def test_missing_unknown_repeated_and_empty_cases_fail(self):
        workload = self.reviewed()
        for cases in ([], [*workload["cases"], *workload["cases"]],
                      [{**workload["cases"][0], "id": "unknown"}]):
            with self.subTest(cases=cases), self.assertRaises(EvidenceError):
                self.validate({**workload, "cases": cases})

    def test_blank_descriptions_and_invalid_types_fail(self):
        for key, value in (("stimulus", " "), ("stimulus", True),
                           ("bindings", "uart0"), ("raw_observations", [])):
            workload = self.reviewed()
            workload["cases"][0][key] = value
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                self.validate(workload)
        with self.assertRaises(EvidenceError):
            self.validate({**self.reviewed(), "schema_version": True})

    def test_empty_authored_workload_cannot_become_unreviewed_plan(self):
        for workload in ({}, [], False):
            with self.subTest(workload=workload), \
                    self.assertRaises(EvidenceError):
                self.plan(workload)

    def test_unknown_mode_containing_dma_does_not_establish_dma_support(self):
        self.resolved["controllers"][1]["mode"] = "future-dma-mode"
        self.resolved["controllers"][2]["mode"] = "future-dma-mode"
        self.assertEqual(self.case("uart_spi_dma")["availability"],
                         "requires_selection")

    def test_polling_adc_does_not_promise_unselected_trigger_capture(self):
        self.resolved["controllers"][5]["mode"] = "single-shot"
        item = self.case("adc_pwm_exti")
        self.assertEqual(item["availability"], "selected")
        self.assertNotIn("adc_trigger_overrun", item["raw_observations"])

    def test_native_backend_never_establishes_physical_workload_selection(self):
        self.resolved["backend"] = "native"
        self.assertTrue(all(item["availability"] == "unsupported" for
                            item in self.plan()["cases"]))

    def test_cli_writes_exact_input_identities_and_does_not_run_equipment(self):
        fixture, resolved = self.input_files()
        report = self.root / "plan.json"
        with patch("subprocess.Popen", side_effect=AssertionError("equipment")):
            self.assertEqual(self.cli(fixture, resolved, report), 0)
        value = json.loads(report.read_text())
        self.assertEqual(value["fixture"], file_identity(fixture))
        self.assertEqual(value["resolved"], file_identity(resolved))
        self.assertEqual(value["physical_status"], "not_executed")

    def test_cli_requires_unused_report_and_preserves_all_inputs(self):
        fixture, resolved = self.input_files()
        workload_path = self.root / "workload.json"
        atomic_json(workload_path, self.reviewed())
        for output in (fixture, resolved, workload_path, self.instrument):
            original = output.read_bytes()
            with self.subTest(output=output), self.assertRaises(SystemExit):
                self.cli(fixture, resolved, output, "--workload", workload_path)
            self.assertEqual(output.read_bytes(), original)
        existing = self.root / "previous.json"
        existing.write_text("retained report")
        with self.assertRaises(SystemExit):
            self.cli(fixture, resolved, existing)
        self.assertEqual(existing.read_text(), "retained report")

    def test_cli_rejects_symlink_report_and_parent_without_replacing(self):
        fixture, resolved = self.input_files()
        target = self.root / "retained.txt"
        target.write_text("retained")
        link = self.root / "report-link"
        link.symlink_to(target)
        with self.assertRaises(SystemExit):
            self.cli(fixture, resolved, link)
        folder = self.root / "folder-link"
        folder.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(SystemExit):
            self.cli(fixture, resolved, folder / "new.json")
        self.assertEqual(target.read_text(), "retained")

    def test_cli_failed_configuration_never_creates_physical_success(self):
        fixture, resolved = self.input_files()
        resolved.write_text('{"schema_version": 1}')
        report = self.root / "failed.json"
        self.assertEqual(self.cli(fixture, resolved, report), 1)
        value = json.loads(report.read_text())
        self.assertEqual(value["status"], "failed")
        self.assertEqual(value["physical_status"], "not_executed")

    def test_cli_preserves_declared_instrument_when_bytes_are_missing(self):
        fixture, resolved = self.input_files()
        workload = self.reviewed()
        missing = self.root / "missing-instrument.json"
        instrument = next(iter(workload["cases"][0]["instruments"].values()))
        instrument["path"] = str(missing)
        path = self.root / "workload.json"
        atomic_json(path, workload)
        with self.assertRaises(SystemExit):
            self.cli(fixture, resolved, missing, "--workload", path)
        self.assertFalse(missing.exists())

    def test_template_is_explicitly_unreviewed_and_not_a_board_fixture(self):
        template_path = Path("tools/hil/templates/workload.json")
        template = json.loads(template_path.read_text())
        self.assertEqual(template["kind"], "hil_workload")
        self.assertIsNone(template["board"])
        self.assertIsNone(template["part"])
        self.assertEqual(len(template["cases"]), 25)
        with self.assertRaises(EvidenceError):
            self.validate(template)


if __name__ == "__main__":
    unittest.main()
