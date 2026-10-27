"""Reviewed I2C IRQ/DMA authoring uses the actual finite-message providers."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


class I2cAsyncConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def authored(self, family, mode, irq=True, priority=5):
        folder = "stm32_assembly" if family == "stm32f407" else "gd32_assembly"
        board = "zg_board" if family == "stm32f407" else "software_board"
        clock = "hse8-pll168" if family == "stm32f407" else "hxtal25-pll200"
        content = (f'schema=2\nboard_package="{gate.ROOT}/tests/contracts/'
                   f'{folder}/{board}"\nbackend="baremetal"\nclock="{clock}"\n'
                   '[factory]\ncapabilities=true\n'
                   '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                   f'[i2c.bus]\nbinding="i2c0"\nmode="{mode}"\n'
                   'max_hz=100000\n')
        if irq:
            content += f'[i2c.bus.irq]\npriority={priority}\ncalls_os=false\n'
        content += ('[i2c_device.sensor]\ncontroller="bus"\n'
                    'driver="i2c-endpoint"\naddress=80\n')
        path = self.root / "assembly.toml"
        path.write_text(content)
        return path

    def test_async_modes_have_immutable_irq_policy_and_exact_directional_claims(self):
        for family, prefix, dma in (("stm32f407", "stm32", "DMA1"),
                                   ("gd32f470", "gd32", "DMA0")):
            for mode in ("irq-messages", "dma-messages"):
                with self.subTest(family=family, mode=mode):
                    path = self.authored(family, mode)
                    ir = gate.resolve_ir(path)
                    item = ir.controllers[0]
                    self.assertEqual(item.options.irq.priority, 5)
                    with self.assertRaises(FrozenInstanceError):
                        item.options.irq.priority = 6
                    claims = {claim.key for claim in ir.claims}
                    controller = "I2C1" if prefix == "stm32" else "I2C0"
                    self.assertIn(f"irq:{controller}_EV", claims)
                    self.assertIn(f"irq:{controller}_ER", claims)
                    self.assertEqual(f"dma:{dma}:stream0" in claims,
                                     mode == "dma-messages")
                    self.assertEqual(f"dma:{dma}:stream6" in claims,
                                     mode == "dma-messages")
                    output = self.root / (family + "-" + mode)
                    gate.configure(path, output)
                    source = (output / "bindings.c").read_text()
                    self.assertIn(f"nx_{prefix}_i2c_async_initialize", source)
                    self.assertIn("&nx_i2c_v1_async_ops,", source)
                    self.assertIn("&nx_i2c_v1_async_endpoint_ops,", source)
                    self.assertNotIn(f"&nx_{prefix}_i2c_async_ops,", source)
                    self.assertIn("nx_i2c_v1_irq(&s_nx_port_bus)", source)
                    self.assertIn("nx_i2c_port_stop(&s_nx_face_bus)", source)
                    self.assertIn("NX_FACTORY_CAP_IRQ", source)
                    self.assertNotIn(f"nx_{prefix}_i2c_initialize(", source)
                    if mode == "dma-messages":
                        self.assertIn("nx_i2c_v1_dma_irq", source)
                        self.assertIn(dma + "EN", source)
                        self.assertIn("NX_FACTORY_CAP_TX_DMA", source)
                        self.assertIn("NX_FACTORY_CAP_RX_DMA", source)
                    else:
                        self.assertNotIn("static const nx_i2c_v1_dma_t", source)
                        self.assertNotIn("NX_FACTORY_CAP_TX_DMA", source)

    def test_async_requires_explicit_irq_and_polling_rejects_irq_options(self):
        for family in ("stm32f407", "gd32f470"):
            for mode in ("irq-messages", "dma-messages"):
                with self.subTest(family=family, mode=mode):
                    with self.assertRaisesRegex(ValueError, "IRQ priority"):
                        gate.resolve_ir(self.authored(family, mode, irq=False))
            with self.assertRaisesRegex(ValueError, "Polling I2C"):
                gate.resolve_ir(self.authored(family, "poll-messages"))

    def test_derived_support_reads_the_actual_shared_production_table(self):
        from tools.maintenance import capabilities
        rows = capabilities.collect(gate.ROOT)
        for family in ("stm32f407", "gd32f470"):
            for mode in ("irq-messages", "dma-messages"):
                selected = next(row for row in rows
                                if row["family"] == family
                                and row["kind"] == "i2c"
                                and row["mode"] == mode)
                self.assertEqual(selected["exports"],
                                 ["drivers/i2c/i2c_v1_async/i2c_v1_async.c"])

    def test_priority_integer_range_is_validated_before_emission(self):
        for priority in ("true", -1, 16):
            with self.subTest(priority=priority):
                with self.assertRaisesRegex(ValueError, "IRQ priority"):
                    gate.resolve_ir(self.authored("stm32f407", "irq-messages",
                                                  priority=priority))


if __name__ == "__main__":
    unittest.main()
