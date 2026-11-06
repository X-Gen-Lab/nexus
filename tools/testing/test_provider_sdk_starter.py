"""Canonical external provider source and manifest-projected SDK boundaries."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "cmake/package"))
import package_source_sdk as source_sdk
import verify_consumers as consumers
from tools.release import transport

REFERENCE = "cmake/package/verification/external_provider"


class ProviderSDKStarterTests(unittest.TestCase):
    """Keep an independent required-member oracle beside manifest expansion."""

    def test_lifecycle_starter_has_one_canonical_sdk_source(self):
        canonical = ROOT / REFERENCE
        # These names are an independent minimum, never derived from inputs.
        required = {"CMakeLists.txt", "README.md", "assembly.toml",
                    "consumer.c",
                    "lifecycle_test.cpp", "board/board.json",
                    "provider/nexus-soc.json",
                    "provider/soc/lifecycle_adapter.c",
                    "provider/soc/lifecycle_adapter.h",
                    "provider/include/nexus_reference/lifecycle_probe.h"}
        self.assertTrue(canonical.is_dir(), "SDK lifecycle starter is missing")
        self.assertTrue(all((canonical / name).is_file() for name in required))
        actual = set(source_sdk.external_fixture_files(ROOT))
        self.assertLessEqual({REFERENCE + "/" + name for name in required},
                             actual)
        self.assertTrue(all(source_sdk.selected(name) for name in actual))
        self.assertFalse((ROOT / "tests/contracts/external_provider_lifecycle/"
                          "consumer.c").exists(), "duplicate lifecycle source")

    def test_complete_transport_includes_canonical_external_lifecycle(self):
        profiles = [item for item in transport.SERVICE_CONSUMERS
                    if item.scenario == "external_provider"]
        self.assertEqual(len(profiles), 1,
                         "complete SDK plan omits external lifecycle")
        profile = profiles[0]
        self.assertEqual(profile.domain, "external-provider")
        self.assertFalse(profile.arm)
        self.assertIn(("services", profile), transport.consumer_plan())
        self.assertIn("external provider build/bin/external_lifecycle_consumer",
                      transport.IMAGES)
        self.assertEqual(transport.CONSUMER_POLICY, "nexus-sdk-consumers-v16")

    def test_lifecycle_execution_rejects_late_source_change(self):
        with tempfile.TemporaryDirectory(prefix="provider lifecycle ") as value:
            root = Path(value)
            sdk = root / "sdk"
            canonical = sdk / REFERENCE
            shutil.copytree(ROOT / REFERENCE, canonical)
            (sdk / ".nexus-source-sdk.json").write_text("{}\n")
            source = root / "consumer"
            shutil.copytree(canonical, source)
            build = root / "build"
            (build / "bin").mkdir(parents=True)
            (build / "resolved-sdk.txt").write_text(str(sdk))
            (build / "bin/external_lifecycle_consumer").write_bytes(b"image")
            rows = [{"file": str(path), "directory": str(build),
                     "arguments": ["cc", "-std=c11", "-c", str(path)]}
                    for path in (source / "consumer.c",
                                 source / "provider/soc/lifecycle_adapter.c")]
            (build / "compile_commands.json").write_text(json.dumps(rows))

            def changed_during_execution(command):
                (source / "consumer.c").write_text("changed during execution\n")
                return subprocess.CompletedProcess(command, 0, "", "")

            with self.assertRaisesRegex(ValueError, "changed"):
                consumers.check_external_provider_consumer(
                    sdk, source, build, changed_during_execution)

    def test_lifecycle_observation_binds_actual_sdk_manifest(self):
        with tempfile.TemporaryDirectory(prefix="SDK observation ") as value:
            root = Path(value)
            sdk = root / "sdk"
            canonical = sdk / REFERENCE
            shutil.copytree(ROOT / REFERENCE, canonical)
            manifest = sdk / ".nexus-source-sdk.json"
            payload = b'{"kind":"unit artifact boundary fixture"}\n'
            manifest.write_bytes(payload)
            source = root / "consumer"
            shutil.copytree(canonical, source)
            build = root / "build"
            (build / "bin").mkdir(parents=True)
            (build / "resolved-sdk.txt").write_text(str(sdk))
            (build / "bin/external_lifecycle_consumer").write_bytes(b"image")
            rows = [{"file": str(path), "directory": str(build),
                     "arguments": ["cc", "-std=c11", "-c", str(path)]}
                    for path in (source / "consumer.c",
                                 source / "provider/soc/lifecycle_adapter.c")]
            (build / "compile_commands.json").write_text(json.dumps(rows))
            observed = consumers.check_external_provider_consumer(
                sdk, source, build,
                lambda argv: subprocess.CompletedProcess(argv, 0, "", ""))
            self.assertEqual(observed["inputs"].get("sdk_manifest"), {
                "path": str(manifest), "size": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest()})

    def test_declared_nested_input_expands_without_loading_provider_code(self):
        with tempfile.TemporaryDirectory(prefix="SDK manifest ") as value:
            root = Path(value)
            shutil.copytree(ROOT / REFERENCE, root / REFERENCE)
            services = "cmake/package/verification/common_services/external"
            shutil.copytree(ROOT / services, root / services)
            package = root / REFERENCE / "provider"
            helper = package / "configure/nested/helper.py"
            helper.parent.mkdir()
            helper.write_text('raise RuntimeError("must never execute")\n')
            manifest = package / "nexus-soc.json"
            authored = json.loads(manifest.read_text())
            authored["inputs"]["configure/nested/helper.py"] = hashlib.sha256(
                helper.read_bytes()).hexdigest()
            manifest.write_text(json.dumps(authored))
            projected = source_sdk.external_fixture_files(root)
            self.assertIn(REFERENCE + "/provider/configure/nested/helper.py",
                          projected)
            # A package-local undeclared note is not an exported build input.
            (package / "unlisted.txt").write_text("consumer-owned note\n")
            self.assertNotIn(REFERENCE + "/provider/unlisted.txt", projected)
            copied = consumers.external_package_sources(root / REFERENCE)
            self.assertEqual(copied["provider/configure/nested/helper.py"],
                             helper)
            helper.write_text("changed without refreshing\n")
            with self.assertRaisesRegex(ValueError, "digest|SHA256|identity"):
                source_sdk.external_fixture_files(root)

    def test_declared_missing_member_rejects_independently(self):
        with tempfile.TemporaryDirectory(prefix="SDK omission ") as value:
            root = Path(value)
            shutil.copytree(ROOT / REFERENCE, root / REFERENCE)
            services = "cmake/package/verification/common_services/external"
            shutil.copytree(ROOT / services, root / services)
            package = root / REFERENCE / "provider"
            (package / "soc/lifecycle_adapter.c").unlink()
            with self.assertRaisesRegex(ValueError, "required|adapter|missing"):
                source_sdk.external_fixture_files(root)


if __name__ == "__main__":
    unittest.main()
