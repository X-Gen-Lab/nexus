"""Public SDK consumer templates choose nominal factory references."""

import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPECIFICATION = importlib.util.spec_from_file_location(
    "strong_factory_sdk_consumers", ROOT / "cmake/package/verify_consumers.py")
CONSUMERS = importlib.util.module_from_spec(SPECIFICATION)
SPECIFICATION.loader.exec_module(CONSUMERS)


class SDKFactoryTemplateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="SDK nominal IDs ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.prefix = self.root / "prefix"
        policy = (self.prefix / "share/nexus/src/dependencies"
                  / "platform-compatibility.json")
        policy.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / "dependencies/platform-compatibility.json", policy)

    def test_base_c_and_cpp_consumers_use_nominal_references(self):
        for name, arm, chip in (("native", False, "stm32"),
                                ("f407", True, "stm32"),
                                ("gd470", True, "gd32")):
            with self.subTest(consumer=name):
                source, _, _ = CONSUMERS.prepare_consumer(
                    self.prefix, self.root, name, lambda _: None,
                    arm=arm, chip=chip)
                for filename in ("main.c", "main.cpp"):
                    code = (source / filename).read_text()
                    self.assertIn('#include "nexus_factory_strong.h"', code)
                    self.assertIn("nx_factory_gpio_ref(NX_GPIO_REF_COUNT)", code)

    def test_modern_consumers_use_selected_nominal_gpio_and_uart_references(self):
        for family in ("h743", "u575", "f103", "gd303", "g474"):
            for backend in ("baremetal", "freertos"):
                with self.subTest(family=family, backend=backend):
                    source, _, _ = CONSUMERS.prepare_modern_consumer(
                        self.prefix, self.root, family, backend, lambda _: None)
                    code = (source / "main.c").read_text()
                    self.assertIn('#include "nexus_factory_strong.h"', code)
                    self.assertIn("nx_factory_gpio_ref(NX_GPIO_REF_LED)", code)
                    self.assertIn("nx_factory_uart_ref(NX_UART_REF_CONSOLE)",
                                  code)


if __name__ == "__main__":
    unittest.main()
