"""Declarative package metadata is independent of executable providers."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.package = self.root / "soc/vendor/family"
        self.package.mkdir(parents=True)
        for name in ("soc.json", "routes.json", "devices.json", "bindings.json"):
            (self.package / name).write_text("{}\n")
        (self.package / "CMakeLists.txt").write_text("# package target\n")
        self.manifest = self.package / "package.json"
        self.value = {
            "schema_version": 1, "interface_version": 1,
            "id": "family", "family": "family", "vendor": "vendor",
            "source_directory": ".",
            "facts": {"soc": "soc.json", "routes": "routes.json",
                      "device_catalog": "devices.json",
                      "bindings": "bindings.json"},
            "dependencies": {"core": "cmsis_core"},
            "cmake": {"entry": "CMakeLists.txt", "target": "nexus_soc_family"},
            "exports": {"roots": ["."], "exclude": ["tests"]},
        }
        self.write()

    def write(self):
        self.manifest.write_text(json.dumps(self.value))

    def load(self):
        from tools.configure import packages
        return packages.load(self.manifest)

    def test_package_is_immutable_and_loads_without_python_entry(self):
        selected = self.load()
        self.assertEqual(selected.source_directory, self.package)
        self.assertEqual(selected.cmake_entry, self.package / "CMakeLists.txt")
        self.assertEqual(selected.target, "nexus_soc_family")
        self.assertEqual(selected.dependencies, (("core", "cmsis_core"),))
        self.assertEqual(selected.export_roots, (self.package,))
        self.assertEqual(selected.export_excludes, (self.package / "tests",))
        self.assertIn(self.manifest, selected.inputs)
        self.assertEqual(selected.fact("soc"), self.package / "soc.json")
        with self.assertRaises(FrozenInstanceError):
            selected.family = "changed"
        with self.assertRaises(TypeError):
            selected.facts["soc"] = "other.json"

    def test_discovery_reads_only_declarative_manifests(self):
        from tools.configure import packages
        (self.package / "__init__.py").write_text("raise RuntimeError('imported')")
        selected = packages.discover(self.root)
        self.assertEqual(tuple(item.family for item in selected), ("family",))

    def test_unknown_fields_versions_targets_and_escaping_paths_reject(self):
        for field, value in (("unknown", True), ("interface_version", 2),
                             ("schema_version", True),
                             ("source_directory", ".."),
                             ("cmake", {"entry": "else.cmake", "target": "bad;id"}),
                             ("facts", {"soc": "../soc.json", "routes": "routes.json"}),
                             ("exports", {"roots": ["../"], "exclude": []})):
            with self.subTest(field=field, value=value):
                old = self.value.copy()
                self.value[field] = value
                self.write()
                with self.assertRaises(ValueError):
                    self.load()
                self.value = old

    def test_symlink_and_missing_fact_reject_before_execution(self):
        source = self.package / "soc.json"
        source.unlink()
        with self.assertRaises(ValueError):
            self.load()
        outside = self.root / "outside.json"
        outside.write_text("{}")
        source.symlink_to(outside)
        with self.assertRaises(ValueError):
            self.load()

    def test_duplicate_ids_reject_instead_of_using_search_order(self):
        from tools.configure import packages
        second = self.root / "soc/other/family"
        second.mkdir(parents=True)
        for path in self.package.iterdir():
            (second / path.name).write_bytes(path.read_bytes())
        with self.assertRaisesRegex(ValueError, "[Dd]uplicate"):
            packages.discover(self.root)

    def test_binding_fragments_are_closed_package_inputs(self):
        fragment = self.package / "gpio.json"
        fragment.write_text('{"schema_version":1,"bindings":[]}\n')
        self.value["binding_files"] = ["gpio.json"]
        self.write()
        selected = self.load()
        self.assertEqual(selected.binding_files, (fragment,))
        self.assertEqual(selected.inputs.count(fragment), 1)
        for names in (["../gpio.json"], ["gpio.json", "gpio.json"],
                      ["missing.json"]):
            with self.subTest(names=names):
                self.value["binding_files"] = names
                self.write()
                with self.assertRaises(ValueError):
                    self.load()

    def test_production_editor_schema_is_the_live_declarative_contract(self):
        from tools.configure import packages, integration_package
        schema = packages.manifest_schema()
        self.assertEqual(schema, integration_package.manifest_schema())
        self.assertIn("facts", schema["required"])
        self.assertIn("cmake", schema["required"])
        self.assertIn("binding_files", schema["properties"])
        self.assertNotIn("configuration_directory", schema["properties"])
        self.assertNotIn("version", schema["required"])
        path = Path(packages.__file__).with_name("soc-package.schema.json")
        self.assertEqual(json.loads(path.read_bytes()), schema)


if __name__ == "__main__":
    unittest.main()
