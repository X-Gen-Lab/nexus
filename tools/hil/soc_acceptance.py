#!/usr/bin/env python3
"""Prepare, capture and evaluate one exact SoC physical acceptance case."""

from __future__ import annotations

import argparse
import hashlib
import math
from pathlib import Path
import re
import sys
from types import MappingProxyType

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (
    EvidenceError, atomic_json, fields, file_identity, load_json, regular_file,
    verify_file_identity,
)
from tools.evidence.identity import canonical_digest, resolved_identity
from tools.hil.acquire import (
    OPERATIONS, _observation, acquisition_plan, execute_acquisition,
)
from tools.hil.admit import admit, require
from tools.hil.measure import _freshness
from tools.hil.prepare import preparation_plan as fixture_plan
from tools.hil import selected_modes


def _case(title, kinds, hardware, stimulus, observations, assertions, metrics,
          safety=("reviewed_wiring", "isolated_outputs", "controlled_recovery")):
    return MappingProxyType({
        "title": title, "required_kinds": tuple(kinds),
        "hardware": tuple(hardware), "stimulus": stimulus,
        "observations": tuple(observations), "assertions": tuple(assertions),
        "metrics": MappingProxyType(metrics), "safety": tuple(safety),
    })


# Obligations are not capability, route, Board or numeric-budget authorities.
CASES = MappingProxyType({
    "SOC-37": _case("Station binding", (),
        ("debug_probe", "serial_capture", "supply_meter"),
        "Identify probe, chip UID, exact part and PCB; verify firmware readback",
        ("probe_identity", "chip_identity", "pcb_revision", "firmware_readback"),
        ("Identities match the unchanged admission", "At least one identification"),
        {"identity_mismatches": ("count", "zero"), "identifications": ("count", "positive")}),
    "SOC-38": _case("PCB and electrical facts", (),
        ("continuity_meter", "supply_meter", "pcb_record"),
        "Inspect actual PCB revision, crystal, bridges, fitted devices and wiring",
        ("pcb_inspection", "oscillator", "pin_continuity", "power_ground"),
        ("No wiring mismatch", "Unknown facts remain outside recorded scope"),
        {"wiring_mismatches": ("count", "zero"), "inspections": ("count", "positive")}),
    "SOC-39": _case("Fixed clock profile", (),
        ("calibrated_counter", "oscilloscope", "supply_meter"),
        "Observe reviewed SYSCLK/APB/timer outputs and voltage/Flash conditions",
        ("clock_outputs", "voltage_flash_latency", "load_conditions"),
        ("Every observed frequency meets the reviewed error budget",),
        {"clock_error": ("ppm", "budget"), "clock_observations": ("count", "positive")}),
    "SOC-40": _case("Startup rollback faults", (),
        ("debug_probe", "clock_fault_fixture", "logic_analyzer"),
        "Inject bounded crystal/PLL/switch faults and observe primary/cleanup/residue",
        ("fault_injection", "clock_source_readback", "failure_residue", "retry"),
        ("No live clock source disabled", "Failures preserve truthful cleanup responsibility"),
        {"fault_cases": ("count", "positive"), "residue_mismatches": ("count", "zero")}),
    "SOC-41": _case("Monotonic time", (),
        ("calibrated_timer", "debug_probe", "logic_analyzer"),
        "Exercise wrap, pending overflow, masks and load; measure read IRQ blackout",
        ("time_sequence", "overflow_pending", "incoming_masks", "read_blackout"),
        ("No reversal within one live domain", "Blackout meets reviewed budget"),
        {"wrap_cases": ("count", "positive"), "time_reversals": ("count", "zero"),
         "read_blackout": ("us", "budget")}),
    "SOC-42": _case("RAM and DMA domains", (),
        ("debug_probe", "memory_capture"),
        "Check selected placement/init/reset domains and DMA reachability rejection",
        ("linker_regions", "initialization", "dma_access", "reset_retention"),
        ("Selected initialization matches bytes", "CPU-only domains remain DMA-inaccessible"),
        {"region_checks": ("count", "positive"), "memory_mismatches": ("count", "zero"),
         "dma_rejection_failures": ("count", "zero")}),
    "SOC-43": _case("UART", ("uart",),
        ("serial_peer", "logic_analyzer", "debug_probe"),
        "Exercise only selected UART mode, TC, loss, cancel and wired flow/direction",
        ("wire_sequence", "tc_settlement", "loss_backpressure", "cancel_drain", "mode_scope"),
        ("Wire TC precedes settlement", "Loans survive until withdrawal"),
        {"received_bytes": ("count", "positive"), "ownership_failures": ("count", "zero"),
         "wire_completion_latency": ("us", "budget")}),
    "SOC-44": _case("SPI", ("spi",),
        ("reviewed_slave", "logic_analyzer", "debug_probe"),
        "Capture selected mode/SCK/CS interval, dummy/discard and DMA fault drain",
        ("spi_waveform", "endpoint_isolation", "memory_wire_drain", "mode_scope"),
        ("Complete CS interval", "No endpoint or loan isolation failure"),
        {"transfers": ("count", "positive"), "isolation_failures": ("count", "zero"),
         "sck_error": ("ppm", "budget")}),
    "SOC-45": _case("I2C", ("i2c",),
        ("reviewed_slave", "logic_analyzer", "oscilloscope"),
        "Exercise ACK tails, repeated START, NACK/arbitration/STOP and selected bus-clear",
        ("scl_sda", "ack_tail", "fault_stop", "bus_clear_scope"),
        ("Messages and failure outcomes agree with wire", "Reviewed rise-time limit"),
        {"messages": ("count", "positive"), "protocol_errors": ("count", "zero"),
         "rise_time": ("ns", "budget")}),
    "SOC-46": _case("ADC", ("adc",),
        ("reference_source", "signal_generator", "debug_probe"),
        "Apply calibrated samples and selected scan/trigger/block replacement load",
        ("reference_samples", "trigger_scan", "block_loans", "backpressure", "mode_scope"),
        ("Complete scans", "Borrowed blocks never overwritten"),
        {"samples": ("count", "positive"), "adc_error": ("counts", "budget"),
         "ownership_failures": ("count", "zero")}),
    "SOC-47": _case("EXTI and capture", ("exti|timer_capture",),
        ("signal_generator", "logic_analyzer", "debug_probe"),
        "Apply selected edge/capture load, shared-vector and overflow cases",
        ("edge_sequence", "pending_coalescing", "capture_overflow", "shared_vector_scope"),
        ("Loss/coalescing remains observable", "Shared owner remains isolated"),
        {"edges": ("count", "positive"), "isolation_failures": ("count", "zero"),
         "timestamp_error": ("us", "budget")}),
    "SOC-48": _case("PWM and break", ("pwm|advanced_pwm",),
        ("oscilloscope", "logic_analyzer", "reviewed_break_source"),
        "Measure selected period/duty/inactive levels and selected deadtime/break",
        ("output_waveform", "inactive_levels", "break_scope", "controlled_rearm"),
        ("No unsafe observed output", "Waveform meets reviewed limit"),
        {"output_cases": ("count", "positive"), "unsafe_outputs": ("count", "zero"),
         "waveform_error": ("ns", "budget")}),
    "SOC-49": _case("Classic CAN", ("can",),
        ("can_peer", "can_capture", "oscilloscope"),
        "Exercise selected Classic CAN timing/filter/mailbox/abort/bus-off cases",
        ("can_frames", "filter_domain", "mailbox_abort", "bus_off_recovery", "mode_scope"),
        ("No cross-controller or loan isolation failure",),
        {"frames": ("count", "positive"), "isolation_failures": ("count", "zero"),
         "bit_timing_error": ("ppm", "budget")}),
    "SOC-50": _case("USB FS device", ("usb_device",),
        ("usb_host", "usb_capture", "calibrated_counter"),
        "Observe actual USB48, FS enumeration, SETUP epochs, reset and detach",
        ("usb_clock", "enumeration", "setup_epochs", "endpoint_loans", "detach"),
        ("No stale control or endpoint access", "Only selected FS PIO scope"),
        {"packets": ("count", "positive"), "epoch_failures": ("count", "zero"),
         "usb_clock_error": ("ppm", "budget")}),
    "SOC-51": _case("Raw Ethernet and PHY", ("eth",),
        ("ethernet_peer", "packet_capture", "oscilloscope"),
        "Observe RMII clock/PHY/link, descriptor ownership, RX loan and disconnect",
        ("phy_straps_reset", "rmii_clock", "raw_frames", "descriptor_loans", "disconnect"),
        ("Descriptor/loan ownership preserved", "No protocol-stack qualification inferred"),
        {"frames": ("count", "positive"), "ownership_failures": ("count", "zero"),
         "rmii_clock_error": ("ppm", "budget")}),
    "SOC-52": _case("Flash and power loss", ("flash",),
        ("power_interrupt", "oscilloscope", "debug_probe"),
        "Use only externally assigned erase-aligned region for erase/program/power-cut",
        ("region_geometry", "erase_program_phase", "power_waveform", "readback_recovery"),
        ("Protected regions unchanged", "Product-owned recovery outcome explicit"),
        {"power_cut_cases": ("count", "positive"), "protected_region_changes": ("count", "zero"),
         "recovery_failures": ("count", "zero")},
        ("external_erase_region", "protected_firmware", "isolated_supply", "product_recovery")),
    "SOC-53": _case("Watchdog", ("watchdog",),
        ("calibrated_timer", "debug_probe", "logic_analyzer"),
        "Observe actual RC timeout/feed/reset cause/debug freeze and irreversible enable",
        ("timeout_feed", "reset_cause", "debug_freeze", "irreversible_enable"),
        ("Measured timeout meets reviewed bounds", "No reversible-enable claim"),
        {"resets": ("count", "positive"), "reset_cause_mismatches": ("count", "zero"),
         "watchdog_timeout": ("ms", "budget")}),
    "SOC-54": _case("Shallow sleep and tickless", (),
        ("calibrated_timer", "logic_analyzer", "current_meter"),
        "Exercise explicit FreeRTOS tickless pending wake/phase/overflow/compare faults",
        ("sleep_phase", "latched_wake", "overflow_compare", "restored_ticks", "current"),
        ("Continuous fixed clock only", "No lost wake or falsely restored phase"),
        {"wake_cases": ("count", "positive"), "wake_loss": ("count", "zero"),
         "wake_lateness": ("us", "budget")}),
    "SOC-55": _case("Load and endurance", ("uart",),
        ("serial_capture", "logic_analyzer", "debug_probe", "stack_capture"),
        "Run selected concurrent DMA/IRQ/service load, M4F context and lifecycle recovery",
        ("load_inventory", "irq_service", "stack_peaks", "context_scope", "endurance_recovery"),
        ("No stale access or context mismatch", "Positive actual work interval"),
        {"duration": ("s", "positive"), "stale_accesses": ("count", "zero"),
         "context_mismatches": ("count", "zero"), "irq_latency": ("us", "budget"),
         "service_interval": ("us", "budget"), "stack_used_peak": ("bytes", "budget")}),
    **selected_modes.soc_definitions(_case),
})

