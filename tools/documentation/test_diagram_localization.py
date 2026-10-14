"""Translated diagrams preserve topology and reject incomplete label maps."""

import importlib
from pathlib import Path
import tempfile
import unittest

from tools.documentation import diagrams


class DiagramLocalizationTests(unittest.TestCase):
    def setUp(self):
        self.module = importlib.import_module("tools.documentation.diagram_localization")
        self.source = ('digraph flow { graph [label="当前生命周期"]; '
                       'a [label="请求"]; b [label="SETTLED"]; '
                       'a -> b [label="最后访问后发布"]; }\n')
        self.mapping = {
            "schema_version": 1,
            "en": {"当前生命周期": "Current lifecycle", "请求": "Request",
                   "SETTLED": "SETTLED", "最后访问后发布": "Publish after final access"},
            "zh_CN": {"当前生命周期": "当前生命周期", "请求": "请求",
                      "SETTLED": "SETTLED", "最后访问后发布": "最后访问后发布"},
        }

    def test_only_labels_change_and_technical_state_is_preserved(self):
        result = self.module.localize(self.source, self.mapping, "en")
        self.assertIn('label="Current lifecycle"', result)
        self.assertIn('label="SETTLED"', result)
        self.assertEqual(diagrams.LABEL.sub('label=""', result),
                         diagrams.LABEL.sub('label=""', self.source))

    def test_canonical_language_uses_its_explicit_labels(self):
        self.assertEqual(self.module.localize(self.source, self.mapping, "zh_CN"),
                         self.source)

    def test_missing_label_or_locale_fails_without_silent_fallback(self):
        del self.mapping["en"]["请求"]
        with self.assertRaisesRegex(ValueError, "missing"):
            self.module.localize(self.source, self.mapping, "en")
        with self.assertRaisesRegex(ValueError, "language"):
            self.module.localize(self.source, self.mapping, "fr")

    def test_new_source_label_requires_translation(self):
        with self.assertRaisesRegex(ValueError, "missing"):
            self.module.localize(self.source.replace("请求", "新的请求"),
                                 self.mapping, "en")

    def test_quotes_and_multiline_labels_remain_safe_dot(self):
        self.mapping["en"]["请求"] = 'Request "A"\nretain storage'
        result = self.module.localize(self.source, self.mapping, "en")
        self.assertIn('Request \\"A\\"\\nretain storage', result)
        diagrams.validate_source(result)
        self.assertEqual(diagrams.LABEL.sub('label=""', result),
                         diagrams.LABEL.sub('label=""', self.source))

    def test_unknown_mapping_fields_and_nontext_values_fail(self):
        self.mapping["latest"] = {}
        with self.assertRaisesRegex(ValueError, "schema"):
            self.module.localize(self.source, self.mapping, "en")
        del self.mapping["latest"]
        self.mapping["en"]["请求"] = None
        with self.assertRaisesRegex(ValueError, "text"):
            self.module.localize(self.source, self.mapping, "en")

    def test_image_selection_is_local_and_cannot_hide_missing_assets(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "docs"
            page = root / "explanation/factory.md"
            page.parent.mkdir(parents=True)
            folder = root / "assets/diagrams"
            target = folder / "localized/en/factory.svg"
            target.parent.mkdir(parents=True)
            (folder / "labels.json").write_text("{}")
            target.write_text("svg")
            uri = "../assets/diagrams/factory.svg"
            self.assertEqual(self.module.image_uri(uri, page, root, "en"),
                             "/assets/diagrams/localized/en/factory.svg")
            self.assertEqual(self.module.image_uri("https://example.com/factory.svg",
                                                  page, root, "en"),
                             "https://example.com/factory.svg")
            target.unlink()
            with self.assertRaisesRegex(ValueError, "missing"):
                self.module.image_uri(uri, page, root, "en")
if __name__ == "__main__":
    unittest.main()
