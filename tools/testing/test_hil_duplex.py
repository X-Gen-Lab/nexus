"""Independent duplex acquisition contracts; all captures are synthetic."""

import copy
import tempfile
import unittest
from pathlib import Path

from tools.configure import configure
from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.hil import selected_modes, soc_acceptance, workloads


CASE = "H743-DUPLEX-CONTINUOUS"
SCENARIOS = ("tx_min", "tx_max", "cancel_tx_tc", "stop_rx_held",
             "tx_drain_timeout", "rx_drain_timeout", "domain_taint",
             "reject_tx_overlap", "reject_rx_overlap")
TX_CR = 0x120457
RX_CR = 0x160517


class DuplexHilTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.resolved = {}
        for cached in (False, True):
            suffix = "-cache-on" if cached else ""
            cls.resolved[cached] = configure.resolve_ir(configure.ROOT /
                "tools/configure/assemblies" /
                ("h743-uart-duplex-continuous" + suffix + ".toml")).to_dict()

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def data(self, name, payload):
        path = self.root / name
        path.write_bytes(payload)
        return file_identity(path)

    def documents(self, cached=False):
        resolved = copy.deepcopy(self.resolved[cached])
        controller = resolved["controllers"][0]
        protocol = {"case_id": CASE, "controller_ids": [controller["id"]],
                    "instruments": {key: self.data(key, b"identity") for key in
                                    selected_modes.HARDWARE}}
        protocol_id, context_id = {"sha256": "1" * 64}, {"sha256": "2" * 64}
        session = "3" * 32
        facts = {"controller": "USART3", "mode": "dma-duplex-continuous",
            "device_id": 0x450, "revision_id": 0x2003,
            "nominal_core_hz": 64000000,
            "clock_profile": resolved["clock_profile"],
            "cache_line_bytes": 32 if cached else 0, "hsi_trim": 0x40,
            "tzen": None, "swap_bank": None, "irq_priority": 5,
            "calls_os": False,
            "dma_lanes": [{key: lane[key] for key in
                           ("engine", "lane", "request")}
                          for lane in resolved["dma_topology"]["lanes"]]}
        documents = {"mode_registers": {"facts": facts},
            **{role: {"trials": []} for role in
               selected_modes.OBSERVATIONS if role != "mode_registers"}}
        for scenario in SCENARIOS:
            rejected = scenario.startswith("reject_")
            reject_tx = scenario == "reject_tx_overlap"
            reject_rx = scenario == "reject_rx_overlap"
            tainted = scenario == "domain_taint"
            tx_timeout = scenario == "tx_drain_timeout"
            rx_timeout = scenario == "rx_drain_timeout"
            length = ((32 if cached else 1) if scenario == "tx_min" else
                      (65504 if cached else 65535) if scenario == "tx_max"
                      else 1024 if tainted
                      else 64)
            rx_addresses = [0x24000000 + index * 0x10000 for index in range(3)]
            tx_address, request = 0x24040000, 0x20000400
            buffers = [{"address": address, "size": 32, "exclusive": True}
                       for address in rx_addresses]
            tx_buffer = {"address": tx_address, "size": length,
                         "exclusive": True}
            if scenario == "reject_tx_overlap":
                tx_buffer["address"] = rx_addresses[0]
            if scenario == "reject_rx_overlap":
                buffers[1]["address"] = rx_addresses[0]
            tx_bytes = bytes(index % 251 for index in range(length))
            rx_bytes = bytes(range(32))
            identity = lambda key, value: self.data(scenario + key, value)
            tx_payload = identity("tx", tx_bytes)
            rx_payload = identity("rx", rx_bytes)
            poison = identity("poison", bytes([0xa5]) * 32)
            memory = {"id": scenario, "rx_buffers": buffers,
                "tx_buffer": tx_buffer,
                "controls": [{"role": role, "address": address,
                    "size": size, "static": True} for role, address, size in
                    (("owner", 0x20000000, 272),
                     ("stream", 0x20000200, 48),
                     ("slots", 0x20000300, 120),
                     ("tx_request", request, 60))],
                "tx_before": tx_payload, "tx_after": tx_payload,
                "rx_before": poison,
                "rx_after": poison if reject_rx else rx_payload,
                "rx_cpu_visible": poison if reject_rx else rx_payload,
                "held_before": rx_payload, "held_after": rx_payload,
                "cpu_acquire_ns": 10500000,
                "rx_observation": {"address": rx_addresses[2], "epoch": 9,
                    "length": 32, "dma_complete_ns": 10000000,
                    "invalidate_ns": 10100000 if cached else None,
                    "publish_ns": 10200000, "acquire_ns": 10300000,
                    "physical_sample_ns": 10400000,
                    "cpu_sample_ns": 10500000},
                "held_observation": {"address": rx_addresses[0], "epoch": 7,
                    "length": 32, "before_ns": 4100000, "after_ns": 6900000,
                    "before_sequence": None, "after_sequence": None},
                "clean_span": ({"address": tx_address, "size": length}
                               if cached and not rejected else None),
                "invalidate_spans": ([{"address": address, "size": 32}
                    for address in rx_addresses] if cached and not reject_rx
                    else [])}
            wire_at = 1003000 + length * 10 * 556 * 1000 // 64 + 2000
            memory_at = max(1004000, wire_at - 100000)
            drain_deadline = memory_at + (20000000 // 115200 + 100) * 1000
            event_pairs = [(1000000, "request"), (1001000, "borrow")]
            if cached:
                event_pairs += [(1001500, "clean_begin"),
                                (1002000, "clean_end")]
            event_pairs += [(1003000, "launch"), (memory_at, "memory_tc")]
            if scenario == "cancel_tx_tc":
                event_pairs += [(memory_at + 1000, "cancel")]
            event_pairs += [(wire_at, "wire_tc")]
            if tx_timeout:
                event_pairs += [(drain_deadline, "quarantined")]
            else:
                event_pairs += [(wire_at + 1000, "withdraw"),
                    (wire_at + 2000, "tx_irq_joined"),
                    (wire_at + 3000, "settled"),
                    (wire_at + 4000, "release")]
            if rejected:
                event_pairs = ([(11000000, "request"), (11001000, "rejected")]
                               if reject_tx else [])
            if tainted:
                event_pairs = [(1000000, "request"), (1001000, "borrow"),
                    *([(1001500, "clean_begin"), (1002000, "clean_end")]
                      if cached else []), (1003000, "launch"),
                    (1004000, "tainted")]

            def snapshot(phase, instant, state="ACTIVE", active=request,
                         ref=1, cr=TX_CR, ndtr=length, dmat=128, tc=0,
                         lisr=0):
                return {"at_ns": instant, "phase": phase, "cr": cr,
                    "ndtr": ndtr, "par": 0x40004828, "m0ar": tx_address,
                    "fcr": 0, "mux": 46, "usart_cr1": 0,
                    "usart_cr3": 64 | dmat, "usart_isr": tc, "lisr": lisr,
                    "request_state": state, "active_request": active,
                    "ref": ref, "publisher_active": False,
                    "preparing": phase == "preparing"}

            snapshots = [snapshot("preparing", 1001000, "ACTIVE",
                                  cr=0, dmat=0)]
            snapshots[0].update(par=0, m0ar=0)
            snapshots[0]["ndtr"] = 0
            if not rejected:
                snapshots.append(snapshot("launch", 1003000))
                if not tainted:
                    snapshots.append(snapshot("memory_tc", memory_at,
                                              ndtr=0, lisr=32))
            if rejected:
                snapshots = [snapshot("before", 999000, "READY", active=0,
                    ref=0, cr=0, ndtr=0, dmat=0),
                    snapshot("rejected", 1001000, "READY", active=0,
                             ref=0, cr=0, ndtr=0, dmat=0)]
                for sample in snapshots:
                    sample.update(par=0, m0ar=0)
                if reject_tx:
                    snapshots[0]["at_ns"] = 10999000
                    snapshots[1]["at_ns"] = 11001000
                else:
                    snapshots[1]["phase"] = "not_submitted"
            elif tx_timeout:
                snapshots.append(snapshot("quarantined", drain_deadline,
                    "QUARANTINED", ndtr=0, tc=64,
                    cr=TX_CR & ~0x16, dmat=0))
            elif not tainted:
                withdraw_at = wire_at + 1000
                snapshots += [snapshot("withdrawn", withdraw_at, "DRAINING",
                    cr=0, ndtr=0, dmat=0, tc=64),
                    snapshot("settled", withdraw_at + 2000, "SETTLED",
                    active=0, ref=0, cr=0, ndtr=0, dmat=0,
                    tc=64)]
            terminal = (None if tx_timeout or tainted or reject_rx else
                "REJECTED" if rejected else "CANCELLED" if
                scenario.startswith("cancel_") else "SUCCESS")
            tx = {"request_id": scenario, "request_address": request,
                "length": length, "deadline_ns": wire_at + 1000000,
                "drain_start_ns": None if tainted or rejected else memory_at,
                "drain_deadline_ns": (None if tainted or rejected else
                    drain_deadline), "terminal": terminal,
                "transferred": length if terminal == "SUCCESS" else 0,
                "events": [{"at_ns": at, "event": event}
                           for at, event in sorted(event_pairs)],
                "snapshots": snapshots,
                "preparation_baseline": (None if rejected else
                    {key: value for key, value in snapshots[0].items()
                     if key in {"cr", "ndtr", "par", "m0ar", "fcr", "mux",
                                "usart_cr1", "usart_cr3"}}),
                "request_tail": ({"capture_sequence": 103,
                    "state": "QUARANTINED", "active_request": request,
                    "ref": 1} if tainted else None)}
            rotations = ((0, 0, 1, [], []), (1, 2, 1, [0], []),
                         (0, 2, 0, [1], [0]), (1, 1, 0, [2], [1]))
            targets = [{"at_ns": 1000000 + index * 3000000,
                "cr": RX_CR | (ct << 19), "m0ar": rx_addresses[m0],
                "m1ar": rx_addresses[m1], "ndtr": 32,
                "held": [rx_addresses[slot] for slot in held],
                "returned": [rx_addresses[slot] for slot in returned]}
                for index, (ct, m0, m1, held, returned) in enumerate(rotations)]
            held = scenario == "stop_rx_held"
            retained = rx_timeout or tainted
            slots = [{"address": address, "state": (
                "FILLING" if retained and index < 2 else
                "BORROWED" if (held or retained) and index == 2 else "FREE"),
                "epoch": 9 if index == 2 else 10 + index}
                for index, address in enumerate(rx_addresses)]
            rx_events = [(1000000, "dma_enable"), (4000000, "block_ready"),
                (4100000, "loan"), (12000000, "stop")]
            rx_events += ([(14000000, "quarantined")] if rx_timeout else
                [] if tainted else [(12100000, "producer_detached")] +
                ([] if held else [(12200000, "consumer_return"),
                                 (12300000, "drained")]))
            rx = {"stream_address": 0x20000200,
                "stop_budget_start_ns": 12000000,
                "stop_budget_deadline_ns": 14000000,
                "stop_return_ns": 12050000 if rx_timeout else 12400000,
                "result": "BUSY" if held or retained else "SUCCESS",
                "events": [{"at_ns": at, "event": event}
                           for at, event in rx_events],
                "dma_targets": targets,
                "registers": {"par": 0x40004824, "mux": 45, "cr": RX_CR,
                              "fcr": 0, "usart_cr3": 64},
                "quarantined": retained,
                "stop_registers": ({"at_ns": 12050000,
                    "cr": RX_CR & ~0x16, "usart_cr1": 0,
                    "usart_cr3": 0, "lisr": 0} if rx_timeout else None),
                "service_sample_ns": 10000000, "remaining_bytes": 32,
                "service_deadline_ns": 10000000 +
                    max(0, (64 - 20) * 8 * 556 // 64 - 2) * 1000,
                "loans": [{"address": rx_addresses[0], "epoch": 7,
                    "acquired_ns": 4100000, "returned_ns": 7000000},
                    {"address": rx_addresses[2], "epoch": 9,
                    "acquired_ns": 10300000,
                    "returned_ns": None if held or retained else 12200000}],
                "admission": None,
                "states": [{"at_ns": 1000000, "producer_loans": 2,
                    "producer_pending": 2, "outstanding": 2,
                    "stopping": False, "slots": [{"address": address,
                    "state": "FILLING" if index < 2 else "FREE",
                    "epoch": 7 + index if index < 2 else 0}
                    for index, address in enumerate(rx_addresses)]},
                    {"at_ns": 10500000, "producer_loans": 2,
                    "producer_pending": 2, "outstanding": 3,
                    "stopping": False, "slots": [{"address": address,
                    "state": "BORROWED" if index == 2 else "FILLING",
                    "epoch": 9 if index == 2 else 10 + index}
                    for index, address in enumerate(rx_addresses)]},
                    {"at_ns": 14000000, "producer_loans": 2 if retained else 0,
                    "producer_pending": 2 if retained else 0,
                    "outstanding": 3 if retained else 1 if held else 0,
                    "stopping": True, "slots": slots}]}
            if reject_rx:
                rx["events"], rx["dma_targets"], rx["states"] = [], [], []
                rx["loans"] = []
                rx["result"] = "REJECTED"
                rx["admission"] = {"before": {"cr": 0, "m0ar": 0,
                    "m1ar": 0, "ndtr": 0, "dmar": 0, "stream": 0,
                    "producer_loans": 0}, "after": {"cr": 0, "m0ar": 0,
                    "m1ar": 0, "ndtr": 0, "dmar": 0, "stream": 0,
                    "producer_loans": 0}}
            for state in rx["states"]:
                state["capture_sequence"] = None
            fault = {"tx_drain_timeout": "tx_en_refused",
                "rx_drain_timeout": "rx_en_refused",
                "domain_taint": "domain_changed",
                "reject_tx_overlap": "tx_overlap",
                "reject_rx_overlap": "rx_overlap"}.get(scenario, "none")
            lifecycle = {"id": scenario, "clock_domain": "monotonic1",
                "clock_epoch": 1, "tx": tx, "rx": rx, "fault": fault,
                "wake_address": 0,
                "irq_state": {irq: {"enabled": not tainted,
                    "pending": False, "active": False} for irq in
                    ("dma1_stream0", "dma1_stream1", "usart3")},
                "taint": ({"at_ns": 11000000,
                    "capture_sequence": 100,
                    "fault_sequence": 97, "isolation_sequence": 101,
                    "before": {"rcc_cr": 5, "hsicfgr": 0x40000000,
                        "ccr": 0x10000 if cached else 0, "clock_epoch": 1},
                    "after": {"rcc_cr": 13, "hsicfgr": 0x40000000,
                        "ccr": 0x10000 if cached else 0, "clock_epoch": 1},
                    "access_trace": [{"at_ns": 1003000,
                        "capture_sequence": 1,
                        "operation": "mmio_write", "address": 0x40020010},
                        {"at_ns": None, "capture_sequence": 100,
                         "operation": "domain_lost",
                         "address": 0}]} if tainted else None)}
            if tainted:
                tx["events"][-1]["at_ns"] = 11000000
                rx["events"] = rx["events"][:3]
                rx["states"][-1]["at_ns"] = None
                rx["states"][-1]["capture_sequence"] = 104
                rx["states"][-1]["stopping"] = False
            if held or retained:
                memory["held_observation"].update(address=rx_addresses[2],
                    epoch=9, before_ns=10600000, after_ns=14100000)
            if tainted:
                memory["held_observation"].update(after_ns=None,
                    before_sequence=96, after_sequence=102)
            documents["lifecycle_events"]["trials"].append(lifecycle)
            documents["memory_capture"]["trials"].append(memory)
            documents["wire_capture"]["trials"].append({"id": scenario,
                "tx": (None if rejected else
                    identity("wire-prefix", tx_bytes[:64]) if tainted else
                    tx_payload),
                "rx": None if reject_rx else rx_payload,
                "activity_count": 0 if reject_rx else 32 + (
                    0 if reject_tx else 64 if tainted else length),
                "tx_capture_end_ns": (None if rejected else
                                      10800000 if tainted else wire_at),
                "fresh_tc_ns": None if rejected or tainted else
                wire_at})
        observations = {}
        for role, body in documents.items():
            path = self.root / (role + ".json")
            atomic_json(path, {"schema_version": 1,
                "kind": "selected_mode_observation", "case_id": CASE,
                "role": role, "session": session,
                "protocol_sha256": protocol_id["sha256"],
                "context_sha256": context_id["sha256"],
                "controller_id": controller["id"],
                "instrument": selected_modes.instrument_for(CASE, role),
                **body})
            observations[role] = file_identity(path)
        return (CASE, observations, resolved, protocol, session,
                protocol_id, context_id)

    def mutate(self, arguments, role, scenario, change):
        import json
        path = Path(arguments[1][role]["path"])
        document = json.loads(path.read_text())
        trial = next(item for item in document["trials"]
                     if item["id"] == scenario)
        change(trial)
        atomic_json(path, document)
        arguments[1][role] = file_identity(path)

    def rejects(self, role, scenario, change, cached=True):
        arguments = self.documents(cached)
        self.mutate(arguments, role, scenario, change)
        with self.assertRaises(EvidenceError):
            selected_modes.validate_observations(*arguments)

    def test_independent_case_exact_catalog_and_reference_scope(self):
        self.assertIn(CASE, soc_acceptance.CASES)
        self.assertIn("h743_duplex_continuous", workloads.WORKLOADS)
        for resolved in self.resolved.values():
            self.assertEqual(selected_modes.selection(CASE, resolved),
                             ("selected", []))
            self.assertNotEqual(selected_modes.selection(
                "H743-RX-CONTINUOUS", resolved)[0], "selected")
            fixture = selected_modes.fixture_for(resolved)
            plan = soc_acceptance.preparation_plan(fixture, resolved)
            self.assertEqual(plan["status"], "prepared_unbound")
            self.assertEqual(plan["physical_status"], "not_executed")
            from tools.hil.admit import validate_station
            with self.assertRaises(EvidenceError):
                validate_station({}, fixture)

    def test_independent_nine_trials_accept_for_both_cache_profiles(self):
        from tools.hil import h743_duplex
        for cached in (False, True):
            with self.subTest(cached=cached):
                self.assertEqual(h743_duplex.scenarios(), SCENARIOS)
                self.assertEqual(selected_modes.validate_observations(
                    *self.documents(cached)),
                    {"trials": 9, "protocol_errors": 0})

    def test_wrong_request_route_and_backend_are_unselected(self):
        for key in ("request", "lane"):
            resolved = copy.deepcopy(self.resolved[False])
            resolved["dma_topology"]["lanes"][0][key] += 1
            self.assertNotEqual(selected_modes.selection(CASE, resolved)[0],
                                "selected")

    def test_settled_before_fresh_wire_tc_is_rejected(self):
        self.rejects("lifecycle_events", "tx_min", lambda trial:
            trial["tx"]["events"][-2].update(at_ns=1003500))

    def test_preparing_cache_clean_cannot_enable_dma_or_dmat(self):
        for field, value in (("cr", 1), ("usart_cr3", 192)):
            with self.subTest(field=field):
                self.rejects("lifecycle_events", "tx_min",
                    lambda trial: trial["tx"]["snapshots"][0].update(
                        {field: value}))

    def test_model_only_cache_callback_cannot_be_a_physical_trial(self):
        arguments = self.documents(True)
        self.mutate(arguments, "lifecycle_events", "cancel_tx_tc", lambda trial:
            trial.update(id="cancel_tx_preparing"))
        with self.assertRaises(EvidenceError):
            selected_modes.validate_observations(*arguments)

    def test_memory_tc_does_not_release_active_request(self):
        self.rejects("lifecycle_events", "tx_min", lambda trial:
            trial["tx"]["snapshots"][2].update(ref=0, active_request=0))

    def test_wire_completion_cannot_be_replaced_by_dma_tc_label(self):
        self.rejects("wire_capture", "tx_min", lambda trial:
            trial.update(fresh_tc_ns=None))

    def test_tx_settlement_preserves_shared_rx_irq(self):
        arguments = self.documents()
        self.assertEqual(selected_modes.validate_observations(*arguments)
                         ["trials"], 9)

    def test_early_rx_loan_return_and_held_dma_reuse_are_rejected(self):
        self.rejects("lifecycle_events", "rx_drain_timeout", lambda trial:
            trial["rx"]["states"][-1].update(producer_loans=0))
        self.rejects("lifecycle_events", "stop_rx_held", lambda trial:
            trial["rx"]["dma_targets"][-1].update(
                m0ar=0x24020000))

    def test_exact_tx_and_rx_mmio_route_is_checked(self):
        self.rejects("lifecycle_events", "tx_min", lambda trial:
            trial["tx"]["snapshots"][1].update(mux=45))
        self.rejects("lifecycle_events", "tx_min", lambda trial:
            trial["rx"]["registers"].update(par=0x40004828))

    def test_tx_rx_payload_and_static_control_aliases_are_rejected(self):
        self.rejects("memory_capture", "tx_min", lambda trial:
            trial["tx_buffer"].update(address=0x24000000))
        self.rejects("memory_capture", "tx_min", lambda trial:
            trial["controls"][0].update(address=0x24000000))
        for change in ({"size": 1}, {"address": 0x20000001}):
            with self.subTest(control=change):
                self.rejects("memory_capture", "tx_min", lambda trial:
                    trial["controls"][0].update(change))

    def test_joint_taint_blocks_all_three_irqs_and_post_access(self):
        self.rejects("lifecycle_events", "domain_taint", lambda trial:
            trial["irq_state"]["usart3"].update(enabled=True))
        self.rejects("lifecycle_events", "domain_taint", lambda trial:
            trial["taint"]["access_trace"].append({"at_ns": 11001000,
                "capture_sequence": 102, "operation": "publish",
                "address": 0x20000400}))

    def test_actual_drain_and_station_budget_expiry_are_required(self):
        self.rejects("lifecycle_events", "tx_drain_timeout", lambda trial:
            trial["tx"].update(drain_deadline_ns=0))
        self.rejects("lifecycle_events", "rx_drain_timeout", lambda trial:
            trial["rx"].update(stop_budget_deadline_ns=20000000))

    def test_cache_cpu_acquire_must_observe_wire_bytes(self):
        self.rejects("memory_capture", "tx_min", lambda trial:
            trial.update(rx_cpu_visible=trial["rx_before"]))

    def test_rejected_request_cannot_borrow_or_touch_mmio(self):
        self.rejects("lifecycle_events", "reject_tx_overlap", lambda trial:
            trial["tx"]["snapshots"][-1].update(ref=1))

    def test_cache_clean_must_cover_the_actual_tx_payload(self):
        self.rejects("memory_capture", "tx_min", lambda trial:
            trial["clean_span"].update(size=1))

    def test_rx_success_cannot_return_before_memory_is_drained(self):
        self.rejects("lifecycle_events", "tx_min", lambda trial:
            trial["rx"].update(stop_return_ns=12050000))

    def test_cached_request_length_cannot_be_a_partial_cache_line(self):
        arguments = self.documents(True)
        payload = self.data("33bytes", bytes(range(33)))
        self.mutate(arguments, "lifecycle_events", "cancel_tx_tc", lambda trial:
            trial["tx"].update(length=33))
        self.mutate(arguments, "memory_capture", "cancel_tx_tc", lambda trial:
            trial.update(tx_before=payload, tx_after=payload,
                         clean_span={"address": 0x24040000, "size": 33}))
        with self.assertRaises(EvidenceError):
            selected_modes.validate_observations(*arguments)

    def test_post_settlement_provider_action_is_rejected(self):
        def write_after_release(trial):
            events = trial["tx"]["events"]
            events.append({"at_ns": events[-1]["at_ns"] + 1,
                           "event": "provider_write"})
        self.rejects("lifecycle_events", "tx_min", write_after_release)

    def test_withdrawal_cannot_leave_dma_half_transfer_irq(self):
        self.rejects("lifecycle_events", "tx_min", lambda trial:
            trial["tx"]["snapshots"][-2].update(cr=8))

    def test_actual_cpu_slot_epoch_and_retained_tx_tail_are_required(self):
        self.rejects("memory_capture", "tx_min", lambda trial:
            trial["rx_observation"].update(epoch=8))
        self.rejects("lifecycle_events", "domain_taint", lambda trial:
            trial["tx"]["request_tail"].update(ref=0))
        self.rejects("lifecycle_events", "domain_taint", lambda trial:
            trial["rx"]["states"][-1].update(capture_sequence=99))
        self.rejects("lifecycle_events", "tx_min", lambda trial:
            trial["rx"]["states"][0]["slots"][1].update(epoch=7))

    def test_completed_dma_cancel_and_en_refusal_still_require_full_wire(self):
        short = self.data("short-wire", b"\x00")
        for scenario in ("cancel_tx_tc", "tx_drain_timeout"):
            with self.subTest(scenario=scenario):
                self.rejects("wire_capture", scenario, lambda trial:
                    trial.update(tx=short))

    def test_tainted_hardware_launch_cannot_follow_domain_boundary(self):
        self.rejects("lifecycle_events", "domain_taint", lambda trial:
            trial["tx"]["snapshots"][-1].update(at_ns=11000001))

    def test_busy_rx_proof_cannot_use_a_previously_returned_consumer(self):
        for scenario in ("stop_rx_held", "rx_drain_timeout"):
            with self.subTest(scenario=scenario):
                self.rejects("memory_capture", scenario, lambda trial:
                    trial["held_observation"].update(address=0x24000000,
                        epoch=7, before_ns=4100000, after_ns=6900000))
        arguments = self.documents(True)
        def swap_retained_slot(trial):
            slots = trial["rx"]["states"][-1]["slots"]
            slots[0]["state"], slots[2]["state"] = "BORROWED", "FILLING"
            trial["rx"]["loans"].append({"address": 0x24000000, "epoch": 10,
                "acquired_ns": 10600000, "returned_ns": None})
        self.mutate(arguments, "lifecycle_events", "domain_taint",
                    swap_retained_slot)
        self.mutate(arguments, "memory_capture", "domain_taint", lambda trial:
            trial["held_observation"].update(address=0x24000000, epoch=10))
        with self.assertRaises(EvidenceError):
            selected_modes.validate_observations(*arguments)

    def test_domain_fault_allows_access_before_software_observation_only(self):
        arguments = self.documents(True)
        def during_detection(trial):
            trial["taint"]["access_trace"].insert(1, {"at_ns": None,
                "capture_sequence": 98, "operation": "mmio_read",
                "address": 0x40020010})
        self.mutate(arguments, "lifecycle_events", "domain_taint",
                    during_detection)
        self.assertEqual(selected_modes.validate_observations(*arguments)
                         ["trials"], 9)

    def test_legacy_rx_poison_before_differs_from_dma_and_cpu_result(self):
        from tools.testing.test_hil_selected_modes import SelectedModeHilTests
        SelectedModeHilTests.setUpClass()
        helper = SelectedModeHilTests(
            "test_actual_toml_selects_only_the_exact_mode")
        helper.setUp()
        self.addCleanup(helper.doCleanups)
        arguments = helper.documents("H743-RX-CONTINUOUS")
        poison = self.data("legacy-poison", b"\xa5" * 32)
        helper.mutate(arguments, "memory_capture", lambda raw:
            raw["trials"][0].update(before=poison))
        self.assertEqual(selected_modes.validate_observations(*arguments)
                         ["protocol_errors"], 0)


if __name__ == "__main__":
    unittest.main()