MAX_VALUE = 2 ** 64 - 1


def _selection(case_id, definition, resolved, controllers=None):
    if case_id in selected_modes.SCENARIOS:
        return selected_modes.selection(case_id, resolved, controllers)
    selected = resolved["controllers"] if controllers is None else controllers
    kinds = {item["kind"] for item in selected}
    gaps = [group for group in definition["required_kinds"]
            if not kinds.intersection(group.split("|"))]
    if resolved["backend"] == "native":
        return "unsupported", ["physical_target_required"]
    if case_id == "SOC-54" and not (
            resolved["backend"] == "freertos" and
            resolved.get("kernel_profile", {}).get("tickless") is True):
        return "unsupported", ["explicit_freertos_tickless_required"]
    return ("requires_selection" if gaps else "selected"), gaps


def preparation_plan(fixture, resolved):
    """Make generic and exact-mode obligations explicit without equipment."""
    base = (selected_modes.preparation_base(fixture, resolved)
            if selected_modes.has_selected(resolved) else
            fixture_plan(fixture, resolved))
    cases = []
    for case_id, definition in CASES.items():
        availability, gaps = _selection(case_id, definition, resolved)
        cases.append({"id": case_id, "title": definition["title"],
            "status": "not_executed", "availability": availability,
            "selection_gaps": gaps, "required_hardware": list(definition["hardware"]),
            "stimulus": definition["stimulus"],
            "raw_observations": list(definition["observations"]),
            "assertions": list(definition["assertions"]),
            "safety": list(definition["safety"]),
            "limits": {name: None for name in definition["metrics"]},
            "metric_units": {name: unit for name, (unit, _) in definition["metrics"].items()},
            "exit_condition": "Fresh admitted station capture and passing case-specific raw traces"})
    return {"schema_version": 1, "kind": "soc_hil_plan",
            "status": "prepared_unbound", "physical_status": "not_executed",
            "board": base["board"], "part": base["part"],
            "backend": base["backend"], "controllers": base["controllers"],
            "station_prerequisites": base["station_prerequisites"], "cases": cases,
            "limits": "Only selected modes and recorded conditions may be qualified"}



