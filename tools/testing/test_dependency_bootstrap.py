"""Dependency tools load without requesting unselected hardware packages."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure.providers import descriptor

ROOT = Path(__file__).resolve().parents[2]


class DependencyBootstrapTests(unittest.TestCase):
    def test_import_does_not_expand_family_adapters(self):
        specification = importlib.util.spec_from_file_location(
            "independent_dependency_bootstrap", ROOT / "tools/dev/dependencies.py")
        module = importlib.util.module_from_spec(specification)
        with patch.object(descriptor, "families", side_effect=AssertionError(
                "Unselected family metadata was requested during import")):
            specification.loader.exec_module(module)

    def test_default_family_and_dependencies_come_from_the_selected_root(self):
        from tools.dev import dependencies
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "soc/acme/reviewed"
            package.mkdir(parents=True)
            for name in ("soc.json", "routes.json", "CMakeLists.txt"):
                (package / name).write_text("{}\n")
            manifest = "soc/acme/reviewed/package.json"
            (package / "package.json").write_text(json.dumps({
                "schema_version": 1, "interface_version": 1,
                "id": "reviewed", "family": "reviewed", "vendor": "acme",
                "source_directory": ".", "facts": {"soc": "soc.json",
                    "routes": "routes.json"},
                "dependencies": {"core": "cmsis_core"},
                "cmake": {"entry": "CMakeLists.txt", "target": "reviewed_soc"},
                "exports": {"roots": ["."], "exclude": []}}))
            policy = json.loads((ROOT / "dependencies/platform-compatibility.json").read_text())
            policy["source_sdk"]["default_package_manifests"] = [manifest]
            (root / "dependencies").mkdir()
            (root / "dependencies/platform-compatibility.json").write_text(json.dumps(policy))
            self.assertEqual(dependencies.required_submodules("sdk", root=root),
                             ("vendors/arm/CMSIS_6",))
            self.assertEqual(dependencies.required_submodules("arm", "reviewed", root=root),
                             ("vendors/arm/CMSIS_6",))


if __name__ == "__main__":
    unittest.main()
