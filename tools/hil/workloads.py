#!/usr/bin/env python3
"""Prepare exact-build HIL workload protocols without operating equipment."""

from __future__ import annotations

import argparse
import copy
import math
from pathlib import Path
import sys
from types import MappingProxyType

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (
    EvidenceError, atomic_json, fields, file_identity, identifier, load_json,
    verify_file_identity,
)
from tools.evidence.identity import resolved_identity
from tools.hil.admit import require
from tools.hil.measure import _identity_paths
from tools.hil.prepare import preparation_plan


def _definition(work_item, kinds, stimulus, observations, instruments, safety,
                metrics):
    return MappingProxyType({
        "work_item": work_item, "required_kinds": tuple(kinds),
        "stimulus": stimulus, "raw_observations": tuple(observations),
        "instruments": tuple(instruments), "safety": tuple(safety),
        "metric_units": MappingProxyType(metrics),
    })


# These are acquisition obligations, never route/configuration authorities.
WORKLOADS = MappingProxyType({
    "station_binding": _definition(
        47, (), "Identify the actual probe and chip before admitting equipment",
        ("probe_serial", "chip_uid_part", "pcb_revision", "oscillator_supply",
         "wiring_inspection", "firmware_readback"),
        ("debug_probe", "supply_meter", "serial_capture"),
        ("reviewed_pcb", "ground_voltage", "outputs_disconnected"),
        {"identity_mismatches": "count"}),
    "clock_gpio_watchdog": _definition(
        48, ("gpio", "watchdog"),
        "Observe reset/clock startup, GPIO levels and watchdog reset",
        ("clock_frequency", "reset_cause", "gpio_initial_safe_levels",
         "watchdog_enable_feed_reset", "supply_voltage"),
        ("debug_probe", "logic_analyzer", "calibrated_timer", "supply_meter"),
        ("outputs_disconnected", "irreversible_watchdog", "reviewed_pin_load"),
        {"clock_error": "ppm", "watchdog_timeout": "ms",
         "reset_count": "count", "unsafe_outputs": "count"}),
    "uart_spi_dma": _definition(
        49, ("uart", "spi"),
        "Exercise finite UART/SPI modes, wire completion and fault drain",
        ("uart_tx_rx_wire", "spi_cs_sck_data", "dma_completion",
         "cancel_tc_order", "drain_settlement", "buffer_loss_alignment"),
        ("logic_analyzer", "debug_probe", "reviewed_slave"),
        ("reviewed_wire_routes", "legal_dma_memory", "borrow_preserved",
         "no_unselected_stream_mode"),
        {"wire_completion_latency": "us", "dma_loss": "count"}),
    "i2c_electrical": _definition(
        50, ("i2c",),
        "Exercise reviewed messages with ACK/NACK, repeated START and recovery",
        ("scl_sda_waveform", "rise_time_voltage", "ack_nack_stop",
         "repeated_start", "timeout_recovery"),
        ("logic_analyzer", "oscilloscope", "reviewed_slave"),
        ("reviewed_pullups", "reviewed_voltage", "isolated_fault_injection"),
        {"rise_time": "ns", "protocol_errors": "count"}),
    "adc_pwm_exti": _definition(
        51, ("adc", "pwm", "exti"),
        "Apply calibrated analog/edge inputs and selected PWM/ADC workloads",
        ("adc_reference_samples", "adc_samples_timeout", "pwm_period_duty",
         "pwm_shadow_safe_stop", "exti_edge_loss_stop"),
        ("reference_source", "signal_generator", "logic_analyzer"),
        ("analog_limits", "reviewed_timer_routes", "outputs_disconnected",
         "no_unselected_continuous_adc"),
        {"adc_error": "counts", "pwm_error": "ns", "exti_loss": "count"}),
    "flash_power_loss": _definition(
        52, ("flash",),
        "Interrupt supply during reviewed erase/program and observe recovery",
        ("erase_program_phase", "voltage_power_cut", "readback_after_reset",
         "recovery_result", "protected_regions_unchanged"),
        ("power_interrupt", "oscilloscope", "debug_probe"),
        ("external_erase_aligned_layout", "firmware_regions_protected",
         "isolated_supply", "product_recovery_policy"),
        {"power_cut_cases": "count", "recovery_failures": "count"}),
    "concurrent_load": _definition(
        53, ("uart",),
        "Run explicit executors and concurrent load for reviewed duration",
        ("task_irq_load", "wire_bytes_loss", "service_timestamps",
         "controller_isolation", "monotonic_elapsed", "borrow_settlement"),
        ("serial_capture", "logic_analyzer", "calibrated_timer"),
        ("all_executors_progress", "reviewed_irq_ceiling", "borrow_preserved",
         "only_selected_controllers"),
        {"duration": "s", "loss": "count", "service_interval": "us"}),
    "m4f_context": _definition(
        25, (), "Preempt first-release M4F tasks with distinct register banks",
        ("r4_r11_expected_observed", "s0_s31_expected_observed",
         "fp_exception_control", "preemption_nesting_load"),
        ("debug_probe", "register_capture"),
        ("selected_m4f_abi", "reviewed_exception_load", "outputs_disconnected"),
        {"context_mismatches": "count"}),
    "monotonic_deadline": _definition(
        26, (), "Compare actual monotonic/tick time and finite timeout returns",
        ("calibrated_clock_ticks", "deadline_return_timestamps",
         "clock_monotonic_wrap", "load_irq_priorities"),
        ("calibrated_timer", "debug_probe"),
        ("continuous_monotonic_domain", "required_executor_running"),
        {"clock_error": "ppm", "deadline_lateness": "us"}),
    "stack_peaks": _definition(
        27, (), "Watermark every selected task and MSP under nested load",
        ("complete_stack_inventory", "capacity_used_bytes",
         "msp_nested_peak", "task_high_water_bytes"),
        ("debug_probe", "stack_capture"),
        ("stack_inventory_complete", "watermark_without_live_corruption",
         "reviewed_exception_nesting"),
        {"minimum_headroom": "bytes"}),
    "lifecycle_endurance": _definition(
        28, (), "Repeat admit/cancel/drain/join/reclaim with live observers",
        ("iterations_admitted_settled", "joined_observers", "retained_borrows",
         "stale_accesses", "rejected_admission_effects"),
        ("debug_probe", "lifecycle_capture"),
        ("no_forced_borrow_reclaim", "all_publishers_quiesced",
         "required_executor_running"),
        {"iterations": "count", "retained_borrows": "count",
         "stale_accesses": "count"}),
    "tickless_timer": _definition(
        29, (), "Use the selected timer port to sleep and latch wake events",
        ("timer_port_phase_arm", "before_after_monotonic",
         "latched_observed_wake", "deadline_restore_ticks",
         "wake_current_waveform"),
        ("calibrated_timer", "logic_analyzer", "current_meter"),
        ("reviewed_timer_port", "continuous_clock", "wake_sources_latched",
         "no_unreviewed_deep_sleep"),
        {"wake_lateness": "us", "wake_loss": "count"}),
    "fail_stop": _definition(
        30, (), "Trigger assert and stack overflow in an isolated fixture",
        ("assert_trigger_safe_timestamps", "overflow_trigger_safe_timestamps",
         "expected_observed_outputs", "reset_recovery_record"),
        ("logic_analyzer", "debug_probe"),
        ("outputs_disconnected", "external_safe_output_policy",
         "non_destructive_failure_fixture", "reviewed_reset_recovery"),
        {"response_time": "us", "unsafe_outputs": "count"}),
})

