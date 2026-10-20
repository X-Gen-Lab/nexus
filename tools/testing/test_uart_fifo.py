"""FIFO raw parser contracts use software fixtures, never physical results."""

import contextlib
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from tools.hil import uart_fifo


class ArrivalTrace(unittest.TestCase):
    def test_actual_external_tick_difference_is_preserved(self):
        raw = ("sequence,arrival_tick,dispatch_entry_tick,dispatch_exit_tick\n"
               "0,10,13,17\n")
        self.assertEqual(uart_fifo.parse_irq_csv(raw, 1000000, 1, 100),
                         [{"sequence": 0, "value": 3.0}])

    def test_entry_before_arrival_is_rejected(self):
        raw = ("sequence,arrival_tick,dispatch_entry_tick,dispatch_exit_tick\n"
               "0,10,9,17\n")
        with self.assertRaisesRegex(ValueError, "order|arrival"):
            uart_fifo.parse_irq_csv(raw, 1000000, 1, 100)

    def test_sparse_repeated_or_reordered_samples_are_rejected(self):
        header = ("sequence,arrival_tick,dispatch_entry_tick,"
                  "dispatch_exit_tick\n")
        for body in ("1,10,13,17\n", "0,10,13,17\n0,18,19,20\n",
                     "0,10,13,17\n1,18,16,20\n", "0,10,13,17\n1,9,18,20\n"):
            with self.subTest(body=body), self.assertRaises(ValueError):
                uart_fifo.parse_irq_csv(header + body, 1000000, 1, 100)

    def test_reset_wrap_or_capture_window_escape_is_rejected(self):
        header = ("sequence,arrival_tick,dispatch_entry_tick,"
                  "dispatch_exit_tick\n")
        for body in ("0,0,13,17\n", "0,10,13,101\n", "0,10,18,17\n",
                     "0,18446744073709551615,0,17\n"):
            with self.subTest(body=body), self.assertRaises(ValueError):
                uart_fifo.parse_irq_csv(header + body, 1000000, 1, 100)

    def test_dwt_pairs_or_dispatch_only_data_cannot_supply_arrival(self):
        for raw in ("before,after\n10,20\n",
                    "sequence,dispatch_entry_tick\n0,13\n",
                    "sequence,arrival_tick,dispatch_entry_tick,"
                    "dispatch_exit_tick\n"):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                uart_fifo.parse_irq_csv(raw, 1000000, 1, 100)

    def test_zero_boolean_or_nonfinite_capture_rates_are_rejected(self):
        raw = ("sequence,arrival_tick,dispatch_entry_tick,dispatch_exit_tick\n"
               "0,10,13,17\n")
        for rate in (0, True, -1, float("nan"), float("inf"), 2 ** 32):
            with self.subTest(rate=rate), self.assertRaises(ValueError):
                uart_fifo.parse_irq_csv(raw, rate, 1, 100)

    def test_equal_quantized_ticks_remain_zero_not_invented_precision(
            self):
        raw = ("sequence,arrival_tick,dispatch_entry_tick,dispatch_exit_tick\n"
               "0,10,10,17\n")
        self.assertEqual(uart_fifo.parse_irq_csv(raw, 1000000, 1, 100),
                         [{"sequence": 0, "value": 0.0}])


