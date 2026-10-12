"""Public Nucleo references generate exact Board/backend SDK inputs."""

import json
from pathlib import Path
import tempfile
import unittest

from tools.configure.configure import configure
from tools.evidence.identity import _source_sdk_validator


ROOT = Path(__file__).resolve().parents[2]
REFERENCES = ROOT / "tools/configure/assemblies"


class ReferenceAssemblyTests(unittest.TestCase):
    def check_reference(self, chip, backend, workload):
        name = f"nucleo-{chip}-{backend}-{workload}.toml"
        assembly = REFERENCES / name
        with tempfile.TemporaryDirectory(prefix="nucleo-reference-") as temp:
            output = Path(temp) / "generated"
            result = configure(assembly, output)
            self.assertEqual(result, json.loads(
                (output / "resolved.json").read_text()))
            board, part, uart = {
                "h743": ("stm32h743-nucleo-zi2", "STM32H743ZIT6",
                         "USART3"),
                "u575": ("stm32u575-nucleo-zi-q", "STM32U575ZIT6Q",
                         "USART1"),
            }[chip]
            self.assertEqual(result["board"], board)
            self.assertEqual(result["part"], part)
            self.assertEqual(result["backend"], backend)
            selected = result["controllers"]
            if workload == "empty":
                self.assertEqual(selected, [])
            else:
                self.assertEqual([item["kind"] for item in selected],
                                 ["gpio", "uart"])
                self.assertEqual(selected[1]["controller"], uart)
                self.assertEqual(selected[1]["mode"], "irq-bytes")
                self.assertEqual(selected[1]["rx_capacity"], 64)
                aliases = (output / "nexus_bindings.h").read_text()
                self.assertIn("nx_binding_" + uart.lower().replace(
                    "usart", "uart") + ";", aliases)
                self.assertIn("nx_binding_led0;", aliases)
            self.assertTrue((output / "bindings.c").is_file())
            labels = {item["path"] for item in result["inputs"]}
            relative = assembly.relative_to(ROOT).as_posix()
            self.assertIn(relative, labels)
            # These configurations must survive publication outside tests/.
            self.assertTrue(_source_sdk_validator().selected(relative))

    def test_h743_baremetal_empty(self):
        self.check_reference("h743", "baremetal", "empty")

    def test_h743_baremetal_uart(self):
        self.check_reference("h743", "baremetal", "uart")

    def test_h743_freertos_empty(self):
        self.check_reference("h743", "freertos", "empty")

    def test_h743_freertos_uart(self):
        self.check_reference("h743", "freertos", "uart")

    def test_u575_baremetal_empty(self):
        self.check_reference("u575", "baremetal", "empty")

    def test_u575_baremetal_uart(self):
        self.check_reference("u575", "baremetal", "uart")

    def test_u575_freertos_empty(self):
        self.check_reference("u575", "freertos", "empty")

    def test_u575_freertos_uart(self):
        self.check_reference("u575", "freertos", "uart")


if __name__ == "__main__":
    unittest.main()
