"""Reviewed I2C IRQ/DMA authoring uses the actual finite-message providers."""
from tools.configure.tests.native_fixture import native_fixture
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
        folder = "stm32_assembly"
        board = "zg_board"
        clock = "hse8-pll168"
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
        for family, prefix, dma in (('stm32f4', 'stm32', 'DMA1'),):
            for mode in ("irq-messages", "dma-messages"):
                with self.subTest(family=family, mode=mode):
                    path = self.authored(family, mode)
                    ir = gate.resolve_ir(path, test_support=native_fixture(path))
                    item = ir.controllers[0]
                    self.assertEqual(item.options.irq.priority, 5)
                    with self.assertRaises(FrozenInstanceError):
                        item.options.irq.priority = 6
                    claims = {claim.key for claim in ir.claims}
                    controller = "I2C1"
                    self.assertIn(f"irq:{controller}_EV", claims)
                    self.assertIn(f"irq:{controller}_ER", claims)
                    self.assertEqual(f"dma:{dma}:stream0" in claims,
                                     mode == "dma-messages")
                    self.assertEqual(f"dma:{dma}:stream6" in claims,
                                     mode == "dma-messages")
                    output = self.root / (family + "-" + mode)
                    gate.configure(path, output, test_support=native_fixture(path))
                    source = (output / "bindings.c").read_text()
                    self.assertIn(f"nx_{prefix}_i2c_async_prepare", source)
                    self.assertIn("&nx_i2c_v1_async_ops,", source)
                    self.assertIn("&nx_i2c_v1_async_endpoint_ops,", source)
                    self.assertNotIn(f"&nx_{prefix}_i2c_async_ops,", source)
                    self.assertIn("nx_i2c_v1_irq(&s_nx_port_bus)", source)
                    self.assertIn(f"nx_{prefix}_i2c_async_close(&s_nx_port_bus,", source)
                    self.assertIn("NX_IO_CAP_IRQ", source)
                    self.assertNotIn(f"nx_{prefix}_i2c_initialize(", source)
                    if mode == "dma-messages":
                        self.assertIn("nx_i2c_v1_dma_irq", source)
                        self.assertIn(dma + "EN", source)
                        self.assertIn("NX_IO_CAP_TX_DMA", source)
                        self.assertIn("NX_IO_CAP_RX_DMA", source)
                    else:
                        self.assertNotIn("static const nx_i2c_v1_dma_t", source)
                        self.assertNotIn("NX_IO_CAP_TX_DMA", source)

    def test_async_requires_explicit_irq_and_polling_rejects_irq_options(self):
        for family in ('stm32f4',):
            for mode in ("irq-messages", "dma-messages"):
                with self.subTest(family=family, mode=mode):
                    with self.assertRaisesRegex(ValueError, "IRQ priority"):
                        gate.resolve_ir(self.authored(family, mode, irq=False), test_support=native_fixture(self.authored(family, mode, irq=False)))
            with self.assertRaisesRegex(ValueError, "Polling I2C"):
                gate.resolve_ir(self.authored(family, "poll-messages"), test_support=native_fixture(self.authored(family, "poll-messages")))

    def test_derived_support_reads_the_actual_shared_production_table(self):
        from tools.maintenance import capabilities
        rows = capabilities.collect(gate.ROOT)
        for family in ('stm32f4',):
            for mode in ("irq-messages", "dma-messages"):
                selected = next(row for row in rows
                                if row["family"] == family
                                and row["kind"] == "i2c"
                                and row["mode"] == mode)
                self.assertEqual(selected["exports"],
                                 ["soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.c"])

    def test_priority_integer_range_is_validated_before_emission(self):
        for priority in ("true", -1, 16):
            with self.subTest(priority=priority):
                with self.assertRaisesRegex(ValueError, "IRQ priority"):
                    gate.resolve_ir(self.authored('stm32f4', "irq-messages",
                                                  priority=priority), test_support=native_fixture(self.authored('stm32f4', "irq-messages",
                                                  priority=priority)))

    def test_ten_bit_endpoints_share_controller_with_seven_bit_peers(self):
        from tools.configure import selected_contracts
        for mode in ("poll-messages", "irq-messages", "dma-messages"):
            with self.subTest(mode=mode):
                path = self.authored('stm32f4', mode, irq=mode != "poll-messages")
                text = path.read_text().replace('max_hz=100000', 'max_hz=80000')
                text += ('[i2c_device.wide]\ncontroller="bus"\n'
                         'driver="i2c-endpoint"\naddress=80\naddress_bits=10\n')
                path.write_text(text)
                ir = gate.resolve_ir(path, test_support=native_fixture(path))
                self.assertEqual([device.options.address_bits for device in ir.devices], [7, 10])
                for device in ir.devices:
                    facts = selected_contracts.describe(ir.controllers[0], device)
                    self.assertEqual(facts.fields['address_bits'], device.options.address_bits)
                output = self.root / mode
                gate.configure(path, output, test_support=native_fixture(path))
                source = (output / 'bindings.c').read_text()
                self.assertIn('.address_bits = 7u', source)
                self.assertIn('.address_bits = 10u', source)
                path.write_text(text.replace('address=80\naddress_bits=10', 'address=1024\naddress_bits=10'))
                with self.assertRaisesRegex(ValueError, '10-bit I2C address'):
                    gate.resolve_ir(path, test_support=native_fixture(path))

    def test_standard_mode_ceiling_keeps_all_endpoint_widths_restart_safe(self):
        from tools.configure import selected_contracts
        for mode in ("poll-messages", "irq-messages", "dma-messages"):
            for rate, divisor in ((80000, 263), (88000, 239), (88001, 239),
                                  (100000, 239), (100001, 140), (400000, 36)):
                with self.subTest(mode=mode, rate=rate):
                    path = self.authored('stm32f4', mode,
                                         irq=mode != "poll-messages")
                    text = path.read_text().replace('max_hz=100000',
                                                    f'max_hz={rate}')
                    text += ('[i2c_device.wide]\ncontroller="bus"\n'
                             'driver="i2c-endpoint"\naddress=683\n'
                             'address_bits=10\n')
                    path.write_text(text)
                    ir = gate.resolve_ir(path, test_support=native_fixture(path))
                    controller = ir.controllers[0]
                    for device in (None, *ir.devices):
                        fields = selected_contracts.describe(controller,
                                                            device).fields
                        self.assertEqual(fields['configured_max_hz'], rate)
                        self.assertEqual(fields.get('nominal_hz'),
                                         42000000 // (divisor *
                                             (3 if rate > 100000 else 2)))
                    output = self.root / f'{mode}-{rate}'
                    gate.configure(path, output, test_support=native_fixture(path))
                    source = (output / 'nexus_factory.h').read_text()
                    self.assertIn('const nx_i2c_traits_t* nx_factory_i2c_traits(',
                                  source)
                    common = (gate.ROOT / 'io/include/nexus/io/traits.h').read_text()
                    self.assertIn('uint32_t nominal_hz;', common)

    def test_ten_bit_address_duplicates_reject(self):
        path = self.authored('stm32f4', 'irq-messages')
        text = path.read_text().replace('address=80', 'address=683\naddress_bits=10')
        path.write_text(text.replace('max_hz=100000', 'max_hz=80000') +
                        '[i2c_device.duplicate]\ncontroller="bus"\n'
                        'driver="i2c-endpoint"\naddress=683\naddress_bits=10\n')
        with self.assertRaisesRegex(ValueError, 'Duplicate I2C address'):
            gate.resolve_ir(path, test_support=native_fixture(path))

    def test_nominal_timing_uses_the_selected_apb_clock(self):
        from tools.configure import selected_contracts
        for pclk, source, apb_div in ((21000000, "pll", 4),
                                      (8000000, "input", 2)):
            for mode in ("poll-messages", "irq-messages", "dma-messages"):
                for rate, divisor in ((100000, 120 if pclk == 21000000 else 46),
                                       (400000, 18 if pclk == 21000000 else 7)):
                    with self.subTest(pclk=pclk, mode=mode, rate=rate):
                        path = self.authored('stm32f4', mode,
                                             irq=mode != "poll-messages")
                        text = path.read_text().replace('clock="hse8-pll168"',
                                                        'clock="custom"')
                        text = text.replace('max_hz=100000', f'max_hz={rate}')
                        text += ('[clock_parameters]\nsource="hsi"\n'
                                 'input_hz=16000000\npll_m=16\npll_n=336\n'
                                 'pll_p=4\npll_q=7\nahb_div=1\n'
                                 f'apb1_div={apb_div}\napb2_div=2\nadc_div=4\n'
                                 f'sysclk_source="{source}"\n'
                                 'supply_mv=3300\ncss=false\n')
                        path.write_text(text)
                        ir = gate.resolve_ir(path, test_support=native_fixture(path))
                        item = ir.controllers[0]
                        self.assertEqual(item.integration.clock_hz, pclk)
                        fields = selected_contracts.describe(item).fields
                        self.assertEqual(fields['nominal_hz'],
                                         pclk // (divisor *
                                             (3 if rate > 100000 else 2)))

    def test_nominal_projection_participates_in_generator_freshness(self):
        from tools.configure.dependencies import source_names
        self.assertIn("tools/configure/stm32f4/i2c_timing.py",
                      source_names("platform", "stm32f4"))

    def test_endpoint_width_is_an_exact_integer_and_native_stays_seven_bit(self):
        from tools.configure import authored
        width = authored.editor_schema()['properties']['i2c_device'][
            'additionalProperties']['properties']['address_bits']
        self.assertEqual(width, {'type': 'integer', 'enum': [7, 10], 'default': 7})
        path = self.authored('stm32f4', 'poll-messages', irq=False)
        text = path.read_text()
        for width in ('true', '8', '0'):
            with self.subTest(width=width):
                path.write_text(text + f'address_bits={width}\n')
                with self.assertRaisesRegex(ValueError, 'address_bits'):
                    gate.resolve_ir(path, test_support=native_fixture(path))
        from tools.configure.providers.common import EndpointPolicy
        with self.assertRaisesRegex(ValueError, 'reviewed.*10-bit'):
            EndpointPolicy().validate(
                {'driver': 'i2c-endpoint', 'address': 683, 'address_bits': 10},
                {'kind': 'i2c', 'mode': 'poll-messages'})


if __name__ == "__main__":
    unittest.main()
