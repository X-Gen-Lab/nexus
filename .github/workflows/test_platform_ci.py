"""Parse maintained workflows and execute their actual CI command boundaries.

Tiny CMake projects prove Native report admission without repeating the SDK
suite. NEXUS_FIRMWARE_TEST_BUILD selects an actual linked ARM fixture separately.
"""
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/ci'))
from package_release import RELEASE_PROFILES


def workflow(name):
    # BaseLoader keeps GitHub's `on` key intact (YAML 1.1 treats it as a bool).
    return yaml.load((ROOT / '.github/workflows' / name).read_text(), Loader=yaml.BaseLoader)


def step(document, job, name):
    return next(item for item in document['jobs'][job]['steps'] if item.get('name') == name)


class WorkflowStructureTests(unittest.TestCase):
    def test_all_workflows_parse_and_shell_blocks_have_valid_syntax(self):
        self.assertIsNotNone(shutil.which('bash'))
        count = 0
        for path in (ROOT / '.github/workflows').glob('*.yml'):
            document = workflow(path.name)
            self.assertIn('on', document, path.name)
            self.assertIsInstance(document['jobs'], dict)
            for job in document['jobs'].values():
                for item in job.get('steps', []):
                    self.assertNotEqual('run' in item, 'uses' in item, path.name)
                    if 'run' in item:
                        # Parse actual shell, after replacing Actions expressions.
                        script = re.sub(r'\$\{\{.*?\}\}', 'workflow-value', item['run'])
                        result = subprocess.run(['bash', '-n'], input=script, capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, path.name + ': ' + result.stderr)
                        count += 1
        self.assertGreater(count, 20)

    def test_matrix_preserves_all_eight_reviewed_arm_profiles(self):
        document = workflow('build-matrix.yml')
        matrix = document['jobs']['matrix-build']['strategy']['matrix']['include']
        actual = {item['preset'] for item in matrix if item.get('is_arm') == 'true'}
        expected = {name for name, profile in RELEASE_PROFILES.items() if profile['platform'] != 'native'}
        self.assertEqual(actual, expected)
        presets = {item['name'] for item in json.loads((ROOT / 'CMakePresets.json').read_text())['configurePresets']}
        self.assertTrue({item['preset'] for item in matrix} <= presets)
        native_step = step(document, 'matrix-build', 'Run Native contracts and verify fresh JUnit')
        self.assertEqual(native_step['if'], "matrix.preset == 'linux-gcc-release'")
        self.assertEqual(step(document, 'matrix-build', 'Verify ARM Firmware Static Contract')['if'], 'matrix.is_arm')
        self.assertEqual(document['defaults']['run']['shell'], 'bash')

    def test_python_gates_consume_actual_matrix_artifacts(self):
        job = workflow('build-matrix.yml')['jobs']['platform-tooling']
        self.assertEqual(job['needs'], 'matrix-build')
        commands = '\n'.join(item.get('run', '') for item in job['steps'])
        for directory in ('scripts/configure', 'scripts/kconfig', 'scripts/ci', 'tests/validation', 'tests/hil'):
            self.assertIn('discover -s ' + directory, commands)
        env = job['env']
        self.assertEqual(env['NEXUS_RELEASE_TEST_BUILD'], 'build/linux-gcc-release')
        self.assertEqual(env['NEXUS_FIRMWARE_TEST_BUILD'], 'build/stm32-qiming-armgcc-freertos-release')

    def test_dependency_steps_initialize_actual_nested_gitlinks_from_fresh_checkout(self):
        # Local Git fixtures exercise recursive initialization without fetching
        # vendor servers. A nonrecursive command leaves the nested ports absent.
        with tempfile.TemporaryDirectory(prefix='nexus-workflow-gitlinks-') as temporary:
            root = Path(temporary)
            env = dict(os.environ, GIT_ALLOW_PROTOCOL='file', BUILD_PRESET='linux-gcc-release')

            def git(directory, *args):
                result = subprocess.run(['git', '-C', str(directory), *args], env=env,
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result.stdout

            def repository(name):
                directory = root / name
                directory.mkdir()
                git(directory, 'init', '--quiet')
                git(directory, 'config', 'user.name', 'Workflow Test')
                git(directory, 'config', 'user.email', 'workflow-test@local.invalid')
                (directory / 'fixture.c').write_text('/* Dependency model. */\n')
                git(directory, 'add', '.')
                git(directory, 'commit', '--quiet', '-m', 'Dependency model')
                return directory

            leaf = repository('leaf')
            kernel = repository('kernel')
            nested = ('portable/ThirdParty/Community-Supported-Ports',
                      'portable/ThirdParty/Partner-Supported-Ports')
            for name in nested:
                git(kernel, 'submodule', 'add', '--quiet', str(leaf), name)
            git(kernel, 'commit', '--quiet', '-am', 'Nested dependency models')
            source = repository('source')
            dependencies = ('ext/googletest', 'ext/freertos', 'vendors/arm/CMSIS_5',
                            'vendors/st/cmsis_device_f4', 'vendors/st/stm32f4xx_hal_driver')
            for name in dependencies:
                git(source, 'submodule', 'add', '--quiet', str(kernel if name == 'ext/freertos' else leaf), name)
            git(source, 'commit', '--quiet', '-am', 'Maintained dependency models')
            declarations = (
                ('build-matrix.yml', 'matrix-build', 'Initialize Maintained Dependencies'),
                ('release.yml', 'build-release', 'Initialize Maintained Dependencies'),
                ('quality-checks.yml', 'static-analysis', 'Initialize pinned maintained dependencies'),
                ('security.yml', 'codeql', 'Initialize pinned build dependencies'),
            )
            for index, (filename, job, label) in enumerate(declarations):
                with self.subTest(workflow=filename):
                    command = step(workflow(filename), job, label)['run']
                    checkout = root / f'checkout-{index}'
                    git(root, 'clone', '--quiet', '--no-recurse-submodules', str(source), str(checkout))
                    result = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', command], cwd=checkout,
                                            env=env, capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    status = git(checkout, 'submodule', 'status', '--recursive')
                    lines = status.splitlines()
                    self.assertEqual(len(lines), 7)
                    self.assertTrue(all(line.startswith(' ') for line in lines), status)
                    for name in dependencies + tuple('ext/freertos/' + name for name in nested):
                        self.assertTrue(any(' ' + name + ' ' in line for line in lines), status)

    def test_actual_arm_workflow_checker_executes(self):
        self.assertIn('NEXUS_FIRMWARE_TEST_BUILD', os.environ,
                      'Select the actual ARM artifact retained by the matrix')
        build = Path(os.environ['NEXUS_FIRMWARE_TEST_BUILD']).resolve()
        document = workflow('build-matrix.yml')
        command = step(document, 'matrix-build', 'Verify ARM Firmware Static Contract')['run']
        command = command.replace('${{ matrix.preset }}', build.name)
        arguments = shlex.split(command)
        arguments[arguments.index('--build-dir') + 1] = str(build)
        with tempfile.TemporaryDirectory(prefix='nexus-workflow-arm-') as directory:
            report = Path(directory) / 'static-contract.json'
            arguments[arguments.index('--report') + 1] = str(report)
            result = subprocess.run(arguments, cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            contents = json.loads(report.read_text())
            self.assertEqual(contents['kind'], 'arm-static-link-contract')
            self.assertFalse(contents['hardware_verified'])
            self.assertGreater(len(contents['images']), 0)
            board = json.loads((build / 'generated/board-identity.json').read_text())
            self.assertEqual(contents['board_sha256'], board['sha256'])


class WorkflowNativeCommands(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(shutil.which('cmake'))
        self.temp = tempfile.TemporaryDirectory(prefix='nexus-workflow-native-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for relative in ('scripts/ci/ci_build.py', 'scripts/validation/junit.py'):
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        (self.root / 'CMakePresets.json').write_text(json.dumps({
            'version': 3, 'configurePresets': [{'name': 'linux-gcc-release', 'generator': 'Ninja',
                'binaryDir': '${sourceDir}/build/${presetName}',
                'cacheVariables': {'NEXUS_BUILD_TESTS': 'ON', 'NEXUS_PLATFORM': 'native'}}],
            'testPresets': [{'name': 'linux-gcc-release', 'configurePreset': 'linux-gcc-release'}]}))
        self.env = dict(os.environ, BUILD_PRESET='linux-gcc-release')

    def configure(self, mode):
        body = 'cmake_minimum_required(VERSION 3.21)\nproject(workflow_contract NONE)\nenable_testing()\n'
        if mode != 'zero':
            code = 'raise SystemExit(77)' if mode == 'skip' else 'print("finite workflow execution")'
            body += f'add_test(NAME finite.workflow COMMAND "{sys.executable}" -c [=[{code}]=])\n'
            if mode == 'skip':
                body += 'set_tests_properties(finite.workflow PROPERTIES SKIP_RETURN_CODE 77)\n'
        (self.root / 'CMakeLists.txt').write_text(body)
        result = subprocess.run(['cmake', '--preset', 'linux-gcc-release'], cwd=self.root,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def execute(self, command):
        command = command.replace('${{ matrix.preset }}', 'linux-gcc-release')
        return subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', command], cwd=self.root,
                              env=self.env, capture_output=True, text=True, timeout=30)

    def test_native_matrix_command_executes_finite_and_rejects_zero_or_all_skip(self):
        command = step(workflow('build-matrix.yml'), 'matrix-build',
                       'Run Native contracts and verify fresh JUnit')['run']
        for mode in ('finite', 'zero', 'skip'):
            self.configure(mode)
            result = self.execute(command)
            with self.subTest(mode=mode):
                if mode == 'finite':
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn('Verified 1 passed', result.stdout)
                else:
                    self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_release_command_uses_same_real_report_admission(self):
        command = step(workflow('release.yml'), 'build-release',
                       'Test Native platform contracts and verify fresh JUnit')['run']
        self.configure('finite')
        result = self.execute(command)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('Verified 1 passed', result.stdout)
        self.configure('zero')
        self.assertNotEqual(self.execute(command).returncode, 0)

    def test_workflow_cannot_reuse_old_report_when_ctest_emits_nothing(self):
        self.configure('finite')
        report = self.root / 'build/linux-gcc-release/ctest-results.xml'
        report.write_text('<testsuite tests="1"><testcase name="old.pass"/></testsuite>')
        tools = self.root / 'fault-tools'
        tools.mkdir()
        executable = tools / 'ctest'
        executable.write_text('#!' + sys.executable + '\nraise SystemExit(0)\n')
        executable.chmod(0o755)
        self.env['PATH'] = str(tools) + os.pathsep + self.env['PATH']
        command = step(workflow('build-matrix.yml'), 'matrix-build',
                       'Run Native contracts and verify fresh JUnit')['run']
        result = self.execute(command)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(report.exists())


if __name__ == '__main__':
    unittest.main()
