"""Behavior tests use actual linked ARM LOAD segments, not source size guesses."""

import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

from tools.evidence.common import EvidenceError, atomic_json
from tools.measurement.elf import Elf32, interval_bytes
from tools.measurement.resources import allocation, measure, validate_budget
from tests.contracts.test_evidence_support import link_fixture


class ResourceValidationTests(unittest.TestCase):
    def test_interval_union_prevents_duplicate_stack_cost(self):
        self.assertEqual(interval_bytes([(0, 100), (60, 90), (90, 200)]), 200)

    def test_invalid_elf_is_rejected(self):
        for data in (b"", b"not an elf", b"\x7fELF" + b"\0" * 128):
            with self.subTest(data=data):
                with self.assertRaises(EvidenceError):
                    Elf32(data)

    def test_invalid_budget_is_not_disabled(self):
        memory = [{"id": "sram"}]
        valid = {"schema_version": 1, "flash_loaded_max": 4096,
            "flash_footprint_max": 4096, "ram_reserved_max": {"sram": 4096},
            "msp_max": 1024, "heap_max": 0}
        for key, value in (("msp_max", True), ("heap_max", -1),
                           ("flash_loaded_max", None), ("ram_reserved_max", {}),
                           ("unknown", 3)):
            with self.subTest(key=key):
                with self.assertRaises(EvidenceError):
                    validate_budget({**valid, key: value}, memory)

    def test_elf_initialization_bytes_and_msp_are_counted_once(self):
        with tempfile.TemporaryDirectory() as directory:
            elf_path, resolved_path, budget_path = link_fixture(Path(directory))
            report = measure(elf_path, resolved_path, budget_path)
            self.assertEqual(report["status"], "passed")
            elf = Elf32(elf_path.read_bytes())
            flash_bytes = sum(load["files"] for load in elf.loads)
            self.assertEqual(report["metrics"]["flash"]["loaded_bytes"], flash_bytes)
            self.assertEqual(report["metrics"]["reservations"]["msp"]["bytes"], 1024)
            # 4 initialized + 16 BSS + 64 heap + 1024 MSP, no duplicate reservation.
            self.assertEqual(report["metrics"]["ram"]["sram"]["reserved_bytes"], 1108)
            self.assertEqual(report["metrics"]["ram"]["ccm"]["reserved_bytes"], 0)

    def test_exceeded_budget_returns_failed(self):
        with tempfile.TemporaryDirectory() as directory:
            elf, resolved, budget = link_fixture(Path(directory))
            value = json.loads(budget.read_text())
            value["msp_max"] = 1023
            atomic_json(budget, value)
            report = measure(elf, resolved, budget)
            self.assertEqual(report["status"], "failed")
            self.assertEqual(report["violations"][0]["metric"], "msp")

    def test_stale_configuration_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            elf, resolved, budget = link_fixture(Path(directory))
            value = json.loads(resolved.read_text())
            value["board"] = "different-board"
            atomic_json(resolved, value)
            with self.assertRaisesRegex(EvidenceError, "digest mismatch"):
                measure(elf, resolved, budget)

    def test_cli_failure_retains_report_and_nonzero_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ("bad.elf", "resolved.json", "budget.json"):
                (root / name).write_text("invalid")
            report = root / "report.json"
            result = subprocess.run([sys.executable, "tools/measurement/resources.py",
                "--elf", str(root / "bad.elf"), "--resolved", str(root / "resolved.json"),
                "--budget", str(root / "budget.json"), "--report", str(report)],
                capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(json.loads(report.read_text())["status"], "failed")

    def test_report_cannot_destroy_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            elf, resolved, budget = link_fixture(root)
            before = elf.read_bytes()
            result = subprocess.run([sys.executable, "tools/measurement/resources.py",
                "--elf", str(elf), "--resolved", str(resolved), "--budget", str(budget),
                "--report", str(elf)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(elf.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
