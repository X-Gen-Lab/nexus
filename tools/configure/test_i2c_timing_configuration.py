"""Declared I2C envelopes use one resolver and the real selected initializer."""
from pathlib import Path
import re
import tempfile
import unittest

from tools.configure import bindings, configure


class I2CTimingConfiguration(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def resolve(self, fixture="g474_assembly/acquisition.toml", **changes):
        path = configure.ROOT / "tests/contracts" / fixture
        contents = path.read_text()
        match = re.search(r'(?m)^board_package\s*=\s*"([^"]+)"', contents)
        self.assertIsNotNone(match)
        board = (path.parent / match[1]).resolve()
        contents = contents[:match.start()] + (
            'board_package = "' + str(board) + '"') + contents[match.end():]
        name = re.search(r"(?m)^\[i2c\.([^].]+)\]$", contents)[1]
        h7 = fixture.startswith("h743")
        inputs = dict(kernel_hz_min=60000000 if h7 else 15000000,
                      kernel_hz_max=68000000 if h7 else 17000000,
                      pclk_hz_min=(60000000 if h7 else 3900000
                                   if fixture.startswith("u575") else 160000000),
                      bus_hz_min=60000, rise_ns_max=1000, fall_ns_max=300,
                      source="software test declared envelope; not measured")
        inputs.update(changes)
        contents += f"\n[i2c.{name}.timing]\n"
        for field, value in inputs.items():
            literal = '"' + value + '"' if isinstance(value, str) else str(value)
            contents += field + " = " + literal + "\n"
        assembly = self.root / "timing.toml"
        assembly.write_text(contents)
        return configure.resolve_ir(assembly)

    def test_three_selected_v2_providers_apply_resolved_timing(self):
        for fixture in ("g474_assembly/acquisition.toml",
                        "h743_assembly/peripherals.toml",
                        "u575_assembly/peripherals.toml"):
            with self.subTest(fixture=fixture):
                result = self.resolve(fixture)
                item = next(item for item in result.controllers
                            if item.kind == "i2c")
                timing = item.options.timing
                self.assertEqual(timing.provenance,
                                 "authored_declared_assumptions")
                self.assertFalse(timing.physical_qualified)
                self.assertEqual(timing.cr1_filter_bits, 0)
                _, generated = bindings.emit(result)
                self.assertIn(f"0x{timing.timingr:08x}u", generated)
                if fixture.startswith("u575"):
                    self.assertIn("nx_stm32u575_i2c_prepare", generated)

    def test_declared_clock_must_contain_selected_source_nominal(self):
        for change in (dict(kernel_hz_min=30000000, kernel_hz_max=40000000),
                       dict(pclk_hz_min=180000000)):
            with self.subTest(change=change), self.assertRaisesRegex(
                    ValueError, "clock|nominal|source"):
                self.resolve(**change)

    def test_unimplemented_filter_choices_cannot_be_calculation_only(self):
        for changes in (dict(digital_filter_cycles=1),
                        dict(analog_filter_ns_min=0,
                             analog_filter_ns_max=0)):
            with self.subTest(changes=changes), self.assertRaisesRegex(
                    ValueError, "filter|unknown|fields"):
                self.resolve(**changes)

    def test_source_and_all_clock_bounds_are_required(self):
        with self.assertRaisesRegex(ValueError, "source"):
            self.resolve(source="")
        with self.assertRaisesRegex(ValueError, "clock|frequency|kernel"):
            self.resolve(kernel_hz_min=0)

    def test_timing_preserves_default_cost_when_not_selected(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/g474_assembly/acquisition.toml")
        item = next(item for item in result.controllers if item.kind == "i2c")
        self.assertIsNone(item.options.timing)
        _, source = bindings.emit(result)
        self.assertIn("0x30420f13u", source)


if __name__ == "__main__":
    unittest.main()
