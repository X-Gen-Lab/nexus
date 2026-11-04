"""Focused host-tool cleanup tests; mocks are not broker/TLS evidence."""
import importlib.util
import json
from pathlib import Path
import signal
from types import SimpleNamespace
import tempfile
import unittest
import unittest.mock as mock

SPEC = importlib.util.spec_from_file_location("interop", Path(__file__).with_name("mqtt_tls_interop.py"))
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
OWNER = "c" * 32
CID = "a" * 64
NAME = "nexus-mqtt-tls-e2e-" + OWNER


class DockerModel:
    def __init__(self, handlers, repeated_signal=False):
        self.handlers = handlers
        self.repeated_signal = repeated_signal
        self.actions = []
        self.removed = False

    def run(self, command, **kwargs):
        del kwargs
        code, stdout = 0, ""
        if command[0] == "openssl":
            for switch in ("-out", "-keyout"):
                if switch in command:
                    Path(command[command.index(switch) + 1]).write_bytes(b"mock host fixture")
        elif "create" in command:
            code = 124  # Docker client withdrawn; daemon may own the named object.
        elif "inspect" in command:
            if "image" in command:
                stdout = "pinned image identity"
            elif self.removed:
                code = 1
            elif "{{index .Config.Labels" in command[-1]:
                stdout = OWNER  # The original unstructured implementation.
            else:
                stdout = json.dumps({"id": CID, "name": "/" + NAME, "owner": OWNER})
        elif "stop" in command:
            self.actions.append(("stop", command[-1]))
            if self.repeated_signal:
                self.handlers[signal.SIGTERM](signal.SIGTERM, None)
        elif "rm" in command:
            self.actions.append(("rm", command[-1]))
            self.removed = True
        return SimpleNamespace(returncode=code, stdout=stdout, stderr="",
                               receipt_path=Path("mock-receipt.json"))


class Cleanup(unittest.TestCase):
    def invoke(self, repeated_signal=False, transport="posix"):
        handlers = {}
        def set_handler(number, function):
            old = handlers.get(number, signal.SIG_DFL)
            handlers[number] = function
            return old
        model = DockerModel(handlers, repeated_signal)
        with tempfile.TemporaryDirectory(prefix="mqtt-tool-test-") as directory:
            argv = ["run.py", "--client", "/bin/true", "--output-root",
                    str(Path(directory) / "run"), "--transport", transport]
            with mock.patch.object(MODULE.sys, "argv", argv), \
                 mock.patch.object(MODULE.uuid, "uuid4", return_value=SimpleNamespace(hex=OWNER)), \
                 mock.patch.object(MODULE.signal, "signal", side_effect=set_handler), \
                 mock.patch.object(MODULE.process, "run", side_effect=model.run):
                with self.assertRaisesRegex(RuntimeError, "create failed 124"):
                    MODULE.main()
            result = json.loads((Path(directory) / "run/evidence/result.json").read_text())
            return model, result

    def test_external_client_option_preserves_an_unassessed_scope(self):
        model, result = self.invoke(transport="external-client")
        self.assertEqual(result["transport"], "external-client")
        self.assertIn("Externally owned host client", result["scope"])
        self.assertIn("transport, TLS and protocol behavior not assessed",
                      result["scope"])
        self.assertIn("no MCU/PHY qualification", result["scope"])
        self.assertNotIn("POSIX", result["scope"])
        self.assertNotIn("full client lwIP", result["scope"])
        self.assertFalse(result["qualification"])
        self.assertEqual(result["physical_status"], "not_executed")
        self.assertEqual(result["cases"], [])
        self.assertEqual(model.actions, [("stop", CID), ("rm", CID)])

    def test_unknown_create_result_locks_full_identity_before_name_can_retarget(self):
        model, result = self.invoke()
        self.assertEqual(model.actions, [("stop", CID), ("rm", CID)])
        self.assertEqual(result["primary_error"]["type"], "RuntimeError")
        self.assertEqual(result["cleanup"][0]["remove_exit"], 0)

    def test_repeated_controlled_signal_cannot_interrupt_owned_withdrawal(self):
        model, result = self.invoke(repeated_signal=True)
        self.assertTrue(model.removed)
        self.assertEqual(model.actions, [("stop", CID), ("rm", CID)])
        self.assertEqual(result["primary_error"]["type"], "RuntimeError")
        self.assertEqual(result["cleanup_signals"], [signal.SIGTERM])


