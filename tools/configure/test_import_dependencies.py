"""Imported SDK headers retain their separately pinned compiler dependencies."""
from pathlib import Path
import unittest
from unittest import mock

from tools.configure import configure

ROOT = Path(__file__).resolve().parents[2]
ASSEMBLY = ROOT / "tools/configure/assemblies/gd32-f303-baremetal-io.toml"


class ImportedDependencyContracts(unittest.TestCase):
    def test_imported_family_also_binds_its_pinned_cmsis_core(self):
        result = configure.resolve(ASSEMBLY, ROOT)
        records = {item["path"]: item for item in result["source_dependencies"]}
        self.assertEqual(records["vendors/arm/CMSIS_5"]["commit"],
                         "55b19837f5703e418ca37894d5745b1dc05e4c91")
        self.assertEqual(records["vendors/gigadevice/gd32f30x"]["kind"],
                         "vendor-source-import")

    def test_auxiliary_git_identity_failure_cannot_be_ignored(self):
        with mock.patch.object(configure, "dependency_identity",
                               side_effect=configure.ConfigurationError(
                                   "auxiliary CMSIS identity changed")):
            with self.assertRaisesRegex(configure.ConfigurationError,
                                        "auxiliary CMSIS identity changed"):
                configure.resolve(ASSEMBLY, ROOT)


if __name__ == "__main__":
    unittest.main()
