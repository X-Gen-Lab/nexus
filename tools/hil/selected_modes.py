"""Exact new-mode acquisition contracts within the maintained SoC HIL path.

These are observation obligations, never provider or Board authorities. Plans
remain unbound. Parsed observations acquire physical meaning only after the
ordinary SoC validator verifies station admission and the executed capture.
"""

from pathlib import Path
from types import MappingProxyType

from tools.evidence.common import fields, load_json, verify_file_identity
from tools.evidence.identity import canonical_digest
from tools.hil.admit import require, validate_fixture
from tools.configure.providers import maintained
from tools.configure.providers.common import ConfigurationError
from tools.hil import h743_duplex


SCENARIOS = MappingProxyType({
    h743_duplex.CASE: h743_duplex.scenarios(),
    "H743-SPI-CACHE": ("success_min", "success_max", "cancel_tc",
                       "drain_timeout", "reject_unaligned", "reject_length"),
    "H743-RX-CONTINUOUS": ("success_wrap", "held_backpressure", "idle_stop",
                           "service_deadline", "cancel_held",
                           "reject_overlap", "reject_size"),
    "U575-TX-CHAIN": ("success_min", "block_max", "two_block_min",
                      "two_block_max", "three_block_threshold", "cancel_tc",
                      "drain_timeout", "reject_window", "reject_length"),
    "U575-LPTIM-RUN": ("success_min", "success_max", "early_arrm",
                       "synchronization_delay", "drain_timeout",
                       "reject_one_tick"),
})
WORKLOAD_NAMES = MappingProxyType({name.lower().replace("-", "_"): name
                                 for name in SCENARIOS})
OBSERVATIONS = ("mode_registers", "wire_capture", "memory_capture",
                "lifecycle_events")
HARDWARE = ("debug_probe", "logic_analyzer", "calibrated_timer",
            "reviewed_peer")
SAFETY = ("exact_selected_mode", "observed_silicon_clock",
          "independent_control_channel", "reviewed_memory_and_wiring",
          "retained_loans_until_drain", "isolated_fault_injection")
TITLES = MappingProxyType({
    h743_duplex.CASE: "H743 USART3 finite DMA TX and continuous DMA RX",
    "H743-SPI-CACHE": "H743 SPI4 finite cache-line DMA",
    "H743-RX-CONTINUOUS": "H743 USART3 continuous RX, IRQ TX",
    "U575-TX-CHAIN": "U575 USART1 finite GPDMA hardware TX chain",
    "U575-LPTIM-RUN": "U575 LPTIM2 finite RUN one-shot",
})


