"""Authoring/factory/provider consistency and public reference boundaries."""
import unittest
from unittest.mock import patch

import configure
import factory
import matrix


class MaintainedMatrixTests(unittest.TestCase):
    def test_authoring_factory_provider_and_cmake_contracts_agree(self):
        contract = matrix.check_contracts()
        self.assertEqual(set(contract["kinds"]), configure.KINDS)
        self.assertIn("stm32h743", contract["families"])
        self.assertIn("stm32u575", contract["families"])

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

        def without_encoder(kind, mode):
            if kind == "encoder":
                raise ValueError("alternate execution contract is missing")
            return original(kind, mode)

        with patch.object(matrix, "mode_traits", side_effect=without_encoder):
            with self.assertRaisesRegex(ValueError, "alternate execution"):
                matrix.check_contracts()

    def test_explicit_references_resolve_without_claiming_physical_support(self):
        result = matrix.resolve_catalogue()
        self.assertEqual(result["physical_status"], "not_executed")
        references = result["references"]
        self.assertGreaterEqual(len(references), 39)
        self.assertEqual(len({item["assembly"] for item in references}), len(references))
        for item in references:
            self.assertEqual(item["physical_status"], "not_executed")
            self.assertEqual(len(item["configuration_sha256"]), 64)
            if item["family"] == "native":
                self.assertEqual(item["scope"], "native_model")
            elif item["family"] in {"stm32h743", "stm32u575"}:
                self.assertTrue({entry["kind"] for entry in item["selected"]}.issubset(
                    {"gpio", "uart"}))


if __name__ == "__main__":
    unittest.main()
