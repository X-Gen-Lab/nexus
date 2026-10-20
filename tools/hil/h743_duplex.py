"""Exact H743 duplex acquisition obligations, not a hardware qualification.

The finite TX request and RX stream have independent ownership. Register exports
and CPU acquire snapshots are retained inputs. No probe is operated here.
"""

from tools.evidence.common import fields, verify_file_identity
from tools.hil.admit import require


CASE = "H743-DUPLEX-CONTINUOUS"
BASE_SCENARIOS = ("tx_min", "tx_max", "cancel_tx_tc", "stop_rx_held",
                  "tx_drain_timeout", "rx_drain_timeout", "domain_taint",
                  "reject_tx_overlap", "reject_rx_overlap")
TX_CR = 0x120457
RX_CR = 0x160517
TX_SOURCES = 0x1e
IRQS = {"dma1_stream0", "dma1_stream1", "usart3"}


def scenarios():
    """Production single-executor scenarios; model reentry stays host-only."""
    return BASE_SCENARIOS


def _bytes(identity):
    return verify_file_identity(identity).read_bytes()


def _span(buffer, cached, integer):
    fields(buffer, {"address", "size", "exclusive"})
    start = integer(buffer["address"], "duplex address", maximum=2 ** 32 - 1)
    size = integer(buffer["size"], "duplex capacity", 1, 65535)
    require(buffer["exclusive"] is True and
            0x24000000 <= start < start + size <= 0x24080000 and
            (not cached or start % 32 == size % 32 == 0),
            "duplex payload needs exclusive selected AXI/cache-line storage")
    return start, start + size


def _disjoint(spans):
    return all(right <= other_left or left >= other_right
               for index, (left, right) in enumerate(spans)
               for other_left, other_right in spans[index + 1:])


def _memory(memory, cached, rejected, scenario, integer):
    fields(memory, {"id", "rx_buffers", "tx_buffer", "controls", "tx_before",
        "tx_after", "rx_before", "rx_after", "rx_cpu_visible", "held_before",
        "held_after", "clean_span", "invalidate_spans", "cpu_acquire_ns",
        "rx_observation", "held_observation"})
    require(isinstance(memory["rx_buffers"], list) and
            len(memory["rx_buffers"]) == 3, "duplex three RX blocks required")
    rx = [_span(buffer, cached, integer) for buffer in memory["rx_buffers"]]
    tx = _span(memory["tx_buffer"], cached, integer)
    require(len({right - left for left, right in rx}) == 1 and
            rx[0][1] - rx[0][0] >= 32,
            "duplex RX capacities must match and cover one cache line")
    if rejected:
        require((not _disjoint(rx + [tx]) and _disjoint(rx))
                if scenario == "reject_tx_overlap" else not _disjoint(rx),
                "duplex rejected overlap stimulus missing")
    else:
        integer(memory["cpu_acquire_ns"], "duplex CPU acquire timestamp")
        require(_disjoint(rx + [tx]), "duplex TX/RX payload alias")
    controls, roles = [], set()
    require(isinstance(memory["controls"], list) and
            len(memory["controls"]) == 4, "duplex static controls required")
    for control in memory["controls"]:
        fields(control, {"role", "address", "size", "static"})
        start = integer(control["address"], "duplex control address",
                        maximum=2 ** 32 - 1)
        size = integer(control["size"], "duplex ABI control size", 1, 4096)
        minimum = {"owner": 272, "stream": 40, "slots": 120,
                   "tx_request": 32}.get(control["role"])
        require(control["role"] not in roles and control["static"] is True and
                minimum is not None and size >= minimum and start % 8 == 0 and
                (0x20000000 <= start < start + size <= 0x20020000 or
                 0x24000000 <= start < start + size <= 0x24080000),
                "duplex unique static CPU-accessible control required")
        roles.add(control["role"])
        controls.append((start, start + size))
    require(roles == {"owner", "stream", "slots", "tx_request"} and
            _disjoint(controls) and all(
                right <= left_payload or left >= right_payload
                for left, right in controls
                for left_payload, right_payload in rx + [tx]),
            "duplex caller control/payload alias")
    before, after = _bytes(memory["tx_before"]), _bytes(memory["tx_after"])
    require(before == after and len(before) <= tx[1] - tx[0],
            "duplex immutable TX borrow changed or exceeded its span")
    require(_bytes(memory["held_before"]) == _bytes(memory["held_after"]),
            "duplex held consumer memory overwritten")
    if scenario == "reject_rx_overlap":
        require(_bytes(memory["rx_before"]) == _bytes(memory["rx_after"]) ==
                _bytes(memory["rx_cpu_visible"]) and
                memory["clean_span"] is None and
                memory["invalidate_spans"] == [],
                "duplex rejection changed memory/cache")
    else:
        require(len(_bytes(memory["rx_after"])) == rx[0][1] - rx[0][0] and
                _bytes(memory["rx_cpu_visible"]) == _bytes(memory["rx_after"])
                and _bytes(memory["rx_before"]) != _bytes(memory["rx_after"]),
                "duplex physical DMA and CPU acquire-visible bytes disagree")
        expected_clean = {"address": tx[0], "size": len(before)}
        expected_invalidate = [{"address": left, "size": right - left}
                               for left, right in rx]
        require(memory["clean_span"] ==
                (expected_clean if cached and not rejected else None)
                and memory["invalidate_spans"] ==
                (expected_invalidate if cached else []),
                "duplex cache maintenance does not cover exact payload spans")
    return rx, tx, {item["role"]: item["address"]
                    for item in memory["controls"]}


