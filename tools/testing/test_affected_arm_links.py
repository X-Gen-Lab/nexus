"""Bound a commit's hardware report to the selected image and its actual ELF."""

import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

from scripts.ci import tdd_gate
from tools.evidence.common import file_identity
from tools.testing import gate_fixture


def exercise_links():
    """Drive the real gate executor with short external commands in place of CMake."""
    from tools.testing.arm_links import affected_arm_links

    root, mode = Path(sys.argv[1]), sys.argv[2]
    names = ["b.toml", "a.toml", "z.toml"]
    for name in names:
        (root / name).write_text("schema = 2\n")
    command = """
import os, sys, time
from pathlib import Path
root, name, mode = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
assert os.environ['NEXUS_BUILD_JOBS'] == os.environ['CMAKE_BUILD_PARALLEL_LEVEL'] == '1'
(root / (name + '.started')).write_text(str(os.getpid()))
if name in ('a', 'b'):
    peer = root / ('b.started' if name == 'a' else 'a.started')
    deadline = time.monotonic() + 2
    while not peer.exists() and time.monotonic() < deadline: time.sleep(.01)
    assert peer.exists(), 'two independent builds must overlap'
if mode == 'failure' and name == 'b': sys.exit(17)
if mode == 'timeout' or mode == 'signal' or mode == 'failure': time.sleep(8)
if name == 'b': time.sleep(.15)
"""

    def execute(argv, env, log, **options):
        if log.stem == "impact-build":
            tdd_gate.execute([sys.executable, "-c", command, str(root),
                              log.parent.name, mode], env, log, **options)

    env = {**os.environ, "NEXUS_BUILD_JOBS": "2", "NEXUS_COMMAND_TIMEOUT":
           "2" if mode == "timeout" else "10"}
    with patch("tools.testing.build_impact.plan_hardware",
               return_value={"assemblies": names}), \
            patch("tools.testing.arm_links.linked_image", return_value={}):
        report = affected_arm_links(root, [], env, root, execute)
    (root / "result").write_text(json.dumps([item["assembly"] for item in report["links"]]))


class AffectedArmLinkTests(unittest.TestCase):
    def test_build_slots_order_timeout_failure_and_interrupt(self):
        """Real child commands share slots and withdraw before the gate exits."""
        module = "tools.testing.test_affected_arm_links"
        for mode in ("success", "failure", "timeout", *(("signal",) if os.name == "posix" else ())):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                proc = subprocess.Popen([sys.executable, "-c",
                    f"from {module} import exercise_links; exercise_links()",
                    directory, mode], cwd=tdd_gate.ROOT, text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                try:
                    if mode == "signal":
                        deadline = time.monotonic() + 6
                        while (not (root / "a.started").exists() and proc.poll() is None
                               and time.monotonic() < deadline):
                            time.sleep(.02)
                        proc.send_signal(signal.SIGTERM)
                    output, _ = proc.communicate(timeout=12)
                finally:
                    if proc.poll() is None:
                        proc.kill()
                        proc.communicate()
                self.assertEqual((root / "a.started").exists(), True, output)
                if mode == "success":
                    self.assertEqual(proc.returncode, 0, output)
                    self.assertEqual(json.loads((root / "result").read_text()),
                                     ["b.toml", "a.toml", "z.toml"])
                else:
                    self.assertNotEqual(proc.returncode, 0, output)
                    self.assertFalse((root / "z.started").exists(), output)
                    self.assertFalse((root / "result").exists())
                    if mode != "signal":
                        self.assertIn("Command failed (" + ("124" if mode == "timeout"
                                      else "17") + ")", output)
                    if os.name == "posix":
                        pid = int((root / "a.started").read_text())
                        with self.assertRaises(ProcessLookupError):
                            os.kill(pid, 0)

    def test_staged_board_records_arm_link_results(self):
        fixture = gate_fixture.GateFixture()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        row = {"assemblies": ["board.toml"], "links": [{"assembly": "board.toml"}]}
        with patch.object(tdd_gate, "staged_paths", return_value=["boards/demo/board.json"]), \
                patch.object(tdd_gate, "check_staged_snapshot"), \
                patch.object(tdd_gate, "staged_tree", return_value="staged"), \
                patch.object(tdd_gate, "affected_arm_links", create=True,
                             return_value=row) as links:
            status, output = fixture.run_gate(arguments=["--staged"])
        self.assertEqual(status, 0, output)
        links.assert_called_once()
        record = json.loads((fixture.build / "tdd/execution.json").read_text())
        self.assertEqual(record["hardware"], row)

    def test_report_binds_the_elf_configuration_budget_and_authored_assembly(self):
        from tools.testing.arm_links import linked_image
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "bin"
            output.mkdir()
            generated = root / "generated"
            generated.mkdir()
            elf = output / "nexus_firmware.elf"
            resolved = generated / "resolved.json"
            budget = generated / "resource-budget.json"
            assembly = root / "app.toml"
            assembly.write_text("schema = 2")
            elf.write_bytes(b"linked image")
            facts = {"board": "demo", "part": "demo-m4", "soc_family": "family"}
            resolved.write_text(json.dumps(facts))
            budget.write_text("{}")
            report = {"schema_version": 1, "kind": "elf_resource_budget",
                      "status": "passed", "violations": [], **facts,
                      "elf": file_identity(elf), "resolved": file_identity(resolved),
                      "budget": file_identity(budget), "configuration_inputs": [
                          {"label": "assembly", "file": file_identity(assembly)}]}
            destination = output / "nexus_firmware.resources.json"
            destination.write_text(json.dumps(report))
            self.assertEqual(linked_image(output, assembly=assembly)["elf"], file_identity(elf))
            for key in ("board", "part", "resolved", "budget", "configuration_inputs"):
                with self.subTest(key=key):
                    destination.write_text(json.dumps({**report, key: [] if key ==
                                                       "configuration_inputs" else "foreign"}))
                    with self.assertRaises(ValueError):
                        linked_image(output, assembly=assembly)
            destination.write_text(json.dumps(report))
            elf.write_bytes(b"changed data")
            with self.assertRaisesRegex(ValueError, "identity|digest|ELF"):
                linked_image(output, assembly=assembly)
