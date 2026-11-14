"""Common configuration code is independent of unselected family packages."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure, dependencies
from tools.configure.providers import descriptor


class GeneratorScopeTests(unittest.TestCase):
    def test_common_sdk_seals_every_static_assembly_mechanism_it_imports(self):
        names = set(dependencies.source_names("platform", None, sdk=True))
        required = {"tools/configure/" + name + ".py" for name in (
            "static_ir", "calls", "static_recipe", "static_endpoints", "static_fragments",
            "shared_irq", "data_provider", "hardware_bindings", "domain_bridge",
            "domain_policies", "platform_bindings", "startup_memory", "header_projection")}
        self.assertFalse(required - names, "Unsealed configuration code: " +
                         ", ".join(sorted(required - names)))

    def test_common_sdk_bootstrap_does_not_import_any_family_descriptor(self):
        with patch.object(descriptor, "families", side_effect=AssertionError(
                "Unselected family metadata was loaded")):
            names = dependencies.source_names("platform", None, sdk=True)
        self.assertIn("tools/configure/data_provider.py", names)
        self.assertFalse(any(name.startswith("tools/configure/stm32f4/")
                             for name in names))

    def test_data_selection_seals_common_engine_and_package_facts(self):
        path = configure.ROOT / "tools/configure/assemblies/qiming-baremetal-uart-dma.toml"
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "bundle"
            configure.configure(path, output)
            identity = configure.load(output / "generator-identity.json")
            resolved = configure.resolve_ir(path)
        names = {row["path"] for row in identity["files"]}
        self.assertIn("tools/configure/data_provider.py", names)
        self.assertFalse(any(name.startswith("tools/configure/stm32f4")
                             for name in names))
        inputs = {row["path"] for row in resolved.inputs}
        for relative in ("package.json", "bindings.json", "bindings/uart.json"):
            self.assertTrue(any(name.endswith("soc/stm32/stm32f4/" + relative)
                                for name in inputs), relative)


if __name__ == "__main__":
    unittest.main()
