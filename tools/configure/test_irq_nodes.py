"""Selected driver IRQs remain typed until the common static emitter."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure
from tools.configure.calls import Symbol
from tools.configure.data_provider import DataProvider
from tools.configure.providers import for_selection
from tools.configure.static_ir import IrqConnection


class DriverIrqNodesTests(unittest.TestCase):
    def assert_connections(self, construction):
        self.assertTrue(construction.irq_connections)
        self.assertTrue(construction.static_declarations)
        self.assertEqual(construction.definitions, ())
        for entry in construction.irq_connections:
            self.assertIsInstance(entry, IrqConnection)
            self.assertEqual(entry.exit_barrier, Symbol("nx_arch_dsb"))

    def test_domain_constructors_preserve_typed_connections(self):
        cases = (
            ("can_assembly/stm32-first.toml", {"can"}),
            ("stm32f4_crypto/f417zg.toml", {"cipher", "hash"}),
            ("dac_assembly/ve-dma.toml", {"dac"}),
            ("timer_assembly/stm32-ve.toml",
             {"timer_capture", "advanced_pwm"}),
            ("stm32f4_graphics/combined.toml", {"display", "image"}),
            ("f407_capture_audio/ve-capture-audio-uart.toml",
             {"capture", "audio"}),
            ("f407_capture_audio/zg-i2s3-duplex-blocks.toml",
             {"audio"}),
            ("stm32f4_sai/f446ze-master.toml", {"audio"}),
            ("stm32f4_sai/f479ni-sparse-slave-blocks.toml", {"audio"}))
        for path, kinds in cases:
            with self.subTest(path=path):
                result = configure.resolve_ir(configure.ROOT /
                                              "tests/contracts" / path)
                selected = [item for item in result.controllers if item.kind in kinds]
                self.assertTrue(selected)
                provider = for_selection(result)
                self.assertIsInstance(provider, DataProvider)
                for item in selected:
                    self.assert_connections(provider.constructor(item, result,
                        result.board_bindings, result.routes, result.devices))

    def test_pvd_events_keep_shared_dispatch_as_fixed_typed_call(self):
        fixture = configure.ROOT / "tests/contracts/multi_controller_assembly"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "assembly.toml"
            source = (fixture / "stm32-ve-pvd-baremetal.toml").read_text()
            source = source.replace('board_package = "stm32-ve-board"',
                                    f'board_package = "{fixture / "stm32-ve-board"}"')
            source = source.replace('mode = "pvd-level"', 'mode = "pvd-events"')
            path.write_text(source + '\nedge="both"\nevent_capacity=2\n'
                            'irq.priority=5\nirq.calls_os=false\n')
            result = configure.resolve_ir(path)
            provider = for_selection(result)
            self.assertIsInstance(provider, DataProvider)
            construction = provider.constructor(result.controllers[0], result,
                result.board_bindings, result.routes, result.devices)
            self.assert_connections(construction)
            dispatch, = construction.irq_connections[0].calls
            self.assertEqual(dispatch.function, "nx_stm32_exti_dispatch")
            self.assertEqual(dispatch.arguments[1:], (1, 65536))


if __name__ == "__main__":
    unittest.main()
