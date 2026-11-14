"""Explicit static maintenance selection never authorizes a physical action."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure, factory
from tools.configure.tests.native_fixture import native_fixture


class FlashMaintenanceConfigurationTests(unittest.TestCase):
    def resolve(self, value):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        path = Path(temporary.name) / "assembly.toml"
        source = (configure.ROOT /
                  "tests/contracts/flash_factory/stm32.toml").read_text()
        if value is not None:
            source += f"maintenance = {value}\n"
        path.write_text(source)
        return configure.resolve_ir(path, test_support=native_fixture(path))

    def test_ordinary_flash_has_no_maintenance_face(self):
        result = self.resolve(None)
        item, = result.controllers
        self.assertFalse(item.options.maintenance)
        header, source = factory.emit(result)
        self.assertIn("nx_factory_flash_maintenance", header)
        self.assertNotIn("return &s_nx_flash_maintenance_", source)
        self.assertNotIn("nx_stm32_flash_maintenance_ops",
                         bindings.emit(result)[1])

    def test_explicit_selection_is_immutable_and_shares_existing_owner(self):
        result = self.resolve("true")
        item, = result.controllers
        self.assertTrue(item.options.maintenance)
        with self.assertRaises(FrozenInstanceError):
            item.options.maintenance = False
        header, source = factory.emit(result)
        self.assertIn("nx_factory_flash_maintenance", header)
        self.assertIn("return &s_nx_flash_maintenance_physical", source)
        emitted = bindings.emit(result)[1]
        from tools.configure.calls import Address
        from tools.configure.providers import for_selection
        construction = for_selection(result).constructor(
            item, result, result.board_bindings, result.routes, result.devices)
        face = next(declaration for declaration in construction.static_declarations
                    if declaration.name == "s_nx_flash_maintenance_physical")
        self.assertEqual(dict(face.initializer.fields), {
            "ops": Address("nx_stm32_flash_maintenance_ops"),
            "context": Address("s_nx_port_physical")})
        self.assertNotIn("nx_flash_maintenance_set_options(", emitted)
        self.assertNotIn("nx_flash_maintenance_otp_program(", emitted)

    def test_non_boolean_maintenance_is_rejected(self):
        for value in ('"yes"', "1", "[]", "{}"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.resolve(value)


if __name__ == "__main__":
    unittest.main()
