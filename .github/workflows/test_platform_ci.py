"""Exercise workflow syntax and the real CTest admission used by the CLI."""
import configparser
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


def workflow(name):
    # BaseLoader preserves Actions' `on` key independently of YAML 1.1 booleans.
    return yaml.load((ROOT / '.github/workflows' / name).read_text(), Loader=yaml.BaseLoader)


class WorkflowContracts(unittest.TestCase):
    def test_dependency_review_is_weekly_read_only_and_source_bound(self):
        document = workflow('dependency-maintenance.yml')
        self.assertEqual(document['on']['schedule'][0]['cron'], '0 3 * * 1')
        self.assertEqual(document['permissions'], {'contents': 'read'})
        steps = document['jobs']['observe']['steps']
        commands = '\n'.join(step.get('run', '') for step in steps)
        self.assertIn('dependency_proposals.py --observe-upstream', commands)
        self.assertNotIn('git push', commands)
        self.assertIn('compatibility.py', commands)
        upload = next(i for i, step in enumerate(steps) if step.get(
            'uses', '').startswith('actions/upload-artifact@'))
        seal = steps[upload - 1]
        self.assertEqual(seal['uses'], './.github/actions/record-identity')
        self.assertEqual(seal['with']['require-dependencies'], 'true')
        self.assertEqual(steps[upload]['with']['retention-days'], '30')

    def test_docs_preview_has_no_deployment_permissions(self):
        document = workflow('docs-build.yml')
        self.assertEqual(document['permissions'], {'contents': 'read'})
        caller = workflow('ci.yml')['jobs']['docs']
        self.assertEqual(caller['permissions'], {'contents': 'read'})
        self.assertNotIn('secrets', caller)
        self.assertEqual(set(document['jobs']), {'build'})
        steps = document['jobs']['build']['steps']
        checkout = next(step for step in steps if step.get('uses', '').startswith(
            'actions/checkout@'))
        self.assertEqual(checkout['with']['submodules'], 'recursive')
        commands = '\n'.join(step.get('run', '') for step in steps)
        self.assertIn('tools/documentation/install.py', commands)
        self.assertIn('tools/dev/dev.py docs test', commands)
        self.assertIn('tools/dev/dev.py docs check', commands)
        self.assertIn('tools/dev/dev.py docs build', commands)
        self.assertIn('tools/dev/dev.py docs package', commands)
        self.assertIn('--output "$RUNNER_TEMP/nexus-documentation-package"',
                      commands)
        self.assertIn('shutil.copytree(source, staging)', commands)
        self.assertIn('delivery.verify', commands)
        self.assertNotIn('sudo apt-get', commands)
        self.assertFalse(any('pages' in step.get('uses', '') for step in steps))

    def test_docs_preview_only_uploads_successful_bound_outputs(self):
        steps = workflow('docs-build.yml')['jobs']['build']['steps']
        index = next(index for index, step in enumerate(steps) if step.get(
            'uses', '').startswith('actions/upload-artifact@'))
        upload = steps[index]
        self.assertEqual(upload['if'], 'success()')
        self.assertEqual(upload['with']['if-no-files-found'], 'error')
        seal = steps[index - 1]
        self.assertEqual(seal.get('uses'), './.github/actions/record-identity')
        self.assertEqual(seal['if'], 'success()')
        self.assertEqual(seal['with']['artifacts'].splitlines(), [
            'build/documentation/', 'build/documentation-package/',
            'build/documentation-check.json', 'build/documentation-tests.json'])
        self.assertEqual(seal['with']['output'], 'build/docs-job-identity')
        self.assertEqual(seal['with']['require-dependencies'], 'false')
        self.assertIn('build/docs-job-identity/', upload['with']['path'])

    def test_docs_publish_only_consumes_verified_trusted_branch_payload(self):
        document = workflow('docs-publish.yml')
        self.assertEqual(document['on']['push']['branches'], ['docs-delivery'])
        self.assertNotIn('pull_request', document['on'])
        self.assertEqual(document['permissions'], {'contents': 'read'})
        validate = document['jobs']['validate']
        self.assertEqual(validate['permissions'], {'contents': 'read'})
        commands = '\n'.join(step.get('run', '') for step in validate['steps'])
        self.assertIn('python3 verifier/verify_documentation.py public publication.json', commands)
        self.assertNotIn('pip install', commands)
        self.assertNotIn('tools/dev/dev.py', commands)
        upload = next(step for step in validate['steps'] if step.get(
            'uses', '').startswith('actions/upload-pages-artifact@'))
        self.assertEqual(upload['if'], 'success()')
        self.assertEqual(upload['with']['path'], 'public')
        deploy = document['jobs']['deploy']
        self.assertEqual(deploy['needs'], 'validate')
        self.assertEqual(deploy['permissions'], {'pages': 'write', 'id-token': 'write'})
        self.assertEqual(deploy['environment']['name'], 'nexus-documentation')
        self.assertFalse(any(step.get('uses', '').startswith('actions/checkout@')
                             or 'run' in step for step in deploy['steps']))

    def test_docs_publish_upload_preserves_hidden_payload_identities(self):
        steps = workflow('docs-publish.yml')['jobs']['validate']['steps']
        upload = next(step for step in steps if step.get(
            'uses', '').startswith('actions/upload-pages-artifact@'))
        # Output ownership markers and .nojekyll are verified public bytes.
        # The pinned Pages uploader otherwise excludes every hidden path.
        self.assertEqual(upload['with'].get('include-hidden-files'), 'true')

    def test_source_sdk_attests_only_a_successful_observed_manifest(self):
        job = workflow('release.yml')['jobs']['source-sdk']
        self.assertEqual(job.get('permissions', {}).get('id-token'), 'write')
        self.assertEqual(job.get('permissions', {}).get('attestations'), 'write')
        attestations = [step for step in job['steps'] if step.get(
            'uses', '').startswith('actions/attest-build-provenance@')]
        self.assertEqual(len(attestations), 1)
        self.assertEqual(attestations[0]['if'], 'success()')
        self.assertEqual(attestations[0]['with']['subject-path'],
                         'build/job-identities/source-sdk/identity.json')

    def test_every_software_upload_is_bound_to_observed_job_identity(self):
        count = 0
        for name in ('build-matrix.yml', 'enterprise-tools.yml',
                     'quality-checks.yml', 'performance.yml', 'release.yml'):
            for job in workflow(name)['jobs'].values():
                steps = job.get('steps', [])
                for index, upload in enumerate(steps):
                    if not upload.get('uses', '').startswith(
                            'actions/upload-artifact@'):
                        continue
                    with self.subTest(workflow=name, upload=upload['name']):
                        self.assertGreater(index, 0)
                        seal = steps[index - 1]
                        self.assertEqual(seal.get('uses'),
                                         './.github/actions/record-identity')
                        self.assertEqual(seal['if'], 'always()')
                        self.assertEqual(seal['with']['result'],
                                         '${{ job.status }}')
                        self.assertIn(seal['with']['output'],
                                      upload['with']['path'])
                        self.assertEqual(upload['with']['include-hidden-files'],
                                         'true')
                        self.assertGreaterEqual(
                            int(upload['with']['retention-days']), 30)
                        count += 1
        self.assertGreaterEqual(count, 10)

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
        self.assertEqual(native, {'native-debug', 'native-release', 'native-asan',
                                  'native-tsan'})
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
        self.assertIn('tools/dev/dependencies.py prepare', commands)
        self.assertIn('--scope preset --preset', commands)
        self.assertIn('--allow-network', commands)
        from tools.dev import dependencies
        self.assertTrue(dependencies.required_submodules('native'))
        self.assertIn('scripts/ci/install_arm_toolchain.py', commands)
        self.assertNotIn('apt-get', commands)

    def test_actions_initializer_covers_every_declared_gitlink(self):
        modules = configparser.ConfigParser()
        modules.read(ROOT / '.gitmodules')
        expected = {modules[section]['path'] for section in modules.sections()}
        self.assertTrue(expected)
        action = yaml.load((ROOT / '.github/actions/setup-build/action.yml')
                           .read_text(), Loader=yaml.BaseLoader)
        commands = [step['run'].strip() for step in action['runs']['steps']
                    if 'tools/dev/dependencies.py prepare' in step.get('run', '')]
        self.assertEqual(len(commands), 1)
        arguments = shlex.split(commands[0].replace('\\\n', ' '))
        self.assertIn('--scope', arguments)
        self.assertIn('preset', arguments)
        self.assertIn('--report', arguments)
        from tools.dev import dependencies
        # Native models require every declared dependency; an ARM job binds its
        # explicitly selected recursive closure through the preparation receipt.
        self.assertEqual(set(dependencies.required_submodules('native')), expected)

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

    def test_all_arm_provider_fixtures_use_the_source_bound_audit_gate(self):
        job = workflow('build-matrix.yml')['jobs']['controller-routes']
        selected = set(job['strategy']['matrix'].get('fixture', []))
        self.assertEqual(selected, {
            'stm32_assembly', 'gd32_assembly', 'can_assembly', 'usb_assembly',
            'ethernet_assembly', 'timer_assembly', 'g474_assembly',
            'h743_assembly', 'u575_assembly',
            'multi_controller_assembly', 'mainstream_assembly'})
        for name in selected:
            self.assertTrue(list((ROOT / 'tests/contracts' / name).glob('*.toml')))
        commands = '\n'.join(step.get('run', '') for step in job['steps'])
        self.assertEqual(commands.count('scripts/ci/provider_assemblies.py'), 1)
        self.assertIn('arguments+=(--fixture "$assembly")', commands)
        self.assertIn('"${arguments[@]}"', commands)
        self.assertIn('--compiler arm-none-eabi-gcc', commands)
        self.assertNotIn('cmake --build', commands)
        self.assertNotIn('NEXUS_BACKEND', commands)

    def test_cpu_runtime_matrix_executes_real_kernel_links_and_keeps_evidence(self):
        job = workflow('build-matrix.yml')['jobs']['arch']
        commands = '\n'.join(step.get('run', '') for step in job['steps'])
        self.assertEqual(commands.count('scripts/ci/cortex_runtime.py'), 1)
        self.assertIn('--output build/cortex-runtime', commands)
        evidence = [step for step in job['steps']
                    if step.get('name') == 'Preserve compiler evidence'][0]
        self.assertIn('build/cortex-runtime/', evidence['with']['path'])

    def test_cold_cache_instruction_gate_runs_and_preserves_actual_evidence(self):
        job = workflow('build-matrix.yml')['jobs']['arch']
        commands = '\n'.join(step.get('run', '') for step in job['steps'])
        self.assertEqual(commands.count('scripts/ci/cache_control_compile.py'), 1)
        self.assertIn('--output build/cache-control', commands)
        for name in ('Preserve compiler evidence',
                     'Bind the upload to observed execution identity'):
            step = next(item for item in job['steps'] if item.get('name') == name)
            values = '\n'.join(str(value) for value in step['with'].values())
            self.assertIn('build/cache-control/', values)

    def test_os_cost_and_profile_gates_retain_actual_execution_artifacts(self):
        job = workflow('build-matrix.yml')['jobs']['arch']
        commands = '\n'.join(step.get('run', '') for step in job['steps'])
        evidence = next(step for step in job['steps']
                        if step.get('name') == 'Preserve compiler evidence')
        for name in ('os_profiles', 'os_wait_cost', 'os_resources'):
            self.assertEqual(commands.count('scripts/ci/' + name + '.py'), 1)
            directory = 'build/' + name.replace('_', '-')
            self.assertIn('--output ' + directory, commands)
            self.assertIn(directory + '/', evidence['with']['path'])
        native = workflow('build-matrix.yml')['jobs']['native']
        execute = next(step for step in native['steps']
                       if 'scripts/ci/native_contracts.py' in step.get('run', ''))
        self.assertEqual(execute['env']['TSAN_OPTIONS'], 'halt_on_error=1')

    def test_u575_pair_links_both_images_and_mutates_actual_elfs(self):
        job = workflow('build-matrix.yml')['jobs']['arch']
        steps = job['steps']
        commands = '\n'.join(step.get('run', '') for step in steps)
        self.assertEqual(commands.count('scripts/ci/u575_trustzone.py'), 1)
        self.assertIn('--output build/u575-trustzone', commands)
        mutation = next(step for step in steps if
                        'tests/contracts/u575_trustzone' in step.get('run', ''))
        self.assertEqual(mutation['env']['NEXUS_U575_PAIR_BUILD'],
                         'build/u575-trustzone')
        self.assertIn("-p 'test_*.py'", mutation['run'])
        for name in ('Preserve compiler evidence',
                     'Bind the upload to observed execution identity'):
            step = next(item for item in steps if item.get('name') == name)
            values = '\n'.join(str(value) for value in step['with'].values())
            self.assertIn('build/u575-trustzone/', values)

    def test_optional_os_security_has_actual_link_gates_and_artifacts(self):
        job = workflow('build-matrix.yml')['jobs']['os-security']
        commands = '\n'.join(step.get('run', '') for step in job['steps'])
        evidence = next(step for step in job['steps']
                        if step.get('name') == 'Preserve security link evidence')
        for name in ('os_mpu', 'os_secure_runtime'):
            directory = 'build/' + name.replace('_', '-')
            self.assertEqual(commands.count('scripts/ci/' + name + '.py'), 1)
            self.assertIn('--output ' + directory, commands)
            self.assertIn(directory + '/', evidence['with']['path'])
            execute = next(step for step in job['steps']
                           if 'scripts/ci/' + name + '.py' in
                           step.get('run', ''))
            self.assertIn('--compiler arm-none-eabi-gcc', execute['run'])

    def test_threadx_matrix_uses_locked_kernel_and_retains_execution(self):
        job = workflow('build-matrix.yml')['jobs']['threadx']
        commands = '\n'.join(step.get('run', '') for step in job['steps'])
        self.assertEqual(commands.count('scripts/ci/os_threadx.py'), 1)
        self.assertIn('--compiler arm-none-eabi-gcc', commands)
        self.assertIn('--output build/os-threadx', commands)
        evidence = next(step for step in job['steps']
                        if step.get('name') == 'Preserve ThreadX execution')
        self.assertIn('build/os-threadx/', evidence['with']['path'])
        identity = next(step for step in job['steps']
                        if step.get('uses') ==
                        './.github/actions/record-identity')
        self.assertEqual(identity['with']['require-dependencies'], 'true')
        self.assertIn('build/os-threadx/', identity['with']['artifacts'])
        setup = yaml.load((ROOT / '.github/actions/setup-build/action.yml')
                          .read_text(), Loader=yaml.BaseLoader)
        initialization = '\n'.join(step.get('run', '')
                                   for step in setup['runs']['steps'])
        self.assertIn('tools/dev/dependencies.py prepare', initialization)
        from tools.dev import dependencies
        self.assertIn('ext/threadx', dependencies.required_submodules('native'))


class CTestAdmission(unittest.TestCase):
    def execute_fixture(self, kind):
        self.assertIsNotNone(shutil.which('cmake'))
        self.assertIsNotNone(shutil.which('ctest'))
        with tempfile.TemporaryDirectory(prefix='nexus real ctest ') as temporary:
            root = Path(temporary)
            for relative in ('tools/dev/dev.py', 'tools/dev/process.py',
                             'tools/dev/presets.py', 'tools/dev/provenance.py',
                             'tools/dev/qualification.py',
                             'tools/evidence/common.py', 'tools/evidence/identity.py',
                             'scripts/validation/junit.py'):
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
