"""Whole-site canonical and opposite-language translation admission."""

import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.documentation import check


class WholeSiteTranslationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "docs").mkdir()
        self.page("index.md", "# Start\n\nUse the platform.\n")
        self.catalog("index", "zh_CN", {"Start": "开始"})

    def page(self, name, contents, *, language="en"):
        path = self.root / "docs" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.suffix == ".md" and not contents.startswith("---\n"):
            contents = ("---\nid: " + name.replace("/", ".").removesuffix(".md")
                        + "\ntype: reference\naudience: consumer\nsource_language: "
                        + language + "\n---\n" + contents)
        path.write_text(contents, encoding="utf-8")
        return path

    def catalog(self, name, language, messages, *, source=None):
        page = next((self.root / "docs" / (name + suffix)
                     for suffix in (".md",)
                     if (self.root / "docs" / (name + suffix)).is_file()))
        digest = hashlib.sha256((source or page.read_text()).encode()).hexdigest()
        header = ("Language: " + language + "\n"
                  "X-Nexus-Source-SHA256: " + digest + "\n"
                  "X-Nexus-Reviewed: true\n")
        path = self.root / "docs/locale" / language / "LC_MESSAGES" / (name + ".po")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("msgid \"\"\nmsgstr " + json.dumps(header) + "\n\n"
                        + "\n\n".join("msgid " + json.dumps(key, ensure_ascii=False)
                                       + "\nmsgstr " + json.dumps(value, ensure_ascii=False)
                                       for key, value in messages.items()) + "\n",
                        encoding="utf-8")

    def test_noncritical_english_page_requires_chinese_translation(self):
        self.page("explanation/new-provider.md", "# Provider\n\nOwn the resource.\n")
        result = check.validate_translations(self.root)
        self.assertEqual(result["status"], "failed")
        self.assertTrue(any("explanation/new-provider/zh_CN" in error
                            for error in result["errors"]))

    def test_chinese_contract_requires_english_translation(self):
        self.page("design/cpu.md", "# 中断状态\n\n恢复传入的屏蔽状态。\n",
                  language="zh_CN")
        result = check.validate_translations(self.root)
        self.assertTrue(any("design/cpu/en" in error for error in result["errors"]))
        self.catalog("design/cpu", "en", {"中断状态": "Interrupt state"})
        result = check.validate_translations(self.root)
        self.assertEqual(result["errors"], [])
        self.assertEqual(len(result["records"]), 4)

    def test_markdown_contract_and_diagram_catalogue_are_current_sources(self):
        self.page("reference/style.md", "# Style\n\nKeep four spaces.\n")
        self.page("assets/diagrams/README.md", "# 图册\n\n当前视图。\n",
                  language="zh_CN")
        self.catalog("reference/style", "zh_CN", {"Style": "代码风格"})
        self.catalog("assets/diagrams/README", "en", {"图册": "Diagram catalogue"})
        result = check.validate_translations(self.root)
        self.assertEqual(result["errors"], [])
        self.assertEqual(len(result["records"]), 6)

    def test_derived_output_and_nonmarkdown_inputs_are_not_pages(self):
        self.page("_output/generated.md", "# Derived\n")
        self.page("data.json", "{}\n")
        result = check.validate_translations(self.root)
        self.assertEqual(result["errors"], [])
        self.assertEqual(len(result["records"]), 2)

    def test_glob_published_design_and_delivery_readmes_require_translation(self):
        self.page("design/README.md", "# 设计入口\n", language="zh_CN")
        self.page("delivery/README.md", "# Delivery index\n")
        result = check.validate_translations(self.root)
        self.assertTrue(any("design/README/en" in error for error in result["errors"]))
        self.assertTrue(any("delivery/README/zh_CN" in error
                            for error in result["errors"]))

    def test_new_paragraph_invalidates_an_earlier_review(self):
        page = self.page("guides/stop.md", "# Stop\n\nJoin before reuse.\n")
        self.catalog("guides/stop", "zh_CN", {"Stop": "停止"})
        page.write_text(page.read_text() + "Retain storage on failure.\n")
        result = check.validate_translations(self.root)
        self.assertTrue(any("guides/stop/zh_CN: stale" in error
                            for error in result["errors"]))

    def test_full_extraction_rejects_missing_and_extra_messages(self):
        pot = self.root / "gettext/index.pot"
        pot.parent.mkdir()
        pot.write_text('msgid "Start"\nmsgstr ""\n\n'
                       'msgid "Use the platform."\nmsgstr ""\n')
        result = check.validate_translations(self.root, pot_root=pot.parent)
        self.assertEqual(result["status"], "failed")
        self.catalog("index", "zh_CN", {"Start": "开始",
                                         "Use the platform.": "使用平台。"})
        self.assertEqual(check.validate_translations(
            self.root, pot_root=pot.parent)["errors"], [])
        self.catalog("index", "zh_CN", {"Start": "开始",
                                         "Use the platform.": "使用平台。",
                                         "Removed paragraph": "已移除的段落"})
        self.assertEqual(check.validate_translations(
            self.root, pot_root=pot.parent)["status"], "failed")

    def test_verify_rejects_empty_actual_gettext_for_a_nonempty_page(self):
        pot = self.root / "gettext/index.pot"
        pot.parent.mkdir()
        pot.write_text('msgid ""\nmsgstr ""\n')
        self.assertEqual(check.validate_translations(
            self.root, pot_root=pot.parent)["status"], "failed")

    def test_verify_rejects_orphan_extracted_pages(self):
        folder = self.root / "gettext"
        folder.mkdir()
        (folder / "index.pot").write_text('msgid "Start"\nmsgstr ""\n')
        (folder / "removed.pot").write_text('msgid "Old"\nmsgstr ""\n')
        self.assertEqual(check.validate_translations(
            self.root, pot_root=folder)["status"], "failed")

    def test_malformed_actual_pot_returns_a_failed_check(self):
        folder = self.root / "gettext"
        folder.mkdir()
        (folder / "index.pot").write_text('msgid "Start"\nmsgstr ""\n\n'
                                         'msgid "Start"\nmsgstr ""\n')
        self.assertEqual(check.validate_translations(
            self.root, pot_root=folder)["status"], "failed")

    def test_removed_page_catalog_is_rejected(self):
        catalog = self.root / "docs/locale/zh_CN/LC_MESSAGES/removed.po"
        catalog.write_text('msgid "Old"\nmsgstr "旧文档"\n')
        result = check.validate_translations(self.root)
        self.assertEqual(result["status"], "failed")
        self.assertIn("unknown translation", " ".join(result["errors"]))


if __name__ == "__main__":
    unittest.main()
