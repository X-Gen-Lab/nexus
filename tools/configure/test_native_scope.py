"""Native models require explicit host-test configuration admission."""
import unittest
from unittest.mock import patch

from tools.configure import configure, integration_package
from tools.configure.providers.common import ConfigurationError


class NativeScopeTests(unittest.TestCase):
    def test_default_resolution_rejects_host_model(self):
        assembly = (configure.ROOT / "tests/support/native/assemblies/"
                    "native-empty.toml")
        with self.assertRaisesRegex(ConfigurationError, "test support"):
            configure.resolve_ir(assembly)

    def test_explicit_host_test_scope_resolves_model(self):
        assembly = (configure.ROOT / "tests/support/native/assemblies/"
                    "native-empty.toml")
        result = configure.resolve_ir(assembly, test_support=True)
        self.assertEqual(result.soc_family, "native")
        self.assertEqual(result.cpu_profile.arch, "native")

    def test_external_package_cannot_bypass_host_test_scope(self):
        with patch.object(integration_package, "load_manifest",
                          side_effect=AssertionError("manifest was inspected")):
            with self.assertRaisesRegex(ConfigurationError, "test support"):
                integration_package.select(
                    "external", family="native", root=configure.ROOT,
                    assembly_directory=configure.ROOT)


if __name__ == "__main__":
    unittest.main()