def _tx_snapshots(tx, span, rejected, tainted, timeout, integer):
    snapshots = tx["snapshots"]
    require(isinstance(snapshots, list) and 1 <= len(snapshots) <= 16,
            "duplex bounded TX phase snapshots required")
    phases, previous = {}, -1
    registers = {"cr", "ndtr", "par", "m0ar", "fcr", "mux", "usart_cr1",
                 "usart_cr3", "usart_isr", "lisr", "active_request"}
    for snapshot in snapshots:
        fields(snapshot, {"at_ns", "phase", "request_state", "ref", "preparing",
                          "publisher_active"} | registers)
        instant = integer(snapshot["at_ns"], "duplex phase timestamp")
        require(instant > previous and snapshot["phase"] not in phases and
                type(snapshot["publisher_active"]) is bool and
                type(snapshot["preparing"]) is bool,
                "duplex ordered unique hardware phases required")
        for key in registers:
            integer(snapshot[key], "duplex " + key, maximum=2 ** 32 - 1)
        integer(snapshot["ref"], "duplex TX loan", 0, 1)
        if not rejected and snapshot["phase"] != "preparing":
            require(snapshot["par"] == 0x40004828 and snapshot["mux"] == 46
                    and snapshot["m0ar"] == span[0] and
                    snapshot["fcr"] & 0x84 == 0,
                    "duplex TX exact TDR/MUX/byte/direct route mismatch")
        phases[snapshot["phase"]] = snapshot
        previous = instant
    if rejected:
        require(set(phases) in ({"before", "rejected"},
                               {"before", "not_submitted"}),
                "duplex REJECTED effects must include before/after hardware")
        before = phases["before"]
        after = phases.get("rejected", phases.get("not_submitted"))
        require(all(before[key] == after[key] for key in registers) and
                before["ref"] == after["ref"] == 0 and
                after["active_request"] == 0 and after["cr"] == 0 and
                after["usart_cr3"] & 128 == 0 and
                before["request_state"] == after["request_state"] == "READY",
                "duplex REJECTED retained borrow or MMIO effects")
        return phases
    require("preparing" in phases, "duplex PREPARING observation missing")
    baseline = tx["preparation_baseline"]
    fields(baseline, {"cr", "ndtr", "par", "m0ar", "fcr", "mux",
                      "usart_cr1", "usart_cr3"})
    for key, value in baseline.items():
        integer(value, "duplex pre-clean " + key, maximum=2 ** 32 - 1)
    require(all(phases["preparing"][key] == baseline[key]
                for key in ("cr", "ndtr", "par", "m0ar", "fcr", "mux")) and
            baseline["mux"] == 46 and
            (phases["preparing"]["usart_cr1"] & 192) ==
            (baseline["usart_cr1"] & 192) and
            (phases["preparing"]["usart_cr3"] & 128) ==
            (baseline["usart_cr3"] & 128),
            "duplex PREPARING changed pre-clean TX hardware baseline")
    expected_phases = ({"preparing", "launch"} if tainted else
        {"preparing", "launch", "memory_tc", "quarantined"} if timeout else
        {"preparing", "launch", "memory_tc", "withdrawn", "settled"})
    require(set(phases) == expected_phases,
            "duplex unknown or missing TX hardware phase")
    for phase, snapshot in phases.items():
        settled = phase == "settled"
        require(snapshot["ref"] == (0 if settled else 1) and
                snapshot["active_request"] ==
                (0 if settled else tx["request_address"]),
                "duplex TX loan released before acquire-observed SETTLED")
        expected_state = {"withdrawn": "DRAINING", "settled": "SETTLED",
                          "quarantined": "QUARANTINED"}.get(phase, "ACTIVE")
        require(snapshot["request_state"] == expected_state and
                snapshot["preparing"] is (phase == "preparing"),
                "duplex actual Core state/preparing phase mismatch")
        if phase == "preparing":
            require(snapshot["cr"] & 1 == snapshot["usart_cr3"] & 128 == 0
                    and snapshot["request_state"] == "ACTIVE",
                    "duplex PREPARING cache clean launched DMA/DMAT")
    require("launch" in phases and phases["launch"]["cr"] == TX_CR and
            phases["launch"]["ndtr"] == tx["length"] and
            phases["launch"]["usart_cr3"] & 128 != 0,
            "duplex finite byte TX launch registers mismatch")
    if not tainted:
        sample = phases.get("memory_tc", {})
        require(sample.get("ndtr") == 0 and sample.get("lisr", 0) & 32 != 0,
                "duplex memory TC requires actual NDTR0 and TCIF0")
    if not timeout and not tainted:
        require("withdrawn" in phases and "settled" in phases,
                "duplex final withdrawal and SETTLED snapshots required")
        for phase in ("withdrawn", "settled"):
            snapshot = phases[phase]
            require(snapshot["cr"] & (TX_SOURCES | 1) == 0 and
                    snapshot["usart_cr3"] & 128 == 0 and
                    snapshot["usart_cr1"] & 192 == 0 and
                    snapshot["lisr"] & 0x3d == 0 and
                    snapshot["publisher_active"] is False,
                    "duplex TX source/reference/publisher not joined")
        require(phases["settled"]["request_state"] == "SETTLED",
                "duplex final request state is not SETTLED")
    elif timeout:
        sample = phases.get("quarantined", {})
        require(sample.get("cr", 0) & 1 and
                sample["cr"] & TX_SOURCES == 0 and
                sample["usart_cr3"] & 128 == 0 and
                sample.get("at_ns", 0) >= tx["drain_deadline_ns"] and
                not {"withdrawn", "settled"}.intersection(phases),
                "duplex failed TX EN withdrawal lost quarantine")
    else:
        require("settled" not in phases,
                "duplex joint taint cannot settle retained request")
    return phases


