"""One SDIO owner statically selects its finite IRQ or DMA execution face."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure, factory
from tools.configure.providers import for_selection
from tools.configure import test_f407_block_configuration as fixtures
from tools.configure.tests.native_fixture import native_fixture


class SdioAsyncConfigurationTests(unittest.TestCase):
    def resolve(self, mode, *, extra="", protocol=None):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        path = fixtures.F407BlockConfigurationTests().fixture(directory)
        path.write_text(path.read_text().replace('mode="sdio-poll"',
                                                 f'mode="{mode}"') + extra)
        if protocol is not None:
            path.write_text(path.read_text().replace(
                "bus_width=4", f'protocol="{protocol}"\nbus_width=4'))
        return configure.resolve_ir(path, test_support=native_fixture(path))

    def test_irq_has_fixed_vector_and_existing_block_identity(self):
        result = self.resolve("sdio-irq", extra='[block.sd0.irq]\npriority=6\n')
        item, = result.controllers
        self.assertEqual(6, item.options.irq.priority)
        self.assertFalse(item.dma)
        self.assertIn("irq:SDIO", {value.key for value in result.claims})
        emitted = bindings.emit(result)[1]
        self.assertIn("NX_STM32_SDIO_IRQ", emitted)
        self.assertIn("nx_stm32_sdio_irq(&s_nx_port_sd0)", emitted)
        self.assertIn("nx_stm32_sdio_async_ops", emitted)
        header, source = factory.emit(result)
        self.assertIn("nx_factory_block_async(nx_block_id_t id)", header)
        self.assertIn("return &s_nx_block_async_sd0", source)

    def test_dma_has_one_serial_bidirectional_stream_and_reachable_regions(self):
        result = self.resolve("sdio-dma", extra='[block.sd0.irq]\npriority=6\n')
        item, = result.controllers
        self.assertTrue(item.dma)
        claims = {value.key for value in result.claims}
        self.assertIn("dma:DMA2:stream3", claims)
        self.assertIn("irq:DMA2_Stream3", claims)
        emitted = bindings.emit(result)[1]
        self.assertIn("NX_STM32_SDIO_DMA", emitted)
        self.assertIn("nx_stm32_sdio_dma_irq(&s_nx_port_sd0)", emitted)
        self.assertIn(".dma_regions = s_nx_dma_regions_sd0", emitted)
        producer = for_selection(result)
        built = producer.constructor(item, result, None, None, result.devices)
        regions = next(declaration for declaration in built.static_declarations
                       if declaration.type_name == "nx_dma_memory_region_t")
        origins = {dict(record.fields)["begin"] for record in regions.initializer.values}
        self.assertEqual(origins, {0x20000000, 0x08000000})

    def test_async_requires_explicit_irq_priority(self):
        for mode in ("sdio-irq", "sdio-dma"):
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                self.resolve(mode)

    def test_selected_dma_lane_controls_readonly_config_and_thin_irq(self):
        result = self.resolve("sdio-dma", extra='[block.sd0.irq]\npriority=6\n')
        producer = for_selection(result)
        selected, = result.controllers
        self.assertEqual(("DMA2:stream3:channel4",), selected.dma)
        built = producer.constructor(selected, result, None, None, result.devices)
        route = next(node for node in built.static_declarations
                     if node.type_name == "nx_stm32_dma_route_t")
        self.assertTrue(route.const)
        fields = dict(route.initializer.fields)
        self.assertEqual((3, 4), (fields["lane"], fields["channel"]))
        self.assertEqual("DMA2_Stream3", fields["stream"].name)
        self.assertEqual("DMA2_Stream3_IRQn", fields["irq"].name)
        config = next(node for node in built.static_declarations
                      if node.type_name == "nx_stm32_sdio_config_t")
        self.assertTrue(config.const)
        self.assertEqual(route.name, dict(config.initializer.fields)["dma"].name)
        handlers = {node.name for node in built.irq_connections}
        self.assertIn("DMA2_Stream3_IRQHandler", handlers)
        self.assertNotIn("DMA2_Stream6_IRQHandler", handlers)
        # Lane6 is a separate real-C compiled contract, not an admitted catalog
        # route. The old test changed only item.dma and left integration stale;
        # nexus_stm32_sdio_stream6_test owns the actual alternate-lane behavior.
        polling = self.resolve("sdio-poll")
        self.assertIn(".dma = NULL", bindings.emit(polling)[1])
        self.assertIn("#define NEXUS_STM32_SDIO_DMA_LANE 0u",
                      for_selection(polling).configuration_header(polling))
        self.assertIn("#define NEXUS_STM32_SDIO_DMA_LANE 3u",
                      producer.configuration_header(result))

    def test_mmc_protocol_is_explicit_and_never_an_sd_fallback(self):
        result = self.resolve("sdio-irq", protocol="mmc",
                              extra='[block.sd0.irq]\npriority=6\n')
        item, = result.controllers
        self.assertEqual("mmc", item.options.protocol)
        self.assertIn(".protocol = NX_STM32_SDIO_MMC",
                      bindings.emit(result)[1])
        with self.assertRaises(ValueError):
            self.resolve("sdio-poll", protocol="automatic")

    def test_sdio_functions_share_the_host_and_have_their_own_typed_face(self):
        result = self.resolve("sdio-irq", protocol="sdio",
                              extra='[block.sd0.irq]\npriority=6\n')
        item, = result.controllers
        self.assertEqual("sdio", item.options.protocol)
        emitted = bindings.emit(result)[1]
        self.assertIn("NX_STM32_SDIO_IO", emitted)
        self.assertIn(".io = &s_nx_sdio_functions_sd0", emitted)
        self.assertIn(".ops = &nx_stm32_sdio_io_ops, .context = &s_nx_port_sd0", emitted)
        self.assertNotIn("nx_sdio_io_enable(", emitted)
        header, source = factory.emit(result)
        self.assertIn("nx_factory_sdio_io(nx_block_id_t id)", header)
        self.assertIn("return &s_nx_sdio_io_sd0", source)


if __name__ == "__main__":
    unittest.main()
