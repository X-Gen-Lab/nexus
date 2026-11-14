"""Optional packet PEC bindings preserve the selected I2C endpoint identity."""
from dataclasses import replace
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools.configure import configure


class SMBusFactoryTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        fixture = configure.ROOT / "tests/contracts/stm32_assembly"
        self.text = (fixture / "zg-i2c-irq.toml").read_text().replace(
            'board_package = "zg_board"',
            f'board_package = "{fixture / "zg_board"}"')

    def resolve(self, selected=True, mode="irq-messages", extra=""):
        text = self.text.replace('schema = 2', 'schema = 2\n' +
            ('components = ["smbus"]' if selected else 'components = []'))
        text = text.replace('mode = "irq-messages"', f'mode = "{mode}"')
        if mode == "poll-messages":
            text = text.replace('[i2c.bus.irq]\npriority = 5\n'
                                'calls_os = false\n', '')
        path = self.directory / "assembly.toml"
        path.write_text(text + extra)
        return configure.resolve_ir(path, test_support=True)

    def emit(self, result):
        output = self.directory / "bundle"
        output.mkdir(exist_ok=True)
        configure.generate(result, output)
        return output

    def test_opt_in_emits_one_readonly_binding_from_existing_faces(self):
        for mode in ("irq-messages", "dma-messages"):
            with self.subTest(mode=mode):
                output = self.emit(self.resolve(mode=mode))
                companion = (output / "nexus_smbus_factory.h").read_text()
                source = (output / "bindings.c").read_text()
                self.assertIn("nx_factory_smbus_binding(nx_i2c_device_id_t id)",
                              companion)
                self.assertIn("nx_factory_smbus_binding_ref(", companion)
                self.assertIn("static const nx_smbus_binding_t "
                              "s_nx_smbus_binding_sensor", source)
                self.assertIn("&s_nx_device_sensor, &s_nx_face_bus, 80u",
                              source)
                self.assertIn("return &s_nx_smbus_binding_sensor;", source)
                self.assertNotIn("components/smbus.h",
                                (output / "nexus_factory.h").read_text())
                self.assertNotIn("components/smbus.h",
                                (output / "nexus_factory_strong.h").read_text())

    def test_unselected_component_emits_no_binding_or_header_dependency(self):
        output = self.emit(self.resolve(selected=False))
        self.assertFalse((output / "nexus_smbus_factory.h").exists())
        self.assertNotIn("smbus", (output / "bindings.c").read_text())

    def test_polling_and_ten_bit_endpoints_never_gain_a_pec_binding(self):
        for mode, extra in (("poll-messages", ""),
                ("irq-messages", "\n[i2c_device.wide]\ncontroller=\"bus\"\n"
                 'driver="i2c-endpoint"\naddress=683\naddress_bits=10\n')):
            with self.subTest(mode=mode):
                result = self.resolve(mode=mode, extra=extra)
                source = (self.emit(result) / "bindings.c").read_text()
                self.assertNotIn("s_nx_smbus_binding_wide", source)
                if mode == "poll-messages":
                    self.assertNotIn("static const nx_smbus_binding_t", source)
                else:
                    self.assertIn("s_nx_smbus_binding_sensor", source)
                self.assertIn("nx_factory_smbus_binding", source)

    def test_reserved_seven_bit_addresses_cannot_become_packet_bindings(self):
        result = self.resolve()
        for address in (0, 7, 0x78, 0x7F):
            with self.subTest(address=address):
                endpoint = replace(result.devices[0], options=replace(
                    result.devices[0].options, address=address))
                selected = replace(result, devices=(endpoint,))
                self.assertNotIn("static const nx_smbus_binding_t",
                                 (self.emit(selected) / "bindings.c").read_text())

    def test_strong_companion_compiles_and_rejects_controller_identity(self):
        output = self.emit(self.resolve())
        for compiler, standard, extension in (("cc", "c11", "c"),
                                              ("c++", "c++17", "cpp")):
            for identity, accepted in (("NX_I2C_DEVICE_REF_SENSOR", True),
                                        ("NX_I2C_REF_BUS", False)):
                with self.subTest(compiler=compiler, identity=identity):
                    source = self.directory / ("consumer." + extension)
                    source.write_text('#include "nexus_smbus_factory.h"\n'
                        'const nx_smbus_binding_t* lookup(void) {\n'
                        '    return nx_factory_smbus_binding_ref(' + identity +
                        ');\n}\n')
                    command = [compiler, "-std=" + standard, "-Wall", "-Wextra",
                        "-Werror", "-pedantic", "-I" + str(output)]
                    for directory in ("core/include", "io/include",
                                      "components/smbus/include"):
                        command.append("-I" + str(configure.ROOT / directory))
                    command += ["-c", str(source), "-o",
                                str(self.directory / "consumer.o")]
                    execution = subprocess.run(command, capture_output=True,
                                               text=True, check=False)
                    self.assertEqual(execution.returncode == 0, accepted,
                                     execution.stderr)

    def test_source_sdk_retains_the_component_and_exact_public_consumer(self):
        def load(name):
            path = configure.ROOT / "cmake/package" / (name + ".py")
            specification = importlib.util.spec_from_file_location(name, path)
            module = importlib.util.module_from_spec(specification)
            specification.loader.exec_module(module)
            return module
        package_source_sdk, verify_consumers = (load(name) for name in
            ("package_source_sdk", "verify_consumers"))
        from tools.maintenance.consumer_profiles import SERVICE_CONSUMERS
        selected = [item for item in SERVICE_CONSUMERS
                    if item.scenario == "smbus"]
        self.assertEqual(len(selected), 1)
        specification, = selected
        self.assertTrue(specification.arm)
        sources = verify_consumers.service_sources(configure.ROOT, specification)
        names = {"components/smbus/CMakeLists.txt",
                 "components/smbus/include/nexus/components/smbus.h",
                 "components/smbus/src/smbus.c", "components/smbus/README.md",
                 "cmake/package/verification/smbus/f407.toml"}
        names.update(path.relative_to(configure.ROOT).as_posix()
                     for path in sources.values())
        self.assertTrue(names.issubset(package_source_sdk.REQUIRED_FILES))
        for path in sources.values():
            self.assertTrue(path.is_file(), path)
        self.assertIn("nx_factory_smbus_binding",
                      verify_consumers.service_symbols(specification))


if __name__ == "__main__":
    unittest.main()
