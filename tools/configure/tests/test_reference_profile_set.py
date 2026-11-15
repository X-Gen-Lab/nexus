"""Independent document review facts are explicit inputs, never MCU code."""
from copy import deepcopy
import unittest

from tools.configure import fact_references, reference_profiles
from tools.configure.tests.f4_fixture import load_f4_fixture


def facts():
    return {"schema_version": 1,
        "datasheets": {"D1": {"revision": "3", "sha256": "a" * 64,
                              "devices": ["example_device"]}},
        "manuals": {"M1": {"revision": "5", "sha256": "b" * 64,
                            "devices": ["example_device"]}},
        "errata": {"review1": {"document_id": "E1", "revision": "7", "sha256": "c" * 64,
            "revision_order": ["A", "B"], "sections": ["1.2"],
            "applicability": {"1.2": "AN"}, "devices": ["example_device"],
            "reference_manual": "M1", "reviewed_revision_ids": [1, 2],
            "mode_scope": ["gpio:output"], "backends": ["baremetal"],
            "clock_policies": ["fixed-run"]}}}


class ReferenceProfileSetTests(unittest.TestCase):
    def test_document_and_mode_profiles_need_no_tool_identifier_registration(self):
        profiles = reference_profiles.ProfileSet.from_validated(facts())
        self.assertEqual(profiles.datasheet({"document_id": "D1", "revision": "3",
                                            "sha256": "a" * 64}).devices,
                         frozenset({"example_device"}))
        selected = profiles.select("review1")
        self.assertEqual(selected.mode_scope, frozenset({"gpio:output"}))
        fact_references.admit_clock_policy({"software_audit": {"profile": "review1"}},
                                          "fixed-run", profiles=profiles)
        with self.assertRaises(ValueError):
            profiles.manual({"document_id": "M1", "revision": "5", "sha256": "a" * 64})
        with self.assertRaises(ValueError):
            fact_references.admit_clock_policy({"software_audit": {"profile": "review1"}},
                                              "unreviewed", profiles=profiles)
        with self.assertRaises(TypeError):
            profiles.errata["new"] = selected

    def test_profile_rejects_unknown_fields_hashes_and_incomplete_applicability(self):
        for mutate in (lambda value: value.update(unknown=True),
                       lambda value: value["manuals"]["M1"].update(sha256="bad"),
                       lambda value: value["errata"]["review1"].update(applicability={}),
                       lambda value: value["errata"]["review1"].update(revision_order=["A", "A"])):
            value = deepcopy(facts())
            mutate(value)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                reference_profiles.ProfileSet.from_validated(value)

    def test_f4_review_uses_the_profile_bytes_owned_by_its_package(self):
        fixture = load_f4_fixture("STM32F407ZGT6")
        profiles = fixture.provider.REFERENCE_PROFILES
        self.assertIsInstance(profiles, reference_profiles.ProfileSet)
        self.assertIn(fixture.package.fact("review_profiles"), fixture.snapshots)
        from tools.configure import configure
        documents = configure.load(fixture.package.fact("references"))
        soc = configure.load(fixture.package.fact("soc"))
        self.assertIs(fact_references.validate(documents, soc["variants"], profiles=profiles), documents)


if __name__ == "__main__":
    unittest.main()
