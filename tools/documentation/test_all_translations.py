"""Whole-site bilingual admission, including retained RST contracts."""

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

    def page(self, name, contents):
        path = self.root / "docs" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")
        return path

    def catalog(self, name, language, messages, *, source=None):
        page = next((self.root / "docs" / (name + suffix)
                     for suffix in (".md", ".rst")
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
        self.page("design/cpu.md", "# 中断状态\n\n恢复传入的屏蔽状态。\n")
        result = check.validate_translations(self.root)
        self.assertTrue(any("design/cpu/en" in error for error in result["errors"]))
        self.catalog("design/cpu", "en", {"中断状态": "Interrupt state"})
        result = check.validate_translations(self.root)
        self.assertEqual(result["errors"], [])
        self.assertEqual(len(result["records"]), 4)

    def test_rst_and_diagram_catalogue_are_current_sources(self):
        self.page("reference/style.rst", "Style\n=====\n\nKeep four spaces.\n")
        self.page("assets/diagrams/README.md", "# 图册\n\n当前视图。\n")
        self.catalog("reference/style", "zh_CN", {"Style": "代码风格"})
        self.catalog("assets/diagrams/README", "en", {"图册": "Diagram catalogue"})
        result = check.validate_translations(self.root)
        self.assertEqual(result["errors"], [])
        self.assertEqual(len(result["records"]), 6)

    def test_archived_and_nonpage_inputs_do_not_require_new_translation(self):
        self.page("archive/history.md", "# 历史合同\n")
        self.page("README.md", "# 源码维护入口\n")
        self.page("data.json", "{}\n")
        result = check.validate_translations(self.root)
        self.assertEqual(result["errors"], [])
        self.assertEqual(len(result["records"]), 2)

    def test_glob_published_design_and_delivery_readmes_require_translation(self):
        self.page("design/README.md", "# 设计入口\n")
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


if __name__ == "__main__":
    unittest.main()
