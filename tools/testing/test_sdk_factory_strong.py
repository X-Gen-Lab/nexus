"""Public SDK consumer templates choose nominal factory references."""

import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
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
                                ("f407", True, "stm32")):
            with self.subTest(consumer=name):
                source, _, _ = CONSUMERS.prepare_consumer(
                    self.prefix, self.root, name, lambda _: None,
                    arm=arm, chip=chip)
                for filename in ("main.c", "main.cpp"):
                    code = (source / filename).read_text()
                    self.assertIn('#include "nexus_factory_strong.h"', code)
                    self.assertIn("nx_factory_gpio_ref(NX_GPIO_REF_COUNT)", code)
                    self.assertIn('#if NEXUS_FACTORY_ABI_EPOCH != 6U', code)

    def test_explicit_version_still_binds_the_installed_policy_epoch(self):
        policy = (self.prefix / "share/nexus/src/dependencies"
                  / "platform-compatibility.json")
        value = json.loads(policy.read_text())
        value["factory_abi_epoch"] = 7
        policy.write_text(json.dumps(value))
        source, _, _ = CONSUMERS.prepare_consumer(
            self.prefix, self.root, "selected-policy", lambda _: None,
            version=value["semantic_version"])
        self.assertIn('#if NEXUS_FACTORY_ABI_EPOCH != 7U',
                      (source / "main.c").read_text())

    def test_base_consumer_compiles_without_a_gpio_factory(self):
        from tools.configure import configure

        generated = self.root / "generated"
        configure.configure(
            ROOT / "tests/support/native/assemblies/native-empty.toml",
            generated, test_support=True)
        source, _, _ = CONSUMERS.prepare_consumer(
            self.prefix, self.root, "empty", lambda _: None)
        command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                   "-pedantic", "-I" + str(generated)]
        command.extend("-I" + str(ROOT / layer / "include")
                       for layer in ("core", "io", "arch",
                                     "components/algorithms",
                                     "components/runtime/periodic"))
        result = subprocess.run(
            command + ["-c", str(source / "main.c"), "-o",
                       str(self.root / "consumer.o")],
            capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
