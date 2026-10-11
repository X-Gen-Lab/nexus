"""Optional factory metadata and nominal IDs exercise actual generated C."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure import factory


class OptionalFactoryTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.assembly = self.root / "assembly.toml"
        self.contents = (gate.ROOT /
                         "tools/configure/assemblies/native-gpio.toml").read_text()
        self.output = self.root / "bundle"

    def generate(self, policy=""):
        self.assembly.write_text(self.contents + policy)
        return gate.configure(self.assembly, self.output)

    def compile(self, compiler, standard, expression):
        suffix = "c" if compiler == "cc" else "cpp"
        source = self.root / ("consumer." + suffix)
        source.write_text('#include "nexus_factory_strong.h"\n'
                          'const nx_gpio_port_t* lookup(void) {\n'
                          '    return nx_factory_gpio_ref(' + expression
                          + ');\n}\n')
        return subprocess.run(
            [compiler, "-std=" + standard, "-Wall", "-Wextra", "-Werror",
             "-pedantic", "-I" + str(self.output),
             "-I" + str(gate.ROOT / "io/include"),
             "-I" + str(gate.ROOT / "core/include"), "-c", str(source),
             "-o", str(self.root / "consumer.o")],
            capture_output=True, text=True, check=False)

    def test_nominal_ids_compile_in_c11_and_cpp17_and_reject_other_classes(self):
        self.generate()
        for compiler, standard in (("cc", "c11"), ("c++", "c++17")):
            with self.subTest(compiler=compiler):
                valid = self.compile(compiler, standard, "NX_GPIO_REF_LED0")
                self.assertEqual(valid.returncode, 0, valid.stderr)
                for expression in ("NX_UART_REF_COUNT", "NX_GPIO_ID_LED0"):
                    invalid = self.compile(compiler, standard, expression)
                    self.assertNotEqual(invalid.returncode, 0)
                    self.assertNotIn("No such file", invalid.stderr)

    def test_optional_header_adds_no_global_objects(self):
        self.generate()
        header = (self.output / "nexus_factory_strong.h").read_text()
        self.assertIn("static inline", header)
        self.assertNotIn("static const", header)
        self.assertNotIn("malloc", header)

    def test_default_factory_has_no_capability_descriptors(self):
        result = self.generate()
        self.assertFalse(result["factory"]["capabilities"])
        self.assertNotIn("nx_factory_io_capability_t",
                         (self.output / "nexus_factory.h").read_text())

    def test_capability_selection_is_explicit_and_immutable(self):
        self.generate("\n[factory]\ncapabilities = true\n")
        ir = gate.resolve_ir(self.assembly)
        self.assertTrue(ir.factory.capabilities)
        with self.assertRaises(FrozenInstanceError):
            ir.factory.capabilities = False
        source = (self.output / "bindings.c").read_text()
        header = (self.output / "nexus_factory.h").read_text()
        self.assertIn("nx_factory_io_capability_t", header)
        self.assertIn("nx_factory_gpio_capability", header)
        self.assertIn("s_nx_capability_gpio_led0", source)
        self.assertNotIn("s_nx_capability_uart_", source)
        self.assertIn("NX_FACTORY_CAP_MODEL", source)

    def test_unknown_factory_policy_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "unknown"):
            self.generate("\n[factory]\nregistry = true\n")
        self.assertFalse(self.output.exists())

    def test_capabilities_requires_boolean(self):
        with self.assertRaisesRegex(ValueError, "boolean"):
            self.generate("\n[factory]\ncapabilities = 1\n")
        self.assertFalse(self.output.exists())

    def test_mcu_capabilities_execute_compiled_getters_and_exact_bounds(self):
        for fixture in ("stm32_assembly/zg-blocks.toml",
                        "gd32_assembly/blocks.toml"):
            with self.subTest(fixture=fixture):
                path = gate.ROOT / "tests/contracts" / fixture
                normalized = gate.load_assembly(path)
                normalized["board_package"] = str(
                    path.parent / normalized["board_package"])
                normalized["factory"] = {"capabilities": True}
                assembly = self.root / "mcu.json"
                assembly.write_text(json.dumps(normalized))
                result = gate.resolve_ir(assembly)
                header, getters = factory.emit(result)
                (self.root / "nexus_factory.h").write_text(header)
                aliases = []
                checks = []
                for class_name, shape, instances in factory.classes(result):
                    kind = class_name.removesuffix("_device")
                    for instance in instances:
                        name = factory.symbol(instance["id"])
                        alias = ("nx_device_" if shape == "endpoint" else
                                 "nx_binding_") + name
                        aliases.append(f"const nx_{kind}_{shape}_t* const "
                                       f"{alias} = NULL;")
                    checks.append(f"assert(nx_factory_{class_name}_capability("
                                  f"NX_{class_name.upper()}_ID_COUNT) == NULL);")
                for item in result.controllers:
                    identity = f"NX_{item.kind.upper()}_ID_{factory.symbol(item.id).upper()}"
                    function = f"nx_factory_{item.kind}_capability({identity})"
                    if item.kind == "adc":
                        checks += [f"assert({function}->max_transfer_units == 65534U);",
                                   f"assert({function}->buffer_alignment == 2U);",
                                   f"assert(({function}->features & "
                                   "NX_FACTORY_CAP_STREAM_GAPS) != 0U);"]
                    elif item.kind == "spi" and item.mode.startswith("dma"):
                        checks.append(f"assert({function}->max_transfer_units == 65535U);")
                    checks.append(f"assert(({function}->features & NX_FACTORY_CAP_MODEL) == 0U);")
                source = self.root / "caps.c"
                source.write_text('#include "nexus_factory.h"\n#include <assert.h>\n'
                                  + "\n".join(aliases) + "\n" + getters
                                  + "\nint main(void) {\n" + "\n".join(checks)
                                  + "\nreturn 0;\n}\n")
                executable = self.root / "caps"
                command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                           "-pedantic", "-I" + str(self.root),
                           "-I" + str(gate.ROOT / "io/include"),
                           "-I" + str(gate.ROOT / "core/include"), str(source),
                           "-o", str(executable)]
                compile_result = subprocess.run(command, capture_output=True,
                                                text=True, check=False)
                self.assertEqual(compile_result.returncode, 0,
                                 compile_result.stderr)
                execution = subprocess.run([str(executable)], check=False)
                self.assertEqual(execution.returncode, 0)

    def test_i2c_capabilities_follow_the_actual_public_header_bounds(self):
        fixture = gate.ROOT / 'tests/contracts/gd32_assembly/i2c-irq.toml'
        result = gate.resolve_ir(fixture)
        header, getters = factory.emit(result)
        (self.root / 'nexus_factory.h').write_text(header)
        include = self.root / 'include/nexus/io'
        include.mkdir(parents=True)
        public = (gate.ROOT / 'io/include/nexus/io/i2c.h').read_text()
        public = public.replace('NX_I2C_MAX_MESSAGES      8U',
                                'NX_I2C_MAX_MESSAGES      32U')
        public = public.replace('NX_I2C_MAX_MESSAGE_BYTES 256U',
                                'NX_I2C_MAX_MESSAGE_BYTES 64U')
        (include / 'i2c.h').write_text(public)
        source = self.root / 'bounds.c'
        source.write_text(
            '#include "nexus_factory.h"\n#include <stdio.h>\n'
            'const nx_i2c_port_t* const nx_binding_bus = NULL;\n'
            'const nx_i2c_endpoint_t* const nx_device_sensor = NULL;\n'
            + getters + '\nint main(void) {\n'
            'const nx_factory_io_capability_t* port = '
            'nx_factory_i2c_capability(NX_I2C_ID_BUS);\n'
            'const nx_factory_io_capability_t* endpoint = '
            'nx_factory_i2c_device_capability(NX_I2C_DEVICE_ID_SENSOR);\n'
            'printf("actual units=%u messages=%u; public units=%u messages=%u\\n",'
            '(unsigned)port->max_transfer_units, (unsigned)port->max_messages,'
            '(unsigned)NX_I2C_MAX_MESSAGE_BYTES, (unsigned)NX_I2C_MAX_MESSAGES);\n'
            'return port->max_transfer_units != NX_I2C_MAX_MESSAGE_BYTES || '
            'port->max_messages != NX_I2C_MAX_MESSAGES || '
            'endpoint->max_transfer_units != NX_I2C_MAX_MESSAGE_BYTES || '
            'endpoint->max_messages != NX_I2C_MAX_MESSAGES;\n}\n')
        executable = self.root / 'bounds'
        command = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
                   '-I' + str(self.root / 'include'), '-I' + str(self.root),
                   '-I' + str(gate.ROOT / 'io/include'),
                   '-I' + str(gate.ROOT / 'core/include'), str(source),
                   '-o', str(executable)]
        compiled = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        execution = subprocess.run([str(executable)], capture_output=True,
                                   text=True)
        self.assertEqual(execution.returncode, 0, execution.stdout)


if __name__ == "__main__":
    unittest.main()
