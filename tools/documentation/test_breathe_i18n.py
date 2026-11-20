"""Real Breathe detail extraction and source-preserving translation boundaries."""
import hashlib
import json
import os
from pathlib import Path
import pickle
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.documentation import sphinx_ext


INDEX_XML = '''<?xml version="1.0" encoding="UTF-8"?>
<doxygenindex version="1.13.2">
  <compound refid="contract_8h" kind="file"><name>contract.h</name>
    <member refid="contract_8h_1a1" kind="function">
      <name>contract_submit</name>
    </member>
  </compound>
</doxygenindex>
'''
COMPOUND_XML = '''<?xml version="1.0" encoding="UTF-8"?>
<doxygen version="1.13.2">
  <compounddef id="contract_8h" kind="file" language="C">
    <compoundname>contract.h</compoundname>
    <sectiondef kind="func">
      <memberdef kind="function" id="contract_8h_1a1" prot="public"
                 static="no" const="no" explicit="no" inline="no" virt="non-virtual">
        <type>int</type><definition>int contract_submit</definition>
        <argsstring>(int count)</argsstring><name>contract_submit</name>
        <param><type>int</type><declname>count</declname></param>
        <briefdescription><para>Accept one complete request.</para></briefdescription>
        <detaileddescription><para>
          <parameterlist kind="param"><parameteritem>
            <parameternamelist><parametername direction="in">count</parametername></parameternamelist>
            <parameterdescription><para>Positive complete frame count.</para></parameterdescription>
          </parameteritem></parameterlist>
          <simplesect kind="return"><para>Rejected admission retains no pointer.</para></simplesect>
          <simplesect kind="note"><para>QUARANTINED storage remains borrowed.</para></simplesect>
        </para></detaileddescription>
        <inbodydescription/><location file="contract.h" line="7" column="5"/>
      </memberdef>
    </sectiondef>
    <briefdescription/><detaileddescription/>
    <location file="contract.h" line="1"/>
  </compounddef>
</doxygen>
'''


class BreatheDetailSourceTests(unittest.TestCase):
    def test_only_missing_generated_prose_is_sourced_without_altering_tokens(self):
        from docutils import nodes
        from sphinx import addnodes
        from tools.documentation.breathe_i18n import source_generated_prose
        generated = nodes.container()
        note = nodes.paragraph('Keep the loan.', 'Keep the loan.')
        parameter = nodes.inline('Positive count.', 'Positive count.',
                                 translatable=True)
        generated += note
        generated += nodes.paragraph('', '', nodes.strong('', '[in] '), parameter)
        foreign = nodes.paragraph('Foreign text.', 'Foreign text.')
        foreign.source, foreign.line = 'included.rst', 91
        generated += foreign
        signature = addnodes.desc_signature('contract_submit(int count)', '')
        code = nodes.literal_block('count + 1;', 'count + 1;')
        disabled = nodes.paragraph('Do not translate.', 'Do not translate.',
                                   translatable=False)
        generated.extend((signature, code, disabled))
        untouched = nodes.paragraph('Unrelated prose.', 'Unrelated prose.')
        source_generated_prose([generated], 'api/contracts.md', 17)
        self.assertEqual((note.source, note.line), ('api/contracts.md', 17))
        self.assertEqual((parameter.source, parameter.line), ('api/contracts.md', 17))
        self.assertEqual((foreign.source, foreign.line), ('included.rst', 91))
        for node in (signature, code, disabled, untouched):
            self.assertIsNone(node.source)
        self.assertEqual(signature.rawsource, 'contract_submit(int count)')
        self.assertEqual(note.rawsource, 'Keep the loan.')

    def test_wrapper_uses_the_actual_owning_directive_source_and_line(self):
        from docutils import nodes
        from breathe.directives.file import DoxygenFileDirective
        from tools.documentation.breathe_i18n import SourcedDoxygenFileDirective
        text = nodes.paragraph('Detailed ownership.', 'Detailed ownership.')
        directive = object.__new__(SourcedDoxygenFileDirective)
        with patch.object(DoxygenFileDirective, 'run', return_value=[text]), \
                patch.object(directive, 'get_source_info',
                             return_value=('included/api.rst', 29)):
            self.assertEqual(directive.run(), [text])
        self.assertEqual((text.source, text.line), ('included/api.rst', 29))


