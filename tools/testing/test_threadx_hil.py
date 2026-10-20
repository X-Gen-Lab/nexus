"""Synthetic ThreadX acquisition contracts never establish physical results."""

import copy
from pathlib import Path
import unittest
from unittest.mock import patch

from tools.configure import cpu, kernel
from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.identity import canonical_digest
from tools.hil.os_acceptance import os_plan
from tools.testing import test_os_acceptance as os_fixture
from tools.testing import test_hil_workloads as workload_fixture


def resolved_threadx(board="stm32f407zg-qiming-v31", mask="basepri"):
    facts = {"arch": "cortex-m4", "fpu": "none", "float_abi": "soft",
             "dwt_cyccnt": False, "mpu_version": 0, "icache_line_bytes": 0,
             "dcache_line_bytes": 0, "security": "single", "sau": False,
             "mve": "none", "dsp": True}
    interrupts = {"priority_bits": 4, "external_count": 82}
    profile = cpu.resolve(facts, interrupts, "threadx")
    policy = kernel.resolve({"interrupt_mask": mask}, profile, 168000000)
    return {"board": board, "part": "STM32F407ZGT6",
            "soc_family": "stm32f407", "backend": "threadx", "cpu": facts,
            "irq": interrupts, "clock": {"core_hz": 168000000},
            "cpu_profile": kernel.bind(profile, policy).to_dict(),
            "kernel_profile": policy.to_dict()}


def expected_contract(resolved):
    policy = resolved["kernel_profile"]
    return {"backend": "threadx",
            "kernel_profile_sha256": canonical_digest(policy),
            "kernel_port": policy["kernel_port"], "max_priorities": 32,
            "priority_order": "lower_number_higher",
            "timer_process_in_isr": True,
            "tick_source": policy["tick_source"],
            "interrupt_mask": policy["interrupt_mask"],
            "syscall_priority": policy["syscall_priority"]}


