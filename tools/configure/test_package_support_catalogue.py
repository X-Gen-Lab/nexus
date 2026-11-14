"""Support queries follow declared package roots and retain explicit scope."""
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure, matrix, packages
from tools.configure.data_provider import DataProvider
from tools.maintenance import capabilities


class PackageSupportCatalogueTests(unittest.TestCase):
    def root(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        return Path(directory.name)

    def copy_package(self, manifest, root, location="soc/supplier/series", *, rename=False):
        package = packages.load(manifest)
        owner = root / location
        owner.mkdir(parents=True)
        for path in package.inputs:
            target = owner / path.relative_to(package.source_directory)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
        copied = owner / "package.json"
        if rename:
            value = json.loads(copied.read_text())
            for key, target in (("soc", "chip-facts.json"), ("routes", "pin-routes.json")):
                (owner / value["facts"][key]).rename(owner / target)
                value["facts"][key] = target
            copied.write_text(json.dumps(value) + "\n")
        return owner

    def test_catalogue_uses_supplied_root_and_manifest_fact_paths(self):
        root = self.root()
        owner = self.copy_package(configure.ROOT / "soc/stm32/stm32f4/package.json",
                                  root, rename=True)
        rows = matrix.support_catalogue(root)
        self.assertTrue(rows)
        self.assertEqual({row.family for row in rows}, {"stm32f4"})
        self.assertTrue(all(not row.physical_qualified for row in rows))
        self.assertTrue(all((owner / "chip-facts.json").relative_to(root).as_posix()
                            in row.review_sources for row in rows))

    def test_reference_package_does_not_enter_production_support(self):
        root = self.root()
        self.copy_package(configure.ROOT / "tests/contracts/declarative_gd32f470/package.json",
                          root)
        self.assertEqual(matrix.support_catalogue(root), ())

    def test_projection_uses_scope_from_its_sealed_manifest(self):
        root = self.root()
        owner = self.copy_package(configure.ROOT / "soc/stm32/stm32f4/package.json", root)
        discovered = packages.load(owner / "package.json")
        value = json.loads(discovered.manifest_path.read_text())
        value["scope"] = "reference"
        discovered.manifest_path.write_text(json.dumps(value) + "\n")
        with patch.object(packages, "discover", return_value=(discovered,)):
            self.assertEqual(matrix.support_catalogue(root), ())

    def test_catalogue_rejects_changed_bytes_after_fact_values_are_loaded(self):
        root = self.root()
        self.copy_package(configure.ROOT / "soc/stm32/stm32f4/package.json", root)
        original = DataProvider.from_package

        def change(package, *arguments, **options):
            provider = original(package, *arguments, **options)
            package.cmake_entry.write_text(package.cmake_entry.read_text() + "\n")
            return provider

        with patch.object(DataProvider, "from_package", side_effect=change), \
                self.assertRaisesRegex(ValueError, "input changed"):
            matrix.support_catalogue(root)

    def test_capabilities_use_declared_exports_and_skip_reference_fixtures(self):
        root = self.root()
        owner = self.copy_package(configure.ROOT / "soc/stm32/stm32f4/package.json",
                                  root, rename=True)
        manifest = owner / "package.json"
        value = json.loads(manifest.read_text())
        value["exports"] = {"roots": ["implementation"],
                            "exclude": ["implementation/private-tests"]}
        directory = owner / "implementation"
        directory.mkdir()
        (directory / "provider.c").write_text("/* reviewed source input */\n")
        excluded = directory / "private-tests"
        excluded.mkdir()
        (excluded / "test.c").write_text("/* excluded input */\n")
        manifest.write_text(json.dumps(value) + "\n")
        self.copy_package(configure.ROOT / "tests/contracts/declarative_gd32f470/package.json",
                          root, "soc/reference/series")
        fixture = root / "tests/contracts/reference.toml"
        fixture.parent.mkdir(parents=True)
        fixture.write_text('schema=2\nbackend="baremetal"\n'
                           'soc_package="../../soc/reference/series"\n')
        with patch.object(capabilities, "source_ops", return_value=set()) as scan:
            rows = capabilities.collect(root)
        self.assertTrue(rows)
        self.assertEqual({row["family"] for row in rows}, {"stm32f4"})
        scan.assert_called_once_with(directory / "provider.c", root)

    def test_exact_reference_query_keeps_scope_and_unassessed_execution(self):
        selected = capabilities.selection(configure.ROOT /
            "tests/contracts/declarative_gd32f470/gpio-only.toml", test_support=True)
        self.assertEqual(selected["provider_package"]["scope"], "reference")
        package = packages.load(configure.ROOT /
            "tests/contracts/declarative_gd32f470/package.json")
        self.assertEqual(selected["provider_package"]["family"], package.family)
        self.assertEqual(selected["validation"]["configuration"], "validated")
        self.assertEqual(selected["validation"]["physical_hil"], "not_assessed")
        self.assertFalse(selected["physical_qualified"])


if __name__ == "__main__":
    unittest.main()