def selection(name, resolved, controllers=None):
    """Use exact typed selections and lanes without inferring family support."""
    require(name in SCENARIOS, "unknown selected-mode HIL contract")
    if resolved.get("backend") == "native":
        return "unsupported", ["physical_target_required"]
    h743 = name.startswith("H743")
    family = "stm32h743" if h743 else "stm32u575"
    part = "STM32H743ZIT6" if h743 else "STM32U575ZIT6Q"
    clocks = (("hsi64-fixed-cache-on",) if name == "H743-SPI-CACHE" else
              ("hsi64-fixed-cache-off", "hsi64-fixed-cache-on") if h743 else
              ("msis4",))
    expected = {
        h743_duplex.CASE: (
            "uart", "USART3", "dma-duplex-continuous", {(0, 46), (1, 45)}),
        "H743-SPI-CACHE": ("spi", "SPI4", "dma", {(2, 83), (3, 84)}),
        "H743-RX-CONTINUOUS": (
            "uart", "USART3", "rx-dma-continuous", {(1, 45)}),
        "U575-TX-CHAIN": ("uart", "USART1", "dma-chain-tx", {(0, 25)}),
        "U575-LPTIM-RUN": ("lptim", "LPTIM2", "irq-counter16-oneshot", set()),
    }[name]
    selected = (resolved.get("controllers", ())
                if controllers is None else controllers)
    eligible = [item for item in selected if
                (item.get("kind"), item.get("controller"), item.get("mode")) ==
                expected[:3]]
    okay = (resolved.get("backend") == "baremetal" and
            resolved.get("soc_family") == family and
            resolved.get("part") == part and
            resolved.get("clock_profile") in clocks and len(eligible) == 1)
    if okay:
        item = eligible[0]
        try:
            maintained(family).validate_selection(item, item)
        except (ConfigurationError, KeyError, TypeError):
            return ("requires_selection",
                    ["authoritative_provider_route_required"])
        lanes = resolved.get("dma_topology", {}).get("lanes", ())
        actual = {(lane.get("lane"), lane.get("request")) for lane in lanes
                  if lane.get("owner") == item["id"] and
                  lane.get("engine") == ("DMA1" if h743 else "GPDMA1")}
        okay = (actual == expected[3] and item.get("irq_priority") == 5 and
                item.get("calls_os") is False)
        if name == "U575-LPTIM-RUN":
            okay = (okay and item.get("tick_hz") == 1000000 and
                    item.get("pins") == [])
        if name in ("H743-RX-CONTINUOUS", h743_duplex.CASE):
            okay = okay and item.get("rx_profile") == "blocks"
        if name == "U575-TX-CHAIN":
            topology = resolved.get("dma_topology", {})
            chains = [chain for chain in topology.get("chains", ())
                      if chain.get("owner") == item["id"]]
            okay = okay and len(chains) == 1 and all(
                chains[0].get(key) == value for
                key, value in {"node_capacity": 2, "node_bytes": 12,
                    "alignment": 32, "window_bytes": 65536,
                    "block_bytes": 65535, "maximum_bytes": 196605,
                    "region": "SRAM1", "finite": True}.items())
    return (("selected", []) if okay else
            ("requires_selection", ["exact_" + name.lower().replace("-", "_")]))


def _controller(name, resolved, ids=None):
    controllers = [item for item in resolved["controllers"]
                   if ids is None or item["id"] in ids]
    require(selection(name, resolved, controllers)[0] == "selected",
            "exact selected-mode controller binding required")
    return next(item for item in controllers
                if selection(name, resolved, [item])[0] == "selected")


def workload_definitions(make_definition):
    return {workload: make_definition(7,
        ("spi" if name == "H743-SPI-CACHE" else
         "lptim" if name == "U575-LPTIM-RUN" else "uart",),
        TITLES[name] + ": " + ", ".join(SCENARIOS[name]), OBSERVATIONS,
        HARDWARE, SAFETY, {"trials": "count", "protocol_errors": "count"})
        for workload, name in WORKLOAD_NAMES.items()}


def soc_definitions(make_case):
    return {name: make_case(TITLES[name],
        ("spi" if name == "H743-SPI-CACHE" else
         "lptim" if name == "U575-LPTIM-RUN" else "uart",),
        HARDWARE, "Execute every independent scenario: " + ", ".join(scenarios),
        OBSERVATIONS,
        ("Captured mode/silicon/cache/clock agree with selected configuration",
         "Wire, memory, lifecycle and deadlines agree across independent "
         "captures",
         "Rejected requests have no effects; failed drains retain ownership"),
        {"trials": ("count", "positive"), "protocol_errors": ("count", "zero")},
        SAFETY) for name, scenarios in SCENARIOS.items()}


def fixture_for(resolved):
    """Load an explicit maintained software reference; do not relabel a PCB."""
    filename = {
        "stm32h743zi-hsi-reference": "stm32h743_hsi_selected_reference.json",
        "stm32u575-nucleo-zi-q": "stm32u575_nucleo_zi_q.json",
        "stm32u575zi-lptim-reference":
            "stm32u575_lptim_selected_reference.json",
    }.get(resolved["board"])
    require(filename is not None, "no selected-mode software-reference fixture")
    return load_json(Path(__file__).parent / "fixtures" / filename)