class BreatheDetailIntegrationTests(unittest.TestCase):
    def test_real_breathe_parameter_return_and_note_translate_before_locale(self):
        project = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs, xml = root / 'docs', root / 'xml'
            docs.mkdir()
            xml.mkdir()
            (xml / 'index.xml').write_text(INDEX_XML)
            (xml / 'contract_8h.xml').write_text(COMPOUND_XML)
            source = ('---\nid: fixture\ntype: reference\naudience: consumer\nsource_language: en\n---\n\n# Contract fixture\n\n'
                      '```{doxygenfile} contract.h\n:project: nexus\n```\n')
            (docs / 'index.md').write_text(source)
            (docs / 'conf.py').write_text(
                'import sys, os\n' + f'sys.path.insert(0, {str(project)!r})\n'
                + "extensions = ['myst_parser', 'breathe', 'tools.documentation.sphinx_ext']\n"
                + "source_suffix = {'.md': 'markdown'}\nroot_doc = 'index'\n"
                + "project = 'Breathe contract fixture'\nhtml_theme = 'alabaster'\n"
                + "language = os.environ['NEXUS_DOCS_LANGUAGE']\n"
                + "gettext_compact = False\nexclude_patterns = ['locale/**']\n"
                + f"breathe_projects = {{'nexus': {str(xml)!r}}}\n"
                + "breathe_default_project = 'nexus'\n"
                + "breathe_domain_by_extension = {'h': 'c'}\n")
            for args in (['init', '-q'], ['add', 'docs', 'xml'],
                         ['-c', 'user.name=Fixture',
                          '-c', 'user.email=fixture@example.invalid',
                          'commit', '-q', '-m', 'test: Breathe contract fixture']):
                subprocess.run(['git', '-C', str(root), *args], check=True,
                               capture_output=True, text=True)
            environment = dict(os.environ)
            for key in tuple(environment):
                if key.startswith("NEXUS_DOCS_"):
                    environment.pop(key)
            gettext = root / 'gettext'

            def build(builder, language, output):
                current = environment | {'NEXUS_DOCS_LANGUAGE': language}
                if builder != 'gettext':
                    current['NEXUS_DOCS_GETTEXT_ROOT'] = str(gettext)
                result = subprocess.run(
                    [sys.executable, '-B', '-m', 'sphinx', '-E', '-W',
                     '--keep-going', '-b', builder, str(docs), str(output)],
                    env=current, capture_output=True, text=True, timeout=90)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            build('gettext', 'en', gettext)
            _, messages, _ = sphinx_ext.read_catalog(gettext / 'index.pot')
            details = {'[in] Positive complete frame count.',
                       'Rejected admission retains no pointer.',
                       'QUARANTINED storage remains borrowed.'}
            with self.subTest(stage='real_gettext_details'):
                self.assertTrue(details.issubset(messages),
                                sorted(details - set(messages)))
            self.assertNotIn('count', messages)
            self.assertFalse(any('count --' in message for message in messages))
            translations = {
                'Contract fixture': '合同夹具', 'Functions': '函数',
                'Parameters': '参数', 'Returns': '返回',
                'contract.h': 'contract.h',
                'Accept one complete request.': '接受一个完整请求。',
                '[in] Positive complete frame count.': '[in] 完整帧数量必须为正。',
                'Rejected admission retains no pointer.': '拒绝准入时不保留指针。',
                'QUARANTINED storage remains borrowed.': 'QUARANTINED 存储保持借用。',
            }
            self.assertTrue(set(messages).issubset(translations),
                            sorted(set(messages) - set(translations)))
            catalog = docs / 'locale/zh_CN/LC_MESSAGES/index.po'
            catalog.parent.mkdir(parents=True)
            header = ('Language: zh_CN\nContent-Type: text/plain; charset=UTF-8\n'
                      + 'X-Nexus-Source-SHA256: '
                      + hashlib.sha256(source.encode()).hexdigest()
                      + '\nX-Nexus-Reviewed: true\n')
            entries = 'msgid ""\nmsgstr ' + json.dumps(header, ensure_ascii=False) + '\n\n'
            for message in messages:
                entries += ('msgid ' + json.dumps(message, ensure_ascii=False) + '\nmsgstr '
                            + json.dumps(translations[message], ensure_ascii=False) + '\n\n')
            catalog.write_text(entries)
            build('html', 'zh_CN', root / 'zh_CN')
            html = (root / 'zh_CN/index.html').read_text()
            self.assertIn('data-nexus-translation-status="reviewed"', html)
            self.assertIn('接受一个完整请求。', html)
            for original in details:
                with self.subTest(stage='real_Chinese_HTML', detail=original):
                    self.assertTrue(translations[original] in html,
                                    {'expected': translations[original],
                                     'untranslated_contract_present': original in html,
                                     'page_declares_reviewed':
                                         'data-nexus-translation-status="reviewed"' in html})
                    self.assertNotIn(original.removeprefix('[in] '), html)
            self.assertIn('contract_submit', html)