class DeviceExport(unittest.TestCase):
    def setUp(self):
        self.wire_session = 7
        self.rx = bytes((7 + offset * 73 + (offset >> 8)) & 255
                        for offset in range(8))
        self.tx = uart_fifo.tx_pattern(7, 32)
        self.device = {"session": 7, "phase": "FIFO_ACQUISITION_CLOSED",
            "platform_closed": True, "first_error": 0,
            "trace_exhausted": False, "clock_fault": False,
            "started_us": 100, "window_end_us": 1000, "finished_us": 1000,
            "service_times_us": [100, 200, 350],
            "received_hex": self.rx.hex(), "completed_tx_bytes": 32,
            "accepted_requests": 1, "completed_requests": 1,
            "overflow_events": 0, "mismatch_count": 0}

    def parse(self, **changes):
        device = {**self.device, **changes}
        return uart_fifo.parse_device(device, self.wire_session, self.rx,
                                      self.tx)

    def test_service_intervals_come_from_actual_timestamp_differences(self):
        parsed = self.parse()
        self.assertEqual(parsed["service_interval"],
                         [{"sequence": 0, "value": 100},
                          {"sequence": 1, "value": 150}])
        self.assertEqual(parsed["loss"], [{"sequence": 0, "value": 0}])

    def test_cancel_retained_or_unjoined_exports_cannot_complete_acquisition(
            self):
        for changes in ({"phase": "FIFO_ACQUISITION_RETAINED"},
                        {"platform_closed": False}, {"trace_exhausted": True},
                        {"clock_fault": True}, {"completed_requests": 0}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.parse(**changes)

    def test_host_write_success_cannot_replace_actual_wire_bytes(self):
        with self.assertRaisesRegex(ValueError, "wire"):
            uart_fifo.parse_device(self.device, 7, self.rx, self.tx[:-1])

    def test_wrong_session_and_corrupted_tx_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "session"):
            uart_fifo.parse_device(self.device, 8, self.rx, self.tx)
        with self.assertRaisesRegex(ValueError, "TX wire"):
            uart_fifo.parse_device(self.device, 7, self.rx, self.tx[:-1] + b"z")

    def test_zero_repeated_or_reversed_service_samples_are_rejected(self):
        for times in ([], [100], [100, 100], [200, 100], [100, 1000]):
            with self.subTest(times=times), self.assertRaises(ValueError):
                self.parse(service_times_us=times)

    def test_reordered_received_bytes_are_not_hidden_by_set_membership(self):
        with self.assertRaisesRegex(ValueError, "RX|received"):
            self.parse(received_hex=self.rx[::-1].hex())

    def test_overflow_event_is_not_a_missing_byte_count(self):
        received = self.rx[:3] + self.rx[4:]
        mismatches = sum(value != self.rx[index]
                         for index, value in enumerate(received))
        parsed = self.parse(received_hex=received.hex(), first_error=8,
                            overflow_events=5, mismatch_count=mismatches)
        self.assertEqual(parsed["loss"], [{"sequence": 0, "value": 1}])

    def test_prefix_bytes_and_mismatch_count_must_agree(self):
        with self.assertRaisesRegex(ValueError, "mismatch|received"):
            self.parse(mismatch_count=1)

    def test_missing_observed_end_cannot_use_the_configured_deadline(self):
        del self.device["finished_us"]
        with self.assertRaisesRegex(ValueError, "observed|finished"):
            self.parse()

    def test_early_stop_duration_uses_observed_end_not_the_target_window(self):
        samples = self.parse(finished_us=400)
        self.assertEqual(samples["duration"],
                         [{"sequence": 0, "value": 0.0003}])

    def test_request_count_allows_only_one_final_partial_or_empty_block(self):
        for length in (32, 33):
            with self.subTest(wire_bytes=length):
                device = {**self.device, "accepted_requests": 2,
                    "completed_requests": 2, "completed_tx_bytes": length}
                uart_fifo.parse_device(device, 7, self.rx,
                                       uart_fifo.tx_pattern(7, length))
        with self.assertRaisesRegex(ValueError, "settled|request|TX|block"):
            self.parse(accepted_requests=100, completed_requests=100)

    def test_boolean_device_session_cannot_alias_integer_wire_session(self):
        rx = bytes((1 + offset * 73 + (offset >> 8)) & 255
                   for offset in range(8))
        device = {**self.device, "session": True, "received_hex": rx.hex()}
        with self.assertRaisesRegex(ValueError, "session"):
            uart_fifo.parse_device(device, 1, rx,
                                   uart_fifo.tx_pattern(1, 32))


