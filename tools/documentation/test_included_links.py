"""Translated includes retain normalized document, data and source links."""
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import unittest
from html.parser import HTMLParser
from pathlib import Path

from tools.documentation import sphinx_ext


class Anchors(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links = []

    def handle_starttag(self, tag, attributes):
        if tag == 'a':
            self.links.append(dict(attributes).get('href', ''))


class IncludedTranslationTests(unittest.TestCase):
    def test_real_bilingual_include_links_survive_reviewed_translation(self):
        project = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / 'docs'
            for directory in (docs / 'explanation', docs / 'design', root / 'io',
                              root / 'vendor'):
                directory.mkdir(parents=True)
            (docs / 'conf.py').write_text(
                'import os, sys\n' + f'sys.path.insert(0, {str(project)!r})\n'
                + "extensions = ['myst_parser', 'tools.documentation.sphinx_ext']\n"
                + "source_suffix = {'.md': 'markdown'}\nroot_doc = 'index'\n"
                + "project = 'Include translation fixture'\nhtml_theme = 'alabaster'\n"
                + "myst_heading_anchors = 3\n"
                + "language = os.environ['NEXUS_DOCS_LANGUAGE']\n"
                + "gettext_compact = False\nexclude_patterns = ['locale/**']\n",
                encoding='utf-8')
            (docs / 'index.md').write_text(
                '# Include fixture\n\n```{toctree}\nexplanation/source-map\n'
                'design/contract\n```\n', encoding='utf-8')
            included = root / 'io/README.md'
            included.write_text(
                '# I/O contract\n\nSee [current contract]'
                '(../docs/design/contract.md#current-contract), [record]'
                '(../docs/design/status.json), and [vendor header]'
                '(../vendor/provider.h).\n\n'
                'The literal `[example](../vendor/provider.h)` stays code.\n',
                encoding='utf-8')
            source = ('---\nsource_language: en\n---\n\n# Source-adjacent contracts\n\n'
                      '```{include} ../../io/README.md\n'
                      ':relative-docs: ../../io/\n:relative-images:\n```\n')
            (docs / 'explanation/source-map.md').write_text(source, encoding='utf-8')
            (docs / 'design/contract.md').write_text(
                '(scope)=\n# Current contract\n\nRetain the storage.\n', encoding='utf-8')
            (docs / 'design/status.json').write_text('{"status":"not_executed"}\n')
            (root / 'vendor/provider.h').write_text('/* Vendor fixture. */\n')
            for arguments in (['init', '-q'], ['add', '.'],
                              ['-c', 'user.name=Fixture',
                               '-c', 'user.email=fixture@example.invalid',
                               'commit', '-q', '-m', 'test: included translation fixture']):
                subprocess.run(['git', '-C', str(root)] + arguments,
                               check=True, capture_output=True)
            revision = subprocess.check_output(
                ['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
            before = included.read_bytes()
            gettext = root / 'gettext'
            environment = os.environ.copy()
            for name in ('NEXUS_DOCS_REVISION', 'NEXUS_DOCS_CODE_REVISION',
                         'NEXUS_DOCS_TRANSLATION_ROOT', 'NEXUS_DOCS_GETTEXT_ROOT',
                         'NEXUS_DOCS_LANGUAGES'):
                environment.pop(name, None)

            def build(builder, language, output):
                local = environment | {'NEXUS_DOCS_LANGUAGE': language}
                if builder != 'gettext':
                    local['NEXUS_DOCS_GETTEXT_ROOT'] = str(gettext)
                result = subprocess.run(
                    [sys.executable, '-m', 'sphinx', '-E', '-n', '-W',
                     '--keep-going', '-b', builder, str(docs), str(output)],
                    env=local, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            build('gettext', 'en', gettext)
            messages = sphinx_ext.read_catalog(gettext / 'explanation/source-map.pot')[1]
            translations = {}
            for message in messages:
                translations[message] = (message.replace('Source-adjacent contracts', '源码旁合同')
                    .replace('I/O contract', 'I/O 合同').replace('See ', '参见 ')
                    .replace('current contract', '当前合同').replace('vendor header', '厂商头文件')
                    .replace('record', '记录').replace(', and ', '，以及 ')
                    .replace('The literal ', '字面示例 ').replace(' stays code.', ' 保持代码。'))
            catalog = docs / 'locale/zh_CN/LC_MESSAGES/explanation/source-map.po'
            catalog.parent.mkdir(parents=True)
            header = ('Language: zh_CN\nContent-Type: text/plain; charset=UTF-8\n'
                      'X-Nexus-Reviewed: true\nX-Nexus-Source-SHA256: '
                      + hashlib.sha256(source.encode()).hexdigest() + '\n')
            text = 'msgid ""\nmsgstr ' + json.dumps(header) + '\n\n'
            for message, translated in translations.items():
                text += ('msgid ' + json.dumps(message) + '\nmsgstr '
                         + json.dumps(translated, ensure_ascii=False) + '\n\n')
            catalog.write_text(text, encoding='utf-8')
            for language in ('en', 'zh_CN'):
                build('html', language, root / language)
                page = root / language / 'explanation/source-map.html'
                rendered = page.read_text(encoding='utf-8')
                links = Anchors()
                links.feed(rendered)
                self.assertIn('../design/contract.html#current-contract', links.links)
                download = next(link for link in links.links if link.endswith('/status.json'))
                self.assertEqual((page.parent / download).read_bytes(),
                                 (docs / 'design/status.json').read_bytes())
                self.assertIn('https://github.com/X-Gen-Lab/nexus/blob/' + revision
                              + '/vendor/provider.h', links.links)
                if language == 'zh_CN':
                    self.assertIn('源码旁合同', rendered)
                    self.assertIn('data-nexus-translation-status="reviewed"', rendered)
            joined = '\n'.join(messages)
            self.assertIn('(/design/contract.md#current-contract)', joined)
            self.assertIn('{download}`record </design/status.json>`', joined)
            self.assertIn('/NEXUS-DOCUMENTATION-SOURCE/vendor/provider.h', joined)
            self.assertIn('`[example](../vendor/provider.h)`', joined)
            self.assertEqual(included.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
