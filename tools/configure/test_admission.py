"""Check developer admission failures and exact provider generation paths."""
from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock
from tools.configure import configure


class ConfigurationAdmissionTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        for name in ('dependencies/platform-compatibility.json',
                     'soc/native/soc.json', 'soc/native/routes.json',
                     'boards/native_reference/board.json'):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(configure.ROOT / name, target)
        self.assembly = self.root / 'application.toml'
        self.base = ('schema = 2\nboard = "native_reference"\n'
                     'backend = "native"\n'
                     'clock = "model"\n[memory]\nmain_stack_bytes = 2048\n')
        self.assembly.write_text(self.base)

    def command(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            try:
                status = configure.main(
                    [*args, '--source-root', str(self.root)])
            except SystemExit as error:
                status = error.code
        return status, out.getvalue(), err.getvalue()

    def test_unknown_key_diagnostic_keeps_authored_key_and_source(self):
        self.assembly.write_text(self.base + 'dynamic_heap = true\n')
        status, out, err = self.command(
            'check', '--assembly', str(self.assembly), '--diagnostics-json')
        self.assertEqual(status, 1, err)
        self.assertEqual(out, '')
        diagnostic = json.loads(err)
        self.assertEqual(diagnostic['code'], 'unknown_or_missing_key')
        self.assertEqual(diagnostic['source'], str(self.assembly))
        self.assertEqual(diagnostic['context'], 'memory TOML')
        self.assertEqual(diagnostic['unknown'], ['dynamic_heap'])

    def test_conflict_diagnostic_has_both_owners_and_source_routes(self):
        self.assembly.write_text(self.base + '\n[gpio.first]\nbinding="led0"\n'
            'mode="output"\n[gpio.second]\nbinding="led0"\nmode="output"\n')
        status, _, err = self.command('check', '--assembly', str(self.assembly),
                                      '--diagnostics-json')
        self.assertEqual(status, 1, err)
        diagnostic = json.loads(err)
        self.assertEqual(diagnostic['code'], 'resource_conflict')
        self.assertEqual(diagnostic['resource'], 'pin:PA0')
        self.assertEqual([item['owner'] for item in diagnostic['claims']],
                         ['first', 'second'])
        self.assertTrue(all(item['source'] == 'gpio-pa0'
                            for item in diagnostic['claims']))

    def test_private_board_symlink_parent_cannot_be_normalized_away(self):
        real = self.root / 'private'
        real.mkdir()
        shutil.copyfile(self.root / 'boards/native_reference/board.json',
                        real / 'board.json')
        (self.root / 'alias').symlink_to(real, target_is_directory=True)
        self.assembly.write_text(self.base.replace('board = "native_reference"',
            'board_package = "alias/../private"'))
        status, _, err = self.command('check', '--assembly', str(self.assembly))
        self.assertEqual(status, 1, err)
        self.assertIn('symlink', err)

    def test_provider_check_uses_exact_config_without_qualification(self):
        self.assembly.write_text(
            self.base + '\n[gpio.first]\nbinding="led0"\nmode="output"\n')
        status, out, err = self.command(
            'provider-check', '--assembly', str(self.assembly), '--json')
        self.assertEqual(status, 0, err)
        report = json.loads(out)
        self.assertEqual(report['checks'],
                         ['resolved_typed_ir', 'static_bundle_emitted'])
        self.assertEqual(report['physical_verification'], 'not_applicable')
        self.assertEqual(report['controllers'][0]['public_class'], 'gpio')
        self.assertEqual(report['controllers'][0]['id'], 'first')
        self.assertIn('bindings.c', report['generated_files'])
        self.assertFalse((self.root / 'generated').exists())

    def test_missing_selected_provider_entry_has_an_exact_cli_diagnostic(self):
        missing = ModuleNotFoundError(
            "No module named 'soc.native.configure'",
            name='soc.native.configure')
        with mock.patch('tools.configure.providers.import_module',
                        side_effect=missing):
            status, out, err = self.command(
                'provider-check', '--assembly', str(self.assembly),
                '--diagnostics-json')
        self.assertEqual(status, 1)
        self.assertEqual(out, '')
        diagnostic = json.loads(err)
        self.assertEqual(diagnostic['code'], 'provider_module_missing')
        self.assertEqual(diagnostic['family'], 'native')
        self.assertEqual(diagnostic['module'], 'soc.native.configure')
        self.assertEqual(diagnostic['source'],
                         'soc/native/configure/__init__.py')

    def test_provider_internal_import_error_is_not_an_unmaintained_family(self):
        for error in (ModuleNotFoundError(
                "No module named 'internal_dependency'",
                name='internal_dependency'), ImportError('invalid symbol')):
            with self.subTest(error=error), mock.patch(
                    'tools.configure.providers.import_module',
                    side_effect=error):
                with self.assertRaises(type(error)):
                    self.command('check', '--assembly', str(self.assembly),
                                 '--diagnostics-json')


if __name__ == "__main__":
    unittest.main()
