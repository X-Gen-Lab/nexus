"""Generated instance identities remain distinct from private role storage."""
import json
from pathlib import Path
import re
import shutil
import tempfile
import unittest
from unittest.mock import patch

import bindings
import configure as gate


class BindingIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.assembly = self.root / "assembly.json"
        self.board = self.root / "board"
        self.board.mkdir()

    def fixture(self, filename):
        path = gate.ROOT / filename
        data = gate.load_assembly(path)
        source = path.parent / data["board_package"] / "board.json"
        shutil.copyfile(source, self.board / "board.json")
        data["board_package"] = str(self.board)
        return data

    def write(self, data):
        self.assembly.write_text(json.dumps(data), encoding="utf-8")

    def source(self, data):
        self.write(data)
        result = gate.resolve(self.assembly)
        return bindings.emit(result)[1]

    def assert_unique_storage(self, source):
        # A C translation unit cannot define one private object twice. This
        # checks the actual emitted declarations, including shared IRQ arrays.
        names = re.findall(
            r"^static (?:const )?\w+(?:\* const)? (s_\w+)",
            source, re.MULTILINE)
        self.assertEqual(len(names), len(set(names)), names)

    def add_gpio(self, data, identity):
        board_path = self.board / "board.json"
        board = json.loads(board_path.read_text(encoding="utf-8"))
        board["bindings"].append({"id": "independent-output",
                                  "controller": "GPIOE",
                                  "route": "gpio-pe4", "mask": 16,
                                  "initial": 16})
        board_path.write_text(json.dumps(board), encoding="utf-8")
        data["controllers"].append({"id": identity,
                                    "binding": "independent-output",
                                    "mode": "output"})

    def test_platform_state_names_remain_legal_instance_ids(self):
        for filename in ("tools/configure/assemblies/qiming-baremetal.toml",
                         "tools/configure/assemblies/liangshan-baremetal.toml"):
            with self.subTest(fixture=filename):
                data = self.fixture(filename)
                data["controllers"][0]["id"] = "started"
                data["controllers"][1]["id"] = "initialized"
                source = self.source(data)
                self.assert_unique_storage(source)
                self.assertRegex(source, r"static nx_(stm32|gd32)_gpio_state_t s_nx_port_started;")
                self.assertRegex(source, r"static nx_(stm32|gd32)_uart_state_t s_nx_port_initialized;")
                self.assertIn("static bool s_nx_platform_started;", source)
                self.assertIn("static size_t s_nx_platform_initialized;",
                              source)

    def test_gpio_identity_does_not_alias_spi_child_cs_storage(self):
        data = self.fixture("tests/contracts/stm32_assembly/ve-spi.toml")
        data["devices"][0]["id"] = "sensor"
        self.add_gpio(data, "sensor_cs")
        source = self.source(data)
        self.assert_unique_storage(source)
        self.assertIn("static nx_stm32_gpio_state_t s_nx_port_sensor_cs;", source)
        self.assertIn("static nx_stm32_gpio_state_t s_nx_cs_sensor;", source)
        self.assertIn(".cs = &s_nx_cs_sensor;", source)
        self.assertIn("nx_binding_sensor_cs = &s_nx_face_sensor_cs;", source)

    def test_gpio_identity_does_not_alias_pwm_or_adc_storage(self):
        for filename, identity, storage in (
                ("ve-pwm.toml", "pwm0_idle", "s_nx_idle_pwm0"),
                ("ve-adc.toml", "adc0_channels", "s_nx_adc_channels_adc0")):
            with self.subTest(fixture=filename):
                data = self.fixture(
                    "tests/contracts/stm32_assembly/" + filename)
                self.add_gpio(data, identity)
                source = self.source(data)
                self.assert_unique_storage(source)
                self.assertIn("s_nx_port_" + identity, source)
                self.assertIn(storage, source)

    def test_shared_irq_storage_has_a_separate_role(self):
        data = self.fixture("tests/contracts/stm32_assembly/ve-exti.toml")
        self.add_gpio(data, "exti9_5_lines")
        source = self.source(data)
        self.assert_unique_storage(source)
        self.assertIn("s_nx_port_exti9_5_lines", source)
        self.assertIn("s_nx_irq_exti9_5_ports[]", source)
        self.assertIn("{ &s_nx_port_edge5, &s_nx_port_edge6 }", source)

    def test_repeated_generation_is_identical_and_failure_invalidates(self):
        data = self.fixture("tools/configure/assemblies/qiming-baremetal.toml")
        data["controllers"][0]["id"] = "started"
        self.write(data)
        output = self.root / "bundle"
        gate.configure(self.assembly, output)
        first = {path.name: path.read_bytes() for path in output.iterdir()}
        gate.configure(self.assembly, output)
        second = {path.name: path.read_bytes() for path in output.iterdir()}
        self.assertEqual(first, second)
        self.assert_unique_storage((output / "bindings.c").read_text())
        with patch.object(gate, "generate", side_effect=RuntimeError("emit")):
            with self.assertRaisesRegex(RuntimeError, "emit"):
                gate.configure(self.assembly, output)
        self.assertFalse(output.exists())
        self.assertEqual(list(self.root.glob(".nexus-config-*")), [])


if __name__ == "__main__":
    unittest.main()
