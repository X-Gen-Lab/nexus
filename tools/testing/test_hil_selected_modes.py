"""Selected-mode acquisition contracts use synthetic bytes, never equipment."""

import copy
import importlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure
from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.hil import soc_acceptance, workloads


class SelectedModeHilTests(unittest.TestCase):
    assemblies = {
        "H743-SPI-CACHE": "h743-spi-dma-cache-on",
        "H743-RX-CONTINUOUS": "h743-uart-continuous",
        "U575-TX-CHAIN": "u575-uart-chain",
        "U575-LPTIM-RUN": "u575-lptim-run",
    }

    @classmethod
    def setUpClass(cls):
        cls.resolved = {name: configure.resolve_ir(
            configure.ROOT / "tools/configure/assemblies" / (assembly + ".toml")
        ).to_dict() for name, assembly in cls.assemblies.items()}

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def module(self):
        return importlib.import_module("tools.hil.selected_modes")

    def test_four_exact_contracts_exist_in_both_maintained_catalogs(self):
        for name in self.assemblies:
            with self.subTest(case=name):
                self.assertIn(name, soc_acceptance.CASES)
                self.assertIn(name.lower().replace("-", "_"),
                              workloads.WORKLOADS)

    def test_actual_toml_selects_only_the_exact_mode(self):
        module = self.module()
        for name, resolved in self.resolved.items():
            with self.subTest(case=name):
                self.assertEqual(module.selection(name, resolved),
                                 ("selected", []))
                for other in self.assemblies:
                    if name != other:
                        self.assertNotEqual(module.selection(
                            other, resolved)[0],
                                            "selected")

    def test_unreviewed_clock_backend_route_and_dma_lane_fail_closed(self):
        module = self.module()
        for name, original in self.resolved.items():
            for mutation in ("backend", "clock", "controller", "lane"):
                if name == "U575-LPTIM-RUN" and mutation == "lane":
                    continue
                resolved = copy.deepcopy(original)
                if mutation == "backend":
                    resolved["backend"] = "freertos"
                elif mutation == "clock":
                    resolved["clock_profile"] = "unknown-fixed-clock"
                elif mutation == "controller":
                    resolved["controllers"][0]["controller"] = "foreign"
                else:
                    resolved["dma_topology"]["lanes"][0]["request"] += 1
                with self.subTest(case=name, mutation=mutation):
                    self.assertNotEqual(module.selection(name, resolved)[0],
                                        "selected")

    def test_rx_off_and_on_are_distinct_from_duplex_or_finite_idle(self):
        module = self.module()
        resolved = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/h743-uart-continuous-cache-on.toml"
        ).to_dict()
        self.assertEqual(module.selection("H743-RX-CONTINUOUS", resolved)[0],
                         "selected")
        for mode in ("dma-duplex-continuous", "rx-dma-idle", "dma-tx"):
            resolved["controllers"][0]["mode"] = mode
            self.assertNotEqual(module.selection(
                "H743-RX-CONTINUOUS", resolved)[0], "selected")

    def test_new_reference_plans_never_operate_or_admit_a_station(self):
        module = self.module()
        from tools.hil.admit import validate_station
        for name, resolved in self.resolved.items():
            fixture = module.fixture_for(resolved)
            with self.subTest(case=name), patch("subprocess.Popen") as process:
                plan = soc_acceptance.preparation_plan(fixture, resolved)
                process.assert_not_called()
                self.assertEqual(plan["status"], "prepared_unbound")
                self.assertEqual(plan["physical_status"], "not_executed")
                case = next(item for item in plan["cases"]
                            if item["id"] == name)
                self.assertEqual(case["availability"], "selected")
                self.assertEqual(case["status"], "not_executed")
                self.assertTrue(all(value is None
                                    for value in case["limits"].values()))
                with self.assertRaisesRegex(EvidenceError,
                                            "software-reference"):
                    validate_station({}, fixture)
                bad = copy.deepcopy(fixture)
                bad.pop("scope")
                with self.assertRaises(EvidenceError):
                    validate_station({}, bad)

    def test_absent_smoke_console_is_explicitly_unbound(self):
        module = self.module()
        for name in ("H743-SPI-CACHE", "U575-LPTIM-RUN"):
            resolved = self.resolved[name]
            plan = workloads.workload_plan(module.fixture_for(resolved),
                                           resolved)
            self.assertIn("independent_control_channel_unbound",
                          plan["station_prerequisites"])

    def data(self, name, size=32):
        path = self.root / name
        path.write_bytes(bytes(index % 251 for index in range(size)))
        return file_identity(path)

    def documents(self, name="H743-SPI-CACHE", selected=None):
        module = self.module()
        resolved = copy.deepcopy(selected or self.resolved[name])
        controller = resolved["controllers"][0]
        instruments = {key: self.data("instrument-" + key, 8) for key in
                       soc_acceptance.CASES[name]["hardware"]}
        protocol = {"case_id": name, "controller_ids": [controller["id"]],
                    "instruments": instruments}
        protocol_identity = {"sha256": "1" * 64}
        context_identity = {"sha256": "2" * 64}
        session = "3" * 32
        trials = []
        memory, wire = [], []
        cache = resolved["clock_profile"].endswith("cache-on")
        for index, scenario in enumerate(module.SCENARIOS[name]):
            rejected = scenario.startswith("reject_")
            timeout = scenario == "drain_timeout"
            maximum = scenario == "success_max"
            size = (65504 if name == "H743-SPI-CACHE" else
                    131071 if name == "U575-TX-CHAIN" else
                    32) if maximum else 32
            if name == "U575-TX-CHAIN":
                size = {"success_min": 1, "block_max": 65535,
                    "two_block_min": 65536, "two_block_max": 131070,
                    "three_block_threshold": 131071,
                    "drain_timeout": 131071}.get(scenario, 32)
            if name == "U575-LPTIM-RUN":
                size = 65536 if maximum else 2
            events = (["request", "rejected"] if rejected else
                      ["request", "borrow"])
            if not rejected:
                if name == "H743-SPI-CACHE":
                    events += (["clean_tx", "prepare_rx"] if cache else [])
                    events += ["dma_enable", "tx_dma_tc", "rx_dma_tc",
                               "wire_idle"]
                    if cache:
                        events.append("invalidate_rx")
                    events.append("cs_release")
                elif name == "H743-RX-CONTINUOUS":
                    events += (["prepare_rx"] if cache else [])
                    events += ["dma_enable", "block_ready", "loan", "stop"]
                    if scenario in ("held_backpressure", "service_deadline"):
                        events.append("loss")
                    events += ["dma_disabled", "irq_joined"]
                    if cache:
                        events.append("invalidate_rx")
                    events += ["return_loan"]
                elif name == "U575-TX-CHAIN":
                    events += ["node_publish", "dma_enable", "chain_last",
                               "dma_disabled", "irq_joined", "wire_tc"]
                else:
                    events += ["enable_write", "enable_readback", "enable_dsb",
                               "launch", "arrm", "ue", "disable_write",
                               "disable_readback", "disable_dsb"]
                events += (["quarantined"] if timeout else
                           ["drained", "settled", "release"])
                if scenario.startswith("cancel_"):
                    boundary = ("wire_idle" if name == "H743-SPI-CACHE" else
                                "stop" if name == "H743-RX-CONTINUOUS" else
                                "wire_tc")
                    events.insert(events.index(boundary), "cancel")
            time = index * 100000000
            observed = []
            for event in events:
                time += (size * 1000 if name == "U575-LPTIM-RUN" and
                         event == "ue" else 1000)
                if name == "H743-RX-CONTINUOUS" and event == "block_ready":
                    time += 4 * size * 10 * 556 * 1000 // 64
                if (scenario == "synchronization_delay" and
                        event == "enable_readback"):
                    time += 2000
                observed.append({"at_ns": time, "event": event})
            trials.append({"id": scenario, "scenario": scenario,
                "requested_units": size,
                "status": "rejected" if rejected else "accepted",
                "deadline_ns": time + 1000000, "clock_domain": "monotonic1",
                "clock_epoch": 7, "events": observed})
            if timeout:
                # Deliberately fail the late observer/reference withdrawal
                # after physical wire completion, not an arbitrary DMA hang.
                trials[-1]["deadline_ns"] = observed[-2]["at_ns"]
            if name == "H743-RX-CONTINUOUS":
                trials[-1]["service_sample_ns"] = observed[0]["at_ns"]
                trials[-1]["remaining_bytes"] = size
                # TIM2 and USART share the observed64MHz HSI domain. The
                # conservative guard is20bytes and two1us timer quanta.
                trials[-1]["service_deadline_ns"] = (
                    observed[0]["at_ns"] +
                    max(0, (2 * size - 20) * 8 * 556 // 64 - 2) * 1000)
                trials[-1]["dma_targets"] = []
            base = 0x20000000 if name.startswith("U575") else 0x24000000
            buffers = [{"address": base + slot * 0x20000, "size": size,
                        "exclusive": True} for slot in range(
                            3 if name == "H743-RX-CONTINUOUS" else
                            2 if name == "H743-SPI-CACHE" else 1)]
            if scenario == "reject_unaligned":
                buffers[0]["address"] += 1
            if scenario == "reject_overlap":
                buffers[1]["address"] = buffers[0]["address"]
            if scenario == "reject_length":
                trials[-1]["requested_units"] = 0
            if scenario == "reject_size":
                buffers[0]["size"] = 31
            if scenario == "reject_one_tick":
                trials[-1]["requested_units"] = 1
            payload = self.data(name + "-" + scenario + ".bin", size)
            nodes = [{"address": 0x2002ffe0 + slot * 12, "size": 12}
                     for slot in range(2)] if name == "U575-TX-CHAIN" else []
            if scenario == "reject_window":
                nodes[1]["address"] += 65536
            sample = {"id": scenario, "buffers": buffers, "nodes": nodes,
                      "before": payload, "after": payload}
            if name == "H743-RX-CONTINUOUS":
                sample.update(held_before=payload, held_after=payload,
                              cpu_visible=payload)
                if not rejected:
                    addresses = [buffer["address"] for buffer in buffers]
                    enabled = next(entry["at_ns"] for entry in observed
                                   if entry["event"] == "dma_enable")
                    period = size * 10 * 556 * 1000 // 64
                    rotations = ((0, 0, 1, [], []),
                                 (1, 2, 1, [0], []),
                                 (0, 2, 0, [1], [0]),
                                 (1, 1, 0, [2], [1]))
                    targets = [{"at_ns": enabled + 1000 + step * period,
                        "cr": (1 << 18) | (1 << 8) | 1 | (ct << 19),
                        "m0ar": addresses[m0], "m1ar": addresses[m1],
                        "ndtr": size,
                        "held": [addresses[slot] for slot in held],
                        "returned": [addresses[slot] for slot in returned]}
                        for step, (ct, m0, m1, held, returned) in
                        enumerate(rotations)]
                    trials[-1]["dma_targets"] = targets
                    trials[-1]["service_sample_ns"] = targets[-1]["at_ns"]
                    trials[-1]["service_deadline_ns"] = (
                        targets[-1]["at_ns"] +
                        max(0, (2 * size - 20) * 8 * 556 // 64 - 2) * 1000)
                    if scenario == "service_deadline":
                        stop = next(entry for entry in observed
                                    if entry["event"] == "stop")
                        stop_at = stop["at_ns"]
                        delta = (trials[-1]["service_deadline_ns"] +
                                 1000 - stop_at)
                        for entry in observed:
                            if entry["at_ns"] >= stop_at:
                                entry["at_ns"] += delta
                        trials[-1]["deadline_ns"] += delta
            if name == "U575-TX-CHAIN":
                remaining = max(0, size - 65535)
                source = base + min(size, 65535)
                for slot, node in enumerate(nodes):
                    block = min(remaining, 65535) if not rejected else 0
                    remaining -= block
                    node.update(block=block, source=source if block else 0,
                        link=(nodes[1]["address"] & 0xfffc) | 0x30010000
                        if slot == 0 and remaining else 0)
                    source += block
                sample["channel"] = {"CLBAR": nodes[0]["address"] & 0xffff0000,
                    "CLLR": (nodes[0]["address"] & 0xfffc) | 0x30010000
                    if size > 65535 else 0,
                    "CSAR": base,
                    "CBR1": min(size, 65535)} if not rejected else {
                        "CLBAR": 0, "CLLR": 0, "CSAR": 0, "CBR1": 0}
                sample["fetches"] = [{key: node[key] for key in
                    ("block", "source", "link")} for node in nodes
                    if node["block"]]
                sample["withdrawn"] = {"CLBAR": 0, "CLLR": 0,
                    "CSAR": base if timeout else 0,
                    "nodes": ([{key: node[key] for key in
                                ("block", "source", "link")}
                               for node in nodes] if timeout else
                              [{"block": 0, "source": 0, "link": 0}] * 2)}
            memory.append(sample)
            wire.append({"id": scenario,
                "tx": None if rejected else payload,
                "rx": None if rejected else payload,
                "timer_ticks": 0 if rejected else trials[-1]["requested_units"],
                "activity_count": 0 if rejected else size})
        roles = {"mode_registers": {"facts": module.expected_facts(
                    name, resolved)},
                 "wire_capture": {"trials": wire},
                 "memory_capture": {"trials": memory},
                 "lifecycle_events": {"trials": trials}}
        observations = {}
        for role, body in roles.items():
            path = self.root / (role + ".json")
            if role == "mode_registers" and name.startswith("U575"):
                body["facts"]["revision_id"] = 0x1001
            document = {"schema_version": 1,
                "kind": "selected_mode_observation",
                "case_id": name, "role": role, "session": session,
                "protocol_sha256": protocol_identity["sha256"],
                "context_sha256": context_identity["sha256"],
                "controller_id": controller["id"],
                "instrument": module.instrument_for(name, role), **body}
            atomic_json(path, document)
            observations[role] = file_identity(path)
        return (name, observations, resolved, protocol, session,
                protocol_identity, context_identity)

    def mutate(self, args, role, change):
        path = Path(args[1][role]["path"])
        document = json.loads(path.read_text())
        change(document)
        atomic_json(path, document)
        args[1][role] = file_identity(path)

    def test_each_mode_parses_independent_observations_without_qualifying(self):
        for name in self.assemblies:
            with self.subTest(case=name):
                args = self.documents(name)
                result = self.module().validate_observations(*args)
                self.assertGreater(result["trials"], 0)
                self.assertNotIn("physical_status", result)

    def test_continuous_rx_cache_on_capture_is_separate_and_valid(self):
        selected = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/"
            "h743-uart-continuous-cache-on.toml").to_dict()
        args = self.documents("H743-RX-CONTINUOUS", selected=selected)
        self.assertGreater(self.module().validate_observations(*args)["trials"],
                           0)

    def test_raw_role_session_and_selected_controller_mismatch(self):
        for key, value in (("role", "memory_capture"), ("session", "4" * 32),
                           ("protocol_sha256", "4" * 64),
                           ("controller_id", "foreign")):
            args = self.documents()
            self.mutate(args, "wire_capture",
                        lambda raw: raw.update({key: value}))
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)

    def test_raw_register_revision_cache_and_clock_mismatch_reject(self):
        for key, value in (("revision_id", 0x1003),
                           ("nominal_core_hz", 32000000),
                           ("cache_line_bytes", 0)):
            args = self.documents()
            self.mutate(args, "mode_registers",
                        lambda raw: raw["facts"].update({key: value}))
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)

    def test_missing_required_scenario_or_non_monotonic_time_reject(self):
        for change in (lambda raw: raw["trials"].pop(),
                       lambda raw: raw["trials"][0]["events"][1].update(
                           at_ns=0)):
            args = self.documents()
            self.mutate(args, "lifecycle_events", change)
            with self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)

    def test_rejected_request_cannot_have_effects_or_borrow(self):
        args = self.documents()
        self.mutate(args, "lifecycle_events", lambda raw:
                    raw["trials"][-1]["events"][-1].update(event="borrow"))
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_dma_tc_cannot_substitute_wire_settlement_or_drain(self):
        for name, event in (("H743-SPI-CACHE", "wire_idle"),
                            ("U575-TX-CHAIN", "wire_tc"),
                            ("H743-RX-CONTINUOUS", "irq_joined")):
            args = self.documents(name)
            self.mutate(args, "lifecycle_events", lambda raw:
                raw["trials"][0].update(events=[entry for entry in
                    raw["trials"][0]["events"] if entry["event"] != event]))
            with self.subTest(case=name), self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)

    def test_failed_drain_cannot_release_or_publish_settled(self):
        args = self.documents()
        def release(raw):
            trial = next(item for item in raw["trials"]
                         if item["scenario"] == "drain_timeout")
            trial["events"][-1]["event"] = "release"
        self.mutate(args, "lifecycle_events", release)
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_cache_lines_and_stream_block_nonoverlap_are_enforced(self):
        for name in ("H743-SPI-CACHE", "H743-RX-CONTINUOUS"):
            args = self.documents(name)
            self.mutate(args, "memory_capture", lambda raw:
                raw["trials"][0]["buffers"][1].update(
                    address=raw["trials"][0]["buffers"][0]["address"]))
            with self.subTest(case=name), self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)

    def test_u575_chain_window_and_link_termination_are_verified(self):
        for change in (lambda raw: raw["trials"][0]["nodes"][1].update(link=1),
                       lambda raw: raw["trials"][0]["nodes"][1].update(
                           address=0x20070000)):
            args = self.documents("U575-TX-CHAIN")
            self.mutate(args, "memory_capture", change)
            with self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)

    def test_timer_one_tick_and_short_synchronization_do_not_pass(self):
        for change in (lambda raw: raw["trials"][0].update(requested_units=1),
                       lambda raw: next(event for event in
                           raw["trials"][0]["events"] if event["event"] ==
                           "launch").update(at_ns=raw["trials"][0][
                               "events"][2]["at_ns"] + 1)):
            args = self.documents("U575-LPTIM-RUN")
            self.mutate(args, "lifecycle_events", change)
            with self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)

    def test_wire_payload_hash_change_cannot_pass(self):
        args = self.documents()
        path = self.root / "different.bin"
        path.write_bytes(b"x" * 32)
        self.mutate(args, "wire_capture", lambda raw:
                    raw["trials"][0].update(rx=file_identity(path)))
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_stream_service_budget_and_clock_epoch_cannot_be_invented(self):
        for key, value in (("service_deadline_ns", 2 ** 63),
                           ("clock_epoch", 0)):
            args = self.documents("H743-RX-CONTINUOUS")
            self.mutate(args, "lifecycle_events", lambda raw:
                        raw["trials"][0].update({key: value}))
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)

    def test_rx_deadline_uses_actual_equal_block_capacity(self):
        args = self.documents("H743-RX-CONTINUOUS")
        payload = self.data("oversized-metadata-payload.bin", 64)
        def enlarge(raw):
            trial = raw["trials"][0]
            trial.update(requested_units=64, remaining_bytes=64)
            trial["service_deadline_ns"] = (trial["service_sample_ns"] +
                ((128 - 20) * 8 * 556 // 64 - 2) * 1000)
        self.mutate(args, "lifecycle_events", enlarge)
        self.mutate(args, "memory_capture", lambda raw:
                    raw["trials"][0].update(before=payload, after=payload))
        self.mutate(args, "wire_capture", lambda raw:
                    raw["trials"][0].update(tx=payload, rx=payload))
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_rx_wrap_requires_actual_target_rotation_observations(self):
        args = self.documents("H743-RX-CONTINUOUS")
        self.mutate(args, "lifecycle_events", lambda raw:
                    raw["trials"][0].pop("dma_targets", None))
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_rx_retained_loan_needs_independent_held_memory_capture(self):
        args = self.documents("H743-RX-CONTINUOUS")
        self.mutate(args, "memory_capture", lambda raw:
                    raw["trials"][1].pop("held_after", None))
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_post_settlement_access_or_unknown_effect_cannot_be_hidden(self):
        args = self.documents()
        def append(raw):
            events = raw["trials"][0]["events"]
            events.append({"at_ns": events[-1]["at_ns"] + 1,
                           "event": "provider_payload_write"})
        self.mutate(args, "lifecycle_events", append)
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_success_minimum_and_maximum_are_actual_boundary_stimuli(self):
        args = self.documents()
        payload = self.data("nonboundary.bin", 64)
        self.mutate(args, "lifecycle_events", lambda raw:
                    raw["trials"][0].update(requested_units=64))
        def memory(raw):
            trial = raw["trials"][0]
            trial.update(before=payload, after=payload)
            for buffer in trial["buffers"]:
                buffer["size"] = 64
        self.mutate(args, "memory_capture", memory)
        self.mutate(args, "wire_capture", lambda raw:
                    raw["trials"][0].update(tx=payload, rx=payload))
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_timeout_requires_observed_expiry_before_quarantine(self):
        args = self.documents()
        def future_deadline(raw):
            trial = next(item for item in raw["trials"]
                         if item["scenario"] == "drain_timeout")
            trial["deadline_ns"] = trial["events"][-1]["at_ns"] + 1000000
        self.mutate(args, "lifecycle_events", future_deadline)
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_same_kind_first_uart_cannot_replace_exact_controller(self):
        args = self.documents("H743-RX-CONTINUOUS")
        foreign = copy.deepcopy(args[2]["controllers"][0])
        foreign.update(id="foreign", controller="USART2", mode="irq-bytes")
        args[2]["controllers"].insert(0, foreign)
        self.assertGreater(self.module().validate_observations(*args)["trials"],
                           0)

    def test_spi_and_tx_payload_must_fit_actual_borrowed_storage(self):
        for name, size in (("H743-SPI-CACHE", 32), ("U575-TX-CHAIN", 1)):
            args = self.documents(name)
            def shrink(raw):
                for buffer in raw["trials"][1]["buffers"]:
                    buffer["size"] = size
            self.mutate(args, "memory_capture", shrink)
            with self.subTest(case=name), self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)

    def test_rejection_requires_verified_unchanged_memory_snapshot(self):
        for changed in (True, False):
            args = self.documents()
            identity = self.data("changed-rejected-storage.bin")
            if changed:
                Path(identity["path"]).write_bytes(b"changed" * 5)
                identity = file_identity(Path(identity["path"]))
            else:
                identity["path"] = str(self.root / "nonexistent-capture.bin")
            self.mutate(args, "memory_capture", lambda raw:
                        raw["trials"][-1].update(after=identity))
            with self.subTest(changed=changed), self.assertRaises(
                    EvidenceError):
                self.module().validate_observations(*args)

    def test_wire_role_requires_the_reviewed_wire_instrument(self):
        args = self.documents()
        self.mutate(args, "wire_capture", lambda raw:
                    raw.update(instrument="debug_probe"))
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_u575_revision_is_observed_without_inventing_a_revision_gate(self):
        args = self.documents("U575-TX-CHAIN")
        self.mutate(args, "mode_registers", lambda raw:
                    raw["facts"].update(revision_id=0x1001))
        self.assertGreater(self.module().validate_observations(*args)["trials"],
                           0)

    def test_u575_revision_cannot_be_unknown_or_boolean(self):
        for value in (None, True):
            args = self.documents("U575-TX-CHAIN")
            self.mutate(args, "mode_registers", lambda raw:
                        raw["facts"].update(revision_id=value))
            with self.subTest(value=value), self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)

    def test_rx_active_target_cannot_change_during_replacement(self):
        args = self.documents("H743-RX-CONTINUOUS")
        def swap(raw):
            target = raw["trials"][0]["dma_targets"][1]
            target["m0ar"], target["m1ar"] = target["m1ar"], target["m0ar"]
        self.mutate(args, "lifecycle_events", swap)
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_cancel_cases_need_an_observed_cancel_trigger(self):
        for name in ("H743-SPI-CACHE", "H743-RX-CONTINUOUS", "U575-TX-CHAIN"):
            args = self.documents(name)
            def erase(raw):
                trial = next(item for item in raw["trials"]
                             if item["scenario"].startswith("cancel_"))
                trial["events"] = [entry for entry in trial["events"]
                                   if entry["event"] != "cancel"]
            self.mutate(args, "lifecycle_events", erase)
            with self.subTest(case=name), self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)

    def test_timer_delay_stimulus_must_exceed_ordinary_synchronization(self):
        args = self.documents("U575-LPTIM-RUN")
        def shorten(raw):
            trial = next(item for item in raw["trials"]
                         if item["scenario"] == "synchronization_delay")
            write = next(entry["at_ns"] for entry in trial["events"]
                         if entry["event"] == "enable_write")
            next(entry for entry in trial["events"] if
                 entry["event"] == "enable_readback")["at_ns"] = write + 1000
        self.mutate(args, "lifecycle_events", shorten)
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_chain_contract_names_real_block_boundaries(self):
        scenarios = set(self.module().SCENARIOS["U575-TX-CHAIN"])
        self.assertLessEqual({"success_min", "block_max", "two_block_min",
                              "two_block_max", "three_block_threshold"},
                             scenarios)
        self.assertNotIn("success_max", scenarios)

    def test_chain_requires_actual_words_and_initial_channel_snapshot(self):
        for missing in ("channel", "block"):
            args = self.documents("U575-TX-CHAIN")
            def erase(raw):
                trial = raw["trials"][0]
                if missing == "channel":
                    trial.pop("channel", None)
                else:
                    trial["nodes"][0].pop("block", None)
            self.mutate(args, "memory_capture", erase)
            with self.subTest(missing=missing), self.assertRaises(
                    EvidenceError):
                self.module().validate_observations(*args)

    def test_observed_full_rx_blocks_use_real_8n1_wire_duration(self):
        args = self.documents("H743-RX-CONTINUOUS")
        def shorten(raw):
            targets = raw["trials"][0]["dma_targets"]
            first = targets[0]["at_ns"]
            for index, target in enumerate(targets):
                target["at_ns"] = first + index * 32 * 8 * 556 * 1000 // 64
            trial = raw["trials"][0]
            trial["service_sample_ns"] = targets[-1]["at_ns"]
            trial["service_deadline_ns"] = (targets[-1]["at_ns"] +
                ((64 - 20) * 8 * 556 // 64 - 2) * 1000)
        self.mutate(args, "lifecycle_events", shorten)
        with self.assertRaises(EvidenceError):
            self.module().validate_observations(*args)

    def test_selected_mode_route_is_checked_by_the_provider_authority(self):
        for name in ("H743-SPI-CACHE", "H743-RX-CONTINUOUS", "U575-TX-CHAIN"):
            resolved = copy.deepcopy(self.resolved[name])
            resolved["controllers"][0]["pins"][0]["pin"] = "PA0"
            with self.subTest(case=name):
                self.assertNotEqual(self.module().selection(name, resolved)[0],
                                    "selected")

    def test_new_workload_cannot_budget_away_protocol_errors(self):
        resolved = self.resolved["H743-SPI-CACHE"]
        fixture = self.module().fixture_for(resolved)
        definition = workloads.WORKLOADS["h743_spi_cache"]
        instrument = self.data("reviewed-instrument.bin")
        case = {"id": "h743_spi_cache", "bindings": ["bus4"],
            "stimulus": "Synthetic review only; not executed",
            "raw_observations": {key: "Retain " + key for key in
                                 definition["raw_observations"]},
            "instruments": dict.fromkeys(definition["instruments"], instrument),
            "safety": {key: "Review " + key for key in definition["safety"]},
            "limits": {"trials": {"unit": "count", "minimum": 1,
                                  "maximum": 100},
                       "protocol_errors": {"unit": "count", "minimum": 0,
                                           "maximum": 1}}}
        workload = {"schema_version": 1, "kind": "hil_workload",
                    "board": resolved["board"], "part": resolved["part"],
                    "cases": [case]}
        with self.assertRaises(EvidenceError):
            workloads.validate_workload(workload, fixture, resolved)

    def test_failed_chain_withdrawal_keeps_typed_published_words(self):
        for change in (lambda state: state.update(nodes=None),
                       lambda state: state.update(CLBAR=-1, CLLR=0, CSAR=0),
                       lambda state: state.update(nodes=[
                           {"block": 0, "source": 0, "link": 0}] * 2)):
            args = self.documents("U575-TX-CHAIN")
            def corrupt(raw):
                trial = next(item for item in raw["trials"]
                             if item["id"] == "drain_timeout")
                change(trial["withdrawn"])
            self.mutate(args, "memory_capture", corrupt)
            with self.assertRaises(EvidenceError):
                self.module().validate_observations(*args)


if __name__ == "__main__":
    unittest.main()
