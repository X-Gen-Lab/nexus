"""Diagnostic input failures produce fresh evidence without optional tools."""
from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import sys
import tempfile
from types import ModuleType
import unittest
from unittest.mock import patch

from tools.documentation import check_browser_layout


class BrowserLayoutInputTests(unittest.TestCase):
    def invoke(self, root, chromium, report):
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            try:
                return check_browser_layout.main([
                    '--repository', str(root), '--chromium', str(chromium),
                    '--report', str(report)])
            except SystemExit as failure:
                return failure.code

    def failed_receipt(self, root, chromium, error):
        report = root / 'build/quality/receipt.json'
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text('{"passed":true,"previous":"stale"}\n',
                          encoding='utf-8')
        result = self.invoke(root, chromium, report)
        self.assertNotEqual(result, 0)
        receipt = json.loads(report.read_text(encoding='utf-8'))
        self.assertFalse(receipt['passed'])
        self.assertNotIn('previous', receipt)
        self.assertEqual(receipt['cases'], [])
        self.assertTrue(any(error in item for item in receipt['errors']),
                        receipt)

    def fixture_inputs(self, root):
        chromium = root / 'chromium'
        chromium.write_text('fixture executable\n', encoding='utf-8')
        chromium.chmod(0o700)
        config = root / 'docs/sphinx/conf.py'
        config.parent.mkdir(parents=True)
        config.write_text('# fixture config\n', encoding='utf-8')
        styles = root / 'docs/assets/styles/nexus.css'
        styles.parent.mkdir(parents=True)
        styles.write_text('body { color: black; }\n', encoding='utf-8')
        return chromium, styles

    def test_missing_chromium_replaces_old_success_receipt(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.failed_receipt(root, root / 'absent-chromium', 'Chromium')

    def test_missing_production_css_fails_before_browser_execution(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            chromium, styles = self.fixture_inputs(root)
            styles.unlink()
            # Supply the external import only. The failure must precede invoking
            # the browser or building any Sphinx output.
            api = ModuleType('playwright.sync_api')
            api.sync_playwright = lambda: self.fail('unexpected browser call')
            with patch.dict(sys.modules, {'playwright.sync_api': api}):
                self.failed_receipt(root, chromium, 'CSS')

    def test_missing_playwright_replaces_old_success_receipt(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            chromium, _styles = self.fixture_inputs(root)
            with patch.dict(sys.modules, {'playwright': None,
                                          'playwright.sync_api': None}):
                self.failed_receipt(root, chromium, 'Playwright')

    def test_output_cannot_replace_repository_sources(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'docs/source.json'
            source.parent.mkdir()
            source.write_bytes(b'{"source": "preserve"}\n')
            before = source.read_bytes()
            result = self.invoke(root, root / 'absent', source)
            self.assertNotEqual(result, 0)
            self.assertEqual(source.read_bytes(), before)

    def test_symlink_report_cannot_replace_external_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / 'original.json'
            target.write_bytes(b'{"original": "preserve"}\n')
            alias = root / 'alias.json'
            alias.symlink_to(target)
            result = self.invoke(root, root / 'absent', alias)
            self.assertNotEqual(result, 0)
            self.assertEqual(target.read_bytes(), b'{"original": "preserve"}\n')

    def test_symlink_production_inputs_fail_before_browser_import(self):
        for relative, error in (('docs/assets/styles/nexus.css', 'CSS'),
                                ('docs/sphinx/conf.py', 'configuration')):
            with self.subTest(input=relative):
                with tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    chromium, _styles = self.fixture_inputs(root)
                    source = root / relative
                    target = root / 'external'
                    target.write_bytes(source.read_bytes())
                    source.unlink()
                    source.symlink_to(target)
                    with patch.dict(sys.modules, {'playwright': None,
                                                  'playwright.sync_api': None}):
                        self.failed_receipt(root, chromium, error)

    def test_symlink_screenshot_sibling_rejects_before_writing_report(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / 'build/layout.json'
            output.parent.mkdir()
            target = root / 'source.png'
            target.write_bytes(b'preserve image bytes')
            output.with_name('layout-en.png').symlink_to(target)
            result = self.invoke(root, root / 'absent', output)
            self.assertNotEqual(result, 0)
            self.assertFalse(output.exists())
            self.assertEqual(target.read_bytes(), b'preserve image bytes')

    def test_non_json_output_rejects_without_overwriting(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / 'build/layout.css'
            output.parent.mkdir()
            output.write_bytes(b'preserve stylesheet')
            result = self.invoke(root, root / 'absent', output)
            self.assertNotEqual(result, 0)
            self.assertEqual(output.read_bytes(), b'preserve stylesheet')

    def test_hardlinked_report_cannot_replace_source_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _chromium, styles = self.fixture_inputs(root)
            before = styles.read_bytes()
            output = root / 'build/layout.json'
            output.parent.mkdir()
            os.link(styles, output)
            result = self.invoke(root, root / 'absent', output)
            self.assertNotEqual(result, 0)
            self.assertEqual(styles.read_bytes(), before)
            self.assertEqual(output.read_bytes(), before)

    def test_hardlinked_screenshot_rejects_before_writing_report(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _chromium, styles = self.fixture_inputs(root)
            before = styles.read_bytes()
            output = root / 'build/layout.json'
            output.parent.mkdir()
            os.link(styles, output.with_name('layout-zh_CN.png'))
            result = self.invoke(root, root / 'absent', output)
            self.assertNotEqual(result, 0)
            self.assertFalse(output.exists())
            self.assertEqual(styles.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
