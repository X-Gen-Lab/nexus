"""Exact U575 pinless RUN timer uses the ordinary typed static factory."""
from pathlib import Path
import sys
import tempfile
import unittest

from tools.configure import bindings
from tools.configure import configure as gate
from tools.configure import dependencies
from tools.configure import factory


class U575LptimTests(unittest.TestCase):
    def setUp(self):
        self.assembly = gate.ROOT / 'tools/configure/assemblies/u575-lptim-run.toml'

    def test_executed_selected_provider_code_is_bound_before_emission(self):
        root = gate.ROOT
        provider = root / 'soc/stm32/stm32u575/configure'
        paths, _ = dependencies.snapshot(mode='platform', family='stm32u575')
        executed = set()

        def observe(frame, event, argument):
            if event != 'call':
                return
            path = Path(frame.f_code.co_filename)
            if (path.is_relative_to(provider) and
                    not path.name.startswith('test_')):
                executed.add(path.relative_to(root).as_posix())

        previous = sys.getprofile()
        sys.setprofile(observe)
        try:
            result = gate.resolve_ir(self.assembly)
            bindings.emit(result)
            factory.emit(result)
        finally:
            sys.setprofile(previous)
        self.assertTrue(executed)
        self.assertEqual(executed - paths.keys(), set(),
                         'Executed provider code escaped generator identity')

    def test_exact_irq_mode_resolves_and_constructs_static_public_face(self):
        result = gate.resolve_ir(self.assembly)
        item, = result.controllers
        self.assertEqual(('lptim', 'LPTIM2', 'irq-counter16-oneshot'),
                         (item.kind, item.controller, item.mode))
        self.assertEqual(1000000, item.options.tick_hz)
        self.assertEqual(5, item.options.irq.priority)
        self.assertEqual((), tuple(item['pins']))
        _, source = bindings.emit(result)
        self.assertIn('nx_stm32u575_lptim_initialize(', source)
        self.assertIn('nx_stm32u575_lptim_irq(', source)
        self.assertIn('LPTIM2_IRQHandler', source)
        self.assertIn('nx_lptim_port_stop(', source)
        header, lookup = factory.emit(result)
        self.assertIn('nx_factory_lptim', header)
        self.assertNotIn('initialize(', lookup)

    def test_unreviewed_rate_backend_and_mode_are_rejected(self):
        text = self.assembly.read_text()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'assembly.toml'
            text = text.replace('../../../boards/stm32u575zi_lptim_reference',
                                (gate.ROOT / 'boards/stm32u575zi_lptim_reference').as_posix())
            for old, new in [('tick_hz = 1000000', 'tick_hz = 32768'),
                             ('backend = "baremetal"', 'backend = "freertos"'),
                             ('irq-counter16-oneshot', 'counter16-oneshot'),
                             ('priority = 5', 'priority = 4')]:
                path.write_text(text.replace(old, new))
                with self.subTest(new=new), self.assertRaises(ValueError):
                    gate.resolve_ir(path)


if __name__ == '__main__':
    unittest.main()