MAX_VALUE = 2 ** 64 - 1
TASK_CASES = frozenset({"m4f_context", "stack_peaks", "lifecycle_endurance",
                        "tickless_timer", "fail_stop"})


def _selected_dma(controller):
    modes = {"uart": ("dma-tx",), "spi": ("dma", "dma-full-duplex")}
    return controller["mode"] in modes.get(controller["kind"], ())


def _selection(name, definition, resolved):
    """Selected configuration is distinct from chip or fixture capabilities."""
    if resolved["backend"] == "native":
        return "unsupported", ["physical_target_required"]
    if name in TASK_CASES and resolved["backend"] != "freertos":
        return "unsupported", ["freertos_task_workload_required"]
    if name == "m4f_context":
        cpu = resolved.get("cpu_profile") or resolved.get("cpu", {})
        if cpu.get("arch") != "cortex-m4" or cpu.get("fpu") in (None, "none"):
            return "unsupported", ["selected_m4f_abi_required"]
    selected = resolved["controllers"]
    kinds = {controller["kind"] for controller in selected}
    gaps = [kind for kind in definition["required_kinds"] if kind not in kinds]
    if name == "uart_spi_dma" and not any(
            _selected_dma(item) for item in selected):
        gaps.append("uart_or_spi_dma")
    if name == "tickless_timer" and not (
            isinstance(resolved.get("kernel_profile"), dict) and
            resolved["kernel_profile"].get("tickless") is True):
        gaps.append("explicit_tickless_profile")
    return ("requires_selection" if gaps else "selected"), gaps


