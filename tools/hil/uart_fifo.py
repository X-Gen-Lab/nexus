#!/usr/bin/env python3
"""Parse station-owned FIFO raw exports; this tool never qualifies hardware."""

from __future__ import annotations

import csv
import io
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

MAX_TICK = 2 ** 64 - 1
MAX_RAW_BYTES = 4 * 1024 * 1024
IRQ_FIELDS = ("sequence", "arrival_tick", "dispatch_entry_tick",
              "dispatch_exit_tick")


def _require(condition, message):
    if not condition:
        raise ValueError(message)


def _integer(value, minimum, maximum, label):
    _require(type(value) is int and minimum <= value <= maximum,
             "invalid " + label)


def _counter(text):
    _require(isinstance(text, str) and text.isascii() and text.isdecimal(),
             "external counter must be an unsigned decimal integer")
    value = int(text)
    _integer(value, 0, MAX_TICK, "external counter")
    return value


def parse_irq_csv(data, ticks_per_second, first_tick, last_tick):
    """Parse declared tick tuples; arithmetic does not prove IRQ arrival."""
    _integer(ticks_per_second, 1, 2 ** 32 - 1, "external capture rate")
    _integer(first_tick, 0, MAX_TICK - 1, "first capture tick")
    _integer(last_tick, first_tick + 1, MAX_TICK, "last capture tick")
    _require(isinstance(data, str) and len(data) <= MAX_RAW_BYTES and
             data.isascii() and all(data.splitlines()), "invalid raw CSV bytes")
    reader = csv.DictReader(io.StringIO(data), strict=True)
    _require(reader.fieldnames == list(IRQ_FIELDS),
             "exact IRQ CSV fields required")
    samples, previous_arrival, previous_exit = [], first_tick, first_tick
    for index, row in enumerate(reader):
        _require(index < 100000 and set(row) == set(IRQ_FIELDS),
                 "bounded exact IRQ samples required")
        values = {key: _counter(row[key]) for key in IRQ_FIELDS}
        _require(values["sequence"] == index,
                 "missing or repeated IRQ sequence")
        arrival, entry, exit_tick = (values[key] for key in IRQ_FIELDS[1:])
        _require(first_tick <= arrival <= entry <= exit_tick <= last_tick and
                 previous_arrival <= arrival and previous_exit <= entry,
                 "IRQ arrival/dispatch order leaves its capture window")
        value = (entry - arrival) * 1000000 / ticks_per_second
        _require(math.isfinite(value) and value <= MAX_TICK,
                 "IRQ interval cannot be represented")
        samples.append({"sequence": index, "value": value})
        previous_arrival, previous_exit = arrival, exit_tick
    _require(bool(samples), "empty IRQ trace")
    return samples


