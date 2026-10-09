"""Execute scaffold examples and hostile console input without shell expansion."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
EXAMPLES = ROOT / 'scripts/kconfig_tools/examples'


class KconfigExampleTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)

    def run_example(self, name, *args, input=None):
        return subprocess.run([sys.executable, str(EXAMPLES / name), *map(str, args)],
                              cwd=self.work, input=input, text=True, capture_output=True,
                              timeout=15)

    def test_all_examples_have_working_help(self):
        for name in ('generate_all_peripherals.py', 'custom_peripheral_example.py',
                     'validate_project.py', 'quick_start.py'):
            result = self.run_example(name, '--help')
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_real_builtin_and_custom_generation(self):
        output = self.work / 'all'
        result = self.run_example('generate_all_peripherals.py', 'NATIVE', output)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(list(output.glob('*/Kconfig'))), 8)
        result = self.run_example('validate_project.py', output)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for mode, extra in (('code', []), ('json', [EXAMPLES / 'custom_peripheral.json'])):
            result = self.run_example('custom_peripheral_example.py', mode, *extra,
                                      'NATIVE', self.work / mode)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((self.work / mode / 'timer/Kconfig').is_file())
        result = self.run_example('generate_all_peripherals.py', 'NATIVE', output)
        self.assertNotEqual(result.returncode, 0)

    def test_zero_and_invalid_selection_do_not_select_last_peripheral(self):
        result = self.run_example('quick_start.py', input='1\n0\n0\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(list(self.work.rglob('Kconfig')), [])
        result = self.run_example('quick_start.py', input='invalid\n99\n0\n')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_noninteger_instances_fail_cleanly(self):
        result = self.run_example('quick_start.py', input='1\n1\nNATIVE\ninvalid\n0\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('Traceback', result.stderr)

    def test_console_output_path_is_one_literal_argument(self):
        literal = self.work / 'draft; echo example-shell-expansion'
        result = self.run_example('quick_start.py',
                                  input=f'1\n1\nNATIVE\n1\n{literal}\n0\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((literal / 'uart/Kconfig').is_file())

    def test_naming_lint_rejects_zero_scope(self):
        result = self.run_example('validate_project.py', self.work)
        self.assertNotEqual(result.returncode, 0)

    def test_choice_value_wrong_name_and_incomplete_mapping_fail(self):
        output = self.work / 'draft'
        result = self.run_example('generate_all_peripherals.py', 'NATIVE', output,
                                  '--peripheral', 'UART', '--instances', 1)
        self.assertEqual(result.returncode, 0, result.stderr)
        path = output / 'uart/Kconfig'
        original = path.read_text()
        self.assertIn('config UART0_PARITY_VALUE', original)
        malformed = original.replace('config UART0_PARITY_VALUE',
                                     'config UART0_WRONG_VALUE')
        for content in (malformed, original.replace(
                'default 0 if NX_UART0_PARITY_NONE', 'default 0 if NX_UART1_PARITY_NONE')):
            path.write_text(content)
            result = self.run_example('validate_project.py', path)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('choice_value_config_missing', result.stdout)

    def test_static_enum_does_not_require_instance_value(self):
        path = self.work / 'Kconfig'
        path.write_text('choice\n    prompt "UART parity"\n'
                        '    default NX_UART_PARITY_NONE\n'
                        'config NX_UART_PARITY_NONE\n    bool "None"\n'
                        'config NX_UART_PARITY_EVEN\n    bool "Even"\n'
                        'endchoice\n')
        result = self.run_example('validate_project.py', path)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
