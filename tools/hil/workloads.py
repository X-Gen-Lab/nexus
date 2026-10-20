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
from tools.configure.providers.common import ConfigurationError, mode_traits
from tools.hil import selected_modes, threadx


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
         "expected_observed_outputs", "reset_recovery_record",
         "fault_observer_record", "incoming_masks", "retained_loans",
         "terminal_progress"),
        ("logic_analyzer", "debug_probe"),
        ("outputs_disconnected", "external_safe_output_policy",
         "non_destructive_failure_fixture", "reviewed_reset_recovery"),
        {"response_time": "us", "unsafe_outputs": "count"}),
    "uart_rx_dma": _definition(
        12, ("uart",), "Exercise the selected finite/continuous RX DMA mode",
        ("wire_sequence_idle", "block_loans_wrap", "overflow_loss",
         "dma_counter_flags", "cancel_stop_retained_loans",
         "simultaneous_tx_wire_completion"),
        ("logic_analyzer", "serial_generator", "debug_probe"),
        ("selected_rx_mode", "reviewed_dma_memory", "no_reuse_before_drain",
         "bounded_rate_and_service_window"),
        {"received_bytes": "count", "loss": "count",
         "stop_lateness": "us"}),
    "uart_direction_flow": _definition(
        14, ("uart",), "Exercise an explicitly supplied RTS/CTS or RS485 adapter",
        ("firmware_adapter_binding", "rts_cts_backpressure",
         "tx_tc_de_setup_hold", "cancel_drain_direction", "retained_loans"),
        ("logic_analyzer", "reviewed_transceiver", "adapter_policy"),
        ("explicit_external_adapter", "reviewed_signal_voltage_routes",
         "isolated_bus", "no_assumed_board_transceiver"),
        {"flow_cases": "count", "direction_errors": "count",
         "setup_hold_error": "us"}),
    "adc_continuous": _definition(
        15, ("adc",), "Exercise selected continuous trigger DMA at reviewed load",
        ("calibrated_samples_trigger_rate", "two_dma_target_addresses",
         "ready_borrowed_blocks", "starvation_overrun_loss",
         "trigger_stop_drain"),
        ("reference_source", "signal_generator", "debug_probe"),
        ("analog_limits", "selected_continuous_mode", "reviewed_dma_memory",
         "no_borrowed_block_overwrite"),
        {"sample_count": "count", "sample_error": "counts", "loss": "count",
         "trigger_rate_error": "ppm"}),
    "can_bus": _definition(
        54, ("can",), "Exercise selected classic CAN with reviewed bus faults",
        ("bus_frames_ack_arbitration", "mailbox_abort_wire_order",
         "rx_filter_controller_isolation", "bus_off_recovery",
         "retained_tx_frames", "overflow_loss"),
        ("can_analyzer", "reviewed_transceiver", "oscilloscope"),
        ("reviewed_bit_timing", "reviewed_termination_voltage",
         "isolated_fault_injection", "external_bus_off_policy"),
        {"frame_count": "count", "frame_errors": "count", "loss": "count",
         "recovery_lateness": "us"}),
    "ethernet_raw": _definition(
        55, ("eth",), "Exercise selected raw RMII frames with a reviewed PHY",
        ("rmii_clock_link_mdio", "tx_rx_frames_crc", "descriptor_ownership",
         "rx_loan_starvation", "dma_stop_readback", "link_down_recovery"),
        ("ethernet_peer", "logic_analyzer", "reviewed_phy"),
        ("explicit_phy_clock_and_reset", "reviewed_rmii_routes",
         "legal_descriptor_memory", "no_assumed_board_phy"),
        {"frame_count": "count", "frame_errors": "count", "loss": "count",
         "throughput": "bytes_per_s"}),
    "usb_device": _definition(
        56, ("usb_device",), "Exercise selected FS device with reviewed host load",
        ("actual_48mhz_usb_clock", "setup_endpoint_packets",
         "reset_epoch_cancel", "stall_recovery", "disconnect_irq_drain",
         "retained_endpoint_loans"),
        ("usb_analyzer", "reviewed_host", "oscilloscope"),
        ("reviewed_vbus_power", "reviewed_dm_dp_routes",
         "external_descriptors_policy", "no_assumed_board_connector"),
        {"packet_count": "count", "packet_errors": "count", "loss": "count",
         "stop_lateness": "us"}),
    "timer_capture": _definition(
        57, ("timer_capture",), "Apply reviewed edges to the selected capture",
        ("input_edge_timestamps", "counter_wrap_overcapture",
         "filter_polarity", "ring_overflow_loss", "irq_stop_drain"),
        ("signal_generator", "logic_analyzer", "debug_probe"),
        ("reviewed_pin_load", "selected_timer_clock", "bounded_edge_rate"),
        {"edge_count": "count", "timestamp_error": "ns", "loss": "count"}),
    "advanced_pwm_break": _definition(
        57, ("advanced_pwm",), "Exercise isolated complementary PWM and break",
        ("complementary_period_duty", "deadtime_waveform", "break_latch",
         "off_state_and_stop", "controlled_rearm_policy"),
        ("oscilloscope", "logic_analyzer", "reviewed_break_source"),
        ("outputs_disconnected", "reviewed_polarity_idle_levels",
         "bounded_break_fixture", "external_safe_rearm_policy"),
        {"break_cases": "count", "deadtime_error": "ns", "break_lateness": "us",
         "unsafe_outputs": "count"}),
    "uart_word_wire": _definition(
        7, ("uart",),
        "Exchange 000/0ff/100/1ff words and cancel across load, TC and stop",
        ("word_values_bit8", "nine_bit_wire_waveform", "tx_load_tc_settlement",
         "cancel_quarantine_late_tc", "fresh_rx_epoch"),
        ("native_word_peer", "logic_analyzer", "debug_probe"),
        ("reviewed_nine_bit_peer", "separate_control_console",
         "reviewed_voltage_routes", "no_reclaim_before_settlement"),
        {"received_words": "count", "word_errors": "count",
         "stop_lateness": "us"}),
    "spi_dma_whole_cs_segments": _definition(
        7, ("spi",),
        "Exercise 8/16-bit whole-CS segments, fixed cells and retained drain",
        ("cs_sck_mosi_miso_segments", "fixed_dummy_discard_cells",
         "dma_en_count_irq_wire_idle", "cancel_timeout_each_segment",
         "cs_release_settlement_order"),
        ("reviewed_spi_peer", "logic_analyzer", "debug_probe"),
        ("exact_enhanced_endpoint", "reviewed_cs_routes",
         "legal_dma_memory", "isolated_fault_injection",
         "no_reclaim_before_settlement"),
        {"segments_exercised": "count", "protocol_errors": "count",
         "wire_completion_latency": "us"}),
    "i2c_v2_long_timing": _definition(
        7, ("i2c",),
        "Exercise 255/256/257/511/512-byte messages and TCR fault boundaries",
        ("scl_sda_start_tcr_stop", "ack_nack_wire_counts",
         "measured_clock_rise_fall_voltage", "timingr_cr1_readback",
         "register_progress_separate_from_wire_ack", "failed_abort_no_replay"),
        ("oscilloscope", "logic_analyzer", "reviewed_i2c_peer", "debug_probe"),
        ("authored_timing_envelope", "analog_filter_on_digital_filter_zero",
         "measured_pullups_voltage", "isolated_fault_injection"),
        {"message_count": "count", "protocol_errors": "count",
         "rise_time": "ns"}),
    "pwm_group_comp_break": _definition(
        7, ("pwm_group",),
        "Commit CH1..3 shadows and observe COMP1 latched break without restart",
        ("pwm_trigger_common_timebase", "udis_shadow_commits",
         "fresh_uif_no_bootstrap_trigger", "comparator_break_latch",
         "tim1_syscfg_gate_reset", "close_irq_join"),
        ("reference_source", "oscilloscope", "logic_analyzer", "debug_probe"),
        ("exact_group_comparator_route", "analog_input_limits",
         "passive_loads_outputs_disconnected", "external_rearm_policy"),
        {"commits": "count", "break_cases": "count",
         "unsafe_restart_count": "count", "commit_skew": "ns"}),
    **selected_modes.workload_definitions(_definition),
})

