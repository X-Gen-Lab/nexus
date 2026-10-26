"""Enhanced endpoints remain typed through real authored static assembly."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import re
import tempfile
import unittest

from tools.configure import bindings, configure, enhanced_endpoints, factory


class EnhancedEndpointConfiguration(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def resolve(self, fixture, driver, device_kind, all_children=False,
                merge_spi_controllers=False):
        path = configure.ROOT / "tests/contracts" / fixture
        contents = path.read_text()
        match = re.search(r'(?m)^board_package\s*=\s*"([^"]+)"', contents)
        if match:
            board = (path.parent / match[1]).resolve()
            contents = contents[:match.start()] + (
                'board_package = "' + str(board) + '"') + contents[match.end():]
        lines = contents.splitlines()
        starts = [index for index, line in enumerate(lines)
                  if line.startswith("[" + device_kind + "_device.")]
        self.assertTrue(starts, "fixture must contain a real endpoint")
        for start in reversed(starts if all_children else starts[:1]):
            end = next((index for index in range(start + 1, len(lines))
                        if lines[index].startswith("[")), len(lines))
            section = [line for line in lines[start + 1:end]
                       if re.match(r"^driver\s*=", line) is None]
            lines[start + 1:end] = ['driver = "' + driver + '"', *section]
        contents = "\n".join(lines) + "\n"
        if merge_spi_controllers:
            contents = contents.replace('controller = "bus_spi_b"',
                                        'controller = "bus_spi_a"')
        assembly = self.root / "enhanced.toml"
        assembly.write_text(contents)
        return configure.resolve_ir(assembly)

    def test_native_spi_transaction_has_distinct_typed_options_and_factory(self):
        result = self.resolve("native_assembly/multi.toml",
                              "spi-transaction", "spi")
        child = next(item for item in result.devices
                     if item.driver == "spi-transaction")
        self.assertEqual(type(child.options).__name__,
                         "SpiTransactionEndpointOptions")
        with self.assertRaises(FrozenInstanceError):
            child.options.mode = 1
        header, source = factory.emit(result)
        self.assertIn("nx_factory_spi_transaction_device", header)
        self.assertIn("const nx_spi_transaction_endpoint_t*", header)
        self.assertNotIn("NX_SPI_DEVICE_ID_SENSOR_SPI_A", header)
        self.assertIn("NX_SPI_TRANSACTION_DEVICE_ID_SENSOR_SPI_A", header)
        generated_header, generated_source = bindings.emit(result)
        self.assertIn("nx_spi_transaction_endpoint_t* const nx_device_",
                      generated_header)
        self.assertIn("nx_native_spi_transaction_ops", generated_source)
        self.assertNotIn("(const nx_spi_endpoint_t*)", generated_source)

    def test_spi_dma_owner_is_a_normal_explicit_component(self):
        path = configure.ROOT / (
            "tests/contracts/io_evolution_assembly/f407-spi-dma-transaction.toml")
        contents = path.read_text().replace(
            'board_package = "../stm32_assembly/zg_board"',
            'board_package = "' + str(configure.ROOT /
                "tests/contracts/stm32_assembly/zg_board") + '"')
        contents = 'components = ["spi-dma-owner"]\n' + contents
        selected = self.root / "owner.toml"
        selected.write_text(contents)
        result = configure.resolve_ir(selected)
        self.assertIn("spi-dma-owner", result["components"])

    def test_dma_owner_binding_uses_exact_physical_provider_membership(self):
        for family, prefix in (("f407", "stm32"), ("gd470", "gd32")):
            with self.subTest(family=family):
                path = configure.ROOT / (
                    "tests/contracts/io_evolution_assembly/" + family +
                    "-spi-dma-transaction.toml")
                result = configure.resolve_ir(path)
                header, getters = factory.emit(result)
                self.assertIn("const nx_spi_dma_owner_binding_t*", header)
                self.assertIn("nx_factory_spi_dma_owner_binding(", header)
                self.assertIn(f"nx_{prefix}_spi_dma_owner_contains", getters)
                self.assertIn(f"nx_{prefix}_spi_dma_owner_attach_wake", getters)
                self.assertNotIn(".contains = nx_spi_dma_sequence_contains",
                                 getters)

    def test_native_async_assembly_uses_independent_real_model_contexts(self):
        path = configure.ROOT / (
            "tests/contracts/io_evolution_assembly/native-spi-dma-transaction.toml")
        result = configure.resolve_ir(path)
        self.assertEqual(len(result.controllers), 2)
        self.assertEqual(len(result.devices), 3)
        _, source = bindings.emit(result)
        self.assertIn("static nx_native_spi_dma_state_t s_nx_port_bus", source)
        self.assertIn("nx_native_spi_dma_endpoint_state_t", source)
        self.assertIn("nx_native_spi_dma_initialize(", source)
        self.assertIn("nx_native_spi_dma_sequence_backend", source)
        self.assertNotIn("nx_native_spi_port_configure_instance", source)
        self.assertNotIn("static uint8_t s_nx_model_", source)
        header, getters = factory.emit(result)
        self.assertIn("NX_SPI_DMA_TRANSACTION_ID_OTHER_BUS", header)
        self.assertIn("nx_native_spi_dma_owner_contains", getters)
        self.assertNotIn("NX_FACTORY_CAP_TX_DMA", source)

    def test_native_async_model_cannot_become_a_register_file_or_legacy_face(self):
        original = configure.ROOT / (
            "tests/contracts/io_evolution_assembly/native-spi-dma-transaction.toml")
        contents = original.read_text().replace(
            '"../native_assembly/multi_board"',
            '"' + str(configure.ROOT /
                      "tests/contracts/native_assembly/multi_board") + '"')
        for before, after in (("driver = \"spi-dma-transaction\"",
                              "driver = \"spi-dma-transaction\"\nmodel_bytes = 64"),
                             ("driver = \"spi-dma-transaction\"",
                              "driver = \"spi-endpoint\"")):
            path = self.root / "bad-native-async.toml"
            path.write_text(contents.replace(before, after))
            with self.subTest(after=after), self.assertRaisesRegex(
                    ValueError, "model|async|enhanced|transaction"):
                configure.resolve_ir(path)

    def test_reviewed_f407_and_f470_polling_routes_emit_enhanced_faces(self):
        for fixture, operations in (
                ("stm32_assembly/zg-spi.toml", "nx_stm32_spi_transaction_ops"),
                ("gd32_assembly/spi.toml", "nx_gd32_spi_transaction_ops")):
            with self.subTest(fixture=fixture):
                result = self.resolve(fixture, "spi-transaction", "spi")
                header, source = bindings.emit(result)
                self.assertIn("nx_spi_transaction_endpoint_t", header)
                self.assertIn(operations, source)

    def test_transaction_face_requires_exact_family_not_a_vendor_prefix(self):
        for family, prefix, fixture in (
                ("stm32f407", "stm32", "f407-spi-transaction.toml"),
                ("gd32f470", "gd32", "gd470-spi-transaction.toml")):
            with self.subTest(family=family):
                result = configure.resolve_ir(configure.ROOT /
                    "tests/contracts/io_evolution_assembly" / fixture)
                endpoint = result.devices[0]
                with self.assertRaises(ValueError):
                    enhanced_endpoints.face(endpoint, "state", "unused", prefix)
                source = enhanced_endpoints.face(endpoint, "state", "unused",
                                                 family)
                self.assertIn(f"&nx_{prefix}_spi_transaction_ops, &state",
                              "\n".join(source))

    def test_polling_transaction_driver_cannot_silently_use_dma(self):
        with self.assertRaisesRegex(ValueError, "transaction.*poll|poll.*transaction"):
            self.resolve("stm32_assembly/zg-dma.toml", "spi-transaction", "spi")

    def test_unimplemented_g474_transaction_provider_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "transaction.*provider|provider.*transaction"):
            self.resolve("g474_assembly/spi.toml", "spi-transaction", "spi")

    def test_long_i2c_is_distinct_typed_endpoint_on_three_v2_families(self):
        for fixture in ("g474_assembly/acquisition.toml",
                        "h743_assembly/peripherals.toml",
                        "u575_assembly/peripherals.toml"):
            with self.subTest(fixture=fixture):
                result = self.resolve(fixture, "i2c-long-endpoint", "i2c")
                child = next(item for item in result.devices
                             if item.driver == "i2c-long-endpoint")
                self.assertEqual(type(child.options).__name__,
                                 "I2cLongEndpointOptions")
                header, _ = factory.emit(result)
                self.assertIn("nx_factory_i2c_long_device", header)
                self.assertIn("const nx_i2c_long_endpoint_t*", header)
                _, source = bindings.emit(result)
                self.assertIn("nx_i2c_v2_long_endpoint_ops", source)
                self.assertNotIn("(const nx_i2c_endpoint_t*)", source)

    def test_long_i2c_rejects_v1_even_when_pins_are_reviewed(self):
        for fixture in ("stm32_assembly/zg-i2c.toml",
                        "gd32_assembly/i2c.toml"):
            with self.subTest(fixture=fixture):
                with self.assertRaisesRegex(ValueError,
                                            "(?i)long.*provider|provider.*long"):
                    self.resolve(fixture, "i2c-long-endpoint", "i2c")

    def test_default_endpoints_retain_legacy_types(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/native_assembly/multi.toml")
        self.assertTrue(all(type(item.options).__name__ in
                            {"SpiEndpointOptions", "I2cEndpointOptions"}
                            for item in result.devices))
        header, _ = factory.emit(result)
        self.assertNotIn("nx_factory_spi_transaction_device(", header)
        self.assertNotIn("nx_factory_i2c_long_device(", header)

    def test_dma_transaction_has_distinct_port_and_endpoint_contexts(self):
        for fixture, prefix in (("stm32_assembly/zg-dma.toml", "stm32"),
                                ("gd32_assembly/dma.toml", "gd32")):
            with self.subTest(fixture=fixture):
                result = self.resolve(fixture, "spi-dma-transaction", "spi")
                port = next(item for item in result.controllers
                            if item.kind == "spi")
                self.assertEqual(port.interface_kind, "spi_dma_transaction")
                child = next(item for item in result.devices
                             if item.kind == "spi")
                self.assertEqual(type(child.options).__name__,
                                 "SpiDMATransactionEndpointOptions")
                header, _ = factory.emit(result)
                self.assertIn("nx_factory_spi_dma_transaction(", header)
                self.assertIn("const nx_spi_dma_transaction_port_t*", header)
                self.assertIn("nx_factory_spi_dma_transaction_device", header)
                _, source = bindings.emit(result)
                self.assertIn("nx_spi_dma_sequence_state_t", source)
                self.assertIn(f"nx_{prefix}_spi_dma_state_t", source)
                self.assertIn(f"nx_{prefix}_spi_dma_sequence_backend", source)
                self.assertIn("nx_spi_dma_sequence_initialize", source)
                self.assertNotIn("nx_spi_port_stop(nx_binding_", source)
                self.assertIn("nx_spi_dma_transaction_stop(", source)
                self.assertNotIn("nx_spi_dma_transaction_port_stop(", source)

    def test_dma_transaction_driver_cannot_upgrade_polling_silently(self):
        with self.assertRaisesRegex(ValueError, "DMA.*mode|mode.*DMA"):
            self.resolve("stm32_assembly/zg-spi.toml",
                         "spi-dma-transaction", "spi")

    def test_dma_sequence_cannot_expose_a_legacy_endpoint_on_same_controller(self):
        with self.assertRaisesRegex(ValueError, "mixed|every|all"):
            self.resolve("gd32_assembly/dma.toml",
                         "spi-dma-transaction", "spi",
                         merge_spi_controllers=True)

    def test_enhanced_traits_report_words_and_per_message_limits(self):
        for fixture, driver, kind, expected in (
                ("stm32_assembly/zg-spi.toml", "spi-transaction", "spi",
                 "nx_factory_spi_transaction_device_traits_t"),
                ("stm32_assembly/zg-dma.toml", "spi-dma-transaction", "spi",
                 "nx_factory_spi_dma_transaction_device_traits_t"),
                ("g474_assembly/acquisition.toml", "i2c-long-endpoint", "i2c",
                 "nx_factory_i2c_long_device_traits_t")):
            with self.subTest(driver=driver):
                result = self.resolve(fixture, driver, kind)
                data = result.to_dict()
                data["factory"] = {"capabilities": True}
                header, source = factory.emit(data)
                self.assertIn(expected, header)
                if kind == "spi":
                    self.assertIn("supported_word_bits", header)
                    self.assertIn("max_segment_words", header)
                    self.assertIn("NX_SPI_MAX_SEGMENTS", source)
                else:
                    self.assertIn("max_message_bytes", header)
                    self.assertIn("NX_I2C_LONG_MAX_MESSAGE_BYTES", source)

    def test_native_register_model_traits_do_not_claim_sixteen_bit_words(self):
        result = self.resolve("native_assembly/multi.toml",
                              "spi-transaction", "spi")
        _, source = factory.emit(result)
        record = re.search(
            r"s_nx_traits_spi_transaction_device_sensor_spi_a = \{\n([^\n]+)",
            source)[1]
        self.assertIn("<< 8U", record)
        self.assertNotIn("<< 16U", record)
        path = self.root / "enhanced.toml"
        path.write_text(path.read_text().replace("model_bytes = 64", "", 1))
        _, source = factory.emit(configure.resolve_ir(path))
        record = re.search(
            r"s_nx_traits_spi_transaction_device_sensor_spi_a = \{\n([^\n]+)",
            source)[1]
        self.assertIn("<< 16U", record)


if __name__ == "__main__":
    unittest.main()
