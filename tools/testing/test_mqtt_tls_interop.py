"""Focused host-tool cleanup tests; mocks are not broker/TLS evidence."""
import importlib.util
import json
from pathlib import Path
import signal
from types import SimpleNamespace
import tempfile
import unittest
from unittest import mock

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
    def invoke(self, repeated_signal=False):
        handlers = {}
        def set_handler(number, function):
            old = handlers.get(number, signal.SIG_DFL)
            handlers[number] = function
            return old
        model = DockerModel(handlers, repeated_signal)
        with tempfile.TemporaryDirectory(prefix="mqtt-tool-test-") as directory:
            argv = ["run.py", "--client", "/bin/true", "--output-root",
                    str(Path(directory) / "run")]
            with mock.patch.object(MODULE.sys, "argv", argv), \
                 mock.patch.object(MODULE.uuid, "uuid4", return_value=SimpleNamespace(hex=OWNER)), \
                 mock.patch.object(MODULE.signal, "signal", side_effect=set_handler), \
                 mock.patch.object(MODULE.process, "run", side_effect=model.run):
                with self.assertRaisesRegex(RuntimeError, "create failed 124"):
                    MODULE.main()
            result = json.loads((Path(directory) / "run/evidence/result.json").read_text())
            return model, result

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


if __name__ == "__main__":
    unittest.main()