class ChainReport(unittest.TestCase):
    complete = (
        "authenticated_tls=TLSv1.2 cipher=fixture\n"
        "mqtt_clean_close=locally_accepted mqtt_events=15\n"
        "client_tcp_state=3 tcp_pending_rx=0 tcp_pending_tx=0 data_frames=8\n"
        "client_loans_clear=1\n"
        "result=0 rx_frames=7 provider_service_calls=25\n"
        "relay_sent=500 relay_received=600 memory_busy=1 max_chain=512 "
        "relay_rx_frames=8 relay_data_frames=9 byte_budget=17 paused_rounds=30 loans_clear=1\n"
        "relay_joined=1 two_tcp_connections=1 phy_qualified=0\n"
    )

    def test_suite_keeps_original_three_and_requires_five_full_chain_cases(self):
        self.assertEqual(MODULE.case_names("posix"),
                         ("clean", "fault", "missing-client"))
        self.assertEqual(MODULE.case_names("lwip-test-relay"),
                         ("clean", "backpressure", "fault", "deadline",
                          "missing-client"))
        with self.assertRaises(ValueError):
            MODULE.case_names("unknown")

    def test_external_client_uses_plain_bundles_without_chain_probe_admission(self):
        self.assertEqual(MODULE.case_names("external-client"),
                         MODULE.case_names("posix"))
        self.assertEqual(MODULE.case_names("external-client"),
                         ("clean", "fault", "missing-client"))
        self.assertNotEqual(MODULE.case_names("external-client"),
                            MODULE.case_names("lwip-test-relay"))

    def test_success_requires_real_stack_and_cleanup_counts(self):
        metrics = MODULE.chain_report("clean", self.complete)
        self.assertEqual(metrics["relay_sent"], 500)
        self.assertEqual(metrics["mqtt_events"], 15)
        for text in (self.complete.replace("data_frames=8", "data_frames=0"),
                     self.complete.replace("relay_joined=1", "relay_joined=0"),
                     self.complete.replace("client_loans_clear=1", "client_loans_clear=0"),
                     self.complete.replace("rx_frames=7", "rx_frames=0")):
            with self.assertRaises(ValueError):
                MODULE.chain_report("clean", text)

    def test_repeated_human_event_logs_do_not_replace_machine_metrics(self):
        MODULE.chain_report("clean", "event=2 status=0\nevent=0 status=0\n" + self.complete)
        with self.assertRaises(ValueError):
            MODULE.chain_report("clean", "rx_frames=99\n" + self.complete)

    def test_exit_zero_alone_is_not_complete_chain_evidence(self):
        with self.assertRaises(ValueError):
            MODULE.chain_report("clean", "result=0\n")

    def test_partial_mode_requires_the_smaller_byte_budget(self):
        MODULE.chain_report("backpressure", self.complete)
        with self.assertRaises(ValueError):
            MODULE.chain_report("backpressure",
                                self.complete.replace("byte_budget=17", "byte_budget=113"))
        with self.assertRaises(ValueError):
            MODULE.chain_report("backpressure",
                                self.complete.replace("paused_rounds=30", "paused_rounds=0"))

    def test_fault_requires_no_replay_and_explicit_deadline_kind(self):
        base = self.complete.replace("mqtt_clean_close=locally_accepted mqtt_events=15",
                                     "transport_fault=3 deadline=1 replay_provider_calls=0")
        MODULE.chain_report("deadline", base)
        with self.assertRaises(ValueError):
            MODULE.chain_report("deadline", base.replace("deadline=1", "deadline=0"))
        with self.assertRaises(ValueError):
            MODULE.chain_report("deadline", base.replace("replay_provider_calls=0", "replay_provider_calls=2"))