def preparation_base(fixture, resolved):
    """Prepare observations without pretending an absent console is usable."""
    validate_fixture(fixture)
    require(fixture.get("scope") == "software_reference" and
            all(fixture[key] == resolved[key] for key in ("board", "part")) and
            fixture == fixture_for(resolved) and
            any(selection(name, resolved)[0] == "selected"
                for name in SCENARIOS),
            "maintained selected-mode software-reference identity required")
    return {"board": resolved["board"], "part": resolved["part"],
            "backend": resolved["backend"],
            "controllers": [{key: item[key] for key in
                ("id", "kind", "controller", "mode")}
                for item in resolved["controllers"]],
            "station_prerequisites": ["independent_control_channel_unbound",
                "separately reviewed physical fixture and actual PCB/probe/UID",
                "observed silicon/cache/clock/security and reviewed wiring",
                "reviewed instruments, bounded adapter, limits and "
                "resource admission",
                "fresh executed capture; physical status remains not_executed"]}


def has_selected(resolved):
    return any(selection(name, resolved)[0] == "selected" for name in SCENARIOS)


def expected_facts(name, resolved):
    item = _controller(name, resolved)
    h743 = name.startswith("H743")
    return {"controller": item["controller"], "mode": item["mode"],
            "device_id": 0x450 if h743 else 0x482,
            "revision_id": 0x2003 if h743 else None,
            "nominal_core_hz": 64000000 if h743 else 4000000,
            "clock_profile": resolved["clock_profile"],
            "cache_line_bytes": (
                32 if resolved["clock_profile"].endswith("cache-on") else 0),
            "hsi_trim": 0x40 if h743 else None,
            "tzen": None if h743 else 0, "swap_bank": None if h743 else 0,
            "irq_priority": 5, "calls_os": False,
            "dma_lanes": [{key: lane[key]
                           for key in ("engine", "lane", "request")}
                          for lane in resolved["dma_topology"]["lanes"]
                          if lane["owner"] == item["id"]]}


def _integer(value, label, minimum=0, maximum=2 ** 64 - 1):
    require(type(value) is int and minimum <= value <= maximum,
            "bounded integer selected-mode " + label + " required")
    return value


def _document(identity, role, name, controller, protocol, session,
              protocol_identity, context_identity):
    raw = load_json(verify_file_identity(identity), max_bytes=4 * 1024 * 1024)
    body = "facts" if role == "mode_registers" else "trials"
    fields(raw, {"schema_version", "kind", "case_id", "role", "session",
                 "protocol_sha256", "context_sha256", "controller_id",
                 "instrument", body})
    require(type(raw["schema_version"]) is int and
            raw["schema_version"] == 1 and
            raw["kind"] == "selected_mode_observation" and
            raw["role"] == role and
            raw["case_id"] == name and raw["session"] == session and
            raw["protocol_sha256"] == protocol_identity["sha256"] and
            raw["context_sha256"] == context_identity["sha256"] and
            raw["controller_id"] == controller["id"] and
            raw["instrument"] in protocol["instruments"] and
            raw["instrument"] == instrument_for(name, role),
            "selected-mode raw role/session/protocol/controller mismatch")
    if body == "facts":
        return raw[body]
    trials = raw[body]
    require(isinstance(trials, list) and 0 < len(trials) <= 64,
            "bounded nonempty selected-mode trials required")
    require(all(isinstance(trial, dict) and isinstance(trial.get("id"), str)
                for trial in trials), "selected-mode trial identity required")
    indexed = {trial["id"]: trial for trial in trials}
    require(len(indexed) == len(trials) and
            set(indexed) == set(SCENARIOS[name]),
            "all independent selected-mode scenarios required "
            "without duplicates")
    return indexed


def instrument_for(name, role):
    if role == "wire_capture":
        return ("calibrated_timer" if name == "U575-LPTIM-RUN" else
                "logic_analyzer")
    return "debug_probe"


def _events(trial):
    events = trial["events"]
    require(isinstance(events, list) and 0 < len(events) <= 4096,
            "bounded nonempty selected-mode lifecycle events required")
    previous = -1
    result = {}
    for entry in events:
        fields(entry, {"at_ns", "event"})
        instant = _integer(entry["at_ns"], "event timestamp")
        require(instant > previous and isinstance(entry["event"], str) and
                entry["event"] not in result,
                "strictly ordered unique selected-mode events required")
        previous = instant
        result[entry["event"]] = instant
    return result


