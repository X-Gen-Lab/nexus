"""Visible document identity must use the requested UI language."""

from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from tools.documentation import sphinx_ext


class IdentityLocaleTests(unittest.TestCase):
    def render(self, language, *, dirty=False, source="# Factory\n"):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            page = root / "docs/factory.md"
            page.parent.mkdir()
            page.write_text(source)
            app = SimpleNamespace(
                srcdir=str(page.parent),
                env=SimpleNamespace(found_docs={"factory"},
                                    doc2path=lambda name, base: str(page)),
                config=SimpleNamespace(
                    language=language, nexus_gettext_root=None,
                    nexus_translation_root=str(page.parent / "locale"),
                    nexus_revision="a" * 40, nexus_code_revision="b" * 40,
                    nexus_source_dirty=dirty,
                    nexus_available_languages=("en", "zh_CN")),
            )
            context = {"body": "<p>Original content.</p>"}
            sphinx_ext._page_context(app, "factory", "page.html", context, True)
            return context["body"]

    def test_chinese_ui_and_truthful_english_fallback_are_separate(self):
        body = self.render("zh_CN")
        self.assertIn('lang="zh-CN" aria-label="文档身份"', body)
        self.assertIn("文档源码", body)
        self.assertIn("软件或实板资格", body)
        self.assertIn('data-nexus-actual-language="en"', body)
        self.assertIn('data-nexus-translation-status="missing"', body)
        self.assertIn('<div class="nexus-canonical-content" lang="en">', body)

    def test_english_ui_retains_machine_readable_original_state(self):
        body = self.render("en")
        self.assertIn('lang="en" aria-label="Document identity"', body)
        self.assertIn("Document source", body)
        self.assertIn('data-nexus-translation-status="original"', body)
        self.assertIn("English", self.render("zh_CN"))
        self.assertIn("中文", body)

    def test_dirty_preview_is_localized_and_visible(self):
        self.assertIn("工作目录有改动；仅供预览", self.render("zh_CN", dirty=True))
        self.assertIn("working tree modified; preview only", self.render("en", dirty=True))


if __name__ == "__main__":
    unittest.main()