MAX_VALUE = 2 ** 64 - 1
WORK_AMOUNTS = frozenset({"duration", "iterations", "power_cut_cases",
                          "received_bytes", "flow_cases", "sample_count",
                          "frame_count", "packet_count", "edge_count",
                          "break_cases", "received_words", "segments_exercised",
                          "message_count", "commits"})
WORK_AMOUNTS = WORK_AMOUNTS | {"trials"}
TASK_CASES = frozenset({"m4f_context", "stack_peaks", "lifecycle_endurance",
                        "tickless_timer", "fail_stop"})


def _selected_dma(controller):
    if controller["kind"] not in ("uart", "spi"):
        return False
    try:
        return mode_traits(controller["kind"], controller["mode"]).dma
    except ConfigurationError:
        return False


MODE_CASES = frozenset({"uart_rx_dma", "uart_direction_flow", "adc_continuous",
                        "can_bus", "ethernet_raw", "usb_device",
                        "timer_capture", "advanced_pwm_break"})
IO_PROTOCOL_CASES = frozenset({"uart_word_wire", "spi_dma_whole_cs_segments",
                               "i2c_v2_long_timing", "pwm_group_comp_break"})


def _selected_io_protocol(name, controller, resolved):
    """Observe exact typed selections, never infer a provider from a chip name."""
    def child(driver):
        return any(item.get("driver") == driver and
                   item.get("controller") == controller["id"]
                   for item in resolved.get("devices", ()))

    if name == "uart_word_wire":
        return (controller["kind"] == "uart" and
                controller["mode"] == "irq-words" and
                controller.get("rx_profile") == "words" and
                controller.get("frame") == {"data_bits": 9, "parity": "none",
                                            "stop_bits": 1, "flow": "none"})
    if name == "spi_dma_whole_cs_segments":
        return (controller["kind"] == "spi" and _selected_dma(controller) and
                controller.get("interface_kind") == "spi_dma_transaction" and
                child("spi-dma-transaction"))
    if name == "i2c_v2_long_timing":
        timing = controller.get("timing_resolution")
        if (resolved.get("soc_family") not in
                ("stm32g474", "stm32h743", "stm32u575") or
                controller["kind"] != "i2c" or
                controller["mode"] != "poll-messages" or
                not child("i2c-long-endpoint") or not isinstance(timing, dict)):
            return False
        assumptions = timing.get("assumptions", {})
        return (type(timing.get("cr1_filter_bits")) is int and
                timing["cr1_filter_bits"] == 0 and
                timing.get("provenance") == "authored_declared_assumptions" and
                assumptions.get("analog_filter_ns_min") == 50 and
                assumptions.get("analog_filter_ns_max") == 260 and
                assumptions.get("digital_filter_cycles") == 0)
    if name == "pwm_group_comp_break":
        pins = {(pin.get("function"), pin.get("pin"))
                for pin in controller.get("pins", ())}
        return (resolved.get("soc_family") == "stm32g474" and
                controller["kind"] == "pwm_group" and
                controller["mode"] == "coherent-pwm" and
                controller["controller"] == "TIM1" and
                controller.get("exclusive") is True and
                {("ch1", "PA8"), ("ch2", "PA9"), ("ch3", "PA10"),
                 ("comp1-inp", "PA1"), ("comp1-inm", "PA4")} <= pins)
    return False


