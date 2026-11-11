"""Reviewed reference indices reject fictitious identity and qualification."""
from tools.configure.providers.descriptor import source_directory
import copy
import json
import unittest

from tools.configure import configure as gate, fact_references
from tools.configure import stm32f4


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
                                 "software_audited_with_constraints")
                self.assertEqual(
                    data["errata_review"]["hardware_qualification"],
                    "not_executed")

    def test_software_audit_covers_every_locked_section_cut_and_current_mode(self):
        data, variants = self.references()
        fact_references.validate(data, variants)
        audit = data["errata_review"]["software_audit"]
        expected = {f"2.{group}.{item}"
                    for group, count in enumerate(
                        (3, 20, 3, 6, 1, 2, 4, 4, 6, 6, 9, 7, 1, 6, 2, 12), 1)
                    for item in range(1, count + 1)}
        self.assertEqual(expected, {row["section"] for row in audit["items"]})
        self.assertEqual(92, len(audit["items"]))
        self.assertEqual(["A", "Z", "1", "2", "Y", "4", "5", "6"],
                         audit["revision_order"])
        self.assertEqual(sorted(f"{kind}:{mode}"
                                for kind, mode in stm32f4._MODE_IMPLEMENTATIONS),
                         audit["maintained_modes"])
        self.assertEqual(data["documents"]["errata"]["sha256"],
                         audit["document_sha256"])
        for row in audit["items"]:
            for name in row["implementation"] + row["regressions"]:
                with self.subTest(section=row["section"], source=name):
                    self.assertTrue((gate.ROOT / name).is_file())

    def test_audit_rejects_missing_duplicate_foreign_or_unassessed_items(self):
        for mutation in ("missing", "duplicate", "foreign", "unassessed"):
            with self.subTest(mutation=mutation):
                data, variants = self.references()
                items = data["errata_review"]["software_audit"]["items"]
                if mutation == "missing":
                    items.pop()
                elif mutation == "duplicate":
                    items[-1] = copy.deepcopy(items[0])
                elif mutation == "foreign":
                    items[-1]["section"] = "2.99.1"
                else:
                    items[0]["assessment"] = "passed"
                with self.assertRaisesRegex(ValueError, "audit"):
                    fact_references.validate(data, variants)

    def test_audit_revision_and_evidence_cannot_be_invented(self):
        changes = (("revisions", "-------A"),
                   ("revisions", "AAAAAAAAA"),
                   ("implementation", []),
                   ("condition", ""),
                   ("handling", ""),
                   ("regressions", ["../../foreign.py"]))
        for field, value in changes:
            with self.subTest(field=field, value=value):
                data, variants = self.references()
                data["errata_review"]["software_audit"]["items"][0][field] = value
                with self.assertRaisesRegex(ValueError, "audit"):
                    fact_references.validate(data, variants)

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
