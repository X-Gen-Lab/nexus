"""Exact retained RUN RTC is public typed assembly without backup policy."""
from pathlib import Path
import tempfile
import unittest
from tools.configure import bindings
from tools.configure import configure as gate
from tools.configure import factory


class U575RtcTests(unittest.TestCase):
    def setUp(self):
        self.assembly = gate.ROOT / 'tools/configure/assemblies/u575-rtc-run.toml'

    def test_exact_pinless_mode_has_static_face_and_early_readonly_preflight(self):
        result = gate.resolve_ir(self.assembly)
        item, = result.controllers
        self.assertEqual(('rtc', 'RTC0', 'retained-calendar-alarm'),
                         (item.kind, item.controller, item.mode))
        self.assertEqual((), item.pins)
        _, source = bindings.emit(result)
        self.assertIn('nx_stm32u575_rtc_prepare(&s_nx_port_calendar)', source)
        self.assertIn('nx_rtc_port_stop(', source)
        self.assertIn('nx_stm32u575_rtc_preflight()', source)
        self.assertLess(source.index('nx_stm32u575_rtc_preflight()'),
                        source.index('nx_stm32u575_system_start('))
        self.assertNotIn('RTC_IRQHandler', source)
        self.assertNotIn('RCC_BDCR_BDRST', source)
        header, lookup = factory.emit(result)
        self.assertIn('nx_factory_rtc', header)
        self.assertNotIn('initialize(', lookup)

    def test_unmaintained_set_mode_and_os_are_rejected(self):
        text = self.assembly.read_text().replace(
            '../../../boards/stm32u575zi_rtc_reference',
            (gate.ROOT / 'boards/stm32u575zi_rtc_reference').as_posix())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'assembly.toml'
            for old, new in [('retained-calendar-alarm', 'calendar-alarm'),
                             ('backend = "baremetal"', 'backend = "freertos"')]:
                path.write_text(text.replace(old, new))
                with self.subTest(new=new), self.assertRaises(ValueError):
                    gate.resolve_ir(path)


if __name__ == '__main__':
    unittest.main()
