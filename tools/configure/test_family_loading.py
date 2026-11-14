"""Exact SoC packages preserve loading, common types and test discovery."""
import dataclasses
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.configure import configure
from tools.configure.data_provider import DataProvider
from tools.configure.providers import common, descriptor, maintained


ROOT = Path(__file__).resolve().parents[2]
PACKAGES = {
    "native": ("native", "tests/support/native"),
    "stm32f4": ("stm32", "soc/stm32/stm32f4"),
}
CONFIGURATIONS = {
    "native": "tests/support/native/configure",
    "stm32f4": "",
}


class FamilyLoadingTests(unittest.TestCase):
    def test_runtime_imports_are_in_the_runtime_source_closure(self):
        code = "\n".join((
            "import pathlib, sys",
            "from tools.configure import runtime, dependencies",
            "root = pathlib.Path(sys.argv[1])",
            "names = set(dependencies.source_names('runtime', None))",
            "loaded = {pathlib.Path(module.__file__).relative_to(root).as_posix()",
            "          for name, module in tuple(sys.modules.items())",
            "          if name.startswith('tools.configure')}",
            "assert loaded <= names, sorted(loaded - names)",
        ))
        result = subprocess.run([sys.executable, "-B", "-c", code, str(ROOT)],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_loaded_soc_modules_are_in_the_selected_source_closure(self):
        for family in descriptor.families():
            with self.subTest(family=family.family):
                code = "\n".join((
                    "import pathlib, sys",
                    "from tools.configure.providers import maintained",
                    "from tools.configure import dependencies",
                    "maintained(sys.argv[1], test_support=(sys.argv[1]=='native'))",
                    "root = pathlib.Path(sys.argv[2])",
                    "names = set(dependencies.source_names('platform', sys.argv[1], test_support=(sys.argv[1]=='native')))",
                    "loaded = {pathlib.Path(module.__file__).relative_to(root).as_posix()",
                    "          for name, module in tuple(sys.modules.items())",
                    "          if name.startswith(('tools.configure.stm32f4', 'tests.support.native'))}",
                    "assert loaded <= names, sorted(loaded - names)",
                ))
                result = subprocess.run([
                    sys.executable, "-B", "-c", code, family.family, str(ROOT)],
                    capture_output=True, text=True, check=False)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_descriptors_declare_exact_paths_without_family_name_inference(self):
        fields = {field.name for field in dataclasses.fields(
            descriptor.FamilyDescriptor)}
        self.assertLessEqual({"vendor", "source_directory",
                              "configuration_directory"}, fields)
        for family, (vendor, directory) in PACKAGES.items():
            with self.subTest(family=family):
                item = descriptor.maintained(family, test_support=(family == "native"))
                self.assertEqual(item.vendor, vendor)
                self.assertEqual(item.source_directory, directory)
                self.assertEqual(item.configuration_directory,
                                 CONFIGURATIONS[family])

    def test_production_uses_data_and_native_requires_explicit_test_support(self):
        provider = maintained("stm32f4")
        self.assertIs(type(provider), DataProvider)
        self.assertEqual(provider.package.manifest_path,
                         ROOT / "soc/stm32/stm32f4/package.json")
        native = maintained("native", test_support=True)
        self.assertEqual(native.__name__, "tests.support.native.configure")
        self.assertEqual(Path(native.__file__).resolve(),
                         ROOT / "tests/support/native/configure/__init__.py")
        with self.assertRaises(common.ConfigurationError):
            maintained("native")

    def test_selected_native_package_does_not_import_mcu_adapters(self):
        code = (
            "import sys; sys.path.insert(0, sys.argv[1]); "
            "from tools.configure.providers import maintained; "
            "provider = maintained('native', test_support=True); "
            "assert provider.__name__ == 'tests.support.native.configure'; "
            "assert not any(name.startswith('tools.configure.stm32f4') "
            "for name in sys.modules)"
        )
        result = subprocess.run([sys.executable, "-I", "-c", code, str(ROOT)],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_native_construction_and_rejection_share_canonical_types(self):
        result = configure.resolve_ir(
            ROOT / "tests/support/native/assemblies/native-gpio.toml", test_support=True)
        provider = maintained("native", test_support=True)
        construction = provider.constructor(
            result.controllers[0], result, result.board_bindings,
            result.routes, result.devices)
        self.assertIs(type(construction), common.Construction)
        with self.assertRaises(common.ConfigurationError):
            maintained("../native", test_support=False)

    def test_absolute_cli_ignores_ambient_configuration_packages(self):
        with tempfile.TemporaryDirectory(prefix="configuration package ") as name:
            directory = Path(name)
            for package in ("tools", "soc", "providers"):
                (directory / (package + ".py")).write_text(
                    "raise RuntimeError('ambient package executed')\n")
            output = directory / "generated"
            environment = {**os.environ, "PYTHONPATH": str(directory)}
            result = subprocess.run([
                sys.executable, "-B", str(ROOT / "tools/configure/configure.py"),
                "--assembly", str(ROOT /
                                  "tests/support/native/assemblies/native-gpio.toml"),
                "--output", str(output), "--test-support"], cwd=directory, env=environment,
                capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            identity = json.loads((output / "generator-identity.json").read_text())
            names = {item["path"] for item in identity["files"]}
            self.assertIn("tests/support/native/configure/bindings.py", names)
            self.assertNotIn("tools/configure/providers/native/bindings.py", names)

    def test_family_tests_remain_in_the_ordinary_configure_discovery(self):
        suite = unittest.TestLoader().discover(
            str(ROOT / "tools/configure"), pattern="test_stm32f4_family.py",
            top_level_dir=str(ROOT))

        def cases(items):
            for item in items:
                if isinstance(item, unittest.TestSuite):
                    yield from cases(item)
                else:
                    yield item.id()

        identities = set(cases(suite))
        self.assertFalse(any("_FailedTest" in item for item in identities))
        self.assertTrue(any(item.startswith("tools.configure.test_stm32f4_family.")
                            for item in identities))


if __name__ == "__main__":
    unittest.main()
