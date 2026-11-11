"""Reviewed reference indices reject fictitious identity and qualification."""
from tools.configure.providers.descriptor import source_directory
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from tools.configure import configure as gate, dependencies, fact_references
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
        self.assertEqual(data["documents"]["es0182"]["sha256"],
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
        for path in ((), ("documents", "ds8626"), ("package_review", 0),
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
                data["documents"]["ds8626"][field] = value
                with self.assertRaises(ValueError):
                    fact_references.validate(data, variants)

    def test_missing_revision_or_reviewed_section_is_rejected(self):
        for field, value in (("revision", ""), ("document_date", ""),
                             ("anchors", {}),
                             ("anchors", {"package_gpio": ""})):
            with self.subTest(field=field):
                data, variants = self.references()
                data["documents"]["ds8626"][field] = value
                with self.assertRaises(ValueError):
                    fact_references.validate(data, variants)

    def test_document_kind_and_review_link_cannot_be_swapped(self):
        for field, value in (("document", "not-present"),
                             ("document", "es0182")):
            with self.subTest(value=value):
                data, variants = self.references()
                data["package_review"][0][field] = value
                with self.assertRaisesRegex(ValueError, "package"):
                    fact_references.validate(data, variants)
        data, variants = self.references()
        data["documents"]["es0182"]["kind"] = "datasheet"
        with self.assertRaisesRegex(ValueError, "kind|errata"):
            fact_references.validate(data, variants)

    def test_missing_duplicate_or_foreign_part_cannot_share_review(self):
        for parts in ([], ["STM32F407VET6"], ["OTHER_PACKAGE"],
                      ["STM32F407VET6", "STM32F407VET6"]):
            with self.subTest(parts=parts):
                data, variants = self.references()
                data["package_review"][0]["variants"] = parts
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

    def test_named_datasheets_cannot_cover_another_device_group(self):
        for part, wrong in (("STM32F405ZGT6", "ds8597"),
                            ("STM32F415ZGT6", "ds8626"),
                            ("STM32F417ZGT6", "ds8626")):
            with self.subTest(part=part):
                data, variants = self.references()
                for package in data["package_review"]:
                    if part in package["variants"]:
                        package["variants"].remove(part)
                    if package["document"] == wrong:
                        package["variants"].append(part)
                with self.assertRaisesRegex(ValueError, "datasheet"):
                    fact_references.validate(data, variants)

    def test_review_groups_reject_overlap_missing_and_stale_datasheets(self):
        for change in ("overlap", "missing", "digest", "revision"):
            with self.subTest(change=change):
                data, variants = self.references()
                if change == "overlap":
                    data["package_review"][0]["variants"].append("STM32F415ZGT6")
                elif change == "missing":
                    data["package_review"][0]["variants"].remove("STM32F405ZGT6")
                elif change == "digest":
                    data["documents"]["ds8597"]["sha256"] = "a" * 64
                else:
                    data["documents"]["ds8597"]["revision"] = "11"
                with self.assertRaises(ValueError):
                    fact_references.validate(data, variants)

    def test_current_audit_requires_its_explicit_review_profile(self):
        data, variants = self.references()
        data["errata_review"]["software_audit"].pop("profile", None)
        with self.assertRaisesRegex(ValueError, "profile"):
            fact_references.validate(data, variants)

    def test_named_profile_binds_the_complete_locked_document(self):
        data, variants = self.references()
        audit = data["errata_review"]["software_audit"]
        audit["profile"] = "es0182-rev19"
        self.assertIs(fact_references.validate(data, variants), data)
        data["documents"]["es0182"]["sha256"] = "a" * 64
        audit["document_sha256"] = "a" * 64
        with self.assertRaisesRegex(ValueError, "profile.*identity"):
            fact_references.validate(data, variants)

    def test_unknown_or_malformed_audit_profile_cannot_select_policy(self):
        for profile in ("es0182-rev20", "../es0182-rev19", False):
            with self.subTest(profile=profile):
                data, variants = self.references()
                data["errata_review"]["software_audit"]["profile"] = profile
                with self.assertRaisesRegex(ValueError, "profile"):
                    fact_references.validate(data, variants)

    def test_external_code_closure_can_execute_complete_review_profile(self):
        code = """
import json
import sys
sys.path.insert(0, sys.argv[1])
from tools.configure import reference_profiles
profile = reference_profiles.select('es0182-rev19')
assert 'tools.configure.stm32f4' not in sys.modules
try:
    profile.applicability['2.1.3'] = '--------'
except TypeError:
    pass
else:
    raise AssertionError('Review applicability is mutable')
print(json.dumps({
    'document': [profile.document_id, profile.revision, profile.sha256],
    'revisions': profile.revision_order,
    'sections': sorted(profile.sections),
    'applicability': dict(profile.applicability)}))
"""
        with tempfile.TemporaryDirectory() as directory:
            capsule = Path(directory)
            for name in dependencies.source_names(
                    "platform", "external-example", external=True):
                target = capsule / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(gate.ROOT / name, target)
            result = subprocess.run(
                [sys.executable, "-I", "-S", "-B", "-c", code, directory],
                cwd=capsule, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        profile = json.loads(result.stdout)
        self.assertEqual(profile["document"], ["ES0182", "19",
            "1d3da0f99b85013548d17df07919516bf862f8f38cbd1a61f08e75abb8e4c54f"])
        self.assertEqual(profile["revisions"],
                         ["A", "Z", "1", "2", "Y", "4", "5", "6"])
        sections = {f"2.{group}.{item}"
                    for group, count in enumerate(
                        (3, 20, 3, 6, 1, 2, 4, 4, 6, 6, 9, 7, 1, 6, 2, 12), 1)
                    for item in range(1, count + 1)}
        self.assertEqual(len(sections), 92)
        self.assertEqual(set(profile["sections"]), sections)
        self.assertEqual(set(profile["applicability"]), sections)
        for section, value in {
                "2.1.3": "AAAAAAAA", "2.2.1": "N-------",
                "2.2.2": "A-------", "2.2.8": "-NNNNNNN",
                "2.4.1": "NNNNNNNN", "2.7.2": "DDDDDDDD",
                "2.16.3": "PPPPPPPP"}.items():
            with self.subTest(section=section):
                self.assertEqual(profile["applicability"][section], value)

if __name__ == "__main__":
    unittest.main()
