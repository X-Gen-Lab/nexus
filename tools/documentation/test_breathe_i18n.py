"""Real Breathe detail extraction and source-preserving translation boundaries."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

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
        with mock.patch.object(DoxygenFileDirective, 'run', return_value=[text]), \
                mock.patch.object(directive, 'get_source_info',
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
            source = ('---\nsource_language: en\n---\n\n# Contract fixture\n\n'
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
            for key in ('NEXUS_DOCS_REVISION', 'NEXUS_DOCS_CODE_REVISION',
                        'NEXUS_DOCS_TRANSLATION_ROOT', 'NEXUS_DOCS_GETTEXT_ROOT',
                        'NEXUS_DOCS_LANGUAGES'):
                environment.pop(key, None)
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


if __name__ == '__main__':
    unittest.main()