class ThreadXHilTests(unittest.TestCase):
    def test_threadx_plan_has_own_unexecuted_cases_and_no_low_power(self):
        resolved = resolved_threadx()
        with patch("subprocess.run", side_effect=AssertionError("equipment")):
            plan = os_plan(resolved)
        self.assertEqual(plan["status"], "prepared_unbound")
        self.assertEqual(plan["physical_status"], "not_executed")
        self.assertEqual(plan["backend_contract"], expected_contract(resolved))
        self.assertEqual({case["id"] for case in plan["cases"]},
                         {"task", "irq_wake", "context", "stack", "deadline",
                          "lifecycle", "fail_stop"})
        self.assertTrue(all(case["status"] == "not_executed" and
                            case["budget"] is None for case in plan["cases"]))

    def test_threadx_scope_is_not_promoted_from_a_cpu_port(self):
        from tools.hil.threadx import contract
        for change in ({"board": "unknown-pcb"},
                       {"part": "STM32F407VET6"},
                       {"soc_family": "stm32h743"},
                       {"cpu": {**resolved_threadx()["cpu"],
                                "arch": "cortex-m7"}},
                       {"backend": "freertos"}):
            with self.subTest(change=change), self.assertRaises(EvidenceError):
                contract({**resolved_threadx(), **change})

    def test_native_threadx_plan_remains_host_software_only(self):
        resolved = resolved_threadx("native-reference")
        facts = {**resolved["cpu"], "arch": "native", "float_abi": "native",
                 "dsp": False}
        interrupts = {"priority_bits": 4, "external_count": 0}
        profile = cpu.resolve(facts, interrupts, "threadx")
        policy = kernel.resolve({}, profile, 1000000000)
        resolved.update(part="NATIVE_MODEL", soc_family="native", cpu=facts,
                        irq=interrupts, clock={"core_hz": 1000000000},
                        kernel_profile=policy.to_dict(),
                        cpu_profile=kernel.bind(profile, policy).to_dict())
        plan = os_plan(resolved)
        self.assertEqual(plan["scope"], "host_software_only")
        self.assertEqual(plan["physical_status"], "not_executed")
        self.assertTrue(all(case["availability"] == "software_only"
                            for case in plan["cases"]))

    def test_threadx_resolved_policy_is_recomputed_not_trusted(self):
        from tools.hil.threadx import contract
        for change in ({"max_priorities": 64}, {"timer_process_in_isr": False},
                       {"tickless": True}, {"kernel_port": "GCC/ARM_CM4F"}):
            resolved = resolved_threadx()
            resolved["kernel_profile"].update(change)
            with self.subTest(change=change), self.assertRaises(EvidenceError):
                contract(resolved)

    def test_threadx_workloads_are_independent_and_fail_closed(self):
        from tools.hil.threadx import workload_definitions, selection
        from tools.hil.workloads import _definition
        definitions = workload_definitions(_definition)
        self.assertEqual(len(definitions), 7)
        for name in definitions:
            self.assertTrue(name.startswith("threadx_"))
            self.assertEqual(selection(name, resolved_threadx()),
                             ("selected", []))
        self.assertEqual(selection("threadx_context", {
            **resolved_threadx(), "backend": "freertos"})[0], "unsupported")

    def prepare_record(self):
        sample = os_fixture.OSAcceptanceTests()
        sample.setUp()
        self.addCleanup(sample.doCleanups)
        resolved = {**sample.protocol.resolved, **resolved_threadx()}
        sample.protocol.resolved = resolved
        sample.protocol.fixture.update(
            board=resolved["board"], part=resolved["part"])
        sample.admission.update(board=resolved["board"], part=resolved["part"])
        sample.admission["budgets"].pop("os_low_power_lateness")
        sample.admission["budgets"]["os_task_iterations"] = {
            "minimum": 1, "maximum": 100, "unit": "count"}
        for key, value in (("resolved", resolved),
                           ("fixture", sample.protocol.fixture)):
            value = copy.deepcopy(value)
            if key == "resolved":
                value["configuration_sha256"] = canonical_digest(value)
            path = Path(sample.admission[key]["path"])
            atomic_json(path, value)
            sample.admission[key] = file_identity(path)
        sample.mock_admit.return_value = copy.deepcopy(sample.admission)
        digest = canonical_digest(sample.admission)
        sample.context.update(admission_sha256=digest,
                              backend_contract=expected_contract(resolved),
                              cpu_clock_hz=168000000,
                              os_irqs=["UART"],
                              tasks=[{"name": "owner", "priority": 7,
                                      "preemption_threshold": 7,
                                      "time_slice_ticks": 0},
                                     {"name": "producer", "priority": 8,
                                      "preemption_threshold": 8,
                                      "time_slice_ticks": 0}])
        sample.context["stack_names"].append("producer")
        sample.context["requirements"].pop("low_power")
        sample.context["requirements"]["task"] = {"minimum_iterations": 1}
        sample.record.update(board=resolved["board"], part=resolved["part"],
                             admission_sha256=digest,
                             resolved_sha256=sample.admission[
                                 "resolved"]["sha256"])
        sample.traces.pop("low_power")
        sample.traces["task"] = {"tasks": [
            {**row, "iterations": 1, "started_us": 1, "finished_us": 9}
            for row in sample.context["tasks"]],
            "switches": [{"sequence": 0, "expected": "owner",
                          "observed": "owner"},
                         {"sequence": 1, "expected": "producer",
                          "observed": "producer"}]}
        sample.traces["irq_wake"]["samples"][0].update(
            irq="UART", service="semaphore_ceiling_put", task="owner")
        sample.traces["stack"]["stacks"].append(
            {"name": "producer", "capacity_bytes": 512, "used_bytes": 128})
        for row in sample.traces["fail_stop"]["scenarios"]:
            row.update(observer_record={"reason": row["id"], "sequence": 1},
                       incoming_masks={"primask": 0, "basepri": 80, "ipsr": 0},
                       retained_loans=1, terminal_progress=0)
        self.write_context(sample)
        return sample

    def write_context(self, sample):
        atomic_json(sample.context_path, sample.context)
        sample.record["context"] = file_identity(sample.context_path)
        sample.record["traces"] = []
        for name, trace in sample.traces.items():
            sample.write_trace(name, trace)

    def test_threadx_record_uses_own_task_irq_and_failure_observations(self):
        sample = self.prepare_record()
        # Only exercise the raw validator; authored-protocol admission has its
        # own tests, and these synthetic bytes never assert station execution.
        with patch("tools.hil.workloads.admitted_workload", return_value={
                "cases": [{"id": "threadx_" + name}
                          for name in sample.traces]}):
            result = sample.validate()
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["backend"], "threadx")
        self.assertNotIn("low_power", result["cases"])

    def test_threadx_rejects_cross_kernel_missing_case_and_wrong_context(self):
        sample = self.prepare_record()
        with patch("tools.hil.workloads.admitted_workload", return_value={
                "cases": [{"id": "threadx_" + name}
                          for name in sample.traces]}):
            for change in ({"backend": "freertos"},
                           {"priority_order": "higher_number_higher"},
                           {"kernel_profile_sha256": "0" * 64}):
                before = copy.deepcopy(sample.context["backend_contract"])
                sample.context["backend_contract"].update(change)
                self.write_context(sample)
                with self.subTest(change=change), \
                        self.assertRaises(EvidenceError):
                    sample.validate()
                sample.context["backend_contract"] = before
            self.write_context(sample)
            sample.record["traces"] = [
                identity for identity in sample.record["traces"]
                if Path(identity["path"]).stem != "task"]
            with self.assertRaises(EvidenceError):
                sample.validate()

    def test_threadx_negative_observations_are_rejected(self):
        sample = self.prepare_record()
        mutations = (
            ("task", lambda trace: trace["tasks"][0].update(iterations=0)),
            ("task", lambda trace: trace["tasks"][0].update(priority=31)),
            ("task", lambda trace: trace["switches"][0].update(
                observed="producer")),
            ("irq_wake", lambda trace: trace["samples"][0].update(
                irq="unknown")),
            ("irq_wake", lambda trace: trace["samples"][0].update(
                service="freertos_notify")),
            ("fail_stop", lambda trace: trace["scenarios"][0].update(
                terminal_progress=1)),
            ("fail_stop", lambda trace: trace["scenarios"][0].update(
                retained_loans=True)),
        )
        with patch("tools.hil.workloads.admitted_workload", return_value={
                "cases": [{"id": "threadx_" + name}
                          for name in sample.traces]}):
            for name, mutate in mutations:
                trace = copy.deepcopy(sample.traces[name])
                mutate(trace)
                sample.write_trace(name, trace)
                with self.subTest(case=name), self.assertRaises(EvidenceError):
                    sample.validate()
                sample.write_trace(name, sample.traces[name])

    def test_threadx_requires_explicit_protocol_and_irq_ceiling(self):
        sample = self.prepare_record()
        with patch("tools.hil.workloads.admitted_workload", return_value={
                "cases": [{"id": "m4f_context"}]}), \
                self.assertRaises(EvidenceError):
            sample.validate()
        sample.context["irq_priorities"]["UART"] = 4
        self.write_context(sample)
        with patch("tools.hil.workloads.admitted_workload", return_value={
                "cases": [{"id": "threadx_" + name}
                          for name in sample.traces]}), \
                self.assertRaises(EvidenceError):
            sample.validate()

    def test_irq_capture_requires_the_actual_threadx_notify_primitive(self):
        sample = self.prepare_record()
        sample.traces["irq_wake"]["samples"][0]["service"] = (
            "semaphore_ceiling_put")
        sample.write_trace("irq_wake", sample.traces["irq_wake"])
        with patch("tools.hil.workloads.admitted_workload", return_value={
                "cases": [{"id": "threadx_" + name}
                          for name in sample.traces]}):
            self.assertEqual(sample.validate()["status"], "passed")

    def test_irq_capture_rejects_a_different_threadx_service(self):
        sample = self.prepare_record()
        with patch("tools.hil.workloads.admitted_workload", return_value={
                "cases": [{"id": "threadx_" + name}
                          for name in sample.traces]}):
            for service in ("event_flags_set", "semaphore_put"):
                sample.traces["irq_wake"]["samples"][0]["service"] = service
                sample.write_trace("irq_wake", sample.traces["irq_wake"])
                with self.subTest(service=service), \
                        self.assertRaises(EvidenceError):
                    sample.validate()

    def test_backend_contract_requires_exact_json_types(self):
        sample = self.prepare_record()
        with patch("tools.hil.workloads.admitted_workload", return_value={
                "cases": [{"id": "threadx_" + name}
                          for name in sample.traces]}):
            for change in ({"max_priorities": 32.0},
                           {"timer_process_in_isr": 1},
                           {"syscall_priority": 5.0}):
                before = copy.deepcopy(sample.context["backend_contract"])
                sample.context["backend_contract"].update(change)
                self.write_context(sample)
                with self.subTest(change=change), \
                        self.assertRaises(EvidenceError):
                    sample.validate()
                sample.context["backend_contract"] = before

    def test_threadx_irq_capture_names_the_actual_resumed_thread(self):
        sample = self.prepare_record()
        before = copy.deepcopy(sample.traces["irq_wake"])
        with patch("tools.hil.workloads.admitted_workload", return_value={
                "cases": [{"id": "threadx_" + name}
                          for name in sample.traces]}):
            for task in ("unknown", True, None):
                trace = copy.deepcopy(before)
                if task is not None:
                    trace["samples"][0]["task"] = task
                else:
                    trace["samples"][0].pop("task")
                sample.write_trace("irq_wake", trace)
                with self.subTest(task=task), self.assertRaises(EvidenceError):
                    sample.validate()

    def test_real_toml_selects_own_threadx_workloads_on_all_three_boards(self):
        from tools.configure import configure as gate
        protocol = workload_fixture.HilWorkloadTests()
        protocol.setUp()
        self.addCleanup(protocol.doCleanups)
        for board, clock, binding, fixture in (
                ("stm32f407_qiming_v31", "hse8-pll168", "uart1",
                 "stm32f407_qiming_v31"),
                ("stm32f407ve_sky_qingchun", "hse8-pll168", "uart1",
                 "stm32f407ve_sky_qingchun"),
                ("gd32f470_liangshan", "hxtal25-pll200", "console",
                 "gd32f470_liangshan")):
            path = protocol.root / (board + ".toml")
            path.write_text(
                f'schema=2\nboard="{board}"\nbackend="threadx"\n'
                f'clock="{clock}"\n[memory]\nmain_stack_bytes=2048\n'
                f'[uart.console]\nbinding="{binding}"\nmode="irq"\n'
                'baud=115200\nrx={format="events",capacity=16}\n'
                'irq={priority=5,calls_os=true}\n')
            # Source-checkout verification is an external boundary. This test
            # exercises the real TOML/IR/planner, not dependency or ARM gates.
            with patch.object(gate, "dependency_identity", side_effect=
                              lambda root, name, expected=None:
                              {"path": name, "commit": "1" * 40}), \
                    patch.object(gate, "imported_dependency_identity",
                                 side_effect=lambda root, name, read:
                                 {"kind": "vendor-source-import",
                                  "path": Path(name).parent.as_posix(),
                                  "lock_sha256": "1" * 64}):
                protocol.resolved = gate.resolve_ir(path).to_dict()
            protocol.fixture = gate.load(
                gate.ROOT / "tools/hil/fixtures" / (fixture + ".json"))
            report = protocol.plan()
            names = tuple("threadx_" + name for name in
                          ("task", "irq_wake", "context", "stack", "deadline",
                           "lifecycle", "fail_stop"))
            with self.subTest(board=board):
                self.assertTrue(all(protocol.case(name, report)["availability"]
                                    == "selected" for name in names))
                self.assertEqual(protocol.case("tickless_timer", report)[
                    "availability"], "unsupported")
                self.assertEqual(protocol.validate(protocol.reviewed(names))[
                    "board"], protocol.resolved["board"])
                self.assertEqual(report["physical_status"], "not_executed")

    def test_threadx_native_cannot_inherit_generic_physical_workloads(self):
        from tools.hil import workloads
        resolved = {**resolved_threadx(), "cpu": {"arch": "native"},
                    "cpu_profile": {"arch": "native"}, "controllers": []}
        self.assertEqual(workloads._selection(
            "station_binding", workloads.WORKLOADS["station_binding"],
            resolved)[0], "unsupported")
