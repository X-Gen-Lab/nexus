"""Compile selected factory headers without unrelated public I/O headers."""
from tools.configure.tests.native_fixture import canonical_assembly
from pathlib import Path
from dataclasses import FrozenInstanceError
import re
import subprocess
import tempfile
import unittest

from tools.configure import configure, experience, factory
from tools.configure.tests.assembly_fixture import render_toml


class FactoryIncludeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="factory includes ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.output = self.root / "generated"

    def generate(self, fixture, policy=""):
        assembly = canonical_assembly(configure.ROOT, fixture)
        if policy:
            contents = assembly.read_text()
            assembly = self.root / "assembly.toml"
            assembly.write_text(contents + policy)
        configure.configure(assembly, self.output, test_support=True)

    def compile(self, compiler, standard, contents, *, dependency=False):
        source = self.root / ("consumer.c" if compiler == "cc" else "consumer.cpp")
        source.write_text(contents)
        command = [compiler, "-std=" + standard, "-Wall", "-Wextra", "-Werror",
                   "-pedantic", "-I" + str(self.output),
                   "-I" + str(configure.ROOT / "io/include"),
                   "-I" + str(configure.ROOT / "core/include")]
        command += (["-MM", "-MT", "consumer"] if dependency else
                    ["-c", "-o", str(self.root / "consumer.o")])
        return subprocess.run(command + [str(source)], capture_output=True,
                              text=True, check=False)

    def public_dependencies(self):
        result = self.compile("cc", "c11", '#include "nexus_factory.h"\n',
                              dependency=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return set(re.findall(r"nexus/io/([A-Za-z0-9_]+\.h)", result.stdout))

    def test_empty_factory_has_no_full_io_dependency(self):
        self.generate("native-empty.toml")
        self.assertEqual(self.public_dependencies(), {"fwd.h"})

    def test_selected_gpio_keeps_full_api_and_empty_getter_identities(self):
        self.generate("native-gpio.toml")
        self.assertEqual(self.public_dependencies(), {"fwd.h", "gpio.h"})
        contents = ('#include "nexus_factory.h"\n'
                    'const nx_gpio_port_t* gpio(void) {\n'
                    '    return nx_factory_gpio(NX_GPIO_ID_LED0);\n}\n'
                    'const nx_uart_port_t* absent(void) {\n'
                    '    return nx_factory_uart(NX_UART_ID_COUNT);\n}\n')
        for compiler, standard in (("cc", "c11"), ("c++", "c++17")):
            with self.subTest(compiler=compiler):
                result = self.compile(compiler, standard, contents)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_mixed_selected_interfaces_compile_with_nominal_references(self):
        assembly = self.root / "assembly.toml"
        fixtures = configure.ROOT / "tests/support/native/assemblies"
        gpio = (fixtures / "native-gpio.toml").read_text()
        uart = (fixtures / "native-uart.toml").read_text()
        assembly.write_text(gpio + "\n" + uart[uart.index("[uart."):])
        configure.configure(assembly, self.output, test_support=True)
        self.assertEqual(self.public_dependencies(),
                         {"fwd.h", "gpio.h", "uart.h", "stream.h", "wake.h"})
        contents = ('#include "nexus_factory_strong.h"\n'
                    'const nx_gpio_port_t* gpio(void) {\n'
                    '    return nx_factory_gpio_ref(NX_GPIO_REF_LED0);\n}\n'
                    'const nx_uart_port_t* uart(void) {\n'
                    '    return nx_factory_uart_ref(NX_UART_REF_UART0);\n}\n')
        for compiler, standard in (("cc", "c11"), ("c++", "c++17")):
            with self.subTest(compiler=compiler):
                result = self.compile(compiler, standard, contents)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_selected_traits_do_not_import_other_service_interfaces(self):
        self.generate("native-uart.toml", "\n[factory]\ncapabilities = true\n")
        self.assertEqual(self.public_dependencies(),
                         {"fwd.h", "uart.h", "stream.h", "wake.h",
                          "observation.h"})

    def test_empty_getters_execute_without_complete_interface_definitions(self):
        self.generate("native-empty.toml")
        assembly = configure.ROOT / "tests/support/native/assemblies/native-empty.toml"
        _, getters = factory.emit(configure.resolve_ir(assembly, test_support=True))
        contents = ('#include "nexus_factory_strong.h"\n#include <assert.h>\n'
                    + getters + '\nint main(void) {\n'
                    'assert(nx_factory_gpio(NX_GPIO_ID_COUNT) == NULL);\n'
                    'assert(nx_factory_usb_activation(NX_USB_DEVICE_ID_COUNT) '
                    '== NULL);\nreturn 0;\n}\n')
        for compiler, standard in (("cc", "c11"), ("c++", "c++17")):
            with self.subTest(compiler=compiler):
                source = self.root / ("empty.c" if compiler == "cc" else
                                      "empty.cpp")
                source.write_text(contents)
                executable = self.root / "empty"
                command = [compiler, "-std=" + standard, "-Wall", "-Wextra",
                           "-Werror", "-pedantic", "-I" + str(self.output),
                           "-I" + str(configure.ROOT / "io/include"),
                           "-I" + str(configure.ROOT / "core/include"),
                           str(source), "-o", str(executable)]
                built = subprocess.run(command, capture_output=True, text=True,
                                       check=False)
                self.assertEqual(built.returncode, 0, built.stderr)
                executed = subprocess.run([str(executable)], check=False)
                self.assertEqual(executed.returncode, 0)

    def test_shared_arm_retain_fixtures_compile_with_empty_selection(self):
        self.generate("native-empty.toml")
        for name in ('io_evolution_assembly', 'stm32_assembly'):
            with self.subTest(fixture=name):
                source = (configure.ROOT / "tests/contracts" / name /
                          "controller_main.c")
                result = self.compile("cc", "c11", source.read_text())
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_optional_uart_sdk_consumer_compiles_with_spi_only_assembly(self):
        self.generate("native-spi.toml")
        source = ('#include "nexus/io/uart.h"\n'
                  '#include "nexus_factory.h"\n'
                  'const nx_spi_port_t* spi(void) {\n'
                  '    return nx_factory_spi(NX_SPI_ID_SPI0);\n}\n'
                  'const nx_uart_port_t* optional_uart(void) {\n'
                  '    for (nx_uart_id_t id = 0U; id != NX_UART_ID_COUNT; ++id) {\n'
                  '        return nx_factory_uart(id);\n'
                  '    }\n    return NULL;\n}\n')
        result = self.compile("cc", "c11", source)
        self.assertEqual(result.returncode, 0, result.stderr)

    def accessor_fixture(self, fixture, identifier, renamed):
        path = configure.ROOT / fixture
        data = configure.load_assembly(path, test_support=True).to_dict()
        data["board_package"] = str(path.parent / data["board_package"])
        for item in data["controllers"] + data["devices"]:
            if item["id"] == identifier:
                item["id"] = renamed
            if item.get("controller") == identifier:
                item["controller"] = renamed
        assembly = self.root / "accessor.toml"
        assembly.write_text(render_toml(data))
        configure.configure(assembly, self.output, test_support=True)
        return configure.resolve_ir(assembly, test_support=True)

    def test_accessor_projection_keeps_selected_nominal_identity_immutable(self):
        self.generate("native-gpio.toml")
        result = configure.resolve_ir(
            configure.ROOT / "tests/support/native/assemblies/native-gpio.toml", test_support=True)
        accessors = factory.accessors(result)
        self.assertIsInstance(accessors, tuple)
        self.assertEqual(len(accessors), 1)
        accessor = accessors[0]
        self.assertEqual(accessor.id, "led0")
        self.assertEqual(accessor.class_name, "gpio")
        self.assertEqual(accessor.shape, "port")
        self.assertEqual(accessor.include, "nexus_factory_strong.h")
        self.assertEqual(accessor.reference, "NX_GPIO_REF_LED0")
        self.assertEqual(accessor.getter, "nx_factory_gpio_ref")
        self.assertEqual(accessor.return_type, "const nx_gpio_port_t*")
        with self.assertRaises(FrozenInstanceError):
            accessor.reference = "NX_UART_REF_LED0"
        empty = configure.resolve_ir(
            configure.ROOT / "tests/support/native/assemblies/native-empty.toml", test_support=True)
        self.assertEqual(factory.accessors(empty), ())

    def test_explain_accessors_compile_for_words_enhanced_and_hyphenated_ids(self):
        fixtures = (('native-uart9n1.toml', 'uart0', 'serial-link', 'uart_word', 'nx_uart_word_port_t'), ('f407-spi-transaction.toml', 'sensor0', 'sensor-link', 'spi_transaction_device', 'nx_spi_transaction_endpoint_t'), ('native-spi-dma-transaction.toml', 'device_a', 'device-link', 'spi_dma_transaction_device', 'nx_spi_dma_transaction_endpoint_t'))
        for fixture, identifier, renamed, class_name, face in fixtures:
            with self.subTest(fixture=fixture):
                result = self.accessor_fixture(
                    "tests/contracts/io_evolution_assembly/" + fixture,
                    identifier, renamed)
                reference = ("NX_" + class_name.upper() + "_REF_" +
                             renamed.replace("-", "_").upper())
                getter = "nx_factory_" + class_name + "_ref"
                call = getter + "(" + reference + ")"
                contents = ('#include "nexus_factory_strong.h"\n'
                            f'const {face}* lookup(void) {{\n'
                            f'    return {call};\n}}\n')
                for compiler, standard in (("cc", "c11"), ("c++", "c++17")):
                    compiled = self.compile(compiler, standard, contents)
                    self.assertEqual(compiled.returncode, 0, compiled.stderr)
                summary = experience.describe(result)
                self.assertIn('#include "nexus_factory_strong.h"', summary)
                self.assertIn(f"{class_name}.{renamed}: const {face}*", summary)
                self.assertIn(call, summary)
                accessor = next(item for item in factory.accessors(result)
                                if item.id == renamed)
                self.assertEqual(accessor.class_name, class_name)
                self.assertEqual(accessor.return_type, f"const {face}*")
                self.assertEqual(accessor.reference, reference)
                self.assertEqual(accessor.getter, getter)

    def test_forward_faces_coexist_with_complete_headers_in_both_orders(self):
        self.generate("native-empty.toml")
        headers = (configure.ROOT / "io/include/nexus/io").glob("*.h")
        complete = "".join('#include "nexus/io/' + path.name + '"\n'
                           for path in sorted(headers))
        for compiler, standard in (("cc", "c11"), ("c++", "c++17")):
            for before in (True, False):
                with self.subTest(compiler=compiler, before=before):
                    factory = '#include "nexus_factory_strong.h"\n'
                    contents = ((complete + factory) if before else
                                (factory + complete))
                    result = self.compile(compiler, standard, contents)
                    self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
