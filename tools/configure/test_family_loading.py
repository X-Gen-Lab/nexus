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
from tools.configure.providers import common, descriptor, maintained


ROOT = Path(__file__).resolve().parents[2]
PACKAGES = {
    "native": ("native", "soc/native"),
    "stm32f103": ("stm32", "soc/stm32/stm32f103"),
    "stm32f407": ("stm32", "soc/stm32/stm32f407"),
    "stm32g474": ("stm32", "soc/stm32/stm32g474"),
    "stm32h743": ("stm32", "soc/stm32/stm32h743"),
    "stm32u575": ("stm32", "soc/stm32/stm32u575"),
    "gd32f303": ("gd32", "soc/gd32/gd32f303"),
    "gd32f470": ("gd32", "soc/gd32/gd32f470"),
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
                    "maintained(sys.argv[1])",
                    "root = pathlib.Path(sys.argv[2])",
                    "names = set(dependencies.source_names('platform', sys.argv[1]))",
                    "loaded = {pathlib.Path(module.__file__).relative_to(root).as_posix()",
                    "          for name, module in tuple(sys.modules.items())",
                    "          if name == 'soc' or name.startswith('soc.')}",
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
                item = descriptor.maintained(family)
                self.assertEqual(item.vendor, vendor)
                self.assertEqual(item.source_directory, directory)
                self.assertEqual(item.configuration_directory,
                                 directory + "/configure")

    def test_all_families_load_only_their_registered_normal_package(self):
        for family, (_, directory) in PACKAGES.items():
            with self.subTest(family=family):
                provider = maintained(family)
                self.assertEqual(provider.__name__,
                                 directory.replace("/", ".") + ".configure")
                self.assertEqual(Path(provider.__file__).resolve(),
                                 ROOT / directory / "configure/__init__.py")

    def test_selected_native_package_does_not_import_mcu_adapters(self):
        code = (
            "import sys; sys.path.insert(0, sys.argv[1]); "
            "from tools.configure.providers import maintained; "
            "provider = maintained('native'); "
            "assert provider.__name__ == 'soc.native.configure'; "
            "assert not any(name.startswith(('soc.stm32.', 'soc.gd32.')) "
            "for name in sys.modules)"
        )
        result = subprocess.run([sys.executable, "-I", "-c", code, str(ROOT)],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_native_construction_and_rejection_share_canonical_types(self):
        result = configure.resolve_ir(
            ROOT / "tools/configure/assemblies/native-gpio.toml")
        provider = maintained("native")
        construction = provider.constructor(
            result.controllers[0], result, result.board_bindings,
            result.routes, result.devices)
        self.assertIs(type(construction), common.Construction)
        with self.assertRaises(common.ConfigurationError):
            maintained("../native")

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
                                  "tools/configure/assemblies/native-gpio.toml"),
                "--output", str(output)], cwd=directory, env=environment,
                capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            identity = json.loads((output / "generator-identity.json").read_text())
            names = {item["path"] for item in identity["files"]}
            self.assertIn("soc/native/configure/bindings.py", names)
            self.assertNotIn("tools/configure/providers/native/bindings.py", names)

    def test_family_tests_remain_in_the_ordinary_configure_discovery(self):
        suite = unittest.TestLoader().discover(
            str(ROOT / "tools/configure"), pattern="test_provider.py")

        def cases(items):
            for item in items:
                if isinstance(item, unittest.TestSuite):
                    yield from cases(item)
                else:
                    yield item.id()

        identities = set(cases(suite))
        self.assertFalse(any("_FailedTest" in item for item in identities))
        for family in ("stm32f103", "gd32f303", "stm32g474", "stm32h743",
                       "stm32u575"):
            with self.subTest(family=family):
                self.assertTrue(any(item.startswith(
                    f"tests.providers.{family}.test_provider.")
                                    for item in identities))


if __name__ == "__main__":
    unittest.main()
