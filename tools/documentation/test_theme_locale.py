"""Actual bilingual theme rendering preserves navigation and legal credits."""
import os
import subprocess
import sys
import tempfile
import unittest
from html.parser import HTMLParser
from pathlib import Path


class VisibleText(HTMLParser):
    def __init__(self):
        super().__init__()
        self.parts = []

    def handle_data(self, data):
        self.parts.append(data)

    def text(self):
        return ' '.join(' '.join(self.parts).split())


class ThemeLocaleTests(unittest.TestCase):
    def test_production_config_localizes_ui_and_preserves_credits(self):
        project = Path(__file__).resolve().parents[2]
        config = project / 'docs/sphinx/conf.py'
        with tempfile.TemporaryDirectory() as temporary:
            for language in ('en', 'zh_CN'):
                with self.subTest(language=language):
                    root = Path(temporary) / language
                    docs = root / 'docs'
                    docs.mkdir(parents=True)
                    (docs / 'conf.py').write_text(
                        f'__file__ = {str(config)!r}\n'
                        + f'exec(compile(open({str(config)!r}).read(), __file__, "exec"))\n'
                        + "html_sidebars = {'**': ['about.html', 'navigation.html', "
                        + "'relations.html', 'searchbox.html']}\n",
                        encoding='utf-8')
                    title = '平台使用说明' if language == 'zh_CN' else 'Platform guide'
                    next_title = '下一项任务' if language == 'zh_CN' else 'Next task'
                    (docs / 'index.md').write_text(
                        '---\nsource_language: ' + language + '\n---\n\n# '
                        + title + '\n\n```{toctree}\nnext\n```\n', encoding='utf-8')
                    (docs / 'next.md').write_text(
                        '---\nsource_language: ' + language + '\n---\n\n# '
                        + next_title + '\n\n' + title + '.\n', encoding='utf-8')
                    for arguments in (['init', '-q'], ['add', 'docs'],
                                      ['-c', 'user.name=Fixture',
                                       '-c', 'user.email=fixture@example.invalid',
                                       'commit', '-q', '-m', 'test: theme fixture']):
                        subprocess.run(['git', '-C', str(root)] + arguments,
                                       check=True, capture_output=True)
                    environment = os.environ.copy()
                    for name in ('NEXUS_DOCS_REVISION', 'NEXUS_DOCS_CODE_REVISION',
                                 'NEXUS_DOCS_TRANSLATION_ROOT', 'NEXUS_DOCS_GETTEXT_ROOT',
                                 'NEXUS_DOCS_LANGUAGES', 'NEXUS_DOCS_DOXYGEN_XML'):
                        environment.pop(name, None)
                    environment['NEXUS_DOCS_LANGUAGE'] = language
                    output = root / 'html'
                    result = subprocess.run(
                        [sys.executable, '-m', 'sphinx', '-E', '-n', '-W',
                         '--keep-going', '-b', 'html', str(docs), str(output)],
                        env=environment, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    rendered = (output / 'next.html').read_text(encoding='utf-8')
                    parser = VisibleText()
                    parser.feed(rendered)
                    text = parser.text()
                    if language == 'zh_CN':
                        for label in ('相关主题', '文档总览', '上一页',
                                      '技术支持', '可复用的静态嵌入式平台机制'):
                            self.assertIn(label, text)
                        for label in ('Related Topics', 'Documentation overview',
                                      'Previous:', 'Powered by',
                                      'Static embedded platform mechanisms'):
                            self.assertNotIn(label, text)
                    else:
                        for label in ('Related Topics', 'Documentation overview',
                                      'Previous:', 'Powered by',
                                      'Static embedded platform mechanisms'):
                            self.assertIn(label, text)
                    self.assertIn('2026, Nexus Team', text)
                    self.assertIn('Sphinx 7.4.7', text)
                    self.assertIn('Alabaster 0.7.16', text)
                    self.assertIn('https://www.sphinx-doc.org/', rendered)
                    self.assertIn('https://alabaster.readthedocs.io', rendered)
                    self.assertIn('href="index.html"', rendered)
                    self.assertNotIn('_static/nexus-search-data.js', rendered)
                    search = (output / 'search.html').read_text(encoding='utf-8')
                    self.assertIn('_static/nexus-search-data.js', search)


if __name__ == '__main__':
    unittest.main()