def _ordered(events, names):
    require(all(name in events for name in names) and
            all(events[left] < events[right]
                for left, right in zip(names, names[1:])),
            "selected-mode missing or reordered lifecycle/wire/drain boundary")


def _buffers(memory, count, minimum, maximum, alignment, region):
    buffers = memory["buffers"]
    require(isinstance(buffers, list) and len(buffers) == count,
            "selected-mode exact bounded caller buffers required")
    spans = []
    for buffer in buffers:
        fields(buffer, {"address", "size", "exclusive"})
        start = _integer(buffer["address"], "buffer address",
                         maximum=2 ** 32 - 1)
        size = _integer(buffer["size"], "buffer size", minimum, maximum)
        require(start % alignment == 0 and size % alignment == 0 and
                buffer["exclusive"] is True and
                region[0] <= start < start + size <= region[1] and
                all(start + size <= left or start >= right
                    for left, right in spans),
                "selected-mode DMA domain/cache-line/exclusive/"
                "nonoverlap violation")
        spans.append((start, start + size))
    require(len({right - left for left, right in spans}) == 1,
            "selected-mode caller block capacities must match")
    return spans


def _stream_targets(trial, spans, divisor, events):
    """Check observed CT/M0AR/M1AR/NDTR transitions and held-block isolation."""
    snapshots = trial["dma_targets"]
    require(isinstance(snapshots, list) and 4 <= len(snapshots) <= 4096,
            "selected-mode continuous RX target rotations required")
    addresses = {left for left, _ in spans}
    capacity = spans[0][1] - spans[0][0]
    previous_time, previous_ct, previous_held = -1, None, set()
    previous_armed, previous_ndtr = None, None
    for snapshot in snapshots:
        fields(snapshot, {"at_ns", "cr", "m0ar", "m1ar", "ndtr",
                          "held", "returned"})
        instant = _integer(snapshot["at_ns"], "DMA target timestamp")
        cr = _integer(snapshot["cr"], "DMA CR", maximum=2 ** 32 - 1)
        ct = (cr >> 19) & 1
        armed = {snapshot["m0ar"], snapshot["m1ar"]}
        held, returned = snapshot["held"], snapshot["returned"]
        require(isinstance(held, list) and isinstance(returned, list) and
                all(type(address) is int for address in held + returned) and
                len(set(held)) == len(held) and
                len(set(returned)) == len(returned),
                "selected-mode observed unique loan/return addresses required")
        require(cr & ((1 << 18) | (1 << 8) | 1) ==
                ((1 << 18) | (1 << 8) | 1) and len(armed) == 2 and
                armed <= addresses and set(held) <= addresses and
                not armed.intersection(held) and
                set(returned) <= previous_held and
                not (previous_held - set(returned)) - set(held),
                "selected-mode target/held loan reused without observed return")
        _integer(snapshot["ndtr"], "DMA NDTR", 0, capacity)
        if previous_armed is not None:
            # A CT flip makes the formerly inactive target active. Software
            # may only replace the just-completed target while it is inactive.
            current_pair = (snapshot["m0ar"], snapshot["m1ar"])
            completed = previous_armed[previous_ct]
            newly_held = set(held) - (previous_held - set(returned))
            require(current_pair[ct] == previous_armed[ct] and
                    newly_held == {completed},
                    "selected-mode active DMA target changed or "
                    "completed loan mismatch")
            characters = previous_ndtr + capacity - snapshot["ndtr"]
            # Wire8N1 is ten bits. The provider's eight-bit replacement guard
            # is deliberately conservative and is a different calculation.
            minimum_period = max(
                0, characters * 10 * divisor * 1000 // 64 - 2000)
        else:
            minimum_period = 0
        require(events["dma_enable"] <= instant < events["stop"] and
                instant > previous_time and (previous_ct is None or
                (ct != previous_ct and
                 instant - previous_time >= minimum_period)),
                "selected-mode target wrap time/CT sequence mismatch")
        previous_time, previous_ct, previous_held = instant, ct, set(held)
        previous_armed = (snapshot["m0ar"], snapshot["m1ar"])
        previous_ndtr = snapshot["ndtr"]


def _chain_words(memory, spans, units, timeout):
    """Decode the selected three-register packed nodes, not layout aliases."""
    nodes = memory["nodes"]
    require(isinstance(nodes, list) and len(nodes) == 2,
            "selected-mode two packed chain nodes required")
    addresses = []
    for node in nodes:
        fields(node, {"address", "size", "block", "source", "link"})
        address = _integer(node["address"], "node address", maximum=2 ** 32 - 1)
        require(type(node["size"]) is int and node["size"] == 12 and
                0x20000000 <= address < address + 12 <= 0x20030000 and
                all(address + 12 <= left or address >= right
                    for left, right in spans),
                "selected-mode descriptor leaves SRAM1 or overlaps payload")
        addresses.append(address)
    require(addresses[0] % 32 == 0 and addresses[1] == addresses[0] + 12 and
            addresses[0] >> 16 == (addresses[1] + 11) >> 16,
            "selected-mode descriptor pool alignment/window mismatch")
    # CMSIS U575 DMA_CLLR_UB1/USA/ULL, LA and CLBAR_LBA; provider
    # uart_dma_chain.c publishes exactly CBR1, CSAR, CLLR in this order.
    updates, address_mask = 0x30010000, 0xfffc
    first = min(units, 65535)
    source, remaining = spans[0][0] + first, units - first
    expected_nodes = []
    for index, node in enumerate(nodes):
        block = min(remaining, 65535)
        remaining -= block
        expected = {"block": block, "source": source if block else 0,
                    "link": ((addresses[1] & address_mask) | updates)
                    if index == 0 and remaining else 0}
        require(all(type(node[key]) is int and node[key] == value
                    for key, value in expected.items()),
                "selected-mode actual packed block/source/link words mismatch")
        expected_nodes.append(expected)
        source += block
    expected_channel = {"CLBAR": addresses[0] & 0xffff0000,
        "CLLR": (addresses[0] & address_mask) | updates if units > 65535 else 0,
        "CSAR": spans[0][0], "CBR1": first}
    channel = memory["channel"]
    require(canonical_digest(channel) == canonical_digest(expected_channel),
            "selected-mode initial channel differs from packed chain selection")
    fetched = memory["fetches"]
    expected_fetches = [node for node in expected_nodes if node["block"]]
    require(canonical_digest(fetched) == canonical_digest(expected_fetches),
            "selected-mode actual hardware fetch snapshots "
            "missing or mismatched")
    withdrawn = memory["withdrawn"]
    fields(withdrawn, {"CLBAR", "CLLR", "CSAR", "nodes"})
    for register in ("CLBAR", "CLLR", "CSAR"):
        _integer(withdrawn[register], "withdrawn " + register,
                 maximum=2 ** 32 - 1)
    if timeout:
        require(canonical_digest(withdrawn["nodes"]) ==
                canonical_digest(expected_nodes) and
                withdrawn["CLBAR"] in (0, expected_channel["CLBAR"]) and
                withdrawn["CLLR"] in (0, expected_channel["CLLR"],
                    expected_nodes[0]["link"], expected_nodes[1]["link"]) and
                (withdrawn["CSAR"] == 0 or
                 spans[0][0] <= withdrawn["CSAR"] <= spans[0][0] + units) and
                any(withdrawn[key] != 0 for key in ("CLBAR", "CLLR", "CSAR")),
                "selected-mode failed link withdrawal must retain "
                "actual typed references and published nodes")
    else:
        require(canonical_digest(withdrawn) == canonical_digest({
            "CLBAR": 0, "CLLR": 0, "CSAR": 0,
            "nodes": [{"block": 0, "source": 0, "link": 0}] * 2}),
            "selected-mode successful drain left descriptor/source references")


def validate_observations(name, observations, resolved, protocol, session,
                          protocol_identity, context_identity):
    """Parse retained exports without qualifying physical hardware."""
    controller = _controller(name, resolved, protocol["controller_ids"])
    fields(observations, set(OBSERVATIONS))
    docs = {role: _document(observations[role], role, name, controller,
            protocol, session, protocol_identity, context_identity)
            for role in OBSERVATIONS}
    facts, expected = docs["mode_registers"], expected_facts(name, resolved)
    if name.startswith("U575"):
        _integer(facts.get("revision_id"), "observed U575 revision",
                 maximum=65535)
        expected["revision_id"] = facts["revision_id"]
    require(canonical_digest(facts) == canonical_digest(expected),
            "observed silicon/revision/clock/cache/security "
            "selected-mode mismatch")
    cached = expected_facts(name, resolved)["cache_line_bytes"] == 32
    if name == h743_duplex.CASE:
        return h743_duplex.validate(docs, resolved, controller,
            (_integer, _events, _ordered, _stream_targets))
    for scenario, trial in docs["lifecycle_events"].items():
        extras = ({"service_sample_ns", "remaining_bytes",
                   "service_deadline_ns",
                   "dma_targets"}
                  if name == "H743-RX-CONTINUOUS" else set())
        fields(trial, {"id", "scenario", "requested_units", "status",
                       "deadline_ns",
                       "clock_domain", "clock_epoch", "events"} | extras)
        require(trial["scenario"] == scenario and
                trial["clock_domain"] == "monotonic1",
                "selected-mode scenario or observed monotonic domain mismatch")
        _integer(trial["clock_epoch"], "live clock epoch", 1)
        _integer(trial["deadline_ns"], "absolute deadline")
        units = _integer(trial["requested_units"], "request units")
        events = _events(trial)
        memory = docs["memory_capture"][scenario]
        wire = docs["wire_capture"][scenario]
        fields(memory, {"id", "buffers", "nodes", "before", "after"} |
               ({"held_before", "held_after", "cpu_visible"}
                if name == "H743-RX-CONTINUOUS" else
                {"channel", "fetches", "withdrawn"}
                if name == "U575-TX-CHAIN" else set()))
        fields(wire, {"id", "tx", "rx", "timer_ticks", "activity_count"})
        rejected = scenario.startswith("reject_")
        require(trial["status"] == ("rejected" if rejected else "accepted"),
                "selected-mode request acceptance mismatch")
        if rejected:
            require(list(events) == ["request", "rejected"],
                    "rejected selected-mode request retained effects "
                    "or references")
            before, after = [verify_file_identity(memory[key])
                             for key in ("before", "after")]
            require(before.read_bytes() == after.read_bytes() and
                    type(wire["activity_count"]) is int and
                    wire["activity_count"] == 0 and wire["tx"] is None and
                    wire["rx"] is None and wire["timer_ticks"] == 0,
                    "rejected selected-mode request changed retained "
                    "memory or wire")
            # Negative stimuli themselves must be retained and demonstrably bad.
            if scenario in ("reject_length", "reject_one_tick"):
                require(units == (1 if scenario == "reject_one_tick" else 0),
                        "selected-mode rejected length stimulus missing")
            elif scenario == "reject_unaligned":
                require(memory["buffers"][0]["address"] % 32 != 0,
                        "selected-mode unaligned negative stimulus missing")
            elif scenario == "reject_overlap":
                require(memory["buffers"][0]["address"] ==
                        memory["buffers"][1]["address"],
                        "selected-mode overlapping negative stimulus missing")
            elif scenario == "reject_size":
                require(memory["buffers"][0]["size"] < 32,
                        "selected-mode undersized negative stimulus missing")
            else:
                require(memory["nodes"][0]["address"] >> 16 !=
                        memory["nodes"][1]["address"] >> 16,
                        "selected-mode crossed descriptor-window "
                        "stimulus missing")
            continue
        timeout = scenario == "drain_timeout"
        common_events = {"request", "borrow", "quarantined", "drained",
                         "settled", "release", "cancel"}
        allowed = {
            "H743-SPI-CACHE": {"clean_tx", "prepare_rx", "dma_enable",
                "tx_dma_tc", "rx_dma_tc", "wire_idle", "invalidate_rx",
                "cs_release"},
            "H743-RX-CONTINUOUS": {"prepare_rx", "dma_enable", "block_ready",
                "loan", "stop", "loss", "dma_disabled", "irq_joined",
                "invalidate_rx", "return_loan"},
            "U575-TX-CHAIN": {"node_publish", "dma_enable", "chain_last",
                "dma_disabled", "irq_joined", "wire_tc"},
            "U575-LPTIM-RUN": {"enable_write", "enable_readback", "enable_dsb",
                "launch", "arrm", "ue", "disable_write", "disable_readback",
                "disable_dsb"},
        }[name] | common_events
        require(set(events) <= allowed,
                "unknown selected-mode effect or post-settlement access")
        _ordered(events, ["request", "borrow"])
        if scenario.startswith("cancel_"):
            _ordered(events, ["borrow", "cancel", "drained"])
        else:
            require("cancel" not in events, "unselected cancel stimulus")
        _integer(wire["activity_count"], "actual wire activity", 1)
        if timeout:
            require("quarantined" in events and not
                    {"drained", "settled", "release"}.intersection(events) and
                    list(events)[-1] == "quarantined" and
                    events["quarantined"] >= trial["deadline_ns"],
                    "failed selected-mode drain must retain "
                    "quarantined ownership")
        else:
            _ordered(events, ["borrow", "drained", "settled", "release"])
            require(events["settled"] <= trial["deadline_ns"] and
                    list(events)[-2:] == ["settled", "release"],
                    "selected-mode settlement exceeds observed "
                    "absolute deadline")
        if name == "H743-SPI-CACHE":
            require(32 <= units <= 65504 and units % 32 == 0,
                    "selected SPI cache-line request bound violated")
            require(scenario != "success_min" or units == 32,
                    "selected SPI minimum32-byte stimulus required")
            require(scenario != "success_max" or units == 65504,
                    "selected SPI maximum65504-byte stimulus required")
            spans = _buffers(memory, 2, 32, 65504, 32, (0x24000000, 0x24080000))
            require(all(units <= right - left for left, right in spans),
                    "selected-mode SPI transfer exceeds actual "
                    "borrowed buffers")
            _ordered(events, ["borrow", "clean_tx", "prepare_rx", "dma_enable"])
            _ordered(events, ["dma_enable", "tx_dma_tc", "wire_idle",
                              "cs_release"])
            _ordered(events, ["dma_enable", "rx_dma_tc", "wire_idle",
                              "invalidate_rx", "cs_release"])
            if not timeout:
                _ordered(events, ["cs_release", "drained", "settled"])
            if scenario == "cancel_tc":
                _ordered(events, ["tx_dma_tc", "cancel", "wire_idle"])
        elif name == "H743-RX-CONTINUOUS":
            spans = _buffers(memory, 3, 32, 65504 if cached else 65535,
                             32 if cached else 1, (0x24000000, 0x24080000))
            capacity = spans[0][1] - spans[0][0]
            require(units == capacity,
                    "selected-mode RX units differ from actual caller "
                    "block capacity")
            _ordered(events, ["borrow", "dma_enable", "block_ready", "loan",
                              "stop", "dma_disabled", "irq_joined",
                              "return_loan", "drained"])
            if scenario == "cancel_held":
                _ordered(events, ["loan", "cancel", "stop"])
            if cached:
                _ordered(events, ["borrow", "prepare_rx", "dma_enable"])
                _ordered(events, ["irq_joined", "invalidate_rx", "return_loan"])
            else:
                require(not {"prepare_rx", "invalidate_rx"}.intersection(
                    events), "cache-off mode cannot inherit cache-on events")
            remaining = _integer(trial["remaining_bytes"], "sampled NDTR",
                                 0, capacity)
            divisor = (64000000 + controller["baud"] // 2) // controller["baud"]
            sample = _integer(trial["service_sample_ns"], "service sample")
            frames = max(0, remaining + capacity - 20)
            budget = max(0, frames * 8 * divisor // 64 - 2) * 1000
            require(trial["service_deadline_ns"] == sample + budget,
                    "selected-mode service deadline differs from correlated "
                    "HSI/BRR/NDTR guard")
            require(scenario not in ("held_backpressure", "service_deadline") or
                    "loss" in events, "selected-mode stream backpressure/"
                    "deadline loss must remain observable")
            _stream_targets(trial, spans, divisor, events)
            last_target = trial["dma_targets"][-1]
            require(sample == last_target["at_ns"] and
                    remaining == last_target["ndtr"] and sample % 1000 == 0,
                    "selected-mode service sample/NDTR differs from actual "
                    "DMA snapshot")
            require((events["stop"] >= trial["service_deadline_ns"])
                    if scenario == "service_deadline" else
                    (events["stop"] < trial["service_deadline_ns"]),
                    "selected-mode actual service interval/deadline "
                    "stimulus mismatch")
            held_before, held_after = [verify_file_identity(memory[key])
                                       for key in ("held_before", "held_after")]
            require(held_before.read_bytes() == held_after.read_bytes(),
                    "selected-mode held consumer loan was overwritten")
        elif name == "U575-TX-CHAIN":
            require(1 <= units <= 196605,
                    "selected-mode finite chain length violated")
            boundaries = {"success_min": 1, "block_max": 65535,
                "two_block_min": 65536, "two_block_max": 131070,
                "three_block_threshold": 131071}
            require(scenario not in boundaries or units == boundaries[scenario],
                    "selected-mode exact chain block boundary "
                    "stimulus required")
            spans = _buffers(memory, 1, 1, 196605, 1, (0x20000000, 0x20030000))
            require(units <= spans[0][1] - spans[0][0],
                    "selected-mode chain exceeds actual borrowed "
                    "payload capacity")
            _chain_words(memory, spans, units, timeout)
            _ordered(events, ["borrow", "node_publish", "dma_enable",
                              "chain_last",
                              "dma_disabled", "irq_joined", "wire_tc"])
            if not timeout:
                _ordered(events, ["wire_tc", "drained", "settled"])
            if scenario == "cancel_tc":
                _ordered(events, ["chain_last", "cancel", "wire_tc"])
        else:
            require(2 <= units <= 65536 and wire["timer_ticks"] == units,
                    "selected-mode LPTIM RUN requires2..65536 observed ticks")
            require(scenario != "success_min" or units == 2,
                    "selected-mode timer minimum2-tick stimulus required")
            require(scenario != "success_max" or units == 65536,
                    "selected-mode timer maximum65536-tick stimulus required")
            _ordered(events, ["borrow", "enable_write", "enable_readback",
                              "enable_dsb",
                              "launch", "arrm", "ue", "disable_write",
                              "disable_readback", "disable_dsb"])
            require(events["launch"] - events["enable_dsb"] >= 750 and
                    events["disable_dsb"] - events["disable_write"] >= 750 and
                    events["ue"] - events["launch"] >= units * 1000,
                    "selected-mode timer synchronization or full-period UE "
                    "boundary violated")
            if not timeout:
                _ordered(events, ["disable_dsb", "drained", "settled"])
            if scenario == "synchronization_delay":
                require(events["enable_readback"] -
                        events["enable_write"] >= 2000,
                        "selected-mode delayed synchronization "
                        "stimulus missing")
        if name != "U575-LPTIM-RUN":
            before, after, tx, rx = [verify_file_identity(identity)
                                    for identity in
                (memory["before"], memory["after"], wire["tx"], wire["rx"])]
            require(before.stat().st_size == after.stat().st_size == units,
                    "selected-mode retained payload length mismatch")
            require((tx.read_bytes() == after.read_bytes() ==
                     rx.read_bytes() == verify_file_identity(
                         memory["cpu_visible"]).read_bytes())
                    if name == "H743-RX-CONTINUOUS" else
                    (tx.read_bytes() == before.read_bytes() and
                     (name == "U575-TX-CHAIN" or
                      rx.read_bytes() == after.read_bytes())),
                    "selected-mode independent wire/memory payload mismatch")
    return {"trials": len(docs["lifecycle_events"]), "protocol_errors": 0}
