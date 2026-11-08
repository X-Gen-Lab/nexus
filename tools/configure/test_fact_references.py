"""Reviewed reference indices reject fictitious identity and qualification."""
from tools.configure.providers.descriptor import source_directory
import copy
import json
import unittest

from tools.configure import configure as gate, fact_references


class FactReferenceContractTests(unittest.TestCase):

    def references(self, family='stm32f4'):
        package = gate.ROOT / source_directory(family)
        return (json.loads((package / "references.json").read_text()),
                json.loads((package / "soc.json").read_text())["variants"])

    def test_pin_source_keeps_errata_and_hardware_limits_explicit(self):
        for family in ('stm32f4',):
            with self.subTest(family=family):
                data, variants = self.references(family)
                before = copy.deepcopy(data)
                self.assertEqual(fact_references.validate(data, variants), data)
                self.assertEqual(data, before)
                self.assertEqual(data["errata_review"]["status"],
                                 "applicability_only")
                self.assertEqual(
                    data["errata_review"]["hardware_qualification"],
                    "not_executed")

    def test_unknown_fields_cannot_invent_workaround_or_document_scope(self):
        for path in ((), ("documents", "datasheet"), ("package_review",),
                     ("errata_review",)):
            with self.subTest(path=path):
                data, variants = self.references()
                node = data
                for name in path:
                    node = node[name]
                node["unreviewed_extension"] = True
                with self.assertRaisesRegex(ValueError, "fields"):
                    fact_references.validate(data, variants)

    def test_reference_identity_requires_https_digest_and_nonzero_bytes(self):
        for field, value in (("url", "http://example.com/doc.pdf"),
                             ("url", "https://secret@example.com/doc.pdf"),
                             ("url", "https:///doc.pdf"),
                             ("sha256", "a" * 63), ("sha256", "G" * 64),
                             ("bytes", 0), ("bytes", True)):
            with self.subTest(field=field, value=value):
                data, variants = self.references()
                data["documents"]["datasheet"][field] = value
                with self.assertRaises(ValueError):
                    fact_references.validate(data, variants)

    def test_missing_revision_or_reviewed_section_is_rejected(self):
        for field, value in (("revision", ""), ("document_date", ""),
                             ("anchors", {}),
                             ("anchors", {"package_gpio": ""})):
            with self.subTest(field=field):
                data, variants = self.references()
                data["documents"]["datasheet"][field] = value
                with self.assertRaises(ValueError):
                    fact_references.validate(data, variants)

    def test_document_kind_and_review_link_cannot_be_swapped(self):
        for field, value in (("document", "not-present"),
                             ("document", "errata")):
            with self.subTest(value=value):
                data, variants = self.references()
                data["package_review"][field] = value
                with self.assertRaisesRegex(ValueError, "package"):
                    fact_references.validate(data, variants)
        data, variants = self.references()
        data["documents"]["errata"]["kind"] = "datasheet"
        with self.assertRaisesRegex(ValueError, "kind"):
            fact_references.validate(data, variants)

    def test_missing_duplicate_or_foreign_part_cannot_share_review(self):
        for parts in ([], ["STM32F407VET6"], ["OTHER_PACKAGE"],
                      ["STM32F407VET6", "STM32F407VET6"]):
            with self.subTest(parts=parts):
                data, variants = self.references()
                data["package_review"]["variants"] = parts
                with self.assertRaisesRegex(ValueError, "variants"):
                    fact_references.validate(data, variants)

    def test_document_availability_never_establishes_errata_or_hil(self):
        for field, value in (("status", "fully_audited"),
                             ("silicon_revision", "A"),
                             ("mode_workarounds", "all_qualified"),
                             ("hardware_qualification", "passed")):
            with self.subTest(field=field):
                data, variants = self.references()
                data["errata_review"][field] = value
                with self.assertRaisesRegex(ValueError, "qualification|scope"):
                    fact_references.validate(data, variants)






if __name__ == "__main__":
    unittest.main()
