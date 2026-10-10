"""Exercise workflow syntax and the real CTest admission used by the CLI."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[2]


def workflow(name):
    # BaseLoader preserves Actions' `on` key independently of YAML 1.1 booleans.
    return yaml.load((ROOT / '.github/workflows' / name).read_text(), Loader=yaml.BaseLoader)


class WorkflowContracts(unittest.TestCase):
    def test_actual_workflows_and_shell_blocks_parse(self):
        count = 0
        for path in (ROOT / '.github/workflows').glob('*.yml'):
            document = workflow(path.name)
            self.assertIn('on', document, path.name)
            self.assertIsInstance(document['jobs'], dict)
            for job in document['jobs'].values():
                for item in job.get('steps', []):
                    self.assertNotEqual('run' in item, 'uses' in item, path.name)
                    if 'run' in item:
                        script = re.sub(r'\$\{\{.*?\}\}', 'workflow-value', item['run'])
                        result = subprocess.run(['bash', '-n'], input=script,
                                                capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, path.name + ': ' + result.stderr)
                        count += 1
        self.assertGreater(count, 20)

    def test_six_selected_board_backend_link_profiles_have_actual_presets(self):
        jobs = workflow('build-matrix.yml')['jobs']
        selected = set(jobs['arm']['strategy']['matrix']['preset'])
        self.assertEqual(selected, {
            'stm32-qiming-baremetal', 'stm32-qiming-freertos',
            'stm32-sky-baremetal', 'stm32-sky-freertos',
            'gd32-liangshan-baremetal', 'gd32-liangshan-freertos'})
        presets = json.loads((ROOT / 'CMakePresets.json').read_text())
        configured = {p['name'] for p in presets['configurePresets']}
        self.assertLessEqual(selected, configured)
        native = set(jobs['native']['strategy']['matrix']['preset'])
        self.assertEqual(native, {'native-debug', 'native-release', 'native-asan'})
        self.assertLessEqual(native, configured)
        for name in ('arm', 'native'):
            commands = '\n'.join(s.get('run', '') for s in jobs[name]['steps'])
            self.assertIn('tools/dev/dev.py configure', commands)
            self.assertIn('tools/dev/dev.py build', commands)
            self.assertNotIn('kconfig', commands.lower())

    def test_actions_initializer_uses_recursive_pinned_gitlinks(self):
        action = yaml.load((ROOT / '.github/actions/setup-build/action.yml').read_text(),
                           Loader=yaml.BaseLoader)
        commands = '\n'.join(s.get('run', '') for s in action['runs']['steps'])
        self.assertIn('git submodule update --init --recursive', commands)
        self.assertIn('scripts/ci/install_arm_toolchain.py', commands)
        self.assertNotIn('apt-get', commands)

    def test_native_executes_complete_ctest_and_google_plan_once(self):
        native = workflow('build-matrix.yml')['jobs']['native']
        commands = '\n'.join(step.get('run', '') for step in native['steps'])
        self.assertEqual(commands.count('scripts/ci/native_contracts.py'), 1)
        self.assertNotIn('scripts/ci/tdd_gate.py', commands)
        self.assertNotIn('tools/dev/dev.py test', commands)
        self.assertIn('--preset "${{ matrix.preset }}"', commands)
        evidence = [step for step in native['steps']
                    if step.get('name') == 'Preserve execution evidence'][0]
        self.assertIn('/tdd/', evidence['with']['path'])
        self.assertIn('/google-contracts.json', evidence['with']['path'])

    def test_tooling_runs_google_report_and_hil_preparation_boundaries(self):
        tools = workflow('enterprise-tools.yml')['jobs']['tooling']
        commands = '\n'.join(step.get('run', '') for step in tools['steps'])
        self.assertIn('tools/testing/run_tool_tests.py --suite testing', commands)
        self.assertIn('tools-testing.json', commands)
        self.assertEqual(commands.count('--suite configure'), 1)
        self.assertIn('tools.hil.prepare', commands)

    def test_tool_contracts_have_one_automatic_workflow_caller(self):
        events = workflow('enterprise-tools.yml')['on']
        self.assertEqual(set(events), {'workflow_call', 'workflow_dispatch'})
        # Maintained work branches must retain pre-PR checks.
        ci_events = workflow('ci.yml')['on']
        self.assertEqual(set(ci_events['push']['branches']),
                         {'main', 'develop', 'codex/**'})
        self.assertIn('pull_request', ci_events)

    def test_each_arm_link_retains_an_unbound_hil_work_plan(self):
        arm = workflow('build-matrix.yml')['jobs']['arm']
        commands = '\n'.join(step.get('run', '') for step in arm['steps'])
        self.assertIn('tools/hil/prepare.py', commands)
        evidence = [step for step in arm['steps']
                    if step.get('name') == 'Preserve linked software evidence'][0]
        self.assertIn('/hil-preparation.json', evidence['with']['path'])

    def test_advanced_fixture_loop_uses_the_authored_toml_set(self):
        job = workflow('build-matrix.yml')['jobs']['controller-routes']
        commands = '\n'.join(step.get('run', '') for step in job['steps'])
        self.assertIn('"$fixture"/*.toml', commands)
        self.assertNotIn('"$fixture"/*.json', commands)


class CTestAdmission(unittest.TestCase):
    def execute_fixture(self, kind):
        self.assertIsNotNone(shutil.which('cmake'))
        self.assertIsNotNone(shutil.which('ctest'))
        with tempfile.TemporaryDirectory(prefix='nexus real ctest ') as temporary:
            root = Path(temporary)
            for relative in ('tools/dev/dev.py', 'scripts/validation/junit.py'):
                output = root / relative
                output.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / relative, output)
            presets = {
                'version': 6,
                'configurePresets': [{'name': 'fixture', 'generator': 'Unix Makefiles',
                                      'binaryDir': '${sourceDir}/build/fixture'}],
                'testPresets': [{'name': 'fixture', 'configurePreset': 'fixture',
                                 'execution': {'noTestsAction': 'error'}}]}
            (root / 'CMakePresets.json').write_text(json.dumps(presets))
            cmake = 'cmake_minimum_required(VERSION 3.31)\nproject(admission NONE)\nenable_testing()\n'
            if kind != 'empty':
                exit_code = {'pass': 0, 'failure': 1, 'skip': 7}[kind]
                cmake += f'add_test(NAME contract COMMAND "{sys.executable}" -c "raise SystemExit({exit_code})")\n'
                if kind == 'skip':
                    cmake += 'set_tests_properties(contract PROPERTIES SKIP_RETURN_CODE 7)\n'
            (root / 'CMakeLists.txt').write_text(cmake)
            configured = subprocess.run(['cmake', '--preset', 'fixture'], cwd=root,
                                        capture_output=True, text=True, timeout=30)
            self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
            report = root / 'build/fixture/ctest-results.xml'
            # A stale success must never admit a later failed/empty test run.
            report.write_text('<testsuite tests="1"><testcase name="stale"/></testsuite>')
            os.utime(report, (1, 1))
            result = subprocess.run([sys.executable, root / 'tools/dev/dev.py',
                                     'test', '--preset', 'fixture'], cwd=root,
                                    capture_output=True, text=True, timeout=30)
            return result, report.read_text() if report.exists() else ''

    def test_real_success_requires_fresh_test_execution(self):
        result, report = self.execute_fixture('pass')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('contract', report)
        self.assertNotIn('stale', report)

    def test_failed_ctest_cannot_reuse_stale_success(self):
        result, report = self.execute_fixture('failure')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('stale', report)

    def test_zero_tests_fail(self):
        result, report = self.execute_fixture('empty')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('stale', report)

    def test_all_skipped_does_not_establish_execution(self):
        result, report = self.execute_fixture('skip')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('stale', report)
        self.assertIn('skipped', report)


if __name__ == '__main__':
    unittest.main()
