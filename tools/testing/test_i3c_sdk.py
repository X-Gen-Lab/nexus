"""I3C source SDK required closure and actual public consumer selection."""
import importlib.util
import os
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(os.environ.get("NEXUS_I3C_TEST_ROOT",
                           str(Path(__file__).resolve().parents[2])))


def load_module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


sdk = load_module("i3c_source_sdk", "cmake/package/package_source_sdk.py")
consumer = load_module("i3c_sdk_consumers", "cmake/package/verify_consumers.py")


class I3CSourceSDKTests(unittest.TestCase):
    def test_required_payload_contains_the_whole_optional_core(self):
        required = {
            "components/i3c_controller/CMakeLists.txt",
            "components/i3c_controller/src/i3c_controller.c",
            "components/i3c_controller/include/nexus/components/i3c_controller.h",
            "components/i3c_controller/include/nexus/components/i3c_controller_storage.h",
        }
        self.assertFalse(required - set(sdk.REQUIRED_FILES))
        self.assertTrue(all(sdk.selected(name) for name in required))

    def test_installed_native_and_arm_consumers_retain_public_core_call(self):
        choices = ((False, "stm32"), (True, "stm32"), (True, "gd32"))
        for arm, chip in choices:
            with self.subTest(arm=arm, chip=chip):
                self.check_consumer(arm, chip)

    def check_consumer(self, arm, chip):
        with tempfile.TemporaryDirectory() as value:
            directory = Path(value)
            policy = directory / (
                "prefix/share/nexus/src/dependencies/"
                "platform-compatibility.json")
            policy.parent.mkdir(parents=True)
            shutil.copyfile(
                ROOT / "dependencies/platform-compatibility.json", policy)
            source, _, _ = consumer.prepare_consumer(
                directory / "prefix", directory, "i3c", lambda _: None,
                arm=arm, chip=chip)
            self.assertIn("Nexus::I3CController",
                          (source / "CMakeLists.txt").read_text())
            code = (source / "main.cpp").read_text()
            self.assertIn('"nexus/components/i3c_controller_storage.h"', code)
            self.assertIn("nx_i3c_controller_construct(nullptr", code)

if __name__ == "__main__":
    unittest.main()
