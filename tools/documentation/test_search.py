"""Contracts for local search identity, excerpts and translation boundaries."""
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from docutils import nodes

from tools.documentation import sphinx_ext


class SearchMetadataTests(unittest.TestCase):
    def tree(self):
        tree = nodes.document('', '')
        section = nodes.section(ids=['factory'])
        section += nodes.title('', 'Factory <script>alert(1)</script>')
        section += nodes.paragraph('', 'Returns an existing static interface.')
        section += nodes.literal_block('', 'password = secret')
        table = nodes.table()
        table += nodes.paragraph('', 'Repeated generated table value.')
        section += table
        section += nodes.paragraph('', 'Cancellation retains borrowed storage.')
        navigation = nodes.container(classes=['toctree-wrapper'])
        navigation += nodes.paragraph('', 'Duplicated sidebar navigation.')
        section += navigation
        tree += section
        return tree

    def test_excerpt_excludes_code_tables_and_preserves_plain_text(self):
        self.assertEqual(sphinx_ext.search_excerpt(self.tree()),
                         'Returns an existing static interface. '
                         'Cancellation retains borrowed storage.')

    def test_excerpt_is_bounded_without_empty_or_duplicate_paragraphs(self):
        tree = nodes.document('', '')
        for text in ('', ' repeated\n prose ', 'repeated prose', 'x' * 400):
            tree += nodes.paragraph('', text)
        excerpt = sphinx_ext.search_excerpt(tree, limit=80)
        self.assertEqual(excerpt.count('repeated prose'), 1)
        self.assertLessEqual(len(excerpt), 80)

    def test_page_type_uses_declared_role_then_explicit_scope(self):
        self.assertEqual(sphinx_ext.search_page_type(
            '---\ntype: tutorial\nsource_language: en\n---\n# Factory',
            'tutorials/factory'), 'tutorial')
        self.assertEqual(sphinx_ext.search_page_type('# API', 'reference/api'), 'reference')
        self.assertEqual(sphinx_ext.search_page_type('# Contract', 'design/factory'),
                         'explanation')
        self.assertEqual(sphinx_ext.search_page_type('# Status', 'delivery/architecture'),
                         'status')
        self.assertEqual(sphinx_ext.search_page_type('# Legacy', 'legacy'), 'reference')

    def test_invalid_declared_type_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'page_type'):
            sphinx_ext.search_page_type('---\ntype: misleading\n---\n# Title', 'guide')

    def test_metadata_script_cannot_close_a_script_or_interpret_markup(self):
        payload = {'records': {'guide.html': {
            'title': '</script><script>alert(1)</script>',
            'excerpt': 'line\u2028split\u2029&<tag>'}}}
        encoded = sphinx_ext.search_data_script(payload)
        self.assertNotIn('</script>', encoded)
        self.assertNotIn('\u2028', encoded)
        self.assertNotIn('\u2029', encoded)
        restored = json.loads(encoded.split(' = ', 1)[1].rstrip(';\n'))
        self.assertEqual(restored, payload)

    def test_unknown_or_parent_page_name_is_rejected(self):
        for name in ('../outside', '/absolute', 'guide/../../outside',
                     'guide?query', 'guide#anchor', 'guide\\outside'):
            with self.subTest(name=name):
                with self.assertRaisesRegex(ValueError, 'search page'):
                    sphinx_ext.search_record(
                        name, 'guide.html', '# Title', self.tree(),
                        {'source_language': 'en', 'requested_language': 'en',
                         'actual_language': 'en', 'status': 'original'})

    def test_stale_translation_record_keeps_actual_source_language(self):
        record = sphinx_ext.search_record(
            'design/factory', 'design/factory.html', '# Factory', self.tree(),
            {'source_language': 'en', 'requested_language': 'zh_CN',
             'actual_language': 'en', 'status': 'stale'})
        self.assertEqual(record['type'], 'explanation')
        self.assertEqual(record['actual_language'], 'en')
        self.assertEqual(record['requested_language'], 'zh_CN')
        self.assertEqual(record['status'], 'stale')
        self.assertEqual(record['title'], 'Factory <script>alert(1)</script>')

    def test_generated_search_pages_are_ignored_without_source(self):
        app = SimpleNamespace(env=SimpleNamespace(found_docs={'index'}))
        sphinx_ext.collect_search_record(app, 'search', 'search.html', {}, None)
        sphinx_ext.collect_search_record(app, 'genindex', 'genindex.html', {}, None)


class SearchBuildIntegrationTests(unittest.TestCase):
    def test_real_build_creates_local_records_and_assets_in_single_locale(self):
        project = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / 'docs'
            docs.mkdir()
            (docs / 'conf.py').write_text(
                'import sys\n' + f'sys.path.insert(0, {str(project)!r})\n'
                + "extensions = ['myst_parser', 'tools.documentation.sphinx_ext']\n"
                + "source_suffix = {'.md': 'markdown'}\nroot_doc = 'index'\n"
                + "project = 'Search fixture'\nhtml_theme = 'alabaster'\n"
                + f"html_static_path = [{str(project / 'docs/assets/styles')!r}]\n"
                + "html_css_files = ['nexus.css']\n"
                + "language = 'zh_CN'\nversion = release = 'docs-fixture'\n",
                encoding='utf-8')
            (docs / 'index.md').write_text(
                '---\nsource_language: en\ntype: tutorial\n---\n\n'
                '# SDK fixture\n\nSDK returns an existing interface.\n', encoding='utf-8')
            for arguments in (['init', '-q'], ['add', 'docs'],
                              ['-c', 'user.name=Fixture',
                               '-c', 'user.email=fixture@example.invalid',
                               'commit', '-q', '-m', 'test: search fixture']):
                subprocess.run(['git', '-C', str(root)] + arguments,
                               check=True, capture_output=True)
            environment = os.environ.copy()
            for name in ('NEXUS_DOCS_REVISION', 'NEXUS_DOCS_CODE_REVISION',
                         'NEXUS_DOCS_TRANSLATION_ROOT', 'NEXUS_DOCS_GETTEXT_ROOT',
                         'NEXUS_DOCS_LANGUAGES'):
                environment.pop(name, None)
            output = root / 'zh_CN'
            result = subprocess.run(
                [sys.executable, '-m', 'sphinx', '-E', '-W', '--keep-going',
                 '-b', 'html', str(docs), str(output)], env=environment,
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            data = (output / '_static/nexus-search-data.js').read_text(encoding='utf-8')
            payload = json.loads(data.split(' = ', 1)[1].rstrip(';\n'))
            self.assertEqual(payload['version'], 'docs-fixture')
            self.assertEqual(payload['requested_language'], 'zh_CN')
            self.assertEqual(set(payload['records']), {'index.html'})
            record = payload['records']['index.html']
            self.assertEqual(record['actual_language'], 'en')
            self.assertEqual(record['status'], 'missing')
            self.assertEqual(record['excerpt'], 'SDK returns an existing interface.')
            self.assertNotIn('qualification', record['excerpt'])
            self.assertTrue((output / '_static/nexus-search.js').is_file())
            search = (output / 'search.html').read_text(encoding='utf-8')
            self.assertIn('_static/nexus-search-data.js', search)
            self.assertIn('_static/nexus-search.js', search)
            article = (output / 'index.html').read_text(encoding='utf-8')
            self.assertNotIn('_static/nexus-search-data.js', article)
            self.assertNotIn('_static/nexus-search.js', article)
            options = (output / '_static/documentation_options.js').read_text(encoding='utf-8')
            self.assertIn('SHOW_SEARCH_SUMMARY: false', options)


if __name__ == '__main__':
    unittest.main()
