"""External provider models are explicit test fixtures outside the F4 SDK."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "cmake/package"))
import package_source_sdk as source_sdk
from tools.configure import integration_package
from tools.maintenance import consumer_profiles

REFERENCE = "tests/support/external_provider"


class ProviderSDKStarterTests(unittest.TestCase):
    def test_one_sealed_host_fixture_is_excluded_from_all_sdk_profiles(self):
        canonical = ROOT / REFERENCE
        required = {"CMakeLists.txt", "README.md", "assembly.toml", "consumer.c",
                    "lifecycle_test.cpp", "board/board.json",
                    "provider/nexus-soc.json", "provider/soc/lifecycle_adapter.c",
                    "provider/soc/lifecycle_adapter.h",
                    "provider/include/nexus_reference/lifecycle_probe.h"}
        self.assertTrue(all((canonical / name).is_file() for name in required))
        integration_package.load_manifest(canonical / "provider")
        for host_support in (False, True):
            profile = source_sdk.source_profile(host_support=host_support)
            for path in canonical.rglob("*"):
                if path.is_file():
                    self.assertFalse(source_sdk.selected(
                        path.relative_to(ROOT).as_posix(), profile=profile))

    def test_production_sdk_plan_excludes_external_host_provider_models(self):
        self.assertFalse((ROOT /
            "cmake/package/verification/external_provider").exists())
        self.assertFalse(any(item.scenario == "external_provider"
                             for item in consumer_profiles.SERVICE_CONSUMERS))


if __name__ == "__main__":
    unittest.main()