def case_template(case_id, fixture, resolved):
    """Supply editable obligations with no invented instruments or limits."""
    plan = preparation_plan(fixture, resolved)
    require(case_id in CASES, "unknown SoC HIL template case")
    definition = CASES[case_id]
    controller_ids = [item["id"] for item in resolved["controllers"]
                      if any(item["kind"] in group.split("|")
                             for group in definition["required_kinds"])]
    protocol = {"schema_version": 1, "kind": "soc_hil_protocol",
        "case_id": case_id, "board": plan["board"], "part": plan["part"],
        "backend": plan["backend"], "controller_ids": controller_ids,
        "stimulus": definition["stimulus"],
        "observations": {name: "Review actual acquisition of " + name
                         for name in definition["observations"]},
        "instruments": {name: None for name in definition["hardware"]},
        "safety": {name: "Review actual station obligation: " + name
                   for name in definition["safety"]},
        "limits": {name: {"unit": unit, "minimum": None, "maximum": None}
                   for name, (unit, _) in definition["metrics"].items()}}
    if case_id == "SOC-52":
        protocol["flash_region"] = {"layout": None, "start": None, "size": None,
                                     "recovery_policy": None}
    return {"schema_version": 1, "kind": "soc_hil_protocol_template",
            "status": "prepared_unbound", "physical_status": "not_executed",
            "protocol": protocol,
            "limits": "Copy protocol and replace every unreviewed value using actual station facts"}