class BreatheIncrementalTests(unittest.TestCase):
    def fixture(self, root):
        project = Path(__file__).resolve().parents[2]
        docs, xml = root / 'docs', root / 'xml'
        docs.mkdir()
        xml.mkdir()
        (root / '.gitignore').write_text('/build/\n')
        (docs / 'conf.py').write_text(
            'import sys\n' + f'sys.path.insert(0, {str(project)!r})\n'
            + "extensions = ['myst_parser', 'breathe', 'tools.documentation.sphinx_ext']\n"
            + "source_suffix = {'.md': 'markdown'}\nroot_doc = 'index'\n"
            + "project = 'Incremental API fixture'\nhtml_theme = 'alabaster'\n"
            + "language = 'en'\ngettext_compact = False\n"
            + f"breathe_projects = {{'nexus': {str(xml)!r}}}\n"
            + "breathe_default_project = 'nexus'\n"
            + "breathe_domain_by_extension = {'h': 'c'}\n")
        prefix = ('---\nid: {name}\ntype: reference\naudience: consumer\n'
                  'source_language: en\n---\n# {name}\n\n')
        (docs / 'index.md').write_text(prefix.format(name='index')
            + '```{toctree}\nalpha\nbeta\n```\n')
        compounds = []
        for name in ('alpha', 'beta'):
            compounds.append(INDEX_XML.split('<doxygenindex', 1)[1]
                .split('>', 1)[1].split('</doxygenindex>', 1)[0]
                .replace('contract', name))
            (xml / (name + '_8h.xml')).write_text(
                COMPOUND_XML.replace('contract', name))
            (docs / (name + '.md')).write_text(prefix.format(name=name)
                + '```{doxygenfile} ' + name + '.h\n:project: nexus\n```\n')
        (xml / 'index.xml').write_text(
            '<?xml version="1.0"?><doxygenindex version="1.13.2">'
            + ''.join(compounds) + '</doxygenindex>')
        for args in (['init', '-q'], ['add', '.'],
                     ['-c', 'user.name=Fixture', '-c',
                      'user.email=fixture@example.invalid', 'commit', '-q',
                      '-m', 'test: incremental API fixture']):
            subprocess.run(['git', '-C', str(root), *args], check=True,
                           capture_output=True)
        # Keep presentation-only dirty state constant while editing API bytes.
        (root / '.gitignore').write_text('/build/\n# Author workspace.\n')
        environment = {key: value for key, value in os.environ.items()
                       if not key.startswith('NEXUS_DOCS_')}
        return docs, xml, environment

    def build(self, root, docs, environment, *, status=0):
        result = subprocess.run([sys.executable, '-B', '-m', 'sphinx', '-W',
            '--keep-going', '-d', str(root / 'build/doctrees'), '-b', 'html',
            str(docs), str(root / 'build/site')], env=environment,
            capture_output=True, text=True, timeout=90)
        self.assertEqual(result.returncode, status, result.stdout + result.stderr)
        return result

    def test_file_directive_does_not_depend_on_unrelated_file_compounds(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs, xml, environment = self.fixture(root)
            self.build(root, docs, environment)
            with (root / 'build/doctrees/environment.pickle').open('rb') as stream:
                env = pickle.load(stream)
            alpha = env.breathe_file_state[str(xml / 'alpha_8h.xml')][1]
            beta = env.breathe_file_state[str(xml / 'beta_8h.xml')][1]
            self.assertEqual(alpha, {'alpha'})
            self.assertEqual(beta, {'beta'})

    def test_same_mtime_xml_edit_refreshes_only_its_actual_page(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs, xml, environment = self.fixture(root)
            self.build(root, docs, environment)
            beta = root / 'build/site/beta.html'
            beta_details = beta.stat()
            source = xml / 'alpha_8h.xml'
            details = source.stat()
            source.write_text(source.read_text().replace(
                'Accept one complete request.', 'Accept two complete request.'))
            os.utime(source, ns=(details.st_atime_ns, details.st_mtime_ns))
            self.build(root, docs, environment)
            self.assertIn('Accept two complete request.',
                          (root / 'build/site/alpha.html').read_text())
            self.assertNotIn('Accept one complete request.',
                             (root / 'build/site/alpha.html').read_text())
            self.assertEqual(beta.stat().st_mtime_ns, beta_details.st_mtime_ns)

    def test_new_commit_updates_links_without_reparsing_the_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs, xml, environment = self.fixture(root)
            header = root / 'public.h'
            header.write_text('/* Public source. */\n')
            page = docs / 'alpha.md'
            page.write_text(page.read_text() + '\n[Source](../public.h)\n')
            self.build(root, docs, environment)
            subprocess.run(['git', '-C', str(root), '-c', 'user.name=Fixture',
                '-c', 'user.email=fixture@example.invalid', 'commit', '-q',
                '--allow-empty', '-m', 'test: new source identity'], check=True,
                capture_output=True)
            revision = subprocess.check_output(['git', '-C', str(root),
                'rev-parse', 'HEAD'], text=True).strip()
            result = self.build(root, docs, environment)
            self.assertIn('0 added, 0 changed, 0 removed', result.stdout)
            content = (root / 'build/site/alpha.html').read_text()
            self.assertIn('/blob/' + revision + '/public.h', content)
            self.assertIn('>' + revision[:12] + '</a>', content)

    def test_static_asset_bytes_refresh_without_source_reparse(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs, xml, environment = self.fixture(root)
            config = docs / 'conf.py'
            config.write_text(config.read_text()
                + "html_static_path = ['static']\nhtml_css_files = ['local.css']\n")
            style = docs / 'static/local.css'
            style.parent.mkdir()
            style.write_text('body { color: red; }\n')
            self.build(root, docs, environment)
            details = style.stat()
            style.write_text('body { color: tan; }\n')
            os.utime(style, ns=(details.st_atime_ns, details.st_mtime_ns))
            environment['NEXUS_DOCS_CHANGED_INPUTS'] = '["docs/static/local.css"]'
            result = self.build(root, docs, environment)
            self.assertIn('0 added, 0 changed, 0 removed', result.stdout)
            self.assertEqual((root / 'build/site/_static/local.css').read_bytes(),
                             style.read_bytes())

    def test_file_lookup_keeps_path_matching_and_ambiguous_name_diagnostics(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs, xml, environment = self.fixture(root)
            source = xml / 'alpha_8h.xml'
            source.write_text(source.read_text().replace('file="alpha.h"',
                                                         'file="left/alpha.h"'))
            (xml / 'right_8h.xml').write_text(COMPOUND_XML.replace(
                'contract', 'right').replace('file="right.h"',
                                             'file="right/alpha.h"'))
            index = xml / 'index.xml'
            index.write_text(index.read_text().replace('</doxygenindex>',
                '<compound refid="right_8h" kind="file"><name>alpha.h</name>'
                '</compound></doxygenindex>'))
            alpha, beta = docs / 'alpha.md', docs / 'beta.md'
            alpha.write_text(alpha.read_text().replace('doxygenfile} alpha.h',
                                                       'doxygenfile} left/alpha.h'))
            beta.write_text(beta.read_text().replace('doxygenfile} beta.h',
                                                     'doxygenfile} right/alpha.h'))
            self.build(root, docs, environment)
            self.assertIn('right_submit', (root / 'build/site/beta.html').read_text())
            beta.write_text(beta.read_text().replace('doxygenfile} right/alpha.h',
                                                     'doxygenfile} alpha.h'))
            result = self.build(root, docs, environment, status=1)
            self.assertIn('Found multiple matches for file',
                          result.stdout + result.stderr)

    def test_file_renderer_retains_its_transitive_struct_xml_dependency(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs, xml, environment = self.fixture(root)
            source = xml / 'alpha_8h.xml'
            source.write_text(source.read_text().replace('<sectiondef kind="func">',
                '<innerclass refid="shared" prot="public">shared</innerclass>'
                '<sectiondef kind="func">'))
            index = xml / 'index.xml'
            index.write_text(index.read_text().replace('</doxygenindex>',
                '<compound refid="shared" kind="struct"><name>shared</name>'
                '</compound></doxygenindex>'))
            shared = xml / 'shared.xml'
            shared.write_text('<?xml version="1.0"?><doxygen version="1.13.2">'
                '<compounddef id="shared" kind="struct" language="C">'
                '<compoundname>shared</compoundname><briefdescription>'
                '<para>Shared ownership counter.</para></briefdescription>'
                '<detaileddescription/><location file="alpha.h" line="1"/>'
                '</compounddef></doxygen>')
            self.build(root, docs, environment)
            with (root / 'build/doctrees/environment.pickle').open('rb') as stream:
                env = pickle.load(stream)
            self.assertEqual(env.breathe_file_state[str(shared)][1], {'alpha'})
            self.assertIn('Shared ownership counter.',
                          (root / 'build/site/alpha.html').read_text())


if __name__ == '__main__':
    unittest.main()
