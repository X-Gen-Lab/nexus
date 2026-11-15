"""Malformed EXTI enums reject before any generated bundle is published."""
from tools.configure.tests.native_fixture import native_fixture
from pathlib import Path
import tempfile
import unittest

from tools.configure.tests.assembly_fixture import render_toml
from tools.configure import configure as gate
from tools.configure.providers.common import ConfigurationError


class ExtiEdgeTypeTests(unittest.TestCase):
    def test_unhashable_authored_edge_has_a_configuration_error(self):
        fixture = gate.ROOT / "tests/contracts/stm32_assembly/zg-exti.toml"
        for edge in ([], {}, ["rising"], {"edge": "both"}):
            with self.subTest(edge=edge), tempfile.TemporaryDirectory() as value:
                directory = Path(value)
                authored = gate.load_assembly(fixture, test_support=native_fixture(fixture)).to_dict()
                authored["board_package"] = str(fixture.parent / "zg_board")
                selection = next(item for item in authored["controllers"]
                                 if "edge" in item)
                selection["edge"] = edge
                assembly = directory / "assembly.toml"
                assembly.write_text(render_toml(authored))
                output = directory / "generated"
                with self.assertRaisesRegex(ConfigurationError, "EXTI edge"):
                    gate.configure(assembly, output, test_support=native_fixture(assembly))
                self.assertFalse(output.exists())



if __name__ == "__main__":
    unittest.main()