def _text(value):
    require(isinstance(value, str) and 0 < len(value.strip()) <= 4096,
            "nonempty bounded SoC acquisition description required")


def _descriptions(value, names):
    fields(value, set(names))
    for entry in value.values():
        _text(entry)


def validate_protocol(protocol, fixture, resolved):
    """Review strict station-authored obligations; no numeric defaults exist."""
    fields(protocol, {"schema_version", "kind", "case_id", "board", "part",
        "backend", "controller_ids", "stimulus", "observations", "instruments",
        "safety", "limits"}, {"flash_region"})
    require(type(protocol["schema_version"]) is int and
            protocol["schema_version"] == 1 and protocol["kind"] == "soc_hil_protocol",
            "reviewed schema-1 SoC protocol required")
    case_id = protocol["case_id"]
    require(isinstance(case_id, str) and case_id in CASES, "unknown SoC HIL case")
    definition = CASES[case_id]
    if protocol["case_id"] in selected_modes.SCENARIOS:
        selected_modes.preparation_base(fixture, resolved)
    else:
        fixture_plan(fixture, resolved)
    require(all(protocol[key] == resolved[key] for key in ("board", "part", "backend")),
            "SoC protocol fixture/build identity mismatch")
    ids = protocol["controller_ids"]
    require(isinstance(ids, list) and len(ids) <= 128 and
            all(isinstance(item, str) for item in ids) and len(ids) == len(set(ids)),
            "unique bounded selected controller IDs required")
    controllers = {item["id"]: item for item in resolved["controllers"]}
    require(set(ids) <= controllers.keys(), "unselected SoC controller binding")
    availability, _ = _selection(case_id, definition, resolved,
                                 [controllers[name] for name in ids])
    require(availability == "selected", "SoC protocol requires selected supported mode")
    _text(protocol["stimulus"])
    _descriptions(protocol["observations"], definition["observations"])
    _descriptions(protocol["safety"], definition["safety"])
    fields(protocol["instruments"], set(definition["hardware"]))
    for identity in protocol["instruments"].values():
        verify_file_identity(identity)
    fields(protocol["limits"], set(definition["metrics"]))
    for name, (unit, rule) in definition["metrics"].items():
        bound = protocol["limits"][name]
        fields(bound, {"minimum", "maximum", "unit"})
        require(bound["unit"] == unit, "SoC metric unit mismatch")
        for key in ("minimum", "maximum"):
            number = bound[key]
            require(type(number) in (int, float) and math.isfinite(number) and
                    0 <= number <= MAX_VALUE, "reviewed finite bounded SoC limit required")
            if unit in ("count", "counts", "bytes"):
                require(type(number) is int, "discrete SoC bounds must be integers")
        require(bound["minimum"] <= bound["maximum"], "reversed SoC metric bounds")
        require(rule != "zero" or bound["minimum"] == bound["maximum"] == 0,
                "SoC invariant failures must remain zero")
        require(rule != "positive" or bound["minimum"] > 0,
                "positive actual SoC work lower bound required")
    require(("flash_region" in protocol) == (case_id == "SOC-52"),
            "Flash tests require an external region; unrelated cases reject it")
    if case_id == "SOC-52":
        region = protocol["flash_region"]
        fields(region, {"layout", "start", "size", "recovery_policy"})
        layout = load_json(verify_file_identity(region["layout"]))
        fields(layout, {"schema_version", "kind", "board", "part", "test_region", "protected_regions"})
        require(type(layout["schema_version"]) is int and layout["schema_version"] == 1 and
                layout["kind"] == "product_flash_test_layout" and
                all(layout[key] == protocol[key] for key in ("board", "part")),
                "external product Flash layout identity mismatch")
        _text(region["recovery_policy"])
        fields(layout["test_region"], {"start", "size"})
        require(layout["test_region"] == {key: region[key] for key in ("start", "size")},
                "SoC Flash region differs from external product layout")
        flash = resolved.get("flash", {})
        start, size = region["start"], region["size"]
        require(type(start) is int and type(size) is int and size > 0 and
                flash.get("origin", 0) <= start < start + size <=
                flash.get("origin", 0) + flash.get("size", 0),
                "SoC Flash region leaves exact physical Flash")
        boundary = flash["origin"]
        boundaries = {boundary}
        for erase_bytes in flash["erase_blocks"]:
            boundary += erase_bytes
            boundaries.add(boundary)
        require(start in boundaries and start + size in boundaries,
                "SoC Flash region must use exact erase boundaries")
        protected = layout["protected_regions"]
        require(isinstance(protected, list) and 0 < len(protected) <= 128,
                "external product protected Flash regions required")
        names = set()
        for item in protected:
            fields(item, {"id", "start", "size"})
            require(isinstance(item["id"], str) and item["id"] not in names,
                    "unique product Flash region identity required")
            names.add(item["id"])
            require(type(item["start"]) is int and type(item["size"]) is int and
                    item["size"] > 0 and flash["origin"] <= item["start"] <
                    item["start"] + item["size"] <= flash["origin"] + flash["size"] and
                    not (start < item["start"] + item["size"] and item["start"] < start + size),
                    "Flash test overlaps protected region or leaves physical Flash")
    return protocol