def _rx(rx, spans, scenario, taint, controller, helpers):
    integer, events_fn, ordered, targets_fn = helpers
    fields(rx, {"stream_address", "stop_budget_start_ns",
        "stop_budget_deadline_ns", "stop_return_ns", "result", "events",
        "dma_targets", "registers", "states", "service_sample_ns",
        "remaining_bytes", "service_deadline_ns", "quarantined",
        "stop_registers", "loans", "admission"})
    require(rx["admission"] is None,
            "duplex active RX cannot inherit rejected admission")
    registers = rx["registers"]
    fields(registers, {"par", "mux", "cr", "fcr", "usart_cr3"})
    for key, value in registers.items():
        integer(value, "duplex RX " + key, maximum=2 ** 32 - 1)
    require(registers["par"] == 0x40004824 and registers["mux"] == 45 and
            registers["cr"] == RX_CR and registers["fcr"] & 0x84 == 0 and
            registers["usart_cr3"] & 64,
            "duplex RX exact RDR/MUX/byte/direct route mismatch")
    events = events_fn(rx)
    ordered(events, ["dma_enable", "block_ready", "loan"])
    stop = taint["at_ns"] if taint else events.get("stop")
    require(type(stop) is int, "duplex RX stop/taint boundary missing")
    divisor = (64000000 + controller["baud"] // 2) // controller["baud"]
    targets_fn(rx, spans, divisor, {**events, "stop": stop})
    capacity = spans[0][1] - spans[0][0]
    sample = integer(rx["service_sample_ns"], "duplex RX service sample")
    remaining = integer(rx["remaining_bytes"], "duplex RX NDTR", 0, capacity)
    budget = max(0, (remaining + capacity - 20) * 8 * divisor // 64 - 2) * 1000
    require(sample == rx["dma_targets"][-1]["at_ns"] and
            remaining == rx["dma_targets"][-1]["ndtr"] and
            rx["service_deadline_ns"] == sample + budget and
            stop < rx["service_deadline_ns"],
            "duplex actual RX service/NDTR replacement budget mismatch")
    require(all(sample["cr"] & ~(1 << 19) == RX_CR
                for sample in rx["dma_targets"]),
            "duplex RX observed rotation changed selected DMA configuration")
    states = rx["states"]
    require(isinstance(states, list) and 2 <= len(states) <= 64,
            "duplex producer/slot ownership observations required")
    addresses = {left for left, _ in spans}
    previous = -1
    for index, state in enumerate(states):
        fields(state, {"at_ns", "producer_loans", "producer_pending",
                       "outstanding", "stopping", "slots",
                       "capture_sequence"})
        if taint and index == len(states) - 1:
            require(state["at_ns"] is None and
                    integer(state["capture_sequence"],
                        "duplex retained RX capture order", 1) >
                    taint["isolation_sequence"],
                    "duplex quarantined RX tail needs post-isolation capture")
        else:
            instant = integer(state["at_ns"], "duplex RX ownership timestamp")
            require(state["capture_sequence"] is None and instant > previous,
                    "duplex ordered live RX ownership observations required")
            previous = instant
        require(type(state["stopping"]) is bool,
                "duplex ordered RX ownership states required")
        for key in ("producer_loans", "producer_pending", "outstanding"):
            integer(state[key], "duplex RX " + key, 0, 3)
        slots = state["slots"]
        require(isinstance(slots, list) and len(slots) == 3,
                "duplex exact stream slot inventory required")
        for slot in slots:
            fields(slot, {"address", "state", "epoch"})
            integer(slot["address"], "duplex slot address", maximum=2 ** 32 - 1)
            integer(slot["epoch"], "duplex stream slot epoch",
                    0 if slot["state"] == "FREE" else 1)
            require(slot["state"] in {"FREE", "FILLING", "READY", "BORROWED"},
                    "duplex unknown stream slot state")
        live_epochs = [slot["epoch"] for slot in slots
                       if slot["state"] != "FREE"]
        require(len(set(live_epochs)) == len(live_epochs) and
                {slot["address"] for slot in slots} == addresses and
                state["outstanding"] == sum(slot["state"] != "FREE"
                                            for slot in slots) and
                state["producer_loans"] == sum(slot["state"] == "FILLING"
                                               for slot in slots) and
                state["producer_pending"] == state["producer_loans"],
                "duplex producer refs and independent stream slots disagree")
    final = states[-1]
    retained = scenario in ("rx_drain_timeout", "domain_taint")
    held = scenario == "stop_rx_held"
    last_target = rx["dma_targets"][-1]
    require({slot["address"] for slot in final["slots"]
             if slot["state"] == "BORROWED"} <= set(last_target["held"]) and
            (not retained or
             {slot["address"] for slot in final["slots"]
              if slot["state"] == "FILLING"} ==
             {last_target["m0ar"], last_target["m1ar"]}),
            "duplex retained stream inventory differs from actual DMA targets")
    require(final["stopping"] is (scenario != "domain_taint") and
            rx["quarantined"] is retained and
            final["producer_loans"] == (2 if retained else 0) and
            rx["result"] == ("BUSY" if retained or held else "SUCCESS"),
            "duplex RX drain cannot revoke producer/consumer loans")
    if held:
        require(final["outstanding"] == 1 and
                any(slot["state"] == "BORROWED" for slot in final["slots"])
                and "consumer_return" not in events,
                "duplex stopped stream lost independent consumer HELD loan")
    elif retained:
        require(final["outstanding"] == 3 and any(
            slot["state"] == "BORROWED" for slot in final["slots"]),
            "duplex quarantine lost held consumer or producer loan")
    elif not retained:
        require(final["outstanding"] == 0,
                "duplex successful RX stop left outstanding stream slots")
        ordered(events, ["stop", "producer_detached", "consumer_return",
                          "drained"])
        require(events["drained"] <= rx["stop_return_ns"],
                "duplex RX SUCCESS returned before actual drain")
    if not retained:
        require(events["producer_detached"] <= rx["stop_return_ns"],
                "duplex RX returned before producer detach")
    if scenario == "rx_drain_timeout":
        start = integer(rx["stop_budget_start_ns"], "RX external budget start")
        deadline = integer(rx["stop_budget_deadline_ns"], "RX external budget")
        returned = integer(rx["stop_return_ns"], "RX BUSY return")
        require(events["stop"] == start < returned < deadline <=
                events.get("quarantined", -1) and "drained" not in events,
                "duplex RX BUSY retention needs actual external budget expiry")
        snapshot = rx["stop_registers"]
        fields(snapshot, {"at_ns", "cr", "usart_cr1", "usart_cr3", "lisr"})
        for key, value in snapshot.items():
            integer(value, "duplex RX stop " + key, maximum=2 ** 32 - 1)
        require(start < snapshot["at_ns"] <= returned and
                snapshot["cr"] & 1 and snapshot["cr"] & TX_SOURCES == 0 and
                snapshot["usart_cr3"] & 65 == 0 and
                snapshot["usart_cr1"] & 0x30 == 0,
                "duplex RX EN refusal lacks actual stopped-source readback")
    else:
        require(rx["stop_registers"] is None,
                "duplex unselected RX EN refusal readback")


def _bindings(memory, rx, cached, integer, taint=None):
    """Bind probe bytes and CPU acquire bytes to actual borrowed slot epochs."""
    observed, held = memory["rx_observation"], memory["held_observation"]
    fields(observed, {"address", "epoch", "length", "dma_complete_ns",
        "invalidate_ns", "publish_ns", "acquire_ns", "physical_sample_ns",
        "cpu_sample_ns"})
    fields(held, {"address", "epoch", "length", "before_ns", "after_ns",
                  "before_sequence", "after_sequence"})
    loans = rx["loans"]
    require(isinstance(loans, list) and 2 <= len(loans) <= 4096,
            "duplex observed consumer loan identities required")
    for loan in loans:
        fields(loan, {"address", "epoch", "acquired_ns", "returned_ns"})
        for key in ("address", "epoch", "acquired_ns"):
            integer(loan[key], "duplex actual consumer " + key, 1)
        if loan["returned_ns"] is not None:
            require(integer(loan["returned_ns"], "duplex consumer return") >
                    loan["acquired_ns"], "duplex consumer return precedes loan")
    for binding in (observed, held):
        for key, value in binding.items():
            if value is not None:
                integer(value, "duplex memory binding " + key, 1)
        matches = [loan for loan in loans if
                   (loan["address"], loan["epoch"]) ==
                   (binding["address"], binding["epoch"])]
        require(len(matches) == 1 and any(buffer["address"] ==
                binding["address"] and buffer["size"] >= binding["length"]
                for buffer in memory["rx_buffers"]),
                "duplex memory bytes lack actual borrowed address/epoch")
        loan = matches[0]
        if binding is held:
            require(loan["acquired_ns"] <= binding["before_ns"] and
                    binding["length"] ==
                    len(_bytes(memory["held_before"])),
                    "duplex HELD byte samples leave retained loan interval")
            if rx["result"] == "BUSY":
                require(loan["returned_ns"] is None and any(
                    slot["address"] == binding["address"] and
                    slot["epoch"] == binding["epoch"] and
                    slot["state"] == "BORROWED"
                    for slot in rx["states"][-1]["slots"]) and
                    (taint is not None or
                     binding["after_ns"] > rx["stop_return_ns"]),
                    "duplex failed-stop proof must sample the consumer "
                    "still retained after return/isolation")
            if taint:
                require(binding["before_ns"] < taint["at_ns"] and
                        binding["after_ns"] is None and
                        binding["before_sequence"] < taint["fault_sequence"]
                        < taint["isolation_sequence"] <
                        binding["after_sequence"] and
                        loan["returned_ns"] is None,
                        "duplex tainted held proof needs independent capture "
                        "order, not invalid Core elapsed time")
            else:
                require(binding["before_sequence"] is None and
                        binding["after_sequence"] is None and
                        binding["before_ns"] < binding["after_ns"] and
                        (loan["returned_ns"] is None or binding["after_ns"] <
                         loan["returned_ns"]),
                        "duplex HELD samples leave live retained loan interval")
        else:
            require(binding["dma_complete_ns"] < binding["publish_ns"] <=
                    binding["acquire_ns"] <= binding["physical_sample_ns"] <=
                    binding["cpu_sample_ns"] and
                    binding["acquire_ns"] == loan["acquired_ns"] and
                    (loan["returned_ns"] is None or binding["cpu_sample_ns"] <
                     loan["returned_ns"]) and binding["length"] ==
                    len(_bytes(memory["rx_cpu_visible"])) and
                    memory["cpu_acquire_ns"] == binding["cpu_sample_ns"] and
                    any(sample["at_ns"] == binding["dma_complete_ns"] and
                        binding["address"] in sample["held"]
                        for sample in rx["dma_targets"]) and
                    any(state["at_ns"] == binding["cpu_sample_ns"] and any(
                        slot["address"] == binding["address"] and
                        slot["epoch"] == binding["epoch"] and
                        slot["state"] == "BORROWED" for slot in state["slots"])
                        for state in rx["states"]),
                    "duplex CPU acquire is not bound to published DMA loan")
            require((binding["dma_complete_ns"] <
                     integer(binding["invalidate_ns"], "duplex invalidate") <
                     binding["publish_ns"]) if cached else
                    binding["invalidate_ns"] is None,
                    "duplex CPU observation lacks ordered cache invalidation")


def validate(docs, resolved, controller, helpers):
    """Validate independent trial roles after ordinary raw/header admission."""
    integer, events_fn, ordered, targets_fn = helpers
    cached = resolved["clock_profile"].endswith("cache-on")
    for scenario in scenarios():
        trial, memory, wire = [docs[role][scenario] for role in
            ("lifecycle_events", "memory_capture", "wire_capture")]
        fields(trial, {"id", "clock_domain", "clock_epoch", "tx", "rx",
                       "wake_address",
                       "irq_state", "fault", "taint"})
        require(trial["clock_domain"] == "monotonic1",
                "duplex observation domain mismatch")
        integer(trial["clock_epoch"], "duplex collection epoch", 1)
        require(type(trial["wake_address"]) is int and
                trial["wake_address"] == 0,
                "duplex reference workload has no attached external wake")
        rejected = scenario.startswith("reject_")
        tainted = scenario == "domain_taint"
        timeout = scenario == "tx_drain_timeout"
        rx_spans, tx_span, controls = _memory(memory, cached, rejected,
                                             scenario, integer)
        fields(wire, {"id", "tx", "rx", "activity_count", "fresh_tc_ns",
                      "tx_capture_end_ns"})
        tx, rx = trial["tx"], trial["rx"]
        fields(tx, {"request_id", "request_address", "length", "deadline_ns",
            "drain_start_ns", "drain_deadline_ns", "terminal", "transferred",
            "events", "snapshots", "request_tail", "preparation_baseline"})
        require(tx["request_id"] == scenario and
                tx["request_address"] == controls["tx_request"] and
                rx["stream_address"] == controls["stream"],
                "duplex request/stream static correlation mismatch")
        length = integer(tx["length"], "duplex TX length", 1,
                         65504 if cached else 65535)
        require(length == len(_bytes(memory["tx_before"])) and
                length <= tx_span[1] - tx_span[0] and
                (not cached or length % 32 == 0),
                "duplex TX length exceeds actual caller payload")
        require(scenario != "tx_min" or length == (32 if cached else 1),
                "duplex actual minimum TX boundary missing")
        require(scenario != "tx_max" or length == (65504 if cached else 65535),
                "duplex actual maximum TX boundary missing")
        events = {} if scenario == "reject_rx_overlap" else events_fn(tx)
        require(set(events) <= {"request", "borrow", "clean_begin",
            "clean_end", "launch", "memory_tc", "wire_tc", "withdraw",
            "tx_irq_joined", "settled", "release", "cancel", "quarantined",
            "tainted", "rejected"},
            "duplex unknown provider effect or post-settlement access")
        phases = _tx_snapshots(tx, tx_span, rejected, tainted,
                               timeout, integer)
        if not rejected:
            require(phases["launch"]["at_ns"] == events.get("launch") and
                    events.get("borrow", 2 ** 64) <=
                    phases["preparing"]["at_ns"] < phases["launch"]["at_ns"],
                    "duplex actual PREPARING/launch phase is unbound")
        expected_fault = {"tx_drain_timeout": "tx_en_refused",
            "rx_drain_timeout": "rx_en_refused",
            "domain_taint": "domain_changed",
            "reject_tx_overlap": "tx_overlap", "reject_rx_overlap": "rx_overlap"
        }.get(scenario, "none")
        require(trial["fault"] == expected_fault,
                "duplex actual lane/domain fault stimulus missing")
        irqs = trial["irq_state"]
        fields(irqs, IRQS)
        for state in irqs.values():
            fields(state, {"enabled", "pending", "active"})
            require(all(type(value) is bool for value in state.values()) and
                    state["active"] is False,
                    "duplex external IRQ publisher join observation missing")
        if rejected:
            require(tx["transferred"] == 0 and wire["tx"] is None and
                    wire["fresh_tc_ns"] is None,
                    "duplex rejected/unsubmitted TX retained wire effects")
            if scenario == "reject_tx_overlap":
                require(list(events) == ["request", "rejected"] and
                        tx["terminal"] == "REJECTED" and
                        rx["events"][0]["at_ns"] < events["request"],
                        "duplex TX overlap requires already bound RX pool")
                _rx(rx, rx_spans, scenario, None, controller, helpers)
                require(_bytes(wire["rx"]) == _bytes(memory["rx_after"]),
                        "duplex rejected TX corrupted independent active RX")
                _bindings(memory, rx, cached, integer)
            else:
                require(tx["terminal"] is None and tx["events"] == [] and
                        wire["activity_count"] == 0 and wire["rx"] is None and
                        rx["result"] == "REJECTED" and
                        rx["events"] == rx["dma_targets"] == rx["states"] == [],
                        "duplex RX rejection changed wire/stream/TX state")
                admission = rx["admission"]
                fields(admission, {"before", "after"})
                for state in admission.values():
                    fields(state, {"cr", "m0ar", "m1ar", "ndtr", "dmar",
                                   "stream", "producer_loans"})
                    require(all(type(value) is int and value == 0
                                for value in state.values()),
                            "duplex rejected RX retained refs or MMIO effects")
            continue
        _rx(rx, rx_spans, scenario, trial["taint"], controller, helpers)
        _bindings(memory, rx, cached, integer, trial["taint"])
        require(_bytes(wire["rx"]) == _bytes(memory["rx_after"]),
                "duplex independent RX wire differs from DMA/CPU bytes")
        integer(wire["activity_count"], "duplex actual activity", 1)
        captured = _bytes(wire["tx"])
        require(_bytes(memory["tx_before"]).startswith(captured),
                "duplex independently captured TX is not a source prefix")
        integer(wire["tx_capture_end_ns"], "duplex TX capture endpoint")
        minimum = len(captured) * 10 * (
            (64000000 + controller["baud"] // 2) // controller["baud"]
        ) * 1000 // 64
        require(wire["tx_capture_end_ns"] - events["launch"] >=
                minimum - 2000,
                "duplex captured prefix predates actual8N1 transmission")
        ordered(events, ["request", "borrow"])
        if cached:
            ordered(events, ["borrow", "clean_begin", "clean_end", "launch"])
        else:
            require(not {"clean_begin", "clean_end"}.intersection(events),
                    "duplex cache-off inherited cache-on observation")
        if tainted:
            taint = trial["taint"]
            fields(taint, {"at_ns", "before", "after", "access_trace",
                "capture_sequence", "fault_sequence", "isolation_sequence"})
            for key in ("capture_sequence", "fault_sequence",
                        "isolation_sequence"):
                integer(taint[key], "duplex independent " + key, 1)
            require(taint["fault_sequence"] < taint["capture_sequence"] <
                    taint["isolation_sequence"],
                    "duplex fault/observation/isolation order mismatch")
            for state in (taint["before"], taint["after"]):
                fields(state, {"rcc_cr", "hsicfgr", "ccr", "clock_epoch"})
                for key, value in state.items():
                    integer(value, "duplex observed domain " + key,
                            maximum=2 ** 32 - 1)
            before, after = taint["before"], taint["after"]
            trace = taint["access_trace"]
            require(isinstance(trace, list) and 1 <= len(trace) <= 4096,
                    "duplex bounded provider access trace required")
            previous = -1
            for access in trace:
                fields(access, {"at_ns", "operation", "address",
                                "capture_sequence"})
                instant = integer(access["capture_sequence"],
                                  "duplex access capture order", 1)
                if instant < taint["fault_sequence"]:
                    integer(access["at_ns"],
                            "duplex pre-fault access timestamp")
                else:
                    require(access["at_ns"] is None,
                            "duplex invalid Core time after domain fault")
                integer(access["address"], "duplex accessed address",
                        maximum=2 ** 32 - 1)
                require(instant > previous and
                        access["operation"] in {"mmio_read", "mmio_write",
                            "cache", "publish", "domain_lost"} and
                        (instant < taint["capture_sequence"] or
                         (instant == taint["capture_sequence"] and
                          access["operation"] == "domain_lost")),
                        "duplex provider MMIO/cache/publication after taint")
                if access["operation"].startswith("mmio_"):
                    require(any(start <= access["address"] < end
                        for start, end in ((0x40004800, 0x40004c00),
                            (0x40020000, 0x40020400),
                            (0x40020800, 0x40020c00))),
                        "duplex access trace is scoped to invalid I/O domain")
                previous = instant
            tail = tx["request_tail"]
            fields(tail, {"capture_sequence", "state", "active_request", "ref"})
            require(events.get("tainted") == taint["at_ns"] and
                    before["rcc_cr"] & 0x1d == 5 and
                    before["hsicfgr"] & 0x7f000000 == 0x40000000 and
                    before["ccr"] & 0x10000 == (0x10000 if cached else 0) and
                    after["rcc_cr"] & 0x18 != 0 and
                    before["clock_epoch"] == trial["clock_epoch"] and
                    all(sample["at_ns"] < taint["at_ns"]
                        for sample in phases.values()) and
                    wire["tx_capture_end_ns"] <= taint["at_ns"] and
                    memory["rx_observation"]["cpu_sample_ns"] <
                    taint["at_ns"] and
                    type(tail["capture_sequence"]) is int and
                    tail["capture_sequence"] > taint["isolation_sequence"] and
                    tail["state"] == "QUARANTINED" and
                    tail["active_request"] == tx["request_address"] and
                    type(tail["ref"]) is int and tail["ref"] == 1 and
                    all(not state["enabled"] for state in irqs.values()) and
                    not {"settled", "release", "withdraw"}.intersection(events)
                    and tx["terminal"] is None,
                    "duplex joint taint requires three-IRQ isolation and loans")
            continue
        require(trial["taint"] is None, "duplex unselected domain taint")
        require(tx["request_tail"] is None,
                "duplex unselected quarantined request snapshot")
        start = integer(tx["drain_start_ns"], "duplex actual drain start")
        deadline = integer(tx["drain_deadline_ns"], "duplex actual drain end")
        require(deadline == start +
                (20000000 // controller["baud"] + 100) * 1000,
                "duplex finite TX drain budget differs from provider baud")
        ordered(events, ["borrow", "launch", "memory_tc"])
        require(phases["launch"]["at_ns"] == events["launch"] and
                phases["memory_tc"]["at_ns"] == events["memory_tc"] and
                start == events["memory_tc"],
                "duplex actual TX phase and drain endpoints mismatch")
        source = _bytes(memory["tx_before"])
        require(source == captured and
                wire["fresh_tc_ns"] == events.get("wire_tc"),
                "duplex post-DMA-TC wire bytes/fresh TC disagree with source")
        minimum_wire = length * 10 * (
            (64000000 + controller["baud"] // 2) // controller["baud"]
        ) * 1000 // 64
        require(events.get("wire_tc", -1) - events["launch"] >=
                minimum_wire - 2000,
                "duplex observed full finite TX predates actual8N1 wire")
        if timeout:
            require(events.get("quarantined", -1) >= deadline and
                    tx["terminal"] is None and
                    not {"settled", "release"}.intersection(events),
                    "duplex failed TX drain must retain quarantine to expiry")
        else:
            ordered(events, ["withdraw", "tx_irq_joined", "settled", "release"])
            ordered(events, ["launch", "wire_tc", "settled"])
            ordered(events, ["memory_tc", "settled"])
            require(events["settled"] <= deadline,
                    "duplex TX settled beyond actual drain budget")
            integer(tx["deadline_ns"], "duplex request absolute deadline")
            require(events["settled"] <= tx["deadline_ns"] and
                    list(events)[-2:] == ["settled", "release"],
                    "duplex release exceeds request deadline/final access")
            require(phases["settled"]["at_ns"] == events["settled"] and
                    phases["withdrawn"]["at_ns"] == events["withdraw"],
                    "duplex SETTLED/withdraw events lack actual phase facts")
            terminal = ("CANCELLED" if scenario.startswith("cancel_") else
                        "SUCCESS")
            require(tx["terminal"] == terminal and
                    type(tx["transferred"]) is int and
                    tx["transferred"] ==
                    (length if terminal == "SUCCESS" else 0),
                    "duplex terminal status/count differs from provider result")
            if terminal == "SUCCESS":
                require(_bytes(wire["tx"]) == _bytes(memory["tx_before"]),
                        "duplex SUCCESS missed finite wire bytes")
            if scenario == "cancel_tx_tc":
                ordered(events, ["memory_tc", "cancel", "wire_tc"])
        # These exported epochs are collection/stream identities. The finite
        # nx_request_t has no epoch field. Do not assign a fictitious one.
    return {"trials": len(scenarios()), "protocol_errors": 0}
