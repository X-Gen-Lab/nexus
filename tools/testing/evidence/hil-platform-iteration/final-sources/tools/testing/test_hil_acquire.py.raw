"""Real subprocess fixtures exercise acquisition, never MCU qualification."""

import copy
import json
import os
from pathlib import Path
import selectors
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.identity import canonical_digest
from tools.hil.acquire import acquisition_plan, execute_acquisition
from tools.testing import test_hil_workloads


class HilAcquireTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.protocol = test_hil_workloads.HilWorkloadTests()
        self.protocol.setUp()
        self.addCleanup(self.protocol.doCleanups)
        self.python = file_identity(Path(sys.executable).resolve())
        self.station = {
            "station_id": "synthetic-station", "board": "synthetic-board",
            "part": "synthetic-part", "chip_uid": "synthetic-uid",
            "probe": {"serial": "synthetic-probe", "tool": self.python}}
        self.station.update(board=self.protocol.fixture["board"],
                            part=self.protocol.fixture["part"])
        self.inputs = {}
        for name in ("station", "fixture", "elf", "resolved", "resources"):
            path = self.root / (name + ".json")
            value = {"scope": "synthetic tooling test; no MCU executed"}
            if name == "station":
                value = self.station
            elif name == "fixture":
                value = self.protocol.fixture
            elif name == "resolved":
                value = copy.deepcopy(self.protocol.resolved)
                value["configuration_sha256"] = canonical_digest(value)
            atomic_json(path, value)
            self.inputs[name] = file_identity(path)
        self.admission = {
            "schema_version": 1, "kind": "hil_admission", "status": "ready",
            "physical_status": "not_executed", **self.inputs,
            "station_id": self.station["station_id"],
            "board": self.station["board"], "part": self.station["part"],
            "chip_uid": self.station["chip_uid"]}
        self.admission_path = self.root / "admission.json"
        atomic_json(self.admission_path, self.admission)
        self.script = self.root / "synthetic_adapter.py"
        self.script.write_text(
            "import json, pathlib, sys\n"
            "operation, destination, token, admission = sys.argv[1:5]\n"
            "bound = json.loads(pathlib.Path(admission).read_text())\n"
            "path = pathlib.Path(bound['station']['path'])\n"
            "station = json.loads(path.read_text())\n"
            "observations = {'scope': 'synthetic; no equipment'}\n"
            "if operation == 'identify':\n"
            " for key in ('part', 'chip_uid'):\n"
            "  observations[key] = station[key]\n"
            " observations['probe_serial'] = station['probe']['serial']\n"
            "record = {'schema_version': 1,\n"
            "          'kind': 'hil_adapter_observation',\n"
            "          'operation': operation, 'session': token,\n"
            "          'observations': observations}\n"
            "pathlib.Path(destination).write_text(json.dumps(record))\n"
            "print('synthetic-' + operation)\n")
        config = self.root / "reviewed-config.txt"
        config.write_text("synthetic acquisition config; no equipment\n")
        self.workload = self.root / "workload.json"
        atomic_json(self.workload, self.protocol.reviewed())
        self.recipe = {
            "schema_version": 1, "kind": "hil_acquisition_recipe",
            "admission_sha256": canonical_digest(self.admission),
            "workload": file_identity(self.workload),
            "station_lock": str(self.root / "station.lock"),
            "operations": [{
                "name": name, "tool": copy.deepcopy(self.python),
                "inputs": [file_identity(self.script), file_identity(config)],
                "arguments": [str(self.script), name, "{record}", "{session}",
                              "{admission}"],
                "timeout_s": 2, "maximum_output_bytes": 65536}
                for name in ("identify", "flash", "verify_flash", "reset",
                             "serial", "cleanup")]}
        self.recipe_path = self.root / "recipe.json"
        self.output = self.root / "attempt"
        self.write_recipe()
        self.service = patch("tools.hil.acquire.admit",
                             return_value=copy.deepcopy(self.admission),
                             create=True)
        self.mock_admit = self.service.start()
        self.addCleanup(self.service.stop)

    def write_recipe(self):
        atomic_json(self.recipe_path, self.recipe)

    def change_script(self, prefix):
        self.script.write_text(prefix + self.script.read_text())
        for operation in self.recipe["operations"]:
            operation["inputs"][0] = file_identity(self.script)
        self.write_recipe()

    def test_plan_never_executes_process_or_creates_station_lock(self):
        with patch("subprocess.Popen") as process:
            result = acquisition_plan(self.recipe_path, self.admission_path)
        process.assert_not_called()
        self.assertEqual(result["status"], "prepared_bound")
        self.assertEqual(result["physical_status"], "not_executed")
        self.assertFalse(Path(self.recipe["station_lock"]).exists())

    def test_actual_subprocess_pipeline_retains_raw_and_never_qualifies(self):
        result = execute_acquisition(self.recipe_path, self.admission_path,
                                     self.output)
        self.assertEqual(result["status"], "captured")
        self.assertEqual(result["physical_status"], "executed_unqualified")
        self.assertEqual([step["name"] for step in result["operations"]],
                         [step["name"] for step in self.recipe["operations"]])
        for step in result["operations"]:
            self.assertEqual(step["exit_code"], 0)
            log = Path(step["stdout"]["path"]).read_bytes()
            self.assertEqual(log, ("synthetic-" + step["name"] + "\n").encode())
        self.assertFalse(Path(self.recipe["station_lock"]).exists())
        self.assertEqual(json.loads((self.output / "capture.json").read_text()),
                         result)

    def test_wrong_observed_uid_prevents_flash_but_runs_cleanup(self):
        self.script.write_text(self.script.read_text().replace(
            "record = {", "observations['chip_uid'] = 'wrong-uid'\nrecord = {"))
        for operation in self.recipe["operations"]:
            operation["inputs"][0] = file_identity(self.script)
        self.write_recipe()
        result = execute_acquisition(self.recipe_path, self.admission_path,
                                     self.output)
        self.assertEqual(result["status"], "failed")
        self.assertEqual([step["name"] for step in result["operations"]],
                         ["identify", "cleanup"])

    def test_failed_command_retains_stderr_and_stops_later_operations(self):
        self.change_script("import sys\n"
                           "if sys.argv[1] == 'flash':\n"
                           " print('observed flash failure', file=sys.stderr)\n"
                           " raise SystemExit(9)\n")
        result = execute_acquisition(self.recipe_path, self.admission_path,
                                     self.output)
        self.assertEqual(result["status"], "failed")
        self.assertEqual([step["name"] for step in result["operations"]],
                         ["identify", "flash", "cleanup"])
        self.assertEqual(result["operations"][1]["exit_code"], 9)
        raw = Path(result["operations"][1]["stderr"]["path"]).read_bytes()
        self.assertEqual(raw, b"observed flash failure\n")

    def test_cleanup_failure_retains_exclusive_station_quarantine(self):
        self.change_script("import sys\n"
                           "if sys.argv[1] == 'cleanup':\n"
                           " raise SystemExit(8)\n")
        result = execute_acquisition(self.recipe_path, self.admission_path,
                                     self.output)
        self.assertEqual(result["status"], "failed")
        self.assertTrue(result["station_quarantined"])
        self.assertTrue(Path(self.recipe["station_lock"]).exists())
        with self.assertRaises(EvidenceError):
            execute_acquisition(self.recipe_path, self.admission_path,
                                self.root / "next-attempt")

    def test_timeout_retains_raw_and_cleanup_still_executes(self):
        self.change_script("import sys, time\n"
                           "if sys.argv[1] == 'flash':\n"
                           " print('flash started', flush=True)\n"
                           " time.sleep(10)\n")
        self.recipe["operations"][1]["timeout_s"] = 1
        self.write_recipe()
        result = execute_acquisition(self.recipe_path, self.admission_path,
                                     self.output)
        self.assertEqual(result["status"], "failed")
        self.assertTrue(result["operations"][1]["timed_out"])
        self.assertIn(b"flash started", Path(
            result["operations"][1]["stdout"]["path"]).read_bytes())
        self.assertEqual(result["operations"][-1]["name"], "cleanup")

    def test_failed_process_creation_cannot_claim_equipment_execution(self):
        with patch("subprocess.Popen",
                   side_effect=OSError("cannot execute tool")):
            result = execute_acquisition(self.recipe_path, self.admission_path,
                                         self.output)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["physical_status"], "not_executed")
        self.assertTrue(result["station_quarantined"])

    def test_changed_lock_ownership_is_retained_as_failed_quarantine(self):
        lock_path = repr(self.recipe["station_lock"])
        self.change_script(
            "import pathlib, sys\n"
            "if sys.argv[1] == 'cleanup':\n"
            f" path = pathlib.Path({lock_path})\n"
            " path.write_text('changed-owner\\n')\n")
        result = execute_acquisition(self.recipe_path, self.admission_path,
                                     self.output)
        self.assertEqual(result["status"], "failed")
        self.assertTrue(result["station_quarantined"])
        self.assertIn("ownership", result["failure"])
        self.assertEqual(Path(self.recipe["station_lock"]).read_text(),
                         "changed-owner\n")

    def test_identify_losing_lock_ownership_prevents_all_later_device_actions(
            self):
        lock_path = repr(self.recipe["station_lock"])
        self.change_script(
            "import pathlib, sys\n"
            "if sys.argv[1] == 'identify':\n"
            f" pathlib.Path({lock_path}).write_text('different-owner\\n')\n")
        result = execute_acquisition(self.recipe_path, self.admission_path,
                                     self.output)
        self.assertEqual([step["name"] for step in result["operations"]],
                         ["identify"])
        self.assertEqual(result["status"], "failed")
        self.assertTrue(result["station_quarantined"])

    def sleeping_adapter(self):
        pid = self.root / "adapter-pid.txt"
        self.change_script(
            "import os, pathlib, sys, time\n"
            "if sys.argv[1] == 'identify':\n"
            f" pathlib.Path({str(pid)!r}).write_text(str(os.getpid()))\n"
            " print('started', flush=True)\n"
            " time.sleep(5)\n")
        return pid

    def test_capture_reader_failure_reaps_process_before_timeout(self):
        pid = self.sleeping_adapter()
        original = os.read

        def fail_capture(descriptor, count):
            if count == 65536:
                raise OSError("capture reader failed")
            return original(descriptor, count)

        started = time.monotonic()
        with patch("tools.hil.acquire.os.read", side_effect=fail_capture):
            result = execute_acquisition(self.recipe_path, self.admission_path,
                                         self.output)
        self.assertLess(time.monotonic() - started, 3)
        self.assertEqual(result["status"], "failed")
        self.assertIn("capture reader failed", result["failure"])
        with self.assertRaises(ProcessLookupError):
            os.kill(int(pid.read_text()), 0)

    def test_interrupt_reaps_process_and_keeps_station_quarantine(self):
        pid = self.sleeping_adapter()
        original = selectors.EpollSelector.select
        interrupted = False

        def interrupt_capture(selector, timeout):
            nonlocal interrupted
            events = original(selector, timeout)
            if events and not interrupted:
                interrupted = True
                raise KeyboardInterrupt()
            return events

        with patch.object(selectors.EpollSelector, "select", interrupt_capture):
            try:
                result = execute_acquisition(
                    self.recipe_path, self.admission_path, self.output)
            except KeyboardInterrupt:
                self.fail("capture interrupt escaped bounded process cleanup")
        self.assertTrue(result["cancelled"])
        self.assertTrue(result["station_quarantined"])
        self.assertEqual(result["status"], "failed")
        self.assertEqual([step["name"] for step in result["operations"]],
                         ["identify"])
        with self.assertRaises(ProcessLookupError):
            os.kill(int(pid.read_text()), 0)

    def test_changed_input_and_wrong_admission_reject_before_process(self):
        for mutation in ("tool", "admission"):
            with self.subTest(mutation=mutation):
                if mutation == "tool":
                    self.recipe["operations"][0]["tool"]["sha256"] = "0" * 64
                else:
                    tool = copy.deepcopy(self.python)
                    self.recipe["operations"][0]["tool"] = tool
                    self.recipe["admission_sha256"] = "0" * 64
                self.write_recipe()
                with patch("subprocess.Popen") as process:
                    with self.assertRaises(EvidenceError):
                        execute_acquisition(
                            self.recipe_path, self.admission_path, self.output)
                process.assert_not_called()

    def test_existing_output_and_symlink_cannot_be_overwritten(self):
        self.output.mkdir()
        retained = self.output / "retained.txt"
        retained.write_text("preserve old attempt")
        with self.assertRaises(EvidenceError):
            execute_acquisition(
                self.recipe_path, self.admission_path, self.output)
        self.assertEqual(retained.read_text(), "preserve old attempt")
        alias = self.root / "alias"
        alias.symlink_to(self.output, target_is_directory=True)
        with self.assertRaises(EvidenceError):
            execute_acquisition(self.recipe_path, self.admission_path,
                                alias / "child")

    def test_wrong_order_or_unknown_fields_fail_before_execution(self):
        for mutation in ("order", "unknown", "shell"):
            original = copy.deepcopy(self.recipe)
            if mutation == "order":
                self.recipe["operations"].reverse()
            elif mutation == "unknown":
                self.recipe["physical_status"] = "passed"
            else:
                self.recipe["operations"][0]["shell"] = True
            self.write_recipe()
            with patch("subprocess.Popen") as process:
                with self.assertRaises(EvidenceError):
                    execute_acquisition(self.recipe_path, self.admission_path,
                                        self.output)
            process.assert_not_called()
            self.recipe = original

    def test_json_schema_supports_runtime_reviewed_lower_bound(self):
        path = Path("tools/schemas/hil-station.schema.json")
        schema = json.loads(path.read_text())
        budget = schema["properties"]["budgets"]["additionalProperties"]
        self.assertIn("minimum", budget["properties"])
        self.assertNotIn("minimum", budget["required"])
        self.assertEqual(budget["properties"]["minimum"]["minimum"], 0)


if __name__ == "__main__":
    unittest.main()