def _admission(admission):
    require(admission.get("kind") == "hil_admission" and
            admission.get("status") == "ready", "ready physical admission required")
    paths = [verify_file_identity(admission[name]) for name in
             ("station", "fixture", "elf", "resolved", "resources")]
    rebuilt = admit(*paths)
    require(all(admission.get(key) == value for key, value in rebuilt.items()),
            "SoC admission is stale or inconsistent")
    station = load_json(paths[0])
    require(isinstance(station.get("pcb_revision"), str) and
            station["pcb_revision"].lower() not in ("unknown", "unreviewed", "tbd"),
            "observed PCB revision required for physical SoC acceptance")
    resolved, _ = resolved_identity(paths[3])
    return load_json(paths[1]), resolved



def _flash_elf(protocol, admission):
    if protocol["case_id"] != "SOC-52":
        return
    from tools.measurement.elf import Elf32
    region = protocol["flash_region"]
    layout = load_json(verify_file_identity(region["layout"]))
    elf = Elf32(verify_file_identity(admission["elf"]).read_bytes())
    for load in elf.loads:
        if not load["files"]:
            continue
        start, end = load["physical"], load["physical"] + load["files"]
        require(not (region["start"] < end and start < region["start"] + region["size"]),
                "Flash test region overlaps the actual admitted firmware")
        require(any(item["start"] <= start < end <= item["start"] + item["size"]
                    for item in layout["protected_regions"]),
                "external layout must protect all admitted firmware LOAD bytes")

def capture(protocol_path, recipe_path, admission_path, output):
    """Use the maintained exclusive, bounded acquisition runner; never qualify."""
    admission = load_json(admission_path)
    fixture, resolved = _admission(admission)
    protocol = validate_protocol(load_json(protocol_path), fixture, resolved)
    _flash_elf(protocol, admission)
    plan = acquisition_plan(recipe_path, admission_path)
    serial = next(step for step in plan["operations"] if step["name"] == "serial")
    identity = file_identity(protocol_path)
    require(identity in serial["inputs"] and str(Path(protocol_path).absolute())
            in serial["arguments"], "serial adapter must bind and receive exact SoC protocol")
    for instrument in protocol["instruments"].values():
        require(instrument in serial["inputs"], "serial adapter must bind instrument closure")
    result = execute_acquisition(recipe_path, admission_path, output)
    result.update(soc_case_id=protocol["case_id"], soc_protocol=identity)
    atomic_json(Path(output) / "soc-capture.json", result)
    return result


