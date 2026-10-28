"""Exact finite SPI4 DMA selection and generated ownership contracts."""
import copy
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure, dma_topology, selected_contracts
from soc.stm32.stm32h743 import configure as provider
from tools.configure.providers.common import ConfigurationError


ROOT = Path(__file__).resolve().parents[5]
FIXTURES = ROOT / "tools/configure/assemblies"


class H743SpiDmaConfiguration(unittest.TestCase):
    def source(self, directory, name="spi-dma", transform=None):
        source = (FIXTURES / ("h743-" + name + ".toml")).read_text()
        source = source.replace(
            '../../../boards/stm32h743zi_hsi_reference',
            str(ROOT / "boards/stm32h743zi_hsi_reference"))
        if transform is not None:
            source = transform(source)
        path = Path(directory) / "assembly.toml"
        path.write_text(source)
        return path

    def reject(self, transform):
        with tempfile.TemporaryDirectory() as directory:
            source = self.source(directory, transform=transform)
            output = Path(directory) / "generated"
            with self.assertRaises(ConfigurationError):
                configure.configure(source, output)
            self.assertFalse(output.exists())

    def test_exact_ordered_provider_tuple(self):
        self.assertEqual("spi_dma", provider.mode_implementation("spi", "dma"))
        route = {"kind": "spi", "controller": "SPI4", "mode_dma": {
            "dma": ["DMA1:stream2:channel83", "DMA1:stream3:channel84"]}}
        provider.validate_dma(route, "dma", lambda value: value)
        invalid = (["DMA1:stream3:channel84", "DMA1:stream2:channel83"],
                   ["DMA1:stream2:channel84", "DMA1:stream3:channel83"],
                   ["DMA1:stream2:channel83"],
                   ["DMA1:stream0:channel83", "DMA1:stream3:channel84"],
                   ["DMA2:stream2:channel83", "DMA1:stream3:channel84"])
        for paths in invalid:
            with self.subTest(paths=paths):
                wrong = copy.deepcopy(route)
                wrong["mode_dma"]["dma"] = paths
                with self.assertRaises(ConfigurationError):
                    provider.validate_dma(wrong, "dma", lambda value: value)
        self.assertEqual("irq:DMA1_Stream2", provider.dma_irq(
            "DMA1:stream2:channel83"))
        self.assertEqual("irq:DMA1_Stream3", provider.dma_irq(
            "DMA1:stream3:channel84"))

    def test_immutable_ir_and_exact_shared_lane_selection(self):
        for fixture, uart in (("spi-dma", None),
                              ("spi-dma-uart-tx", (0, 46)),
                              ("spi-dma-uart-rx", (1, 45))):
            with self.subTest(fixture=fixture):
                result = configure.resolve_ir(FIXTURES / ("h743-" + fixture + ".toml"))
                lanes = {(x.lane, x.request) for x in result.dma_topology.lanes}
                self.assertEqual({(2, 83), (3, 84)} |
                                 ({uart} if uart is not None else set()), lanes)
                spi = next(x for x in result.controllers if x.kind == "spi")
                self.assertEqual("dma", spi.mode)
                self.assertEqual(5, spi.options.irq.priority)
                self.assertFalse(spi.options.irq.calls_os)
                with self.assertRaises(FrozenInstanceError):
                    spi.options.max_hz = 1
                self.assertFalse(result.physical_qualified)

    def test_static_emission_uses_dedicated_faces_and_three_irqs(self):
        with tempfile.TemporaryDirectory() as directory:
            source = self.source(directory)
            output = Path(directory) / "generated"
            configure.configure(source, output)
            body = (output / "bindings.c").read_text()
            self.assertIn("static nx_stm32h743_spi_dma_state_t s_nx_port_bus4;", body)
            self.assertIn("nx_stm32h743_spi_dma_endpoint_t", body)
            self.assertIn("&nx_stm32h743_spi_dma_endpoint_ops", body)
            self.assertIn("nx_stm32h743_spi_dma_initialize(&s_nx_port_bus4)", body)
            self.assertNotIn("nx_stm32h743_spi_initialize(", body)
            for name in ("DMA1_Stream2_IRQHandler", "DMA1_Stream3_IRQHandler",
                         "SPI4_IRQHandler"):
                self.assertIn("void " + name + "(void)", body)
            self.assertEqual(1, body.count("static uint32_t s_nx_dma_clocks_added;"))
            self.assertIn("(uintptr_t)0x24000000u", body)
            self.assertNotIn("(uintptr_t)0x30000000u", body)
            paths = json.loads((output / "input_paths.json").read_text())
            self.assertIn("soc/stm32/stm32h743/routes.json", paths)

    def test_traits_do_not_claim_cache_or_enhanced_interfaces(self):
        result = configure.resolve_ir(FIXTURES / "h743-spi-dma.toml")
        item = result.controllers[0]
        for endpoint in (None, result.devices[0]):
            record = selected_contracts.describe(item, endpoint)
            self.assertEqual(("bytes", 1, 1, "UINT16_MAX", (8,), ("DMA1",)),
                             (record.unit, record.unit_bytes,
                              record.buffer_alignment, record.max_transfer_units,
                              record.word_bits, record.dma_engines))
            self.assertEqual(("provider_domain", 0),
                             (record.dma_policy, record.cache_line_bytes))
            self.assertEqual("spi" if endpoint is None else "spi_device",
                             record.public_class)
        policy = selected_contracts.silicon_policy(result, item)
        self.assertEqual((0x2003,), policy.reviewed_revision_ids)
        self.assertEqual("reject", policy.unknown_revision)
        self.assertFalse(policy.physical_qualified)
        self.assertIsNone(selected_contracts.silicon_policy(result))

    def test_other_clocks_and_backends_are_rejected_only_for_dma(self):
        for profile in ("hsi32-fixed-cache-off", "hsi32-fixed-cache-on",
                        "hsi64-pll384-cache-off"):
            with self.subTest(profile=profile):
                self.reject(lambda text, p=profile: text.replace(
                    "hsi64-fixed-cache-off", p))
        for backend in ("freertos", "threadx"):
            with self.subTest(backend=backend):
                self.reject(lambda text, b=backend: text.replace(
                    'backend = "baremetal"', 'backend = "' + b + '"'))
        for profile in ("hsi32-fixed-cache-off", "hsi64-fixed-cache-on"):
            with self.subTest(polling_profile=profile), \
                    tempfile.TemporaryDirectory() as directory:
                source = self.source(directory, transform=lambda text: text.replace(
                    'mode = "dma"', 'mode = "short-poll"').replace(
                    "[spi.bus4.irq]\npriority = 5\ncalls_os = false\n", "").replace(
                    "hsi64-fixed-cache-off", profile))
                self.assertEqual("short-poll", configure.resolve_ir(
                    source).controllers[0].mode)

    def test_cache_on_profile_derives_complete_line_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            source = self.source(directory, transform=lambda text: text.replace(
                "hsi64-fixed-cache-off", "hsi64-fixed-cache-on"))
            result = configure.resolve_ir(source)
            item = result.controllers[0]
            for endpoint in (None, result.devices[0]):
                record = selected_contracts.describe(item, endpoint)
                self.assertEqual((32, 32, "domain_cache_line", "65504"),
                                 (record.buffer_alignment,
                                  record.cache_line_bytes, record.dma_policy,
                                  record.max_transfer_units))
            policy = selected_contracts.silicon_policy(result, item)
            self.assertIn("cacheon", policy.profile)
            output = Path(directory) / "generated"
            configure.configure(source, output)
            body = (output / "bindings.c").read_text()
            self.assertIn("nx_stm32h743_spi_dma_initialize", body)

    def test_irq_endpoint_and_unknown_options_reject(self):
        for priority in (0, 4, 6, 15):
            with self.subTest(priority=priority):
                self.reject(lambda text, p=priority: text.replace(
                    "priority = 5", "priority = " + str(p)))
        self.reject(lambda text: text.replace("calls_os = false", "calls_os = true"))
        for mode in (1, 2, 3):
            with self.subTest(mode=mode):
                self.reject(lambda text, m=mode: text.replace(
                    "mode = 0", "mode = " + str(m)))
        self.reject(lambda text: text.replace("max_hz = 1000000\n[spi.bus4.irq]",
                                             "max_hz = 1000000\nregion = 'sram1'\n[spi.bus4.irq]"))
        self.reject(lambda text: text.replace('driver = "spi-endpoint"',
                                             'driver = "spi-dma-transaction"'))

    def test_controller_and_endpoint_divider_bounds(self):
        for place in ("controller", "endpoint"):
            for rate in (250000, 253906):
                with self.subTest(place=place, rate=rate):
                    def change(text):
                        first, second = text.split("[spi_device.device4]", 1)
                        if place == "controller":
                            first = first.replace("max_hz = 1000000",
                                                  "max_hz = " + str(rate))
                        else:
                            second = second.replace("max_hz = 1000000",
                                                    "max_hz = " + str(rate))
                        return first + "[spi_device.device4]" + second
                    self.reject(change)
        with tempfile.TemporaryDirectory() as directory:
            source = self.source(directory, transform=lambda text: text.replace(
                "max_hz = 1000000", "max_hz = 253907"))
            self.assertEqual(253907, configure.resolve_ir(
                source).controllers[0].options.max_hz)

    def test_controller_and_physical_lane_conflicts_cannot_alias(self):
        self.reject(lambda text: text + '\n[spi.other]\nbinding="spi4"\n'
                    'mode="dma"\nmax_hz=1000000\n[spi.other.irq]\n'
                    'priority=5\ncalls_os=false\n')
        selected = [{"id": "first", "controller": "SPI4",
                     "dma": ["DMA1:stream2:channel83"]},
                    {"id": "second", "controller": "USART3",
                     "dma": ["DMA1:stream2:channel84"]}]
        with self.assertRaisesRegex(ConfigurationError, "physical lane conflict"):
            dma_topology.resolve_selected(selected, provider)

    def test_failed_reconfigure_withdraws_old_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            source = self.source(directory)
            output = Path(directory) / "generated"
            configure.configure(source, output)
            self.assertTrue((output / "bindings.c").is_file())
            source.write_text(source.read_text().replace(
                'mode = "dma"', 'mode = "dma-linked"'))
            with self.assertRaises(ConfigurationError):
                configure.configure(source, output)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