def tx_pattern(session, count):
    """The fixed application wire format, not bytes accepted by a host write."""
    _integer(session, 1, 2 ** 32 - 1, "wire session")
    _integer(count, 0, 65536, "wire byte count")
    result = bytearray()
    for sequence in range((count + 31) // 32):
        result.extend(b"NXF1" + session.to_bytes(4, "little") +
                      sequence.to_bytes(4, "little"))
        result.extend((session ^ sequence ^ offset) & 255
                      for offset in range(12, 32))
    return bytes(result[:count])


def parse_device(device, wire_session, rx_wire, tx_wire):
    """Parse a post-join export, retaining errors without a pass claim."""
    from tools.evidence.common import fields
    fields(device, {"session", "phase", "platform_closed", "first_error",
                    "trace_exhausted", "clock_fault", "started_us",
                    "window_end_us", "finished_us", "service_times_us",
                    "received_hex",
                    "completed_tx_bytes", "accepted_requests",
                    "completed_requests", "overflow_events", "mismatch_count"})
    _integer(device["session"], 1, 2 ** 32 - 1, "device wire session")
    _require(device["session"] == wire_session and type(wire_session) is int,
             "device wire session mismatch")
    _integer(wire_session, 1, 2 ** 32 - 1, "wire session")
    _require(device["phase"] == "FIFO_ACQUISITION_CLOSED" and
             device["platform_closed"] is True,
             "device did not join its provider and Platform")
    _require(device["trace_exhausted"] is False and
             device["clock_fault"] is False,
             "truncated observation or broken clock epoch")
    _integer(device["started_us"], 0, MAX_TICK - 1, "device start")
    _integer(device["window_end_us"], device["started_us"] + 1, MAX_TICK - 1,
             "device window end")
    _integer(device["finished_us"], device["started_us"] + 1, MAX_TICK - 1,
             "observed acquisition end")
    for name in ("completed_tx_bytes", "accepted_requests",
                 "completed_requests",
                 "overflow_events", "mismatch_count", "first_error"):
        _integer(device[name], 0, 2 ** 32 - 1, name)
    _require(0 < device["accepted_requests"] == device["completed_requests"] and
             32 * (device["accepted_requests"] - 1) <=
             device["completed_tx_bytes"] and
             0 < device["completed_tx_bytes"] <=
             32 * device["accepted_requests"],
             "fixed TX blocks permit only one final partial or empty request")
    _require(isinstance(rx_wire, bytes) and isinstance(tx_wire, bytes) and
             0 < len(rx_wire) <= 4096 and 0 < len(tx_wire) <= 65536,
             "bounded nonempty actual bidirectional wire capture required")
    _require(tx_wire == tx_pattern(wire_session, len(tx_wire)) and
             device["completed_tx_bytes"] == len(tx_wire),
             "TX wire bytes differ from the settled application/session")
    expected_rx = bytes((wire_session + offset * 73 + (offset >> 8)) & 255
                        for offset in range(len(rx_wire)))
    _require(rx_wire == expected_rx,
             "incoming capture differs from the reviewed peer sequence")
    encoded = device["received_hex"]
    _require(isinstance(encoded, str) and len(encoded) <= 8192 and
             len(encoded) % 2 == 0 and
             all(c in "0123456789abcdef" for c in encoded),
             "bounded canonical received bytes required")
    received = bytes.fromhex(encoded)
    _require(0 < len(received) <= len(rx_wire),
             "invalid observed RX byte count")
    # Count differences describe end-to-end loss only for the station's complete
    # start/peer-stop/drain capture. Overflow reports are not loss sizes.
    remaining = iter(rx_wire)
    _require(all(any(candidate == value for candidate in remaining)
                 for value in received), "received RX order differs from wire")
    mismatches = sum(
        value != ((wire_session + offset * 73 + (offset >> 8)) & 255)
        for offset, value in enumerate(received))
    _require(device["mismatch_count"] == mismatches,
             "device mismatch count differs from received bytes")
    times = device["service_times_us"]
    _require(isinstance(times, list) and 2 <= len(times) <= 256,
             "nonempty bounded raw service intervals required")
    for value in times:
        _integer(value, device["started_us"], device["window_end_us"] - 1,
                 "service timestamp")
        _require(value <= device["finished_us"],
                 "service timestamp leaves the observed acquisition")
    _require(all(a < b for a, b in zip(times, times[1:])),
             "service timestamps must be strictly ordered in one epoch")
    samples = {
        "service_interval": [{"sequence": i, "value": b - a}
                             for i, (a, b) in enumerate(zip(times, times[1:]))],
        "loss": [{"sequence": 0, "value": len(rx_wire) - len(received)}],
        "duration": [{"sequence": 0, "value":
                      (device["finished_us"] - device["started_us"]) /
                      1000000}],
    }
    return samples


def collect(*, context_path, device_path, irq_path, rx_wire_path, tx_wire_path,
            elf_path, resolved_path, wire_session, ticks_per_second,
            first_tick, last_tick, origin):
    """Bind actual raw files for the existing station adapter; never qualify."""
    from tools.evidence.common import file_identity, load_json
    paths = [Path(p) for p in (context_path, device_path, irq_path,
                               rx_wire_path,
                               tx_wire_path, elf_path, resolved_path)]
    before = [file_identity(p) for p in paths]
    _require(origin in {"external-capture", "software-fixture"},
             "explicit capture origin required")
    _require(all(before[i]["size"] <= MAX_RAW_BYTES
                 for i in (0, 1, 2, 3, 4, 6)),
             "input exceeds bounded parser size")
    context, device, resolved = (load_json(paths[i]) for i in (0, 1, 6))
    _require(context.get("kind") == "soc_hil_context" and
             context.get("case_id") == "SOC-55" and
             type(context.get("cpu_clock_hz")) is int and
             0 < context["cpu_clock_hz"] < 2 ** 32 and
             bool(context.get("irq_priorities")) and
             bool(context.get("session")),
             "bound SOC-55 clock/IRQ context required")
    _require(context.get("mode_scope") ==
             {"console": {"kind": "uart", "mode": "irq-fifo-bytes"}},
             "exact selected FIFO context required")
    _require(resolved.get("soc_family") in {"stm32h743", "stm32u575"} and
             len(resolved.get("controllers", [])) == 1 and
             resolved["controllers"][0].get("id") == "console" and
             resolved["controllers"][0].get("mode") == "irq-fifo-bytes",
             "selected exact FIFO resolved identity required")
    selected_irq = {"stm32h743": "USART3_IRQn",
                    "stm32u575": "USART1_IRQn"}[resolved["soc_family"]]
    priorities = context["irq_priorities"]
    _require(isinstance(priorities, dict) and
             priorities.get(selected_irq) == 5 and
             all(isinstance(key, str) and type(value) is int and
                 0 <= value <= 255 for key, value in priorities.items()),
             "observed selected IRQ priority differs from exact FIFO reference")
    method = context.get("methods", {}).get("irq_latency", {})
    _require(method.get("method") ==
             "external_irq_assert_to_provider_dispatch" and
             type(method.get("resolution")) in (int, float) and
             math.isfinite(method["resolution"]) and method["resolution"] > 0,
             "declared external assertion/dispatch boundary required")
    samples = parse_device(device, wire_session, paths[3].read_bytes(),
                            paths[4].read_bytes())
    candidates = parse_irq_csv(paths[2].read_text(encoding="ascii"),
        ticks_per_second, first_tick, last_tick)
    _require(before == [file_identity(p) for p in paths],
             "raw capture inputs changed during parsing")
    return {"kind": "fifo_station_raw_parse", "status": "parsed_unqualified",
            "capture_origin": origin, "software_qualified": False,
            "hardware_qualified": False, "wire_session": wire_session,
            "inputs": before, "samples": samples,
            "irq_capture": {
                "status": "unverified", "measurement_status": "not_executed",
                "ticks_per_second": ticks_per_second,
                "first_tick": first_tick, "last_tick": last_tick,
                "raw_identity": before[2],
                "declared_method": method["method"],
                "candidate_intervals": candidates,
            },
            "acquisition_failed": device["first_error"] != 0 or
                device["overflow_events"] != 0 or device["mismatch_count"] != 0,
            "limits": ["Parser consistency is not station admission or "
                       "physical truth.",
                       "IRQ conversion rate/window are retained caller "
                       "declarations, not calibrated facts.",
                       "Candidate intervals are arithmetic microseconds, "
                       "not measured IRQ latency.",
                       "Actual assertion source, dispatch association and "
                       "complete capture remain unverified.",
                       "Target timestamps use nominal monotonic units; "
                       "external SI calibration is unestablished.",
                       "Dispatcher entry is not Cortex exception entry.",
                       "Raw modulo/DWT pairs are not IRQ arrival latency.",
                       "SOC-55 still needs all existing metric/observation "
                       "inputs."]}


def main(argv=None):
    """Produce a fresh parse; the existing HIL validator owns admission."""
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("context", "device", "irq-csv", "rx-wire",
                 "tx-wire", "elf", "resolved", "report"):
        parser.add_argument("--" + name, type=Path, required=True)
    for name in ("wire-session", "ticks-per-second", "first-tick", "last-tick"):
        parser.add_argument("--" + name, type=int, required=True)
    parser.add_argument("--origin",
                        choices=("external-capture", "software-fixture"),
                        required=True)
    args = parser.parse_args(argv)
    from tools.evidence.common import atomic_json
    if args.report.exists() or args.report.is_symlink():
        parser.exit(1, "fresh parser output is required\n")
    try:
        result = collect(context_path=args.context, device_path=args.device,
            irq_path=args.irq_csv, rx_wire_path=args.rx_wire,
            tx_wire_path=args.tx_wire, elf_path=args.elf,
            resolved_path=args.resolved, wire_session=args.wire_session,
            ticks_per_second=args.ticks_per_second, first_tick=args.first_tick,
            last_tick=args.last_tick, origin=args.origin)
    except (ValueError, OSError, UnicodeError, csv.Error) as error:
        atomic_json(args.report, {"kind": "fifo_station_raw_parse",
            "status": "rejected", "software_qualified": False,
            "hardware_qualified": False, "error": str(error)})
        print("FIFO raw parse rejected: " + str(error), file=sys.stderr)
        return 1
    atomic_json(args.report, result)
    print("FIFO raw parsed; software and hardware qualification remain false")
    return 1 if result["acquisition_failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