class BoundCollector(unittest.TestCase):
    setUp = DeviceExport.setUp

    def inputs(self, root):
        context = {"kind": "soc_hil_context", "case_id": "SOC-55",
                "cpu_clock_hz": 64000000, "irq_priorities": {"USART3_IRQn": 5},
                "session": "software-session", "mode_scope": {
                    "console": {"kind": "uart", "mode": "irq-fifo-bytes"}},
                "methods": {"irq_latency": {
                    "method": "external_irq_assert_to_provider_dispatch",
                    "resolution": 1}}}
        resolved = {"soc_family": "stm32h743", "controllers": [
                {"id": "console", "kind": "uart", "mode": "irq-fifo-bytes"}]}
        for name, data in (("context", context), ("device", self.device),
                           ("resolved", resolved)):
            (root / f"{name}.json").write_text(json.dumps(data),
                                              encoding="utf-8")
        (root / "irq.csv").write_text(
            "sequence,arrival_tick,dispatch_entry_tick,"
            "dispatch_exit_tick\n0,10,13,17\n")
        (root / "rx.bin").write_bytes(self.rx)
        (root / "tx.bin").write_bytes(self.tx)
        # This is deliberately not presented as a production ELF proof.
        (root / "fixture.elf").write_bytes(b"software-fixture-only")
        return dict(context_path=root / "context.json",
            device_path=root / "device.json",
            irq_path=root / "irq.csv", rx_wire_path=root / "rx.bin",
            tx_wire_path=root / "tx.bin", elf_path=root / "fixture.elf",
            resolved_path=root / "resolved.json", wire_session=7,
            ticks_per_second=1000000, first_tick=1, last_tick=100,
            origin="software-fixture")

    def test_parsing_bound_model_files_never_sets_qualification(self):
        with tempfile.TemporaryDirectory() as directory:
            args = self.inputs(Path(directory))
            for origin in ("software-fixture", "external-capture"):
                parsed = uart_fifo.collect(**{**args, "origin": origin})
                self.assertFalse(parsed["software_qualified"])
                self.assertFalse(parsed["hardware_qualified"])
                self.assertEqual(parsed["status"], "parsed_unqualified")
                self.assertEqual(len(parsed["inputs"]), 7)

    def test_wrong_observed_selected_irq_priority_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            args = self.inputs(Path(directory))
            context = json.loads(args["context_path"].read_text())
            context["irq_priorities"] = {"USART3_IRQn": 0}
            args["context_path"].write_text(json.dumps(context))
            with self.assertRaisesRegex(ValueError, "priority"):
                uart_fifo.collect(**args)

    def test_wrong_mode_cannot_relabel_a_different_provider_as_fifo(self):
        with tempfile.TemporaryDirectory() as directory:
            args = self.inputs(Path(directory))
            resolved = json.loads(args["resolved_path"].read_text())
            resolved["controllers"][0]["mode"] = "irq-byte-event"
            args["resolved_path"].write_text(json.dumps(resolved))
            with self.assertRaisesRegex(ValueError, "FIFO"):
                uart_fifo.collect(**args)

    def test_unassociated_irq_conversion_is_retained_as_unverified_only(self):
        with tempfile.TemporaryDirectory() as directory:
            args = self.inputs(Path(directory))
            raw = args["irq_path"].read_bytes()
            identity = {"path": str(args["irq_path"]), "size": len(raw),
                        "sha256": hashlib.sha256(raw).hexdigest()}
            for origin in ("software-fixture", "external-capture"):
                for rate in (1000000, 1000000000):
                    with self.subTest(origin=origin, rate=rate):
                        parsed = uart_fifo.collect(**{**args,
                            "origin": origin, "ticks_per_second": rate})
                        self.assertNotIn("irq_latency", parsed["samples"])
                        self.assertFalse(parsed["software_qualified"])
                        self.assertFalse(parsed["hardware_qualified"])
                        self.assertEqual(parsed["irq_capture"], {
                            "status": "unverified",
                            "measurement_status": "not_executed",
                            "ticks_per_second": rate,
                            "first_tick": args["first_tick"],
                            "last_tick": args["last_tick"],
                            "raw_identity": identity,
                            "declared_method":
                                "external_irq_assert_to_provider_dispatch",
                            "candidate_intervals": [
                                {"sequence": 0, "value": 3000000 / rate}],
                        })


class PlatformCli(unittest.TestCase):
    setUp = DeviceExport.setUp
    inputs = BoundCollector.inputs

    def test_cli_retains_unverified_capture_and_refuses_existing_report(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            inputs = self.inputs(root)
            report = root / "parse.json"
            arguments = []
            for name, key in (("context", "context_path"),
                              ("device", "device_path"),
                              ("irq-csv", "irq_path"),
                              ("rx-wire", "rx_wire_path"),
                              ("tx-wire", "tx_wire_path"),
                              ("elf", "elf_path"),
                              ("resolved", "resolved_path")):
                arguments.extend(("--" + name, str(inputs[key])))
            arguments.extend(("--report", str(report), "--wire-session", "7",
                              "--ticks-per-second", "1000000", "--first-tick",
                              "1", "--last-tick", "100", "--origin",
                              "software-fixture"))
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(uart_fifo.main(arguments), 0)
            parsed = json.loads(report.read_text())
            self.assertFalse(parsed["software_qualified"])
            self.assertFalse(parsed["hardware_qualified"])
            self.assertEqual(parsed["irq_capture"]["measurement_status"],
                             "not_executed")
            self.assertNotIn("irq_latency", parsed["samples"])
            original = report.read_bytes()
            with contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as rejected:
                    uart_fifo.main(arguments)
            self.assertEqual(rejected.exception.code, 1)
            self.assertEqual(report.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