def _capture_record(identity, admission, protocol_identity, protocol, now):
    capture = load_json(verify_file_identity(identity))
    require(type(capture.get("schema_version")) is int and capture["schema_version"] == 1 and
            capture.get("kind") == "hil_acquisition_capture" and
            capture.get("status") == "captured" and
            capture.get("physical_status") == "executed_unqualified" and
            capture.get("admission_sha256") == canonical_digest(admission),
            "actual successful bounded SoC acquisition capture required")
    recipe_path = verify_file_identity(capture["recipe"])
    admission_path = verify_file_identity(capture["admission"])
    plan = acquisition_plan(recipe_path, admission_path)
    require(plan["admission_sha256"] == canonical_digest(admission),
            "SoC capture recipe/admission identity mismatch")
    serial = next(step for step in plan["operations"] if step["name"] == "serial")
    require(protocol_identity in serial["inputs"] and
            protocol_identity["path"] in serial["arguments"],
            "SoC protocol missing from captured adapter closure")
    for instrument in protocol["instruments"].values():
        require(instrument in serial["inputs"], "captured adapter instrument closure incomplete")
    require(capture.get("soc_case_id") == protocol["case_id"] and
            capture.get("station_quarantined") is False and "failure" not in capture and "cleanup_failure" not in capture,
            "SoC capture case failed or station quarantined")
    require(capture.get("soc_protocol") == protocol_identity,
            "capture does not bind the exact reviewed SoC protocol")
    session = capture.get("session")
    require(isinstance(session, str) and re.fullmatch(r"[0-9a-f]{32}", session),
            "fresh SoC capture session required")
    operations = capture.get("operations")
    require(isinstance(operations, list) and
            [step.get("name") for step in operations] == list(OPERATIONS),
            "complete SoC acquisition operation sequence required")
    from datetime import datetime, timezone
    _freshness(capture, 86400, now)
    output = Path(identity["path"]).parent
    payload = None
    previous_finish = datetime.fromisoformat(capture["started_utc"].replace("Z", "+00:00"))
    for step, reviewed in zip(operations, plan["operations"]):
        record_path = output / (step["name"] + ".observation.json")
        substitutions = {"{record}": str(record_path), "{session}": session,
                         "{admission}": str(admission_path),
                         "{elf}": str(verify_file_identity(admission["elf"])),
                         "{output}": str(output)}
        argv = [str(verify_file_identity(reviewed["tool"]))]
        argv.extend(substitutions.get(arg, arg) for arg in reviewed["arguments"])
        require(step.get("argv") == argv, "SoC executed argv differs from reviewed recipe")
        _freshness(step, 86400, now)
        def instant(value):
            return datetime.fromisoformat(value.replace("Z", "+00:00"))
        require(instant(capture["started_utc"]) <= instant(step["started_utc"]) <=
                instant(step["finished_utc"]) <= instant(capture["finished_utc"]),
                "SoC operation time leaves capture interval")
        require(previous_finish <= instant(step["started_utc"]),
                "SoC acquisition operations overlap or reorder")
        previous_finish = instant(step["finished_utc"])
        require(type(step.get("elapsed_s")) in (int, float) and
                math.isfinite(step["elapsed_s"]) and
                0 <= step["elapsed_s"] <= reviewed["timeout_s"] + 5,
                "SoC operation exceeds reviewed timeout and cleanup allowance")
        require(step.get("status") == "captured" and
                type(step.get("exit_code")) is int and step["exit_code"] == 0 and
                step.get("process_started") is True and "failure" not in step and
                not any(step.get(key) for key in
                        ("timed_out", "output_exceeded", "residual_processes", "cancelled")),
                "failed/skipped/unstarted SoC acquisition operation")
        captured_bytes = 0
        for stream in ("stdout", "stderr"):
            stream_identity = step[stream]
            fields(stream_identity, {"path", "sha256", "size"})
            path = regular_file(stream_identity["path"], nonempty=False)
            require(type(stream_identity["size"]) is int and
                    0 <= stream_identity["size"] <= reviewed["maximum_output_bytes"] and
                    path.stat().st_size == stream_identity["size"],
                    "SoC raw stream exceeds reviewed output bound")
            captured_bytes += stream_identity["size"]
            data = path.read_bytes()
            require(type(stream_identity["size"]) is int and
                    len(data) == stream_identity["size"] and
                    hashlib.sha256(data).hexdigest() == stream_identity["sha256"],
                    "SoC acquisition raw stream changed")
        require(captured_bytes <= reviewed["maximum_output_bytes"],
                "SoC combined raw streams exceed reviewed output bound")
        observed_path = verify_file_identity(step["observation"])
        require(observed_path == record_path, "SoC observation leaves its capture operation")
        require(_observation(observed_path, reviewed, session, admission) == step["observation"],
                "SoC observation identity changed")
        observed = load_json(observed_path)
        if step["name"] == "serial":
            payload = observed["observations"].get("soc")
            fields(payload, {"schema_version", "kind", "case_id", "protocol_sha256",
                             "context", "observations", "traces"})
            require(type(payload["schema_version"]) is int and payload["schema_version"] == 1 and
                    payload["kind"] == "soc_hil_observation" and
                    payload["case_id"] == protocol["case_id"] and
                    payload["protocol_sha256"] == protocol_identity["sha256"],
                    "serial acquisition did not capture the reviewed SoC payload")
    serial_step = next(step for step in operations if step["name"] == "serial")
    return session, payload, serial_step["elapsed_s"], capture


