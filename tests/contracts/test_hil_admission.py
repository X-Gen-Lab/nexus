"""Offline admission fault tests; synthetic records never qualify physical boards."""

import copy
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import tempfile
import unittest

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.identity import canonical_digest
from tools.hil.admit import admit, validate_fixture, validate_raw, validate_station
from tools.measurement.resources import measure
from tests.contracts.test_evidence_support import link_fixture


class HilAdmissionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        raw = self.root / "model-record.txt"
        raw.write_text("synthetic validator fault fixture; no equipment executed\n")
        identity = file_identity(raw)
        self.fixture = {"schema_version": 1, "kind": "hil_fixture", "board": "test-board",
            "part": "STM32F407ZGT6", "transport": "swd", "probe_interfaces": ["cmsis-dap"],
            "serial": {"controller": "USART1", "tx": "PA9", "rx": "PA10", "baudrate": 115200,
                       "electrical_mode": "ttl_3v3"},
            "required_tests": ["probe_identity", "flash_readback", "reset", "uart_echo"],
            "physical_status": "not_executed", "pending_qualification": ["physical pending"]}
        self.station = {"schema_version": 1, "kind": "hil_station", "station_id": "test-station",
            "board": "test-board", "part": "STM32F407ZGT6", "pcb_revision": "test-revision",
            "chip_uid": "test-uid", "probe": {"serial": "test-serial", "interface": "cmsis-dap",
                "transport": "swd", "tool": identity, "interface_config": identity,
                "target_config": identity}, "serial": {"path": "/dev/serial/by-id/test-fixture",
                "baudrate": 115200, "electrical_mode": "ttl_3v3", "wiring_checked": True,
                "ground_connected": True}, "power": {"source": "model-supply", "voltage_mv": 3300},
            "outputs_disconnected": True, "budgets": {"latency": {"maximum": 100, "unit": "us"}}}
        self.identity = identity

    def prepared(self):
        elf, resolved, budget = link_fixture(self.root)
        resources = self.root / "resources.json"
        atomic_json(resources, measure(elf, resolved, budget))
        fixture_path, station_path = self.root / "fixture.json", self.root / "station.json"
        atomic_json(fixture_path, self.fixture)
        atomic_json(station_path, self.station)
        admission = admit(station_path, fixture_path, elf, resolved, resources)
        return admission, (station_path, fixture_path, elf, resolved, resources)

    def raw_record(self, admission):
        now = datetime.now(timezone.utc)
        return {"schema_version": 1, "kind": "physical_hil", "status": "passed",
            "started_utc": (now - timedelta(seconds=2)).isoformat(),
            "finished_utc": (now - timedelta(seconds=1)).isoformat(),
            "station_id": admission["station_id"], "board": admission["board"],
            "part": admission["part"], "chip_uid": admission["chip_uid"],
            "elf_sha256": admission["elf"]["sha256"],
            "resolved_sha256": admission["resolved"]["sha256"],
            "admission_sha256": canonical_digest(admission),
            "operations": [{"name": name, "status": "passed", "exit_code": 0,
                "raw": self.identity} for name in
                ("identify", "flash", "verify_flash", "reset", "serial", "cleanup")],
            "tests": [{"name": name, "status": "passed", "executed_cases": 1,
                "raw": self.identity} for name in admission["required_tests"]],
            "metrics": {"latency": {"value": 20, "unit": "us", "raw": self.identity}}}

    def test_three_actual_templates_are_unqualified(self):
        fixtures = list(Path("tools/hil/fixtures").glob("*.json"))
        actual = [path for path in fixtures if path.name != "station.template.json"]
        self.assertEqual(len(actual), 3)
        for path in actual:
            value = json.loads(path.read_text())
            validate_fixture(value)
            self.assertEqual(value["physical_status"], "not_executed")

    def test_empty_station_is_rejected(self):
        station = json.loads(Path("tools/hil/fixtures/station.template.json").read_text())
        with self.assertRaises(EvidenceError):
            validate_station(station, self.fixture)

    def test_wrong_board_and_part_are_rejected(self):
        for key in ("board", "part"):
            value = {**self.station, key: "wrong-hardware"}
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                validate_station(value, self.fixture)

    def test_missing_probe_identity_and_unsafe_wiring_are_rejected(self):
        for key, replacement in (("serial", None), ("transport", "jtag")):
            station = copy.deepcopy(self.station)
            station["probe"][key] = replacement
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                validate_station(station, self.fixture)
        station = copy.deepcopy(self.station)
        station["serial"]["ground_connected"] = False
        with self.assertRaises(EvidenceError):
            validate_station(station, self.fixture)

    def test_readiness_does_not_claim_hardware_execution(self):
        admission, _ = self.prepared()
        self.assertEqual(admission["status"], "ready")
        self.assertEqual(admission["physical_status"], "not_executed")

    def test_changed_elf_and_resources_are_rejected(self):
        _, inputs = self.prepared()
        inputs[2].write_bytes(inputs[2].read_bytes() + b"different")
        with self.assertRaises(EvidenceError):
            admit(*inputs)

    def test_stale_empty_all_skipped_and_wrong_board_raw_rejected(self):
        admission, _ = self.prepared()
        original = self.raw_record(admission)
        changed = []
        value = copy.deepcopy(original)
        value["started_utc"] = (datetime.now(timezone.utc) - timedelta(days=2)).isoformat()
        changed.append(value)
        changed.append({**original, "tests": []})
        value = copy.deepcopy(original)
        for test in value["tests"]:
            test["status"] = "skipped"
        changed.append(value)
        changed.append({**original, "board": "different-board"})
        changed.append({**original, "kind": "host_model"})
        for value in changed:
            with self.subTest(value=value), self.assertRaises(EvidenceError):
                validate_raw(value, admission)

    def test_missing_raw_and_zero_cases_rejected(self):
        admission, _ = self.prepared()
        original = self.raw_record(admission)
        for replacement in (0, -1, True):
            value = copy.deepcopy(original)
            value["tests"][0]["executed_cases"] = replacement
            with self.subTest(replacement=replacement), self.assertRaises(EvidenceError):
                validate_raw(value, admission)
        value = copy.deepcopy(original)
        value["operations"][0]["raw"]["sha256"] = "0" * 64
        with self.assertRaises(EvidenceError):
            validate_raw(value, admission)

    def test_physical_budget_failure_rejected(self):
        admission, _ = self.prepared()
        value = self.raw_record(admission)
        value["metrics"]["latency"]["value"] = 101
        with self.assertRaises(EvidenceError):
            validate_raw(value, admission)


if __name__ == "__main__":
    unittest.main()
