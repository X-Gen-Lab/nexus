"""SoC HIL contracts use synthetic bytes; no equipment is qualified here."""

import copy
from datetime import datetime, timedelta, timezone
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.identity import canonical_digest
from tools.hil import soc_acceptance
from tools.testing import test_hil_workloads


class SocHilTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        fixture = test_hil_workloads.HilWorkloadTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        self.fixture = copy.deepcopy(fixture.fixture)
        self.resolved = copy.deepcopy(fixture.resolved)
        self.resolved["configuration_sha256"] = canonical_digest(self.resolved)
        self.admission = {"schema_version": 1, "kind": "hil_admission",
                          "status": "ready", "physical_status": "not_executed",
                          "board": self.fixture["board"],
                          "part": self.fixture["part"],
                          "station_id": "synthetic-soc-station",
                          "chip_uid": "synthetic-soc-uid"}
        station = {"pcb_revision": "synthetic-revision"}
        for name, content in (("station", station), ("fixture", self.fixture),
                              ("resolved", self.resolved), ("elf", {}),
                              ("resources", {})):
            path = self.root / (name + ".json")
            atomic_json(path, content)
            self.admission[name] = file_identity(path)
        self.instrument = self.root / "instrument.txt"
        self.instrument.write_text("synthetic calibration for parser tests\n")
        self.admission_path = self.root / "admission.json"
        atomic_json(self.admission_path, self.admission)
        self.service = patch("tools.hil.soc_acceptance.admit",
                             return_value=copy.deepcopy(self.admission))
        self.mock_admit = self.service.start()
        self.addCleanup(self.service.stop)

    def protocol(self, case_id="SOC-37"):
        definition = soc_acceptance.CASES[case_id]
        return {"schema_version": 1, "kind": "soc_hil_protocol",
                "case_id": case_id, "board": self.fixture["board"],
                "part": self.fixture["part"], "backend": self.resolved["backend"],
                "controller_ids": [], "stimulus": "synthetic parser stimulus",
                "observations": {name: "synthetic observation obligation"
                                 for name in definition["observations"]},
                "instruments": {name: file_identity(self.instrument)
                                for name in definition["hardware"]},
                "safety": {name: "synthetic isolation obligation"
                           for name in definition["safety"]},
                "limits": {name: {"minimum": 1 if rule == "positive" else 0,
                                  "maximum": 0 if rule == "zero" else 100,
                                  "unit": unit}
                           for name, (unit, rule) in definition["metrics"].items()}}

    def record(self):
        protocol = self.protocol()
        protocol_path = self.root / "protocol.json"
        atomic_json(protocol_path, protocol)
        from tools.testing.test_hil_acquire import HilAcquireTests
        acquisition = HilAcquireTests()
        acquisition.setUp()
        self.addCleanup(acquisition.doCleanups)
        acquisition.station["pcb_revision"] = "synthetic-revision"
        station_path = Path(acquisition.admission["station"]["path"])
        atomic_json(station_path, acquisition.station)
        acquisition.admission["station"] = file_identity(station_path)
        acquisition.mock_admit.return_value = copy.deepcopy(acquisition.admission)
        atomic_json(acquisition.admission_path, acquisition.admission)
        acquisition.recipe["admission_sha256"] = canonical_digest(acquisition.admission)
        self.admission = copy.deepcopy(acquisition.admission)
        self.mock_admit.return_value = copy.deepcopy(self.admission)
        serial = next(step for step in acquisition.recipe["operations"]
                      if step["name"] == "serial")
        serial["inputs"].extend([file_identity(protocol_path), file_identity(self.instrument)])
        serial["arguments"].append(str(protocol_path))
        script = acquisition.script.read_text()
        marker = "record = {'schema_version': 1,"
        payload_code = r"""
if operation == 'serial':
 import hashlib
 protocol_path = pathlib.Path(sys.argv[5])
 protocol = json.loads(protocol_path.read_text())
 def identity(path):
  data = path.read_bytes()
  return {'path': str(path), 'sha256': hashlib.sha256(data).hexdigest(), 'size': len(data)}
 def write(path, value):
  path.write_text(json.dumps(value, sort_keys=True, indent=2)+'\n')
  return identity(path)
 def canonical(value):
  return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
 output = pathlib.Path(destination).parent
 observed = {}
 for name in protocol['observations']:
  path = output/('soc-'+name+'.txt')
  path.write_text('synthetic retained instrument byte fixture\n')
  observed[name] = identity(path)
 context = {'schema_version': 1, 'kind': 'soc_hil_context',
            'session': token, 'case_id': protocol['case_id'],
            'protocol_sha256': identity(protocol_path)['sha256'],
            'admission_sha256': canonical(bound),
            'cpu_clock_hz': 168000000, 'load': 'synthetic parser load',
            'executor': 'synthetic parser executor', 'mode_scope': {}, 'time_resolution_s': 0.001,
            'methods': {name: {'instrument': 'debug_probe', 'method': 'synthetic parser',
                               'resolution': 1, 'window_s': 0.001}
                        for name in protocol['limits']}}
 context_id = write(output/'soc-context.json', context)
 traces = {}
 for metric, limit in protocol['limits'].items():
  trace = {'schema_version': 1, 'kind': 'soc_hil_trace',
           'case_id': protocol['case_id'], 'metric': metric, 'unit': limit['unit'],
           'session': token, 'protocol_sha256': identity(protocol_path)['sha256'],
           'context_sha256': context_id['sha256'], 'observations_sha256': canonical(observed),
           'samples': [{'sequence': 0, 'value': limit['minimum']}]}
  traces[metric] = write(output/('soc-'+metric+'.json'), trace)
 observations['soc'] = {'schema_version': 1, 'kind': 'soc_hil_observation',
                        'case_id': protocol['case_id'],
                        'protocol_sha256': identity(protocol_path)['sha256'],
                        'context': context_id, 'observations': observed, 'traces': traces}
"""
        acquisition.script.write_text(script.replace(marker, payload_code + "\n" + marker))
        for step in acquisition.recipe["operations"]:
            step["inputs"][0] = file_identity(acquisition.script)
        acquisition.write_recipe()
        capture = soc_acceptance.capture(protocol_path, acquisition.recipe_path,
                                         acquisition.admission_path, acquisition.output)
        capture_path = acquisition.output / "soc-capture.json"
        import json
        serial_step = next(step for step in capture["operations"] if step["name"] == "serial")
        payload = json.loads(Path(serial_step["observation"]["path"]).read_text())["observations"]["soc"]
        context_id = payload["context"]
        observations = payload["observations"]
        traces = payload["traces"]
        now = datetime.now(timezone.utc)
        return {"schema_version": 1, "kind": "physical_soc_hil",
                "case_id": "SOC-37", "started_utc": capture["started_utc"],
                "finished_utc": capture["finished_utc"],
                "station_id": self.admission["station_id"], "chip_uid": self.admission["chip_uid"],
                "board": self.admission["board"], "part": self.admission["part"],
                "admission_sha256": canonical_digest(self.admission),
                "elf_sha256": self.admission["elf"]["sha256"],
                "resolved_sha256": self.admission["resolved"]["sha256"],
                "protocol": file_identity(protocol_path), "capture": file_identity(capture_path),
                "context": context_id,
                "observations": observations, "traces": traces}

    def test_catalog_covers_exactly_nineteen_physical_items(self):
        self.assertEqual(set(soc_acceptance.CASES), {f"SOC-{i}" for i in range(37, 56)})
        for case in soc_acceptance.CASES.values():
            for key in ("hardware", "stimulus", "observations", "assertions", "metrics", "safety"):
                self.assertTrue(case[key])

    def test_plan_keeps_all_cases_unexecuted_without_process(self):
        with patch("subprocess.Popen") as process:
            result = soc_acceptance.preparation_plan(self.fixture, self.resolved)
        process.assert_not_called()
        self.assertEqual(result["status"], "prepared_unbound")
        self.assertEqual(result["physical_status"], "not_executed")
        self.assertEqual(len(result["cases"]), 19)
        self.assertTrue(all(case["status"] == "not_executed" for case in result["cases"]))

    def test_unselected_peripheral_is_not_promoted(self):
        self.resolved["controllers"] = [item for item in self.resolved["controllers"]
                                        if item["kind"] != "usb_device"]
        result = soc_acceptance.preparation_plan(self.fixture, self.resolved)
        case = next(item for item in result["cases"] if item["id"] == "SOC-50")
        self.assertEqual(case["availability"], "requires_selection")

    def test_reviewed_protocol_accepts_exact_obligations(self):
        soc_acceptance.validate_protocol(self.protocol(), self.fixture, self.resolved)

    def test_zero_invariant_cannot_be_relaxed_by_budget(self):
        value = self.protocol()
        value["limits"]["identity_mismatches"]["maximum"] = 1
        with self.assertRaises(EvidenceError):
            soc_acceptance.validate_protocol(value, self.fixture, self.resolved)

    def test_positive_work_cannot_be_zero_or_boolean(self):
        for minimum in (0, False):
            value = self.protocol()
            value["limits"]["identifications"]["minimum"] = minimum
            with self.assertRaises(EvidenceError):
                soc_acceptance.validate_protocol(value, self.fixture, self.resolved)

    def test_unreviewed_null_budget_fails(self):
        value = self.protocol()
        value["limits"]["identifications"]["maximum"] = None
        with self.assertRaises(EvidenceError):
            soc_acceptance.validate_protocol(value, self.fixture, self.resolved)

    def test_protocol_unknown_case_and_field_rejected(self):
        for key, val in (("case_id", "SOC-56"), ("unknown", True)):
            value = self.protocol()
            value[key] = val
            with self.assertRaises(EvidenceError):
                soc_acceptance.validate_protocol(value, self.fixture, self.resolved)

    def test_unknown_controller_and_missing_instrument_rejected(self):
        for mutation in (lambda p: p.update(controller_ids=["missing"]),
                         lambda p: p["instruments"].clear()):
            value = self.protocol(); mutation(value)
            with self.assertRaises(EvidenceError):
                soc_acceptance.validate_protocol(value, self.fixture, self.resolved)

    def test_unselected_case_protocol_rejected(self):
        self.resolved["controllers"] = [item for item in self.resolved["controllers"]
                                        if item["kind"] != "usb_device"]
        with self.assertRaises(EvidenceError):
            soc_acceptance.validate_protocol(self.protocol("SOC-50"), self.fixture, self.resolved)

    def test_valid_record_only_qualifies_recorded_case(self):
        result = soc_acceptance.validate_record(self.record(), self.admission)
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["physical_status"], "qualified_for_recorded_soc_case")
        self.assertEqual(result["case_id"], "SOC-37")
        self.assertEqual(result["sample_counts"], {"identifications": 1, "identity_mismatches": 1})

    def test_changed_raw_bytes_rejected(self):
        value = self.record()
        Path(next(iter(value["observations"].values()))["path"]).write_text("changed")
        with self.assertRaises(EvidenceError):
            soc_acceptance.validate_record(value, self.admission)

    def test_empty_missing_or_zero_samples_rejected(self):
        for mutate in (lambda v: v["traces"].clear(),
                       lambda v: v.update(observations={}),
                       lambda v: v.update(elf_sha256="0"*64)):
            value = self.record(); mutate(value)
            with self.assertRaises(EvidenceError):
                soc_acceptance.validate_record(value, self.admission)

    def synchronize_synthetic_receipt(self, record):
        """Bind intentionally malformed adapter payloads to reach parser branches."""
        import json
        context_identity = record["context"]
        for name, identity in record["traces"].items():
            path = Path(identity["path"]); trace = json.loads(path.read_text())
            trace["context_sha256"] = context_identity["sha256"]
            trace["observations_sha256"] = canonical_digest(record["observations"])
            atomic_json(path, trace); record["traces"][name] = file_identity(path)
        capture_path = Path(record["capture"]["path"])
        capture = json.loads(capture_path.read_text())
        serial = next(step for step in capture["operations"] if step["name"] == "serial")
        path = Path(serial["observation"]["path"])
        observed = json.loads(path.read_text())
        observed["observations"]["soc"].update(
            context=record["context"], observations=record["observations"], traces=record["traces"])
        atomic_json(path, observed); serial["observation"] = file_identity(path)
        atomic_json(capture_path, capture); record["capture"] = file_identity(capture_path)

    def test_failed_invariant_derived_from_actual_samples(self):
        value = self.record(); path = Path(value["traces"]["identity_mismatches"]["path"])
        import json
        trace = json.loads(path.read_text()); trace["samples"][0]["value"] = 1
        atomic_json(path, trace); value["traces"]["identity_mismatches"] = file_identity(path)
        self.synchronize_synthetic_receipt(value)
        with self.assertRaisesRegex(EvidenceError, "sample fails reviewed limit"):
            soc_acceptance.validate_record(value, self.admission)

    def test_stale_record_rejected(self):
        value = self.record()
        value["started_utc"] = (datetime.now(timezone.utc)-timedelta(days=2)).isoformat()
        with self.assertRaises(EvidenceError):
            soc_acceptance.validate_record(value, self.admission)

    def test_capture_session_replay_rejected(self):
        value = self.record(); path = Path(value["traces"]["identifications"]["path"])
        import json
        trace = json.loads(path.read_text()); trace["session"] = "0"*32
        atomic_json(path, trace); value["traces"]["identifications"] = file_identity(path)
        self.synchronize_synthetic_receipt(value)
        with self.assertRaisesRegex(EvidenceError, "trace context or metric mismatch"):
            soc_acceptance.validate_record(value, self.admission)

    def test_malformed_admission_not_executed_before_capture(self):
        protocol = self.root / "protocol.json"; atomic_json(protocol, self.protocol())
        with patch("tools.hil.soc_acceptance.execute_acquisition") as execute:
            with self.assertRaises(EvidenceError):
                soc_acceptance.capture(protocol, self.root / "missing-recipe.json",
                                       self.admission_path, self.root / "output")
        execute.assert_not_called()

    def flash_protocol(self):
        protocol = self.protocol("SOC-52")
        protocol["controller_ids"] = ["flash0"]
        self.resolved["flash"] = {"origin": 0x08000000, "size": 0x100000,
                                   "erase_blocks": [16384]*4 + [65536] + [131072]*7}
        layout_path = self.root / "product-layout.json"
        layout = {"schema_version": 1, "kind": "product_flash_test_layout",
                  "board": self.fixture["board"], "part": self.fixture["part"],
                  "test_region": {"start": 0x080E0000, "size": 131072},
                  "protected_regions": [{"id": "firmware", "start": 0x08000000,
                                         "size": 0xE0000}]}
        atomic_json(layout_path, layout)
        protocol["flash_region"] = {"layout": file_identity(layout_path),
                                     **layout["test_region"],
                                     "recovery_policy": "synthetic parser recovery only"}
        return protocol

    def test_flash_requires_external_geometry_and_protected_layout(self):
        protocol = self.flash_protocol()
        soc_acceptance.validate_protocol(protocol, self.fixture, self.resolved)
        del protocol["flash_region"]
        with self.assertRaises(EvidenceError):
            soc_acceptance.validate_protocol(protocol, self.fixture, self.resolved)

    def test_flash_rejects_wrong_density_alignment_and_protected_overlap(self):
        import json
        for start, size in ((0x08100000, 131072), (0x080E0001, 131071),
                            (0x080C0000, 131072)):
            protocol = self.flash_protocol()
            protocol["flash_region"].update(start=start, size=size)
            path = Path(protocol["flash_region"]["layout"]["path"])
            layout = json.loads(path.read_text())
            layout["test_region"] = {"start": start, "size": size}
            atomic_json(path, layout)
            protocol["flash_region"]["layout"] = file_identity(path)
            with self.assertRaises(EvidenceError):
                soc_acceptance.validate_protocol(protocol, self.fixture, self.resolved)

    def test_flash_uses_actual_firmware_loads_before_capture(self):
        from tests.contracts.test_evidence_support import link_fixture
        elf, _, _ = link_fixture(self.root)
        protocol = self.flash_protocol()
        admission = {"elf": file_identity(elf)}
        soc_acceptance._flash_elf(protocol, admission)
        protocol["flash_region"].update(start=0x08000000, size=16384)
        with self.assertRaises(EvidenceError):
            soc_acceptance._flash_elf(protocol, admission)

    def test_threadx_does_not_inherit_freertos_tickless_case(self):
        self.resolved["backend"] = "threadx"
        case = next(item for item in soc_acceptance.preparation_plan(self.fixture, self.resolved)["cases"]
                    if item["id"] == "SOC-54")
        self.assertEqual(case["availability"], "unsupported")

    def test_all_metrics_require_actual_nonempty_contiguous_samples(self):
        import json
        for samples in ([], [{"sequence": 1, "value": 1}], [{"sequence": 0, "value": True}]):
            value = self.record()
            path = Path(value["traces"]["identifications"]["path"])
            trace = json.loads(path.read_text()); trace["samples"] = samples
            atomic_json(path, trace); value["traces"]["identifications"] = file_identity(path)
            self.synchronize_synthetic_receipt(value)
            with self.assertRaisesRegex(EvidenceError, "samples required|sequence is incomplete|sample fails reviewed limit"):
                soc_acceptance.validate_record(value, self.admission)

    def test_capture_cannot_be_qualification_without_actual_execution(self):
        import json
        value = self.record(); path = Path(value["capture"]["path"])
        capture = json.loads(path.read_text()); capture["physical_status"] = "not_executed"
        atomic_json(path, capture); value["capture"] = file_identity(path)
        with self.assertRaises(EvidenceError):
            soc_acceptance.validate_record(value, self.admission)

    def test_missing_context_or_changed_mode_scope_rejected(self):
        import json
        value = self.record(); path = Path(value["context"]["path"])
        context = json.loads(path.read_text()); context["mode_scope"] = {"uart0": "invented"}
        atomic_json(path, context); value["context"] = file_identity(path)
        self.synchronize_synthetic_receipt(value)
        with self.assertRaisesRegex(EvidenceError, "mode scope mismatch|window exceeds actual"):
            soc_acceptance.validate_record(value, self.admission)

    def test_unknown_pcb_revision_blocks_capture(self):
        station_path = Path(self.admission["station"]["path"])
        atomic_json(station_path, {"pcb_revision": "unknown"})
        self.admission["station"] = file_identity(station_path)
        self.mock_admit.return_value = copy.deepcopy(self.admission)
        with self.assertRaises(EvidenceError):
            soc_acceptance._admission(self.admission)

    def rebind_capture(self, record, capture):
        import json
        path = Path(record["capture"]["path"]); atomic_json(path, capture)
        record["capture"] = file_identity(path)
        context_path = Path(record["context"]["path"])
        context = json.loads(context_path.read_text())
        if "capture_sha256" in context:
            context["capture_sha256"] = record["capture"]["sha256"]
        atomic_json(context_path, context); record["context"] = file_identity(context_path)
        for name, identity in record["traces"].items():
            path = Path(identity["path"]); trace = json.loads(path.read_text())
            trace["context_sha256"] = record["context"]["sha256"]
            atomic_json(path, trace); record["traces"][name] = file_identity(path)

    def test_rehashed_wrong_observed_chip_identity_rejected(self):
        import json
        value = self.record(); capture = json.loads(Path(value["capture"]["path"]).read_text())
        step = capture["operations"][0]; path = Path(step["observation"]["path"])
        observed = json.loads(path.read_text()); observed["observations"]["chip_uid"] = "wrong-chip"
        atomic_json(path, observed); step["observation"] = file_identity(path)
        self.rebind_capture(value, capture)
        with self.assertRaises(EvidenceError):
            soc_acceptance.validate_record(value, self.admission)

    def test_opaque_serial_observation_cannot_admit_afterward_authored_traces(self):
        import json
        value = self.record(); capture = json.loads(Path(value["capture"]["path"]).read_text())
        step = next(item for item in capture["operations"] if item["name"] == "serial")
        path = Path(step["observation"]["path"]); observed = json.loads(path.read_text())
        observed["observations"] = {"scope": "synthetic opaque record"}
        atomic_json(path, observed); step["observation"] = file_identity(path)
        self.rebind_capture(value, capture)
        with self.assertRaises(EvidenceError):
            soc_acceptance.validate_record(value, self.admission)

    def test_wrong_case_argv_and_failure_residue_rejected(self):
        import json
        for mutate in (lambda c: c.update(soc_case_id="SOC-52"),
                       lambda c: c["operations"][0].update(argv=["unreviewed-tool"]),
                       lambda c: c.update(station_quarantined=True),
                       lambda c: c.update(cleanup_failure="retained equipment"),
                       lambda c: c["operations"][0].update(failure="failed tool")):
            value = self.record(); capture = json.loads(Path(value["capture"]["path"]).read_text())
            mutate(capture); self.rebind_capture(value, capture)
            with self.assertRaises(EvidenceError):
                soc_acceptance.validate_record(value, self.admission)

    def test_unbounded_post_capture_measurement_window_rejected(self):
        import json
        value = self.record(); path = Path(value["context"]["path"])
        context = json.loads(path.read_text())
        for conditions in context["methods"].values():
            conditions["window_s"] = 3600
        atomic_json(path, context); value["context"] = file_identity(path)
        self.synchronize_synthetic_receipt(value)
        with self.assertRaisesRegex(EvidenceError, "mode scope mismatch|window exceeds actual"):
            soc_acceptance.validate_record(value, self.admission)

    def test_case_template_keeps_limits_and_instruments_unreviewed(self):
        template = soc_acceptance.case_template("SOC-37", self.fixture, self.resolved)
        self.assertEqual(template["status"], "prepared_unbound")
        self.assertEqual(template["physical_status"], "not_executed")
        protocol = template["protocol"]
        self.assertEqual(protocol["case_id"], "SOC-37")
        self.assertTrue(all(value is None for value in protocol["instruments"].values()))
        self.assertTrue(all(value["minimum"] is None and value["maximum"] is None
                            for value in protocol["limits"].values()))
        with self.assertRaises(EvidenceError):
            soc_acceptance.validate_protocol(protocol, self.fixture, self.resolved)


if __name__ == "__main__":
    unittest.main()