def validate_record(raw, admission, *, now=None):
    """Derive per-case results from retained samples, never declared pass flags."""
    fields(raw, {"schema_version", "kind", "case_id", "started_utc", "finished_utc",
        "station_id", "board", "part", "chip_uid", "admission_sha256",
        "elf_sha256", "resolved_sha256", "protocol", "capture", "context", "observations", "traces"})
    require(type(raw["schema_version"]) is int and raw["schema_version"] == 1 and
            raw["kind"] == "physical_soc_hil", "physical schema-1 SoC record required")
    from datetime import datetime, timezone
    now = now or datetime.now(timezone.utc)
    _freshness(raw, 86400, now)
    fixture, resolved = _admission(admission)
    for key in ("station_id", "board", "part", "chip_uid"):
        require(raw[key] == admission[key], "SoC physical identity mismatch: " + key)
    require(raw["admission_sha256"] == canonical_digest(admission) and
            raw["elf_sha256"] == admission["elf"]["sha256"] and
            raw["resolved_sha256"] == admission["resolved"]["sha256"],
            "SoC record is not bound to admitted artifacts")
    protocol = validate_protocol(load_json(verify_file_identity(raw["protocol"])),
                                 fixture, resolved)
    require(raw["case_id"] == protocol["case_id"], "SoC protocol case mismatch")
    _flash_elf(protocol, admission)
    session, payload, capture_window, capture_record = _capture_record(
        raw["capture"], admission, raw["protocol"], protocol, now)
    from datetime import datetime
    def timestamp(value):
        return datetime.fromisoformat(value.replace("Z", "+00:00"))
    require(timestamp(capture_record["started_utc"]) <= timestamp(raw["started_utc"]) <=
            timestamp(raw["finished_utc"]) <= timestamp(capture_record["finished_utc"]),
            "SoC physical record time leaves admitted capture")
    require(all(raw[key] == payload[key] for key in ("context", "observations", "traces")),
            "SoC records differ from the serial adapter captured artifact manifest")
    definition = CASES[raw["case_id"]]
    fields(raw["observations"], set(definition["observations"]))
    for identity in raw["observations"].values():
        verify_file_identity(identity)
    context = load_json(verify_file_identity(raw["context"]))
    extras = set()
    if "irq_latency" in definition["metrics"]:
        extras.add("irq_priorities")
    if "stack_used_peak" in definition["metrics"]:
        extras.add("stack_capacity_bytes")
    fields(context, {"schema_version", "kind", "session", "case_id", "protocol_sha256",
                     "admission_sha256", "cpu_clock_hz", "load", "executor", "mode_scope", "methods",
                     "time_resolution_s"} | extras)
    require(type(context["schema_version"]) is int and context["schema_version"] == 1 and
            context["kind"] == "soc_hil_context" and context["session"] == session and
            context["case_id"] == raw["case_id"] and
            context["protocol_sha256"] == raw["protocol"]["sha256"] and
            context["admission_sha256"] == raw["admission_sha256"],
            "SoC acquisition context is not bound to capture/protocol")
    require(type(context["cpu_clock_hz"]) is int and
            0 < context["cpu_clock_hz"] < 2 ** 32, "observed SoC CPU clock required")
    tolerance = context["time_resolution_s"]
    require(type(tolerance) in (int, float) and math.isfinite(tolerance) and
            0 < tolerance <= 0.001, "SoC wall-clock resolution must be at most one millisecond")
    if "irq_priorities" in extras:
        priorities = context["irq_priorities"]
        require(isinstance(priorities, dict) and 0 < len(priorities) <= 256 and
                all(isinstance(name, str) and type(value) is int and 0 <= value <= 255
                    for name, value in priorities.items()), "observed SoC IRQ priorities required")
    if "stack_capacity_bytes" in extras:
        require(type(context["stack_capacity_bytes"]) is int and
                0 < context["stack_capacity_bytes"] <= MAX_VALUE,
                "positive observed SoC stack capacity required")
    _text(context["load"])
    _text(context["executor"])
    controllers = {item["id"]: item for item in resolved["controllers"]}
    expected_modes = {name: {key: controllers[name][key] for key in ("kind", "mode")}
                      for name in protocol["controller_ids"]}
    require(context["mode_scope"] == expected_modes, "SoC captured mode scope mismatch")
    mode_results = None
    if raw["case_id"] in selected_modes.SCENARIOS:
        mode_results = selected_modes.validate_observations(raw["case_id"],
            raw["observations"], resolved, protocol, session, raw["protocol"],
            raw["context"])
    fields(context["methods"], set(definition["metrics"]))
    for conditions in context["methods"].values():
        fields(conditions, {"instrument", "method", "resolution", "window_s"})
        require(conditions["instrument"] in protocol["instruments"],
                "SoC measurement uses unreviewed instrument")
        _text(conditions["method"])
        for key in ("resolution", "window_s"):
            value = conditions[key]
            require(type(value) in (int, float) and math.isfinite(value) and
                    0 < value <= MAX_VALUE, "positive bounded SoC acquisition condition required")
        require(conditions["window_s"] <= capture_window + tolerance,
                "SoC measurement window exceeds actual adapter acquisition")
    fields(raw["traces"], set(definition["metrics"]))
    counts, extrema = {}, {}
    for metric, identity in raw["traces"].items():
        trace = load_json(verify_file_identity(identity))
        fields(trace, {"schema_version", "kind", "case_id", "metric", "unit",
                       "session", "protocol_sha256", "context_sha256", "observations_sha256", "samples"})
        bound = protocol["limits"][metric]
        require(type(trace["schema_version"]) is int and trace["schema_version"] == 1 and
                trace["kind"] == "soc_hil_trace" and trace["case_id"] == raw["case_id"] and
                trace["metric"] == metric and trace["unit"] == bound["unit"] and
                trace["session"] == session and
                trace["protocol_sha256"] == raw["protocol"]["sha256"] and
                trace["context_sha256"] == raw["context"]["sha256"] and
                trace["observations_sha256"] == canonical_digest(raw["observations"]),
                "SoC trace context or metric mismatch")
        samples = trace["samples"]
        require(isinstance(samples, list) and 0 < len(samples) <= 100000,
                "nonempty bounded SoC trace samples required")
        values = []
        for index, sample in enumerate(samples):
            fields(sample, {"sequence", "value"})
            value = sample["value"]
            require(type(sample["sequence"]) is int and sample["sequence"] == index,
                    "SoC trace sample sequence is incomplete")
            require(type(value) in (int, float) and math.isfinite(value) and
                    bound["minimum"] <= value <= bound["maximum"],
                    "SoC sample fails reviewed limit: " + metric)
            if bound["unit"] in ("count", "counts", "bytes"):
                require(type(value) is int, "discrete SoC observation must be integer")
            if metric == "duration":
                require(value <= capture_window + tolerance,
                        "SoC duration exceeds actual bounded acquisition")
            if metric == "stack_used_peak":
                require(value <= context["stack_capacity_bytes"],
                        "SoC stack peak exceeds actual capacity")
            if mode_results is not None:
                require(value == mode_results[metric],
                        "selected-mode metric differs from parsed raw observations")
            values.append(value)
        counts[metric] = len(values)
        extrema[metric] = {"minimum": min(values), "maximum": max(values)}
    return {"schema_version": 1, "kind": "soc_hil_validation", "status": "passed",
            "physical_status": "qualified_for_recorded_soc_case", "case_id": raw["case_id"],
            "sample_counts": counts, "extrema": extrema,
            "limits": "Recorded case, selected modes and conditions only; not WCET or product release"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("plan", "template", "capture", "validate"))
    for key in ("fixture", "resolved", "protocol", "recipe", "admission", "raw"):
        parser.add_argument("--" + key, type=Path)
    parser.add_argument("--case", dest="case_id")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    required = {"plan": ("fixture", "resolved"),
                "template": ("fixture", "resolved", "case_id"),
                "capture": ("protocol", "recipe", "admission"),
                "validate": ("raw", "admission")}[args.mode]
    try:
        require(all(getattr(args, key) is not None for key in required),
                "SoC HIL mode requires " + ", ".join(required))
        require(not args.output.exists(), "fresh SoC output required")
        if args.mode in ("plan", "template"):
            resolved, identity = resolved_identity(args.resolved)
            report = (preparation_plan(load_json(args.fixture), resolved)
                      if args.mode == "plan" else
                      case_template(args.case_id, load_json(args.fixture), resolved))
            report.update(fixture=file_identity(args.fixture), resolved=identity)
        elif args.mode == "capture":
            report = capture(args.protocol, args.recipe, args.admission, args.output)
        else:
            report = validate_record(load_json(args.raw), load_json(args.admission))
            report.update(raw=file_identity(args.raw), admission=file_identity(args.admission))
        if args.mode != "capture":
            atomic_json(args.output, report)
    except (EvidenceError, OSError, KeyError, TypeError, ValueError) as error:
        print("SoC HIL rejected: " + str(error), file=sys.stderr)
        return 1
    print("SoC HIL: " + report["status"] + "; " + report["physical_status"])
    return 0 if report["status"] != "failed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