def _selected_protocol(name, controller):
    """An observation protocol is not a second provider capability table."""
    expected = {
        "uart_rx_dma": "uart", "uart_direction_flow": "uart",
        "adc_continuous": "adc", "can_bus": "can", "ethernet_raw": "eth",
        "usb_device": "usb_device", "timer_capture": "timer_capture",
        "advanced_pwm_break": "advanced_pwm",
    }
    if controller["kind"] != expected[name]:
        return False
    try:
        traits = mode_traits(controller["kind"], controller["mode"])
    except ConfigurationError:
        return False
    if name == "uart_rx_dma":
        return traits.rx_dma and traits.block_stream
    if name == "adc_continuous":
        return traits.rx_dma and traits.continuous
    # Electrical-adapter selection still requires an explicit external binding.
    # A selected UART does not install RTS/CTS or an RS485 transceiver.
    return traits.irq


def _selection(name, definition, resolved):
    """Selected configuration is distinct from chip or fixture capabilities."""
    cpu = resolved.get("cpu_profile") or resolved.get("cpu", {})
    if resolved["backend"] == "native" or cpu.get("arch") == "native":
        return "unsupported", ["physical_target_required"]
    if name in threadx.WORKLOAD_CASES:
        return threadx.selection(name, resolved)
    if name in selected_modes.WORKLOAD_NAMES:
        return selected_modes.selection(selected_modes.WORKLOAD_NAMES[name],
                                        resolved)
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
    if name in MODE_CASES and not any(
            _selected_protocol(name, item) for item in selected):
        gaps.append("selected_" + name + "_mode")
    if name in IO_PROTOCOL_CASES and not any(
            _selected_io_protocol(name, item, resolved) for item in selected):
        gaps.append("selected_" + name + "_protocol")
    if name == "tickless_timer" and not (
            isinstance(resolved.get("kernel_profile"), dict) and
            resolved["kernel_profile"].get("tickless") is True):
        gaps.append("explicit_tickless_profile")
    return ("requires_selection" if gaps else "selected"), gaps


