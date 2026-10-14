"""Native Chinese discovery and index upgrades use public Sphinx contracts."""
import importlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from sphinx.search import js_index


class ChineseLanguageTests(unittest.TestCase):
    def language(self):
        return importlib.import_module('tools.documentation.search_language')

    def test_han_words_and_ascii_identifiers_split_without_dictionary(self):
        tokens = self.language().split_terms('借用，排空 HAL nx_uart_read')
        self.assertEqual(set(tokens), {'借', '用', '借用', '排', '空', '排空',
                                       'HAL', 'nx_uart_read'})

    def test_bigrams_never_cross_punctuation_space_or_ascii(self):
        tokens = self.language().split_terms('借用HAL 排，空 借用 借用')
        self.assertIn('HAL', tokens)
        self.assertNotIn('用排', tokens)
        self.assertNotIn('排空', tokens)
        self.assertEqual(tokens.count('借用'), 1)

    def test_unicode_supplementary_han_uses_codepoints(self):
        self.assertEqual(set(self.language().split_terms('𠮷野')), {'𠮷', '野', '𠮷野'})

    def test_query_uses_bigrams_and_preserves_exclusion_boundaries(self):
        split = self.language().split_terms
        self.assertEqual(split('借用 -排空 HAL', query=True),
                         ['借用', '-排空', 'HAL'])
        self.assertEqual(split('借用HAL 工厂-HAL 厂', query=True),
                         ['借用', 'HAL', '工厂', '厂'])

    def test_declared_han_range_does_not_depend_on_unicode_word_database(self):
        rare = '\U0002ebf0'
        self.assertEqual(set(self.language().split_terms(rare + '野')),
                         {rare, '野', rare + '野'})

    def test_stem_encodes_han_to_required_terms_and_preserves_latin(self):
        language = self.language().NexusChineseSearch({})
        for token in ('借', '借用', '𠮷野'):
            self.assertGreaterEqual(len(language.stem(token)), 3)
            self.assertTrue(language.word_filter(language.stem(token)))
        self.assertEqual(language.stem('-借用'), '-' + language.stem('借用'))
        self.assertEqual(language.stem('RUNNING'), 'run')
        self.assertEqual(language.stem('nx_uart_read'), 'nx_uart_read')


class NativeChineseIndexTests(unittest.TestCase):
    def build(self, root, language='zh_CN', legacy=False):
        project = Path(__file__).resolve().parents[2]
        docs = root / 'docs'
        docs.mkdir(exist_ok=True)
        wrapper = root / 'fixture_extension.py'
        wrapper.write_text(
            'from tools.documentation import sphinx_ext\n'
            + 'def setup(app):\n    result = sphinx_ext.setup(app)\n'
            + ('    from sphinx.search.zh import SearchChinese\n'
               '    class Legacy(SearchChinese):\n'
               '        lang = "zh_CN"\n'
               '    app.add_search_language(Legacy)\n'
               '    result.pop("env_version", None)\n' if legacy else '')
            + '    return result\n', encoding='utf-8')
        (docs / 'conf.py').write_text(
            'import sys\n' + f'sys.path.insert(0, {str(project)!r})\n'
            + f'sys.path.insert(0, {str(root)!r})\n'
            + "extensions = ['myst_parser', 'fixture_extension']\n"
            + "source_suffix = {'.md': 'markdown'}\nroot_doc = 'index'\n"
            + "project = 'Native search fixture'\nhtml_theme = 'alabaster'\n"
            + f'language = {language!r}\n', encoding='utf-8')
        index = docs / 'index.md'
        if not index.exists():
            (root / '.gitignore').write_text(
                'fixture_extension.py\nhtml/\n__pycache__/\n', encoding='utf-8')
            index.write_text(
                '---\nsource_language: zh_CN\n---\n\n# 工厂\n\n'
                + '借用与排空。HAL nx_uart_read。𠮷野。\n', encoding='utf-8')
            for command in (['init', '-q'], ['add', '.'],
                            ['-c', 'user.name=Fixture',
                             '-c', 'user.email=fixture@example.invalid',
                             'commit', '-q', '-m', 'test: search fixture']):
                subprocess.run(['git', '-C', str(root)] + command,
                               check=True, capture_output=True)
        environment = os.environ.copy()
        for name in list(environment):
            if name.startswith('NEXUS_DOCS_'):
                environment.pop(name)
        result = subprocess.run(
            [sys.executable, '-m', 'sphinx', '-n', '-W', '-b', 'html',
             str(docs), str(root / 'html')], env=environment,
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return js_index.loads((root / 'html/searchindex.js').read_text())

    def test_real_chinese_body_and_title_terms_exist_without_optional_jieba(self):
        with tempfile.TemporaryDirectory() as temporary:
            index = self.build(Path(temporary))
            self.assertTrue(any('借用' in term for term in index['terms']))
            self.assertTrue(any('工厂' in term for term in index['titleterms']))
            self.assertIn('nx_uart_read', index['terms'])
            self.assertTrue(any('𠮷野' in term for term in index['terms']))

    def test_public_env_version_rebuilds_legacy_frozen_index_without_E(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            legacy = self.build(root, legacy=True)
            self.assertFalse(any('借用' in term for term in legacy['terms']))
            current = self.build(root)
            self.assertTrue(any('借用' in term for term in current['terms']))

    def test_english_locale_uses_original_porter_index(self):
        with tempfile.TemporaryDirectory() as temporary:
            index = self.build(Path(temporary), language='en')
            self.assertIn('nx_uart_read', index['terms'])
            self.assertFalse(any(term.startswith('nxcjk_')
                                 for term in index['terms']))


if __name__ == '__main__':
    unittest.main()