def workload_plan(fixture: dict, resolved: dict, workload: dict | None = None):
    """Prepare thirteen obligations; no plan establishes station readiness."""
    base = preparation_plan(fixture, resolved)
    require(resolved.get("backend") in ("native", "baremetal", "freertos"),
            "maintained workload backend required")
    controllers = resolved["controllers"]
    require(isinstance(controllers, list), "selected controllers required")
    ids = [identifier(item["id"], "selected controller")
           for item in controllers]
    require(len(ids) == len(set(ids)), "repeated selected controller identity")
    reviewed = (validate_workload(workload, fixture, resolved)
                if workload is not None else None)
    protocols = ({item["id"]: item for item in reviewed["cases"]}
                 if reviewed else {})
    cases = []
    for name, definition in WORKLOADS.items():
        availability, gaps = _selection(name, definition, resolved)
        protocol = protocols.get(name)
        cases.append({
            "id": name, "work_item": definition["work_item"],
            "status": "not_executed", "availability": availability,
            "selection_gaps": gaps,
            "required_kinds": list(definition["required_kinds"]),
            "stimulus": definition["stimulus"],
            "raw_observations": list(definition["raw_observations"]),
            "instruments": list(definition["instruments"]),
            "safety": list(definition["safety"]),
            "metric_units": dict(definition["metric_units"]),
            "limits": (copy.deepcopy(protocol["limits"]) if protocol else
                       {metric: None for metric in definition["metric_units"]}),
            "reviewed_protocol": copy.deepcopy(protocol),
        })
    return {
        "schema_version": 1, "kind": "hil_workload_plan",
        "status": "prepared_unbound", "physical_status": "not_executed",
        "board": base["board"], "part": base["part"],
        "backend": base["backend"], "controllers": copy.deepcopy(controllers),
        "station_prerequisites": base["station_prerequisites"], "cases": cases,
        "limits": ("Authored protocols and selected modes are not observed "
                   "wiring, equipment execution, qualification or a release"),
    }


def _text(value, label):
    require(isinstance(value, str) and 0 < len(value.strip()) <= 4096,
            "nonempty bounded " + label + " description required")


def _descriptions(value, names, label):
    fields(value, set(names))
    for description in value.values():
        _text(description, label)


def _limits(value, units):
    fields(value, set(units))
    for metric, unit in units.items():
        bound = value[metric]
        fields(bound, {"maximum", "unit"}, {"minimum"})
        require(bound["unit"] == unit,
                "workload metric unit mismatch: " + metric)
        for key in ("maximum", "minimum"):
            if key in bound:
                number = bound[key]
                require(type(number) in (int, float) and
                        0 <= number <= MAX_VALUE and math.isfinite(number),
                        "reviewed finite workload bound required")
        require(bound.get("minimum", 0) <= bound["maximum"],
                "reversed workload bounds")
        if metric in ("duration", "iterations", "power_cut_cases"):
            require("minimum" in bound and bound["minimum"] > 0,
                    "positive reviewed amount of workload required")


