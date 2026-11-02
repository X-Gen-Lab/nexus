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


if __name__ == "__main__":
    unittest.main()
