"""Historical wrappers must share preset arguments and preserve runner failures."""
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]


class EntryPointTests(unittest.TestCase):
    def run_entry(self, path, *arguments):
        return subprocess.run([sys.executable, str(ROOT / path), *arguments], cwd='/tmp', text=True, capture_output=True, timeout=15)

    def test_python_wrappers_share_help_and_reject_obsolete_aliases(self):
        for wrapper in ('scripts/building/build.py', 'scripts/setup/quick-start.py'):
            help_result = self.run_entry(wrapper, '--help')
            self.assertEqual(help_result.returncode, 0, help_result.stderr)
            self.assertIn('--preset', help_result.stdout)
            result = self.run_entry(wrapper, '--preset', 'linux-gcc-debug', '--platform', 'stm32f4')
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertIn('unrecognized arguments', result.stderr)

    def test_dispatcher_preserves_invalid_preset_and_embedded_test_rejection(self):
        for args in [('build', '--preset', 'does-not-exist'),
                     ('test', '--preset', 'stm32-armgcc-release')]:
            result = self.run_entry('scripts/nexus.py', *args)
            self.assertEqual(result.returncode, 2, result.stderr)
        result = self.run_entry('scripts/setup/quick-start.py', '--preset', 'stm32-armgcc-release', '--stage', 'test')
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn('disables host tests', result.stderr)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'shell execution is a Linux check')
    def test_shell_wrapper_propagates_runner_failure_from_another_directory(self):
        result = subprocess.run(['sh', str(ROOT / 'scripts/building/build.sh'), '--preset', 'does-not-exist'],
                                cwd='/tmp', text=True, capture_output=True, timeout=15)
        self.assertEqual(result.returncode, 2, result.stderr)


if __name__ == '__main__':
    unittest.main()
