"""Authored logical signals reuse the generated authorized GPIO face."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools.configure import bindings, configure, factory, gpio_electrical


class GpioSignalFactoryTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.text = (configure.ROOT /
                     "tests/support/native/assemblies/native-gpio.toml").read_text()

    def resolve(self, option=""):
        path = self.directory / "assembly.toml"
        path.write_text(self.text + option, encoding="utf-8")
        return configure.resolve_ir(path, test_support=True)

    def test_polarity_is_immutable_and_emits_only_a_readonly_binding(self):
        for polarity in (False, True):
            with self.subTest(polarity=polarity):
                result = self.resolve(f"active_high = {str(polarity).lower()}\n")
                selected, = result.controllers
                self.assertIs(selected.options.active_high, polarity)
                with self.assertRaises(FrozenInstanceError):
                    selected.options.active_high = not polarity
                source = bindings.emit(result)[1]
                self.assertIn("static const nx_gpio_signal_t s_nx_signal_led0", source)
                self.assertIn("&s_nx_face_led0, 1u, " +
                              str(polarity).lower(), source)
                header, lookup = factory.emit(result)
                self.assertIn("nx_factory_gpio_signal(nx_gpio_id_t id)", header)
                self.assertIn("return &s_nx_signal_led0;", lookup)
                self.assertNotIn("nx_gpio_signal_write", source)

    def test_undeclared_gpio_has_no_signal_object_or_inferred_polarity(self):
        result = self.resolve()
        self.assertIsNone(result.controllers[0].options.active_high)
        self.assertNotIn("s_nx_signal_", bindings.emit(result)[1])
        self.assertNotIn("return &s_nx_signal_", factory.emit(result)[1])

    def test_invalid_polarity_rejects_before_bundle_publication(self):
        for value in ('1', '"true"'):
            with self.subTest(value=value):
                with self.assertRaisesRegex(configure.ConfigurationError,
                                            "active_high.*boolean"):
                    self.resolve("active_high = " + value + "\n")

    def test_signal_rejects_ambiguous_views_and_state_switching_modes(self):
        selection = {"kind": "gpio", "mode": "output", "mask": 1,
                     "active_high": True}
        for change in ({"mask": 0}, {"mask": 3}, {"mode": "named-states"}):
            with self.subTest(change=change):
                with self.assertRaisesRegex(ValueError, "one authorized"):
                    gpio_electrical.require_selection({**selection, **change}, {})

    def test_electrical_initial_uses_the_public_32bit_port_mask(self):
        mask = 1 << 31
        binding = {"id": "wide-port", "initial": mask,
                   "electrical": {"mode": "output", "initial": mask}}
        route = {"kind": "gpio", "modes": ["output"]}
        result = gpio_electrical.validate_binding(binding, route, mask)
        self.assertEqual(result.initial, mask)
        for invalid in (1 << 32, mask | 1):
            binding["electrical"]["initial"] = invalid
            with self.subTest(initial=invalid), self.assertRaises(ValueError):
                gpio_electrical.validate_binding(binding, route, mask)

    def test_nominal_signal_getter_compiles_and_rejects_uart_identity(self):
        result = self.resolve("active_high = false\n")
        (self.directory / "nexus_factory.h").write_text(factory.emit(result)[0])
        (self.directory / "nexus_factory_strong.h").write_text(
            factory.emit_strong(result))
        for compiler, standard, extension in (("cc", "c11", "c"),
                                               ("c++", "c++17", "cpp")):
            for identity, accepted in (("NX_GPIO_REF_LED0", True),
                                       ("NX_UART_REF_COUNT", False)):
                with self.subTest(compiler=compiler, identity=identity):
                    path = self.directory / ("consumer." + extension)
                    path.write_text('#include "nexus_factory_strong.h"\n'
                                    'const nx_gpio_signal_t* lookup(void) {\n'
                                    '    return nx_factory_gpio_signal_ref(' +
                                    identity + ');\n}\n')
                    command = [compiler, "-std=" + standard, "-Wall", "-Wextra",
                               "-Werror", "-pedantic", "-I" + str(self.directory),
                               "-I" + str(configure.ROOT / "io/include"),
                               "-I" + str(configure.ROOT / "core/include"),
                               "-c", str(path), "-o",
                               str(self.directory / "consumer.o")]
                    execution = subprocess.run(command, capture_output=True,
                                               text=True, check=False)
                    self.assertEqual(execution.returncode == 0, accepted,
                                     execution.stderr)


if __name__ == "__main__":
    unittest.main()