class CapturedEthernet(unittest.TestCase):
    """Synthetic parser fixtures are not actual captured network evidence."""
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="captured Ethernet ")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.path = self.directory / "client.pcap"

    @staticmethod
    def write(path, records):
        import struct
        header = struct.pack("<IHHIIII", 0xa1b2c3d4, 2, 4, 0, 0, 1514, 1)
        path.write_bytes(header + b"".join(struct.pack("<IIII", 0, i,
            len(frame), len(frame)) + frame for i, frame in enumerate(records)))

    @staticmethod
    def frame(direction, sequence, payload=b"", flags=0x10):
        import struct
        source, peer = ((1, 2) if direction == "client" else (2, 1))
        eth = bytes([2, 0, 0, 0, 0, peer, 2, 0, 0, 0, 0, source, 8, 0])
        ip = struct.pack("!BBHHHBBH4B4B", 0x45, 0, 40 + len(payload), 0, 0,
                         64, 6, 0, 10, 0, 0, source, 10, 0, 0, peer)
        ports = (40000, 8883) if source == 1 else (8883, 40000)
        tcp = struct.pack("!HHIIBBHHH", *ports, sequence, 0, 0x50, flags,
                          1024, 0, 0)
        return eth + ip + tcp + payload

    def traffic(self, direction="client", plaintext=False):
        # An actual TLS session is established only by the separate broker run.
        content = (b"nexus/host/e2e" if plaintext else b"\xa5" * 32)
        tls = b"\x16\x03\x03\x00\x01x" + b"\x17\x03\x03" + len(content).to_bytes(2, "big") + content
        return [self.frame(direction, 100, flags=2),
                self.frame(direction, 101, tls[:8]),
                self.frame(direction, 109, tls[8:])]

    def test_parser_requires_complete_raw_records_and_ethernet_linktype(self):
        self.write(self.path, self.traffic())
        report = MODULE.read_pcap(self.path, "client", complete_tls=True)
        self.assertEqual(report["frames"], 3)
        self.assertEqual(report["tcp_payload_frames"], 2)
        self.assertEqual(report["tls_application_records"], 1)
        original = self.path.read_bytes()
        for bad in (original[:-1], original + b"x", b"",
                    original[:20] + (101).to_bytes(4, "little") + original[24:]):
            self.path.write_bytes(bad)
            with self.assertRaises(ValueError):
                MODULE.read_pcap(self.path, "client", complete_tls=True)

    def test_parser_rejects_truncation_bounds_and_parent_aliases(self):
        self.write(self.path, self.traffic())
        data = bytearray(self.path.read_bytes())
        data[36:40] = (1515).to_bytes(4, "little")
        self.path.write_bytes(data)
        with self.assertRaises(ValueError):
            MODULE.read_pcap(self.path, "client", complete_tls=True)
        self.write(self.path, self.traffic())
        alias = self.directory / "alias.pcap"
        alias.symlink_to(self.path)
        with self.assertRaises(ValueError):
            MODULE.read_pcap(alias, "client", complete_tls=True)
        parent = self.directory / "linked"
        parent.symlink_to(self.directory, target_is_directory=True)
        with self.assertRaises(ValueError):
            MODULE.read_pcap(parent / "client.pcap", "client", complete_tls=True)

    def test_parser_rejects_conflicting_stream_bytes_and_plaintext_fixture(self):
        frames = self.traffic()
        self.write(self.path, frames + [self.frame("client", 101, b"wrong")])
        with self.assertRaises(ValueError):
            MODULE.read_pcap(self.path, "client", complete_tls=True)
        self.write(self.path, self.traffic(plaintext=True))
        with self.assertRaisesRegex(ValueError, "plaintext"):
            MODULE.read_pcap(self.path, "client", complete_tls=True)

    def test_failed_case_can_report_partial_tls_tail_without_truncating_pcap(self):
        frames = self.traffic()
        self.write(self.path, frames[:-1])
        report = MODULE.read_pcap(self.path, "client", complete_tls=False)
        self.assertTrue(report["partial_tls_tail"])
        with self.assertRaises(ValueError):
            MODULE.read_pcap(self.path, "client", complete_tls=True)

    def test_capture_binds_case_receipt_binary_and_exact_frame_counts(self):
        for direction in ("client", "relay"):
            self.write(self.directory / (direction + ".pcap"), self.traffic(direction))
        receipt = self.directory / "command-receipt.json"
        stdout = ("client_capture_frames=3 client_capture_bytes=" +
                  str(self.path.stat().st_size) + " client_capture_complete=1\n" +
                  "relay_capture_frames=3 relay_capture_bytes=" +
                  str(self.path.stat().st_size) + " relay_capture_complete=1\n")
        import hashlib
        binary = self.directory / "client"
        binary.write_bytes(b"synthetic test executable")
        binary_hash = hashlib.sha256(binary.read_bytes()).hexdigest()
        raw_stdout = self.directory / "stdout.txt"
        raw_stdout.write_text(stdout)
        command = {"schema": 1, "outcome": "completed", "exit_code": 0,
                   "withdrawal_complete": True,
                   "command": [str(binary), "18883", "clean", "/credentials",
                               str(self.directory)],
                   "stdout": {"path": str(raw_stdout), "bytes": len(stdout),
                              "sha256": hashlib.sha256(stdout.encode()).hexdigest()}}
        receipt.write_text(json.dumps(command))
        result = MODULE.capture_report("clean", self.directory, stdout, receipt,
                                       binary_hash)
        self.assertEqual(result["case"], "clean")
        self.assertEqual(result["client_binary_sha256"], binary_hash)
        self.assertEqual(result["capture_points"], "successful IPC Ethernet TX before SETTLED")
        self.assertEqual(result["timestamp_domain"], "host monotonic elapsed microseconds")
        self.assertEqual(result["physical_status"], "not_executed")
        self.assertEqual(set(result["directions"]), {"client", "relay"})
        valid_receipt = json.dumps(command)
        ambiguous = (
            valid_receipt.replace('"exit_code": 0',
                                  '"exit_code": 99, "exit_code": 0'),
            valid_receipt.replace('"bytes": ' + str(len(stdout)),
                                  '"bytes": 0, "bytes": ' + str(len(stdout))),
            json.dumps(dict(command, schema=True)),
            json.dumps(dict(command, exit_code=False)),
            valid_receipt[:-1] + ', "unexpected": NaN}',
        )
        for raw_receipt in ambiguous:
            with self.subTest(receipt=raw_receipt):
                receipt.write_text(raw_receipt)
                with self.assertRaises(ValueError):
                    MODULE.capture_report("clean", self.directory, stdout,
                                           receipt, binary_hash)
        receipt.write_text(valid_receipt)
        for wrong in (dict(command, command=[str(binary), "18883", "fault",
                                             "/credentials", str(self.directory)]),
                      dict(command, command=[str(binary), "18883", "clean",
                                             "/credentials", "/another-case"]),
                      dict(command, stdout=dict(command["stdout"], sha256="0" * 64))):
            receipt.write_text(json.dumps(wrong))
            with self.assertRaises(ValueError):
                MODULE.capture_report("clean", self.directory, stdout, receipt,
                                       binary_hash)
        receipt.write_text(json.dumps(command))
        with self.assertRaises(ValueError):
            MODULE.capture_report("clean", self.directory, stdout, receipt,
                                   "f" * 64)
        for text in (stdout.replace("client_capture_frames=3", "client_capture_frames=4"),
                     stdout.replace("relay_capture_complete=1", "relay_capture_complete=0")):
            with self.assertRaises(ValueError):
                MODULE.capture_report("clean", self.directory, text, receipt, binary_hash)


if __name__ == "__main__":
    unittest.main()
