"""Authoring/factory/provider consistency and public reference boundaries."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure
from tools.configure import factory
from tools.configure import matrix



class MaintainedMatrixTests(unittest.TestCase):
    def test_authoring_factory_provider_and_cmake_contracts_agree(self):
        contract = matrix.check_contracts()
        self.assertEqual(set(contract["kinds"]), configure.KINDS)
        self.assertEqual(contract["components"], sorted(matrix.COMPONENTS))
        self.assertEqual(contract["families"], ["stm32f4"])

    def test_cmake_component_mapping_drift_is_rejected(self):
        name = "cmake/platform/Components.cmake"
        original = (configure.ROOT / name).read_text(encoding="utf-8")
        for index, altered in enumerate((
                original.replace('"log=Nexus::Log"', ''),
                original.replace('"log=Nexus::Log"',
                                 '"unknown=Nexus::Log"'))):
            with self.subTest(mapping=index):
                with tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    path = root / name
                    path.parent.mkdir(parents=True)
                    path.write_text(altered, encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, "components"):
                        matrix.check_contracts(root)

    def test_missing_factory_kind_is_rejected(self):
        with patch.object(factory, "KINDS", tuple(
                kind for kind in factory.KINDS if kind != "gpio")):
            with self.assertRaisesRegex(ValueError, "kinds"):
                matrix.check_contracts()

    def test_unmapped_component_is_rejected(self):
        with patch.object(matrix, "COMPONENTS", matrix.COMPONENTS | {"unknown"}):
            with self.assertRaisesRegex(ValueError, "components"):
                matrix.check_contracts()

    def test_reviewed_alternate_interface_requires_execution_traits(self):
        original = matrix.mode_traits

        def without_frames(kind, mode):
            if kind == "eth_frames":
                raise ValueError("alternate execution contract is missing")
            return original(kind, mode)

        with patch.object(matrix, "mode_traits", side_effect=without_frames):
            with self.assertRaisesRegex(ValueError, "alternate execution"):
                matrix.check_contracts()

    def test_endpoint_personality_is_checked_without_becoming_a_controller(self):
        contract = matrix.check_contracts()
        self.assertNotIn("parallel16", contract["kinds"])
        self.assertNotIn("memory_window", contract["kinds"])
        original = matrix.mode_traits

        def without_parallel(kind, mode):
            if kind == "parallel16":
                raise ValueError("endpoint execution contract is missing")
            return original(kind, mode)

        with patch.object(matrix, "mode_traits", side_effect=without_parallel):
            with self.assertRaisesRegex(ValueError, "endpoint execution"):
                matrix.check_contracts()

    def test_explicit_references_resolve_without_claiming_physical_support(self):
        result = matrix.resolve_catalogue()
        self.assertEqual(result["physical_status"], "not_executed")
        references = result["references"]
        self.assertTrue(references)
        self.assertEqual(len({item["assembly"] for item in references}), len(references))
        self.assertEqual({item["family"] for item in references}, {"stm32f4"})
        for item in references:
            self.assertEqual(item["physical_status"], "not_executed")
            self.assertEqual(item["scope"], "public_reference")
            self.assertEqual(len(item["configuration_sha256"]), 64)






    def test_catalogue_still_rejects_missing_duplicate_and_unknown_references(self):
        original = json.loads((configure.ROOT / matrix.CATALOGUE).read_text())
        entries = original["assemblies"]
        selected = entries[0]
        variants = (([name for name in entries if name != selected],
                     "catalogue differs"),
                    (entries + [selected], "duplicate"),
                    (entries + ["unknown-reference.toml"], "catalogue differs"))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "catalogue.json"
            for names, message in variants:
                with self.subTest(message=message, entries=names):
                    path.write_text(json.dumps({"schema_version": 1,
                                                "assemblies": names}))
                    with patch.object(matrix, "CATALOGUE", str(path)):
                        with self.assertRaisesRegex(ValueError, message):
                            matrix.resolve_catalogue()


if __name__ == "__main__":
    unittest.main()
