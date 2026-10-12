"""Malformed EXTI enums reject before any generated bundle is published."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure.providers import stm32g474
from tools.configure.providers.common import ConfigurationError


class ExtiEdgeTypeTests(unittest.TestCase):
    def test_unhashable_authored_edge_has_a_configuration_error(self):
        fixture = gate.ROOT / "tests/contracts/g474_assembly/exti.toml"
        for edge in ([], {}, ["rising"], {"edge": "both"}):
            with self.subTest(edge=edge), tempfile.TemporaryDirectory() as value:
                directory = Path(value)
                authored = gate.load_assembly(fixture)
                authored["board_package"] = str(fixture.parent / "exti_board")
                selection = next(item for item in authored["controllers"]
                                 if "edge" in item)
                selection["edge"] = edge
                assembly = directory / "assembly.json"
                assembly.write_text(json.dumps(authored))
                output = directory / "generated"
                with self.assertRaisesRegex(ConfigurationError, "EXTI edge"):
                    gate.configure(assembly, output)
                self.assertFalse(output.exists())

    def test_family_validation_cannot_leak_a_python_type_error(self):
        route = {"kind": "exti", "controller": "EXTI0",
                 "pins": [{"pin": "PA0", "function": "input", "af": 0}]}
        for edge in ([], {}):
            with self.subTest(edge=edge):
                with self.assertRaises(ConfigurationError):
                    stm32g474.validate_selection(
                        {"mode": "edge-event", "edge": edge,
                         "irq_priority": 5, "event_capacity": 4}, route)


if __name__ == "__main__":
    unittest.main()
