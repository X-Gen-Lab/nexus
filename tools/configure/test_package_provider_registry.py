"""Production package selection never imports retired family host adapters."""
from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
_GUARD = """
import importlib.abc
import sys
class NoFamilyAdapter(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path=None, target=None):
        if fullname == 'tools.configure.stm32f4' or fullname.startswith(
                'tools.configure.stm32f4.') or fullname == 'tools.configure.stm32f4_descriptor':
            raise AssertionError('Retired family adapter imported: ' + fullname)
sys.meta_path.insert(0, NoFamilyAdapter())
"""


class PackageProviderRegistryTests(unittest.TestCase):
    def run_guarded(self, program):
        result = subprocess.run([sys.executable, "-c", _GUARD + program],
                                cwd=ROOT, text=True, capture_output=True,
                                timeout=30, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_default_resolution_uses_only_declared_data_and_common_policies(self):
        self.run_guarded("""
from tools.configure import configure
result = configure.resolve_ir(configure.ROOT /
    'tools/configure/assemblies/qiming-baremetal-gpio.toml')
assert result.provider_context.provider.__class__.__name__ == 'DataProvider'
""")

    def test_metadata_and_direct_provider_queries_load_the_same_package_protocol(self):
        self.run_guarded("""
from tools.configure.providers import descriptor, maintained
metadata = descriptor.maintained('stm32f4')
assert metadata.source_directory == 'soc/stm32/stm32f4'
assert metadata.configuration_directory == ''
assert metadata.configuration_modules == ()
provider = maintained('stm32f4')
assert provider.__class__.__name__ == 'DataProvider'
assert provider.package.family == metadata.family
""")

    def test_editor_schema_has_no_family_implementation_imports(self):
        self.run_guarded("""
from tools.configure import authored
schema = authored.editor_schema()
assert 'clock_parameters' in schema['properties']
assert 'capture' in schema['properties']
""")


if __name__ == "__main__":
    unittest.main()
