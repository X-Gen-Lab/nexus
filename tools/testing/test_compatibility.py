"""Version policy is metadata, never a competing configuration authority."""
import copy
import json
from pathlib import Path
import tempfile
import shutil
from unittest.mock import patch
import unittest
from tools.maintenance import compatibility
from tools.configure import metadata

ROOT = Path(__file__).resolve().parents[2]

class CompatibilityTests(unittest.TestCase):
    def setUp(self):
        self.policy = json.loads((ROOT / compatibility.POLICY).read_text())
        self.observed = {"semantic_version": "1.0.0", "schemas": dict(self.policy["schemas"]),
                         "source_sdk_exact_only": True, "factory_abi_epoch": 1}

    def test_current_policy_matches_actual_authorities(self):
        report = compatibility.check(ROOT)
        self.assertEqual(report["status"], "consistent")
        self.assertEqual(report["factory_abi_epoch"], 1)
        self.assertFalse(report["binary_abi_supported"])
        self.assertFalse(report["physical_qualification"])
        self.assertGreaterEqual(len(report["inputs"]), 7)

    def test_snapshot_policy_checks_actual_supported_factory_epoch(self):
        self.assertIs(metadata.policy_contract(self.policy, expected_factory_epoch=1), self.policy)
        for epoch in (0, 2, True, "1"):
            with self.subTest(epoch=epoch), self.assertRaisesRegex(ValueError, "epoch"):
                metadata.policy_contract(self.policy, expected_factory_epoch=epoch)

    def test_header_lines_come_from_validated_policy(self):
        self.assertEqual(metadata.header_lines(self.policy), [
            '#define NEXUS_PLATFORM_VERSION "1.0.0"',
            '#define NEXUS_FACTORY_ABI_EPOCH 1U'])
        invalid = copy.deepcopy(self.policy); invalid["semantic_version"] = '1.0.0"'
        with self.assertRaises(ValueError): metadata.header_lines(invalid)

    def test_supported_factory_code_generation_is_observed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.fixture(directory)
            path = root / "tools/configure/factory.py"
            path.write_text(path.read_text().replace("FACTORY_ABI_EPOCH = 1", "FACTORY_ABI_EPOCH = 2"))
            with self.assertRaisesRegex(ValueError, "factory.*epoch"):
                compatibility.check(root)

    def test_exact_source_generation_is_accepted(self):
        report = compatibility.validate(self.policy, self.observed, 1)
        self.assertEqual(report["semantic_version"], "1.0.0")

    def test_unknown_requested_epoch_is_rejected(self):
        for epoch in (0, 2, 999, True, "1"):
            with self.subTest(epoch=epoch), self.assertRaises(ValueError):
                compatibility.validate(self.policy, self.observed, epoch)

    def test_missing_policy_is_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(ValueError):
            compatibility.load_policy(Path(directory))

    def test_duplicate_json_policy_keys_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / compatibility.POLICY
            path.parent.mkdir(); path.write_text('{"schema_version":1,"schema_version":1}')
            with self.assertRaisesRegex(ValueError, "Duplicate"):
                compatibility.load_policy(Path(directory))

    def test_policy_symlink_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / compatibility.POLICY
            path.parent.mkdir(); path.symlink_to(ROOT / compatibility.POLICY)
            with self.assertRaisesRegex(ValueError, "symlink|regular"):
                compatibility.load_policy(Path(directory))

    def test_unknown_or_missing_policy_fields_are_rejected(self):
        for key in ("schema_version", "kind", "semantic_version", "factory_abi_epoch", "source_sdk", "schemas"):
            changed = copy.deepcopy(self.policy); del changed[key]
            with self.subTest(key=key), self.assertRaises(ValueError):
                compatibility.validate(changed, self.observed)
        changed = copy.deepcopy(self.policy); changed["binary_compatible"] = True
        with self.assertRaises(ValueError): compatibility.validate(changed, self.observed)

    def test_source_only_exact_policy_cannot_claim_binary_compatibility(self):
        for key, value in (("delivery", "binary"), ("version_matching", "semver_range"), ("binary_abi", "stable")):
            changed = copy.deepcopy(self.policy); changed["source_sdk"][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                compatibility.validate(changed, self.observed)

    def test_boolean_epoch_or_schema_are_not_integer_versions(self):
        for key in self.policy["schemas"]:
            changed = copy.deepcopy(self.policy); changed["schemas"][key] = True
            with self.subTest(key=key), self.assertRaises(ValueError):
                compatibility.validate(changed, self.observed)
        for key in ("schema_version", "factory_abi_epoch"):
            changed = copy.deepcopy(self.policy); changed[key] = True
            with self.subTest(key=key), self.assertRaises(ValueError):
                compatibility.validate(changed, self.observed)

    def test_actual_project_semantic_version_must_match(self):
        observed = copy.deepcopy(self.observed); observed["semantic_version"] = "1.0.1"
        with self.assertRaisesRegex(ValueError, "version"):
            compatibility.validate(self.policy, observed)

    def test_each_real_schema_epoch_must_match(self):
        for key in self.observed["schemas"]:
            observed = copy.deepcopy(self.observed); observed["schemas"][key] += 1
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "schema"):
                compatibility.validate(self.policy, observed)

    def test_new_schema_requires_explicit_policy_review(self):
        observed = copy.deepcopy(self.observed); observed["schemas"]["new_schema"] = 1
        with self.assertRaisesRegex(ValueError, "schema"):
            compatibility.validate(self.policy, observed)

    def test_real_package_cannot_accept_version_ranges(self):
        observed = copy.deepcopy(self.observed); observed["source_sdk_exact_only"] = False
        with self.assertRaisesRegex(ValueError, "exact"):
            compatibility.validate(self.policy, observed)

    def test_generated_factory_macro_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "nexus_factory.h"
            header.write_text('#define NEXUS_FACTORY_ABI_EPOCH 2U\n#define NEXUS_PLATFORM_VERSION "1.0.0"\n')
            with self.assertRaisesRegex(ValueError, "epoch"):
                compatibility.check(ROOT, generated=header)

    def test_generated_factory_version_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "nexus_factory.h"
            header.write_text('#define NEXUS_FACTORY_ABI_EPOCH 1U\n#define NEXUS_PLATFORM_VERSION "1.0.1"\n')
            with self.assertRaisesRegex(ValueError, "version"):
                compatibility.check(ROOT, generated=header)

    def test_generated_factory_matching_macros_are_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "nexus_factory.h"
            header.write_text('#define NEXUS_FACTORY_ABI_EPOCH 1U\n#define NEXUS_PLATFORM_VERSION "1.0.0"\n')
            self.assertEqual(compatibility.check(ROOT, generated=header)["status"], "consistent")

    def test_package_inspection_verifies_real_manifest_not_marker(self):
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / ".nexus-source-sdk.json").write_text('{}')
            with self.assertRaises(ValueError): compatibility.check(ROOT, sdk=Path(directory))


    def fixture(self, directory):
        root = Path(directory)
        names = [compatibility.POLICY, "CMakeLists.txt", "tools/configure/authored.py",
                 "tools/configure/configure.py", "tools/configure/runtime.py",
                 "tools/configure/assembly.schema.json", "tools/configure/factory.py", "cmake/package/package_source_sdk.py",
                 "cmake/package/NexusConfigVersion.cmake.in", "boards/native_reference/board.json",
                 "soc/native/soc.json", "soc/native/routes.json"]
        for name in names:
            target = root / name; target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        return root

    def test_changed_policy_during_observation_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.fixture(directory)
            observed = compatibility.observe(root)
            def changing_reader(_root):
                policy = copy.deepcopy(self.policy); policy["factory_abi_epoch"] = 2
                (root / compatibility.POLICY).write_text(json.dumps(policy))
                return observed
            with patch.object(compatibility, "observe", side_effect=changing_reader):
                with self.assertRaisesRegex(ValueError, "changed"):
                    compatibility.check(root)

    def test_actual_schema_change_is_detected_without_mocking_resolver(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.fixture(directory)
            path = root / "tools/configure/configure.py"
            path.write_text(path.read_text().replace('integer(board["schema_version"], 1, 1, "Board schema")',
                                                    'integer(board["schema_version"], 2, 2, "Board schema")'))
            board = root / "boards/native_reference/board.json"
            data = json.loads(board.read_text()); data["schema_version"] = 2
            board.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "schema"):
                compatibility.check(root)

    def test_actual_package_template_cannot_widen_equal_version_ranges(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "version.cmake.in"
            source = (ROOT / "cmake/package/NexusConfigVersion.cmake.in").read_text()
            path.write_text(source.replace('NOT PACKAGE_FIND_VERSION_RANGE AND ', ''))
            with self.assertRaisesRegex(ValueError, "exact"):
                compatibility.package_exact(path, "1.0.0")

    def test_zero_semantic_version_still_uses_distinct_negative_probe(self):
        self.assertTrue(compatibility.package_exact(
            ROOT / "cmake/package/NexusConfigVersion.cmake.in", "0.0.0"))

if __name__ == "__main__": unittest.main()