def workload_plan(fixture: dict, resolved: dict, workload: dict | None = None):
    """Prepare maintained obligations; no plan establishes station readiness."""
    base = (selected_modes.preparation_base(fixture, resolved)
            if selected_modes.has_selected(resolved) else
            preparation_plan(fixture, resolved))
    require(resolved.get("backend") in
            ("native", "baremetal", "freertos", "threadx"),
            "maintained workload backend required")
    if resolved["backend"] == "threadx":
        threadx.contract(resolved)
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
    definitions = _definitions(resolved)
    for name, definition in definitions.items():
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


def _definitions(resolved):
    return ({**WORKLOADS, **threadx.workload_definitions(_definition)}
            if resolved["backend"] == "threadx" else WORKLOADS)


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
        if metric in WORK_AMOUNTS:
            require("minimum" in bound and bound["minimum"] > 0,
                    "positive reviewed amount of workload required")
            if metric != "duration":
                require(type(bound["minimum"]) is int and
                        type(bound["maximum"]) is int,
                        "discrete workload amount requires integer bounds")


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
    definitions = _definitions(resolved)
    require(isinstance(cases, list) and 0 < len(cases) <= len(definitions),
            "nonempty bounded workload cases required")
    selected = {item["id"]: item for item in resolved["controllers"]}
    available = {item["id"]: item for item in plan["cases"]}
    seen = set()
    for case in cases:
        fields(case, {"id", "bindings", "stimulus", "raw_observations",
                      "instruments", "safety", "limits"})
        name = identifier(case["id"], "workload case")
        require(name in definitions and name not in seen,
                "unknown or repeated workload case")
        seen.add(name)
        require(available[name]["availability"] == "selected",
                "workload case requires explicit supported build selection")
        definition = definitions[name]
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
        if name in MODE_CASES:
            require(any(_selected_protocol(name, selected[value])
                        for value in bound_ids),
                    "workload must bind its actual selected provider mode")
        if name in IO_PROTOCOL_CASES:
            require(any(_selected_io_protocol(name, selected[value], resolved)
                        for value in bound_ids),
                    "workload must bind its actual selected typed protocol")
        if name in selected_modes.WORKLOAD_NAMES:
            require(selected_modes.selection(selected_modes.WORKLOAD_NAMES[name],
                    resolved, [selected[value] for value in bound_ids])[0] ==
                    "selected", "workload must bind its exact selected mode")
        _text(case["stimulus"], "stimulus")
        _descriptions(case["raw_observations"], definition["raw_observations"],
                      "raw acquisition")
        _descriptions(case["safety"], definition["safety"], "safety review")
        fields(case["instruments"], set(definition["instruments"]))
        for instrument in case["instruments"].values():
            verify_file_identity(instrument)
        _limits(case["limits"], definition["metric_units"])
        if name in selected_modes.WORKLOAD_NAMES:
            errors = case["limits"]["protocol_errors"]
            require(type(errors["maximum"]) is int and
                    errors["maximum"] == errors.get("minimum", 0) == 0,
                    "selected-mode protocol errors must remain zero")
    return copy.deepcopy(workload)


def admitted_workload(identity: dict, admission: dict):
    """Hash identity cannot substitute for a reviewed acquisition protocol."""
    path = verify_file_identity(identity)
    fixture = load_json(verify_file_identity(admission["fixture"]))
    resolved, _ = resolved_identity(verify_file_identity(admission["resolved"]))
    return validate_workload(load_json(path), fixture, resolved)


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