def validate_workload(workload: dict, fixture: dict, resolved: dict):
    """Validate authored acquisition obligations, not equipment observations.

    Bindings refer only to selected controller IDs. Instrument identities
    bind configuration/calibration bytes; their presence does not verify wiring
    or physical readiness. Numeric limits require station review.
    """
    plan = workload_plan(fixture, resolved)
    fields(workload, {"schema_version", "kind", "board", "part", "cases"})
    require(type(workload["schema_version"]) is int and
            workload["schema_version"] == 1 and
            workload["kind"] == "hil_workload",
            "authored schema-1 HIL workload required")
    require(workload["board"] == plan["board"] and
            workload["part"] == plan["part"],
            "workload fixture/build Board or part mismatch")
    cases = workload["cases"]
    require(isinstance(cases, list) and 0 < len(cases) <= len(WORKLOADS),
            "nonempty bounded workload cases required")
    selected = {item["id"]: item for item in resolved["controllers"]}
    available = {item["id"]: item for item in plan["cases"]}
    seen = set()
    for case in cases:
        fields(case, {"id", "bindings", "stimulus", "raw_observations",
                      "instruments", "safety", "limits"})
        name = identifier(case["id"], "workload case")
        require(name in WORKLOADS and name not in seen,
                "unknown or repeated workload case")
        seen.add(name)
        require(available[name]["availability"] == "selected",
                "workload case requires explicit supported build selection")
        definition = WORKLOADS[name]
        bindings = case["bindings"]
        require(isinstance(bindings, list) and len(bindings) <= len(selected),
                "bounded selected workload bindings required")
        bound_ids = [identifier(value, "workload controller")
                     for value in bindings]
        require(len(bound_ids) == len(set(bound_ids)) and
                set(bound_ids).issubset(selected),
                "workload bindings are duplicate or not selected")
        kinds = {selected[value]["kind"] for value in bound_ids}
        require(set(definition["required_kinds"]).issubset(kinds),
                "workload bindings omit required selected controller kinds")
        if name == "uart_spi_dma":
            require(any(_selected_dma(selected[value]) for value in bound_ids),
                    "DMA workload must bind its selected UART/SPI controller")
        _text(case["stimulus"], "stimulus")
        _descriptions(case["raw_observations"], definition["raw_observations"],
                      "raw acquisition")
        _descriptions(case["safety"], definition["safety"], "safety review")
        fields(case["instruments"], set(definition["instruments"]))
        for instrument in case["instruments"].values():
            verify_file_identity(instrument)
        _limits(case["limits"], definition["metric_units"])
    return copy.deepcopy(workload)


def _new_destination(path: Path):
    absolute = path.absolute()
    require(not any(item.is_symlink()
                    for item in (absolute, *absolute.parents)),
            "workload report path cannot contain symlinks")
    require(not absolute.exists(),
            "workload report requires an unused destination")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    plan = commands.add_parser("plan")
    for name in ("fixture", "resolved", "report"):
        plan.add_argument("--" + name, type=Path, required=True)
    plan.add_argument("--workload", type=Path)
    args = parser.parse_args(argv)
    inputs = [args.fixture, args.resolved]
    if args.workload is not None:
        inputs.append(args.workload)
    try:
        _new_destination(args.report)
        require(args.report.resolve() not in
                {path.resolve() for path in inputs},
                "workload report cannot overwrite its inputs")
    except EvidenceError as error:
        parser.exit(1, str(error) + "\n")
    try:
        resolved, identity = resolved_identity(args.resolved)
        workload = (load_json(args.workload)
                    if args.workload is not None else None)
        if (workload is not None and
                args.report.resolve() in _identity_paths(workload)):
            parser.exit(1, "workload report cannot overwrite instruments\n")
        report = workload_plan(load_json(args.fixture), resolved, workload)
        report.update(fixture=file_identity(args.fixture), resolved=identity)
        if args.workload is not None:
            report["workload"] = file_identity(args.workload)
    except (EvidenceError, OSError, ValueError, KeyError, TypeError) as error:
        report = {"schema_version": 1, "kind": "hil_workload_plan",
                  "status": "failed", "physical_status": "not_executed",
                  "failure": str(error)}
    atomic_json(args.report, report)
    print("HIL workloads: " + report["status"] + "; equipment not operated")
    return 0 if report["status"] == "prepared_unbound" else 1


if __name__ == "__main__":
    raise SystemExit(main())
