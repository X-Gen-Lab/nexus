"""Regression contracts for documentation identity and translation status."""
import hashlib
import tempfile
import os
import subprocess
import sys
import unittest
from pathlib import Path

from tools.documentation import sphinx_ext


class DocumentationSourceTests(unittest.TestCase):
    def test_canonical_language_is_independent_of_build_locale(self):
        self.assertEqual(sphinx_ext.source_language('# 工厂接口\n静态实例查找。'), 'zh_CN')
        self.assertEqual(sphinx_ext.source_language('# Factory\nStatic instance lookup.'), 'en')
        self.assertEqual(sphinx_ext.source_language('---\nsource_language: en\n---\nEnglish with 中文标识.'), 'en')

    def test_invalid_language_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'source_language'):
            sphinx_ext.source_language('---\nsource_language: automatic\n---\n# Title')

    def test_source_file_links_are_bound_to_revision(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            page = root / 'docs/design/example.md'
            page.parent.mkdir(parents=True)
            header = root / 'core/include/nexus/core/request.h'
            header.parent.mkdir(parents=True)
            header.write_text('header', encoding='utf-8')
            result = sphinx_ext.rewrite_local_links(
                '[header](../../core/include/nexus/core/request.h)', page, root / 'docs', root,
                'a' * 40)
            self.assertIn('/blob/' + 'a' * 40 + '/core/include/nexus/core/request.h', result)

    def test_source_link_code_labels_preserve_markup_and_bind_revision(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            page = root / 'docs/design/example.md'
            page.parent.mkdir(parents=True)
            assembly = root / 'tools/configure/assemblies/h743-high-peripherals.toml'
            assembly.parent.mkdir(parents=True)
            assembly.write_text('# maintained assembly fixture\n', encoding='utf-8')
            (page.parent / 'status.csv').write_text('status\n', encoding='utf-8')
            (page.parent / 'related.md').write_text('# Related\n', encoding='utf-8')
            archived = root / 'docs/archive/history.md'
            archived.parent.mkdir()
            archived.write_text('# History\n', encoding='utf-8')
            target = '../../tools/configure/assemblies/h743-high-peripherals.toml'
            revision = 'a' * 40
            bound = ('https://github.com/X-Gen-Lab/nexus/blob/' + revision
                     + '/tools/configure/assemblies/h743-high-peripherals.toml')
            literal = f'[`h743-high-peripherals.toml`]({target})'
            unchanged = (
                f'Use `[assembly]({target})` as markup.',
                f'Use ``{literal}`` as markup.',
                f'```markdown\n{literal}\n```\n',
                f'~~~markdown\n{literal}\n~~~\n',
                '!' + literal,
                '[`assembly`](https://example.invalid/assembly.toml)',
                '[`status.csv`](status.csv)',
                '[`related.md`](related.md)',
                f'Use ``literal ` [assembly]({target})`` as markup.',
            )
            cases = [(source, source) for source in unchanged]
            for label in ('assembly', '`h743-high-peripherals.toml`',
                          '`h743-high-peripherals.toml` assembly',
                          '``h743-high-peripherals.toml``'):
                cases.append((f'[{label}]({target})', f'[{label}]({bound})'))
            cases.extend((
                ('[`assemblies`](../../tools/configure/assemblies)',
                 f'[`assemblies`](https://github.com/X-Gen-Lab/nexus/tree/{revision}'
                 '/tools/configure/assemblies)'),
                ('[`history.md`](../archive/history.md)',
                 f'[`history.md`](https://github.com/X-Gen-Lab/nexus/blob/{revision}'
                 '/docs/archive/history.md)'),
                (f'Before `[example]({target})` then {literal} and [plain]({target}).',
                 f'Before `[example]({target})` then '
                 f'[`h743-high-peripherals.toml`]({bound}) and [plain]({bound}).'),
            ))
            for document_root_links in (False, True):
                for source, expected in cases:
                    with self.subTest(source=source, root_links=document_root_links):
                        self.assertEqual(sphinx_ext.rewrite_local_links(
                            source, page, root / 'docs', root, revision,
                            document_root_links=document_root_links), expected)

    def test_source_directory_link_uses_revision_bound_tree(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            page = root / 'docs/design/example.md'
            page.parent.mkdir(parents=True)
            (root / 'tests/contracts').mkdir(parents=True)
            result = sphinx_ext.rewrite_local_links('[tests](../../tests/contracts)', page,
                                                    root / 'docs', root, 'a' * 40)
            self.assertIn('/tree/' + 'a' * 40 + '/tests/contracts', result)

    def test_local_data_link_becomes_sphinx_download_role(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            page = root / 'docs/design/example.md'
            page.parent.mkdir(parents=True)
            (page.parent / 'status.csv').write_text('id,status\n1,planned\n', encoding='utf-8')
            result = sphinx_ext.rewrite_local_links('[status](status.csv)', page, root / 'docs', root, 'a' * 40)
            self.assertEqual(result, '{download}`status <status.csv>`')

    def test_inline_code_example_is_not_rewritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            page = root / 'docs/example.md'
            page.parent.mkdir()
            (page.parent / 'status.csv').write_text('status', encoding='utf-8')
            source = 'Use `[example](status.csv)` as markup. [Download](status.csv)'
            result = sphinx_ext.rewrite_local_links(source, page, root / 'docs', root, 'a' * 40)
            self.assertEqual(result, 'Use `[example](status.csv)` as markup. {download}`Download <status.csv>`')

    def test_code_fences_are_not_rewritten(self):
        source = '```markdown\n[example](status.csv)\n```\n'
        self.assertEqual(sphinx_ext.rewrite_local_links(source, Path('/repo/docs/a.md'), Path('/repo/docs'), Path('/repo'), 'a' * 40), source)


class AvailableLanguageTests(unittest.TestCase):
    def test_single_preview_defaults_to_only_its_actual_language(self):
        self.assertEqual(sphinx_ext.available_languages('en'), ('en',))
        self.assertEqual(sphinx_ext.available_languages('zh_CN'), ('zh_CN',))

    def test_explicit_build_languages_must_be_valid_unique_and_include_output(self):
        self.assertEqual(sphinx_ext.available_languages('en', 'en,zh_CN'), ('en', 'zh_CN'))
        for declaration in ('en,fr', 'en,en', '', 'zh_CN', 'en,'):
            with self.subTest(declaration=declaration):
                with self.assertRaises(ValueError):
                    sphinx_ext.available_languages('en', declaration)


class TranslationStatusTests(unittest.TestCase):
    def setUp(self):
        self.source = '# 工厂\n\n返回已有静态对象。\n'
        self.digest = hashlib.sha256(self.source.encode()).hexdigest()
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.catalog = self.root / 'en/LC_MESSAGES/factory.po'
        self.catalog.parent.mkdir(parents=True)

    def write_catalog(self, *, digest=None, reviewed=True, fuzzy=False, translated=True,
                      message='工厂', language='en'):
        self.catalog.write_text(
            'msgid ""\nmsgstr ""\n'
            f'"Language: {language}\\n"\n'
            f'"X-Nexus-Source-SHA256: {digest or self.digest}\\n"\n'
            f'"X-Nexus-Reviewed: {str(reviewed).lower()}\\n"\n\n'
            + ('#, fuzzy\n' if fuzzy else '')
            + f'msgid "{message}"\nmsgstr "{("Factory" if translated else "")}"\n', encoding='utf-8')

    def state(self, language='en', messages=('工厂',)):
        return sphinx_ext.translation_status(self.source, 'factory', language, self.root, messages)

    def test_original_means_actual_source_language(self):
        self.assertEqual(self.state('zh_CN')['status'], 'original')
        self.assertEqual(self.state('zh_CN')['actual_language'], 'zh_CN')

    def test_same_source_locale_catalog_can_normalize_mixed_prose(self):
        self.catalog = self.root / 'zh_CN/LC_MESSAGES/factory.po'
        self.catalog.parent.mkdir(parents=True)
        self.write_catalog(language='zh_CN')
        result = self.state('zh_CN')
        self.assertEqual(result['status'], 'reviewed')
        self.assertEqual(result['actual_language'], 'zh_CN')

    def test_same_source_locale_stale_catalog_is_not_silently_original(self):
        self.catalog = self.root / 'zh_CN/LC_MESSAGES/factory.po'
        self.catalog.parent.mkdir(parents=True)
        self.write_catalog(language='zh_CN', digest='b' * 64)
        self.assertEqual(self.state('zh_CN')['status'], 'stale')

    def test_missing_catalog_falls_back_to_source_language(self):
        self.assertEqual(self.state()['status'], 'missing')
        self.assertEqual(self.state()['actual_language'], 'zh_CN')

    def test_reviewed_catalog_requires_matching_source_and_all_messages(self):
        self.write_catalog()
        self.assertEqual(self.state()['status'], 'reviewed')
        self.assertEqual(self.state()['actual_language'], 'en')
        self.assertEqual(self.state(messages=('工厂', '遗漏的段落'))['status'], 'missing')

    def test_stale_and_fuzzy_catalogs_do_not_claim_translation(self):
        self.write_catalog(digest='b' * 64)
        self.assertEqual(self.state()['status'], 'stale')
        self.write_catalog(fuzzy=True)
        self.assertEqual(self.state()['status'], 'stale')
        self.assertEqual(self.state()['actual_language'], 'zh_CN')

    def test_unreviewed_or_empty_translation_is_missing(self):
        self.write_catalog(reviewed=False)
        self.assertEqual(self.state()['status'], 'missing')
        self.write_catalog(translated=False)
        self.assertEqual(self.state()['status'], 'missing')

    def test_english_source_chinese_translation_round_trip(self):
        source = '# Factory\n\nExisting instances.\n'
        digest = hashlib.sha256(source.encode()).hexdigest()
        path = self.root / 'zh_CN/LC_MESSAGES/factory.po'
        path.parent.mkdir(parents=True)
        path.write_text('msgid ""\nmsgstr ""\n"Language: zh_CN\\n"\n'
                        f'"X-Nexus-Source-SHA256: {digest}\\n"\n"X-Nexus-Reviewed: true\\n"\n\n'
                        'msgid "Factory"\nmsgstr "工厂"\n', encoding='utf-8')
        result = sphinx_ext.translation_status(source, 'factory', 'zh_CN', self.root, ('Factory',))
        self.assertEqual(result['status'], 'reviewed')
        self.assertEqual(result['actual_language'], 'zh_CN')
        self.assertEqual(sphinx_ext.translation_status(source, 'factory', 'en', self.root)['status'], 'original')


class IncludedReferenceTests(unittest.TestCase):
    def test_included_document_and_data_links_use_their_real_source_directory(self):
        from docutils import nodes
        from sphinx import addnodes
        from types import SimpleNamespace
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            origin = root / 'os/README.md'
            origin.parent.mkdir()
            origin.write_text('README', encoding='utf-8')
            target = root / 'docs/design/contracts.md'
            target.parent.mkdir(parents=True)
            target.write_text('# Contracts', encoding='utf-8')
            data = target.with_name('status.csv')
            data.write_text('state,status', encoding='utf-8')
            tree = nodes.document('', '')
            for suffix in ('contracts.md', 'status.csv'):
                ref = addnodes.pending_xref('', reftype='myst', reftarget='../docs/design/' + suffix,
                                           refdoc='explanation/source-map', refdomain=None,
                                           refexplicit=True)
                ref.source = str(origin)
                ref += nodes.inline('', 'contracts')
                tree += ref
            app = SimpleNamespace(srcdir=str(root / 'docs'),
                                  env=SimpleNamespace(found_docs={'design/contracts'}),
                                  config=SimpleNamespace(nexus_revision='a' * 40))
            sphinx_ext.normalize_included_references(app, tree)
            document, download = tree.children
            self.assertEqual(document['refdomain'], 'doc')
            self.assertEqual(document['reftarget'], 'design/contracts')
            self.assertIsInstance(download, addnodes.download_reference)
            self.assertEqual(download['reftarget'], '/design/status.csv')


class ResponsiveTableTests(unittest.TestCase):
    def test_html_table_preserves_semantics_inside_keyboard_scroll_region(self):
        from docutils import nodes
        from types import SimpleNamespace
        tree = nodes.document('', '')
        table = nodes.table(classes=['existing-table'], ids=['support-matrix'])
        tree += table
        app = SimpleNamespace(builder=SimpleNamespace(format='html'),
                              config=SimpleNamespace(language='en'))
        sphinx_ext.wrap_html_tables(app, tree, 'reference/support')
        region = tree.children[0]
        self.assertIn('nexus-table-shell', region['classes'])
        self.assertIn('role="region"', region.children[0].astext())
        self.assertIn('tabindex="0"', region.children[0].astext())
        self.assertIs(region.children[1], table)
        self.assertEqual(table['ids'], ['support-matrix'])
        self.assertEqual(table['classes'], ['existing-table'])
        sphinx_ext.wrap_html_tables(app, tree, 'reference/support')
        self.assertIs(tree.children[0], region)
        self.assertIs(region.children[1], table)

    def test_gettext_tables_are_not_wrapped(self):
        from docutils import nodes
        from types import SimpleNamespace
        tree = nodes.document('', '')
        table = nodes.table()
        tree += table
        app = SimpleNamespace(builder=SimpleNamespace(format='gettext'),
                              config=SimpleNamespace(language='en'))
        sphinx_ext.wrap_html_tables(app, tree, 'reference/support')
        self.assertIs(tree.children[0], table)


class LocalSvgLinkTests(unittest.TestCase):
    def app(self, *, format='html'):
        from types import SimpleNamespace
        return SimpleNamespace(builder=SimpleNamespace(
            format=format, imagedir='_images', get_target_uri=lambda docname: docname + '.html'),
            env=SimpleNamespace(images={'assets/flow.svg': ({'guide/topic'}, 'flow.svg')}),
            config=SimpleNamespace(language='en'))

    def test_local_svg_links_to_copied_asset_with_alt_and_accessible_title(self):
        from docutils import nodes
        tree = nodes.document('', '')
        image = nodes.image(uri='assets/flow.svg', alt='Diagram alternative')
        tree += image
        sphinx_ext.link_local_svg_images(self.app(), tree, 'guide/topic')
        reference = tree.children[0]
        self.assertIsInstance(reference, nodes.reference)
        self.assertEqual(reference['refuri'], '../_images/flow.svg')
        self.assertIn('Open SVG', reference['reftitle'])
        self.assertIs(reference.children[0], image)
        self.assertEqual(image['alt'], 'Diagram alternative')

    def test_existing_links_are_preserved_without_nested_anchors(self):
        from docutils import nodes
        tree = nodes.document('', '')
        reference = nodes.reference('', refuri='https://example.invalid/explanation')
        reference += nodes.image(uri='assets/flow.svg', alt='Existing link')
        tree += reference
        sphinx_ext.link_local_svg_images(self.app(), tree, 'guide/topic')
        self.assertIs(tree.children[0], reference)
        self.assertEqual(reference['refuri'], 'https://example.invalid/explanation')
        self.assertIsInstance(reference.children[0], nodes.image)

    def test_nonhtml_remote_and_nonsvg_images_are_unchanged(self):
        from docutils import nodes
        for format, uri in (('gettext', 'assets/flow.svg'),
                            ('html', 'https://example.invalid/flow.svg'),
                            ('html', 'assets/flow.png')):
            with self.subTest(format=format, uri=uri):
                tree = nodes.document('', '')
                image = nodes.image(uri=uri, alt='Unchanged')
                tree += image
                sphinx_ext.link_local_svg_images(self.app(format=format), tree, 'guide/topic')
                self.assertIs(tree.children[0], image)

    def test_actual_nested_html_uses_existing_svg_bytes_in_both_locales(self):
        from html.parser import HTMLParser
        project = Path(__file__).resolve().parents[2]
        class Images(HTMLParser):
            def __init__(self):
                super().__init__()
                self.anchors = []
                self.images = []
            def handle_starttag(self, tag, attributes):
                if tag == 'a':
                    self.anchors.append(dict(attributes))
                if tag == 'img':
                    self.images.append((dict(attributes), list(self.anchors)))
            def handle_endtag(self, tag):
                if tag == 'a' and self.anchors:
                    self.anchors.pop()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / 'docs'
            for directory in ('guide', 'assets', 'other'):
                (docs / directory).mkdir(parents=True, exist_ok=True)
            (docs / 'conf.py').write_text(
                'import sys, os\n' + f'sys.path.insert(0, {str(project)!r})\n'
                + "extensions = ['myst_parser', 'tools.documentation.sphinx_ext']\n"
                + "source_suffix = {'.md': 'markdown'}\nroot_doc = 'index'\n"
                + "project = 'SVG fixture'\nhtml_theme = 'alabaster'\n"
                + "language = os.environ['NEXUS_DOCS_LANGUAGE']\n", encoding='utf-8')
            (docs / 'index.md').write_text('# SVG fixture\n\n```{toctree}\nguide/topic\n```\n')
            (docs / 'guide/topic.md').write_text(
                '# Diagrams\n\n| Scope | Source |\n| --- | --- |\n| software | `0123456789abcdef0123456789abcdef01234567` |\n\n![First diagram](../assets/flow.svg)\n\n'
                '![Second diagram](../other/flow.svg)\n\n'
                '[![Original link](../assets/flow.svg)](https://example.invalid/explanation)\n')
            svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect width="16" height="16" /></svg>'
            for directory in ('assets', 'other'):
                (docs / directory / 'flow.svg').write_text(svg)
            for arguments in (['init', '-q'], ['add', 'docs'],
                              ['-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                               'commit', '-q', '-m', 'test: SVG fixture']):
                subprocess.run(['git', '-C', str(root)] + arguments,
                               check=True, capture_output=True)
            environment = os.environ.copy()
            for name in ('NEXUS_DOCS_REVISION', 'NEXUS_DOCS_CODE_REVISION',
                         'NEXUS_DOCS_TRANSLATION_ROOT', 'NEXUS_DOCS_GETTEXT_ROOT'):
                environment.pop(name, None)
            environment['NEXUS_DOCS_LANGUAGES'] = 'en,zh_CN'
            for language in ('en', 'zh_CN'):
                environment['NEXUS_DOCS_LANGUAGE'] = language
                output = root / language
                result = subprocess.run([sys.executable, '-m', 'sphinx', '-E', '-W',
                                         '--keep-going', '-b', 'html', str(docs), str(output)],
                                        env=environment, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                page = output / 'guide/topic.html'
                parser = Images()
                rendered = page.read_text(encoding='utf-8')
                parser.feed(rendered)
                self.assertIn('nexus-table-scroll', rendered)
                self.assertIn('role="region"', rendered)
                self.assertIn('tabindex="0"', rendered)
                self.assertIn('<table', rendered)
                for image, references in parser.images:
                    self.assertEqual(len(references), 1, image['alt'])
                    if image['alt'] == 'Original link':
                        self.assertEqual(references[0]['href'], 'https://example.invalid/explanation')
                        continue
                    self.assertIn(image['alt'], ('First diagram', 'Second diagram'))
                    self.assertEqual(references[0]['href'], image['src'])
                    self.assertTrue((page.parent / references[0]['href']).is_file())
                    self.assertIn('title', references[0])
                self.assertEqual(len(parser.images), 3)


class SphinxTranslationIntegrationTests(unittest.TestCase):
    def test_real_gettext_and_two_locales_with_missing_and_stale_fallback(self):
        project = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / 'docs'
            docs.mkdir()
            (docs / 'conf.py').write_text(
                'import sys, os\n'
                + f'sys.path.insert(0, {str(project)!r})\n'
                + "extensions = ['myst_parser', 'tools.documentation.sphinx_ext']\n"
                + "source_suffix = {'.md': 'markdown'}\nroot_doc = 'index'\n"
                + "project = 'Identity fixture'\nhtml_theme = 'alabaster'\n"
                + "language = os.environ['NEXUS_DOCS_LANGUAGE']\n"
                + "gettext_compact = False\nexclude_patterns = ['locale/**']\n",
                encoding='utf-8')
            source_en = '---\nsource_language: en\n---\n\n# Factory contract\n\nReturns an existing instance.\n'
            source_zh = '---\nsource_language: zh_CN\n---\n\n# 工厂合同\n\n返回已有实例。\n'
            (docs / 'english.md').write_text(source_en, encoding='utf-8')
            (docs / 'chinese.md').write_text(source_zh, encoding='utf-8')
            (docs / 'index.md').write_text('---\nsource_language: en\n---\n\n# Fixture\n\n'
                                           '```{toctree}\nenglish\nchinese\n```\n', encoding='utf-8')
            for arguments in (['init', '-q'], ['add', 'docs'],
                              ['-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                               'commit', '-q', '-m', 'test: translation fixture']):
                subprocess.run(['git', '-C', str(root)] + arguments,
                               check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            gettext = root / 'gettext'
            environment = os.environ.copy()
            for name in ('NEXUS_DOCS_REVISION', 'NEXUS_DOCS_CODE_REVISION',
                         'NEXUS_DOCS_TRANSLATION_ROOT', 'NEXUS_DOCS_GETTEXT_ROOT', 'NEXUS_DOCS_LANGUAGES'):
                environment.pop(name, None)

            def build(builder, language, output):
                current = environment | {'NEXUS_DOCS_LANGUAGE': language}
                if builder != 'gettext':
                    current['NEXUS_DOCS_GETTEXT_ROOT'] = str(gettext)
                result = subprocess.run([sys.executable, '-m', 'sphinx', '-E', '-W',
                                         '--keep-going', '-b', builder, str(docs), str(output)],
                                        env=current, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            build('gettext', 'en', gettext)
            mapping = {'zh_CN': {'Factory contract': '工厂合同',
                                 'Returns an existing instance.': '返回已有实例。'},
                       'en': {'工厂合同': 'Factory contract',
                              '返回已有实例。': 'Returns an existing instance.'}}
            import json
            for language, docname, source in (('zh_CN', 'english', source_en),
                                             ('en', 'chinese', source_zh)):
                messages = sphinx_ext.read_catalog(gettext / (docname + '.pot'))[1]
                catalog = docs / 'locale' / language / 'LC_MESSAGES' / (docname + '.po')
                catalog.parent.mkdir(parents=True)
                header = ('Language: ' + language + '\nContent-Type: text/plain; charset=UTF-8\n'
                          + 'X-Nexus-Source-SHA256: ' + hashlib.sha256(source.encode()).hexdigest()
                          + '\nX-Nexus-Reviewed: true\n')
                entries = 'msgid ""\nmsgstr ' + json.dumps(header, ensure_ascii=False) + '\n\n'
                for message in messages:
                    entries += ('msgid ' + json.dumps(message, ensure_ascii=False) + '\nmsgstr '
                                + json.dumps(mapping[language][message], ensure_ascii=False) + '\n\n')
                catalog.write_text(entries, encoding='utf-8')
            build('html', 'en', root / 'en')
            english_view = (root / 'en/chinese.html').read_text(encoding='utf-8')
            self.assertIn('actual: en; translation: <strong>reviewed</strong>', english_view)
            self.assertIn('Returns an existing instance.', english_view)
            self.assertNotIn('href="../zh_CN/chinese.html"', english_view)
            self.assertIn('working tree modified; preview only', english_view)
            build('html', 'zh_CN', root / 'zh_CN')
            chinese_view = (root / 'zh_CN/english.html').read_text(encoding='utf-8')
            self.assertIn('data-nexus-actual-language="zh_CN"', chinese_view)
            self.assertIn('data-nexus-translation-status="reviewed"', chinese_view)
            self.assertIn('返回已有实例。', chinese_view)
            environment['NEXUS_DOCS_LANGUAGES'] = 'en,zh_CN'
            build('html', 'en', root / 'en')
            bilingual = (root / 'en/chinese.html').read_text(encoding='utf-8')
            self.assertIn('href="../zh_CN/chinese.html"', bilingual)
            # Revoked review cannot reuse a previously compiled MO in this output.
            reviewed = docs / 'locale/zh_CN/LC_MESSAGES/english.po'
            catalog_bytes = reviewed.read_text(encoding='utf-8')
            reviewed.write_text(catalog_bytes.replace('X-Nexus-Reviewed: true',
                                                      'X-Nexus-Reviewed: false'), encoding='utf-8')
            build('html', 'zh_CN', root / 'zh_CN')
            unreviewed = (root / 'zh_CN/english.html').read_text(encoding='utf-8')
            self.assertIn('data-nexus-actual-language="en"', unreviewed)
            self.assertIn('data-nexus-translation-status="missing"', unreviewed)
            self.assertIn('Returns an existing instance.', unreviewed)
            reviewed.write_text(catalog_bytes, encoding='utf-8')
            # A changed source must not consume an obsolete translated catalog.
            (docs / 'english.md').write_text(source_en + '\nChanged contract.\n', encoding='utf-8')
            build('html', 'zh_CN', root / 'zh_CN')
            stale_view = (root / 'zh_CN/english.html').read_text(encoding='utf-8')
            self.assertIn('data-nexus-actual-language="en"', stale_view)
            self.assertIn('data-nexus-translation-status="stale"', stale_view)
            self.assertIn('Changed contract.', stale_view)
            self.assertIn('<div class="nexus-canonical-content" lang="en">',
                          stale_view)
            (docs / 'locale/en/LC_MESSAGES/chinese.po').unlink()
            build('html', 'en', root / 'en')
            missing_view = (root / 'en/chinese.html').read_text(encoding='utf-8')
            self.assertIn('actual: zh_CN; translation: <strong>missing</strong>', missing_view)
            self.assertIn('返回已有实例。', missing_view)


if __name__ == '__main__':
    unittest.main()
