"""Selected STM32F4 SDK sources retain their precise CMSIS dependencies."""

from pathlib import Path
import unittest

from tools.configure import configure

ROOT = Path(__file__).resolve().parents[2]
ASSEMBLY = ROOT / "tools/configure/assemblies/qiming-baremetal.toml"


class ImportedDependencyContracts(unittest.TestCase):
    def test_selected_family_binds_the_locked_core_device_and_hal(self):
        result = configure.resolve_ir(ASSEMBLY, ROOT)
        records = {item["path"]: item for item in result.source_dependencies}
        expected = {
            "vendors/arm/CMSIS_6": "af1b110e46a1720349f6f125e386e126767699bd",
            "vendors/st/cmsis_device_f4": "3c77349ce04c8af401454cc51f85ea9a50e34fc1",
            "vendors/st/stm32f4_hal": "b6f0ed3829f3829eb358a2e7417d80bba1a42db7",
        }
        self.assertEqual({path: record["commit"] for path, record in records.items()},
                         expected)


if __name__ == "__main__":
    unittest.main()
