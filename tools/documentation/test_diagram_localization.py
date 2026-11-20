"""Translated diagrams preserve topology and reject incomplete label maps."""

import importlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

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
            folder.mkdir(parents=True)
            output = Path(temporary) / "build/diagrams"
            target = output / "en/factory.svg"
            target.parent.mkdir(parents=True)
            (folder / "factory.dot").write_text(self.source, encoding="utf-8")
            (folder / "factory.labels.json").write_text(json.dumps(self.mapping), encoding="utf-8")
            target.write_text("svg")
            uri = "../assets/diagrams/factory.svg"
            self.assertEqual(self.module.image_uri(
                uri, page, root, "en", output=output),
                os.path.relpath(target, page.parent))
            self.assertEqual(self.module.image_uri("https://example.com/factory.svg",
                                                  page, root, "en"),
                             "https://example.com/factory.svg")
            target.unlink()
            with self.assertRaisesRegex(ValueError, "missing"):
                self.module.image_uri(uri, page, root, "en", output=output)
    def test_label_check_does_not_require_generated_source_assets(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary) / "diagrams"
            folder.mkdir()
            (folder / "flow.dot").write_text(self.source, encoding="utf-8")
            (folder / "flow.labels.json").write_text(json.dumps(self.mapping),
                                                 encoding="utf-8")
            self.assertEqual(self.module.check(folder),
                             {"localized_diagrams": 2, "errors": []})
            self.assertFalse((folder / "localized").exists())

    def test_generation_cannot_write_localized_outputs_into_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary) / "diagrams"
            folder.mkdir()
            with self.assertRaisesRegex(ValueError, "source"):
                self.module.write(folder, env={}, output=folder / "localized")

    def test_generation_cannot_overwrite_source_through_ancestor_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            folder = root / "en"
            folder.mkdir()
            source = folder / "flow.dot"
            source.write_text(self.source, encoding="utf-8")
            (folder / "flow.labels.json").write_text(json.dumps(self.mapping),
                                                 encoding="utf-8")
            with patch.object(diagrams, "render", return_value=b"svg"):
                with self.assertRaisesRegex(ValueError, "source"):
                    self.module.write(folder, env={}, output=root,
                                      languages=("en",))
            self.assertEqual(source.read_text(encoding="utf-8"), self.source)

    def test_generation_rejects_output_and_parent_symlinks(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            folder = root / "source"
            folder.mkdir()
            output = root / "build"
            output.mkdir()
            link = root / "linked"
            link.symlink_to(output, target_is_directory=True)
            for candidate in (link, link / "nested"):
                with self.subTest(output=candidate):
                    with self.assertRaisesRegex(ValueError, "symlink"):
                        self.module.write(folder, env={}, output=candidate)

    def test_each_diagram_requires_its_own_exact_label_set(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            source = folder / "flow.dot"
            source.write_text(self.source, encoding="utf-8")
            policy = folder / "flow.labels.json"
            policy.write_text(json.dumps(self.mapping), encoding="utf-8")
            self.assertEqual(self.module.read_mapping(source), self.mapping)
            self.mapping["en"]["unrelated"] = "Other diagram"
            policy.write_text(json.dumps(self.mapping), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "label"):
                self.module.read_mapping(source)

    def test_selected_language_is_generated_once_and_checked_as_complete(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            folder = root / "sources"
            folder.mkdir()
            (folder / "flow.dot").write_text(self.source, encoding="utf-8")
            (folder / "flow.labels.json").write_text(json.dumps(self.mapping), encoding="utf-8")
            output = root / "generated"
            svg = b'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1 1"/>'
            with patch.object(diagrams, "render", return_value=svg) as render:
                self.module.write(folder, env={}, output=output, languages=("en",))
                self.assertEqual(render.call_count, 1)
            self.assertEqual(self.module.check(folder, output=output, languages=("en",)),
                             {"localized_diagrams": 1, "errors": []})
            self.assertFalse((output / "zh_CN").exists())

    def test_duplicate_languages_cannot_repeat_asset_generation(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "language"):
                self.module.write(Path(temporary) / "source", env={},
                                  output=Path(temporary) / "out", languages=("en", "en"))

if __name__ == "__main__":
    unittest.main()
