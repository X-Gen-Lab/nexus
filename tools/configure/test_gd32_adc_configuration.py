"""Exact GD470 ADC supply declarations precede cold hardware construction."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure


class Gd32AdcConfigurationTests(unittest.TestCase):
    fixtures = ("adc.toml", "adc-scan.toml", "adc-continuous.toml")

    def resolve(self, fixture, reference_mv):
        path = configure.ROOT / "tests/contracts/gd32_assembly" / fixture
        contents = path.read_text().replace(
            'board_package = "software_board"',
            'board_package = "' + str(path.parent / "software_board") + '"')
        self.assertIn("reference_mv = 3300", contents)
        with tempfile.TemporaryDirectory() as temporary:
            assembly = Path(temporary) / "assembly.toml"
            assembly.write_text(contents.replace(
                "reference_mv = 3300", f"reference_mv = {reference_mv}"))
            return configure.resolve_ir(assembly)

    def test_reference_outside_datasheet_range_is_rejected(self):
        for fixture in self.fixtures:
            for reference in (1, 1800, 2599, 3601):
                with self.subTest(fixture=fixture, reference=reference):
                    with self.assertRaises(ValueError):
                        self.resolve(fixture, reference)

    def test_datasheet_reference_boundaries_retain_the_selected_mode(self):
        for fixture in self.fixtures:
            for reference in (2600, 3300, 3600):
                with self.subTest(fixture=fixture, reference=reference):
                    result = self.resolve(fixture, reference)
                    item, = result.controllers
                    self.assertEqual(item.options.reference_mv, reference)
                    self.assertEqual(item.controller, "ADC0")


if __name__ == "__main__":
    unittest.main()
