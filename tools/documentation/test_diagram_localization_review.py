"""Independent regressions for required diagram policies and asset boundaries."""

import json
from pathlib import Path
import tempfile
import unittest

from tools.documentation import diagram_localization


class DiagramLocalizationReviewTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.folder = self.root / "diagrams"
        self.folder.mkdir()
        self.source = 'digraph flow { a [label="Retain borrowed storage"]; }\n'
        self.mapping = {
            "schema_version": 1,
            "en": {"Retain borrowed storage": "Retain borrowed storage"},
            "zh_CN": {"Retain borrowed storage": "保留借用存储"},
        }
        self.policy = self.folder / "labels.json"
        self.policy.write_text(json.dumps(self.mapping), encoding="utf-8")
        (self.folder / "flow.dot").write_text(self.source, encoding="utf-8")
        for language in diagram_localization.LANGUAGES:
            target = self.folder / "localized" / language / "flow.dot"
            target.parent.mkdir(parents=True)
            target.write_text(diagram_localization.localize(
                self.source, self.mapping, language), encoding="utf-8")
            target.with_suffix(".svg").write_text(
                '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1 1"/>',
                encoding="utf-8")

    def test_required_policy_cannot_disappear_while_variants_remain(self):
        self.policy.unlink()
        self.assertTrue(diagram_localization.check(self.folder)["errors"])

    def test_dangling_policy_symlink_cannot_select_untranslated_fallback(self):
        self.policy.unlink()
        self.policy.symlink_to(self.root / "missing-policy.json")
        self.assertTrue(diagram_localization.check(self.folder)["errors"])

    def test_nonempty_contract_label_cannot_be_translated_to_empty(self):
        self.mapping["en"]["Retain borrowed storage"] = ""
        with self.assertRaises(ValueError):
            diagram_localization.localize(self.source, self.mapping, "en")

    def test_whitespace_translation_cannot_erase_contract_conditions(self):
        self.mapping["en"]["Retain borrowed storage"] = " \t\n"
        with self.assertRaises(ValueError):
            diagram_localization.localize(self.source, self.mapping, "en")

    def test_extra_symlink_directory_is_not_an_admitted_local_asset(self):
        outside = self.root / "outside"
        outside.mkdir()
        (outside / "hidden.svg").write_text("external", encoding="utf-8")
        (self.folder / "localized" / "extra").symlink_to(outside,
                                                       target_is_directory=True)
        self.assertTrue(diagram_localization.check(self.folder)["errors"])

    def test_mapping_does_not_read_canonical_source_through_symlink(self):
        outside = self.root / "outside.dot"
        outside.write_text(self.source, encoding="utf-8")
        canonical = self.folder / "flow.dot"
        canonical.unlink()
        canonical.symlink_to(outside)
        with self.assertRaises(ValueError):
            diagram_localization.read_mapping(self.folder)

    def test_image_selector_cannot_hide_missing_policy_for_existing_variants(self):
        folder = self.root / "assets" / "diagrams"
        folder.parent.mkdir()
        self.folder.rename(folder)
        (folder / "labels.json").unlink()
        page = self.root / "explanation" / "factory.md"
        with self.assertRaises(ValueError):
            diagram_localization.image_uri("../assets/diagrams/flow.svg", page,
                                           self.root, "en")

    def test_legacy_fixture_without_policy_or_variants_remains_supported(self):
        folder = self.root / "legacy"
        folder.mkdir()
        (folder / "plain.dot").write_text('digraph plain {}', encoding="utf-8")
        self.assertEqual(diagram_localization.check(folder),
                         {"localized_diagrams": 0, "errors": []})

    def test_original_empty_layout_label_remains_explicitly_allowed(self):
        source = 'digraph layout { graph [label=""]; }'
        mapping = {"schema_version": 1, "en": {"": ""}, "zh_CN": {"": ""}}
        self.assertEqual(diagram_localization.localize(source, mapping, "en"),
                         source)


if __name__ == "__main__":
    unittest.main()
