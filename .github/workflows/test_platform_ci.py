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


def selects_branch(event, branch):
    # The maintained CI policy uses literals and prefix/** only. This does not
    # emulate arbitrary Actions globs, path selection or GitHub run execution.
    if not event:
        return True
    if 'branches-ignore' in event:
        raise AssertionError('CI must not hide stacked PR bases')
    branches = event.get('branches')
    if branches is None:
        return True
    for pattern in branches:
        if pattern.endswith('/**'):
            if branch.startswith(pattern[:-2]):
                return True
        elif '*' in pattern or '?' in pattern or pattern.startswith('!'):
            raise AssertionError('Unexpected maintained branch pattern')
        elif branch == pattern:
            return True
    return False


class WorkflowContracts(unittest.TestCase):
    def test_pushes_select_only_primary_branches(self):
        event = workflow('ci.yml')['on']['push']
        cases = [('main', True), ('develop', True), ('codex/task', False),
                 ('codex/team/task', False), ('feat/io-evolution-20261017', False),
                 ('feat/industrial-platform', False), ('feat/team/task', False),
                 ('fix/task', False), ('fix/team/task', False),
                 ('feature/task', False), ('docs-delivery', False)]
        for branch, expected in cases:
            with self.subTest(branch=branch):
                self.assertEqual(selects_branch(event, branch), expected)
        self.assertEqual(set(event['branches']), {'main', 'develop'})

    def test_pull_requests_admit_stacked_and_other_base_branches(self):
        event = workflow('ci.yml')['on']['pull_request']
        for base in ('main', 'develop', 'feat/industrial-platform',
                     'feat/io-evolution-20261017', 'codex/task', 'fix/task',
                     'maintenance/next'):
            with self.subTest(base=base):
                self.assertTrue(selects_branch(event, base))
        # Actions evaluates PR branch filters against the base, not head.
        # No path/base exclusion may prevent the aggregate status from running.
        self.assertFalse(event)

    def test_ci_keeps_pr_manual_schedule_and_readonly_permissions(self):
        document = workflow('ci.yml')
        self.assertEqual(set(document['on']),
                         {'push', 'pull_request', 'workflow_dispatch', 'schedule'})
        self.assertEqual(document['permissions'],
                         {'contents': 'read', 'pull-requests': 'read'})
        self.assertEqual(document['jobs']['docs']['permissions'],
                         {'contents': 'read'})
        self.assertEqual(document['on']['schedule'], [{'cron': '0 2 * * *'}])
        self.assertEqual(document['jobs']['status-check']['if'], 'always()')

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

    def test_docs_tests_reuse_the_prepared_nondefault_tool_prefix(self):
        steps = workflow('docs-build.yml')['jobs']['build']['steps']
        prepared = next(step['run'] for step in steps
                        if 'tools/documentation/install.py' in step.get('run', ''))
        execute = next(step['run'] for step in steps
                       if 'tools/dev/dev.py docs test' in step.get('run', ''))
        prepared_arguments = shlex.split(prepared.replace('\\\n', ' '))
        test_arguments = shlex.split(execute.replace('\\\n', ' '))
        prefix = prepared_arguments[prepared_arguments.index('--prefix') + 1]
        self.assertEqual(prefix, '$RUNNER_TEMP/nexus-docs-tools')
        self.assertEqual(test_arguments[0], prefix + '/venv/bin/python')
        self.assertIn('--tool-prefix', test_arguments)
        self.assertEqual(test_arguments[test_arguments.index('--tool-prefix') + 1],
                         prefix)
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

    def test_workflow_dsl_hook_and_existing_quality_job_share_the_tool(self):
        configuration = yaml.safe_load(
            (ROOT / '.pre-commit-config.yaml').read_text())
        hooks = {hook['id']: hook for repo in configuration['repos']
                 for hook in repo['hooks']}
        hook = hooks['nexus-workflow-lint']
        self.assertEqual(hook['entry'],
                         'python scripts/ci/lint_workflows.py --staged')
        self.assertEqual(hook['additional_dependencies'],
                         hooks['nexus-tdd-gate']['additional_dependencies'])
        expected = []
        for name in ('development-tools.txt', 'workflow-tools.txt'):
            expected += [line.strip() for line in
                         (ROOT / 'dependencies' / name).read_text().splitlines()
                         if line.strip() and not line.startswith('#')]
        self.assertEqual(hook['additional_dependencies'], expected)
        self.assertTrue(hook['always_run'])
        self.assertFalse(hook['pass_filenames'])
        self.assertTrue(hook['require_serial'])
        document = workflow('quality-checks.yml')
        self.assertEqual(set(document['jobs']),
                         {'commit-style', 'static-analysis'})
        steps = document['jobs']['commit-style']['steps']
        commands = '\n'.join(step.get('run', '') for step in steps)
        self.assertIn('-r dependencies/workflow-tools.txt', commands)
        self.assertIn('scripts/ci/lint_workflows.py --staged', commands)
        self.assertIn('scripts.ci.test_lint_workflows', commands)
        self.assertEqual(document['permissions'], {'contents': 'read'})

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
        from tools.dev.presets import Presets
        presets = Presets(ROOT)
        configured = {name for name, (row, _) in
                      presets.rows['configure'].items()
                      if not row.get('hidden', False)}
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

    def test_mode_independent_suites_have_one_reported_tool_owner(self):
        tools = workflow('enterprise-tools.yml')['jobs']['tooling']
        commands = '\n'.join(step.get('run', '') for step in tools['steps'])
        selected = re.findall(r'--suite\s+(\w+)', commands)
        self.assertCountEqual(selected, [
            'testing', 'configure', 'ci', 'workflows', 'contracts', 'freertos'])
        self.assertNotIn('unittest discover -s tests/contracts', commands)
        seal = next(step for step in tools['steps']
                    if step.get('uses') == './.github/actions/record-identity')
        upload = next(step for step in tools['steps'] if step.get(
            'uses', '').startswith('actions/upload-artifact@'))
        for suite in ('contracts', 'freertos'):
            report = f'build/quality/tools-{suite}.json'
            with self.subTest(suite=suite):
                self.assertIn('--report ' + report, commands)
                self.assertIn(report, seal['with']['artifacts'].splitlines())
                self.assertIn(report, upload['with']['path'].splitlines())

    def test_static_analysis_uses_the_explicit_worker_budget(self):
        steps = workflow('quality-checks.yml')['jobs']['static-analysis']['steps']
        selected = [step['run'] for step in steps
                    if 'scripts/ci/quality_tools.py' in step.get('run', '')]
        self.assertEqual(len(selected), 2)
        for command in selected:
            arguments = shlex.split(command)
            with self.subTest(command=command):
                self.assertIn('--parallel', arguments)
                self.assertEqual(arguments[arguments.index('--parallel') + 1],
                                 '2')

    def analyzer_installation(self, directory, *, update_status=0, install_status=0):
        root = Path(directory)
        tools, temporary = root / 'ports', root / 'runner temporary'
        tools.mkdir()
        temporary.mkdir()
        calls = root / 'apt-calls.jsonl'
        apt = tools / 'apt-get'
        apt.write_text('#!' + sys.executable + '\n'
            'import json,os,sys\nfrom pathlib import Path\n'
            'args=sys.argv[1:];options={}\n'
            'for index,arg in enumerate(args):\n'
            ' if arg=="-o":\n'
            '  key,_,value=args[index+1].partition("=");options[key]=value\n'
            'operation=next((arg for arg in args if arg in ("update","install")),None)\n'
            'source=options.get("Dir::Etc::sourcelist");parts=options.get("Dir::Etc::sourceparts")\n'
            'record={"argv":args,"options":options,"operation":operation,"source":source,\n'
            ' "source_text":Path(source).read_text() if source else None,\n'
            ' "sourceparts":sorted(p.name for p in Path(parts).iterdir()) if parts else None}\n'
            'with open(os.environ["APT_CALLS"],"a") as output:output.write(json.dumps(record)+"\\n")\n'
            'raise SystemExit(int(os.environ["UPDATE_STATUS" if operation=="update" else "INSTALL_STATUS"]))\n')
        apt.chmod(0o755)
        sudo = tools / 'sudo'
        sudo.write_text('#!' + sys.executable + '\n'
            'import os,sys\nfrom pathlib import Path\n'
            'if sys.argv[1:2] != ["apt-get"]:raise SystemExit(97)\n'
            'port=Path(__file__).resolve().parent/"apt-get"\n'
            'os.execv(str(port),[str(port),*sys.argv[2:]])\n')
        sudo.chmod(0o755)
        # Only these harmless host programs are reachable through PATH. Both
        # privileged ports are local fixtures and never delegate to host APT.
        for name in ('cat', 'mkdir'):
            target = shutil.which(name)
            self.assertIsNotNone(target)
            (tools / name).symlink_to(target)
        step = next(step for step in workflow('quality-checks.yml')['jobs']['static-analysis']['steps']
                    if step.get('name') == 'Install analyzers')
        self.assertNotRegex(step['run'], r'/[^\s;]*(?:sudo|apt-get)\b')
        result = subprocess.run([shutil.which('bash'), '-euo', 'pipefail', '-c', step['run']],
            cwd=root, env={'PATH': str(tools), 'RUNNER_TEMP': str(temporary),
                'APT_CALLS': str(calls), 'UPDATE_STATUS': str(update_status),
                'INSTALL_STATUS': str(install_status)}, capture_output=True, text=True, timeout=5)
        records = [json.loads(line) for line in calls.read_text().splitlines()] if calls.exists() else []
        return result, records, step

    def test_analyzer_installation_uses_signed_official_sources_and_bounded_requests(self):
        with tempfile.TemporaryDirectory() as directory:
            result, calls, step = self.analyzer_installation(directory)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual([call['operation'] for call in calls], ['update', 'install'])
            self.assertEqual(int(step['timeout-minutes']), 10)
            self.assertEqual(calls[0]['source'], calls[1]['source'])
            for call in calls:
                with self.subTest(operation=call['operation']):
                    self.assertEqual(call['sourceparts'], [])
                    options = call['options']
                    expected = {'Dir::Etc::sourcelist', 'Dir::Etc::sourceparts', 'Acquire::Retries',
                                'Acquire::http::Timeout', 'Acquire::https::Timeout'}
                    if call['operation'] == 'update':
                        expected.add('APT::Update::Error-Mode')
                    self.assertEqual(set(options), expected)
                    for key, value in {'Acquire::Retries': '2', 'Acquire::http::Timeout': '30',
                                       'Acquire::https::Timeout': '30'}.items():
                        self.assertEqual(options[key], value)
                    paragraphs = [dict(line.split(': ', 1) for line in paragraph.splitlines())
                                  for paragraph in call['source_text'].strip().split('\n\n')]
                    self.assertEqual(len(paragraphs), 2)
                    self.assertEqual({p['URIs']: p['Suites'].split() for p in paragraphs}, {
                        'https://archive.ubuntu.com/ubuntu/': ['noble', 'noble-updates', 'noble-backports'],
                        'https://security.ubuntu.com/ubuntu/': ['noble-security']})
                    for paragraph in paragraphs:
                        self.assertEqual(set(paragraph), {'Types', 'URIs', 'Suites', 'Components', 'Signed-By'})
                        self.assertEqual(paragraph['Types'], 'deb')
                        self.assertEqual(set(paragraph['Components'].split()),
                                         {'main', 'restricted', 'universe', 'multiverse'})
                        self.assertEqual(paragraph['Signed-By'],
                                         '/usr/share/keyrings/ubuntu-archive-keyring.gpg')
                    self.assertNotIn('Dir::State::lists', options,
                                     'Keep the runner-managed APT sandbox and lists permissions')
            self.assertEqual(calls[0]['options']['APT::Update::Error-Mode'], 'any')
            arguments = calls[1]['argv']
            self.assertEqual(arguments[arguments.index('install'):],
                             ['install', '-y', 'clang-tidy-14', 'cppcheck'])

    def test_analyzer_installation_propagates_update_and_install_failures(self):
        for update, install, expected, operations in (
                (100, 0, 100, ['update']),
                (0, 100, 100, ['update', 'install']),
                (0, 124, 124, ['update', 'install'])):
            with self.subTest(update=update, install=install), \
                    tempfile.TemporaryDirectory() as directory:
                result, calls, _ = self.analyzer_installation(
                    directory, update_status=update, install_status=install)
                self.assertEqual(result.returncode, expected, result.stderr)
                self.assertEqual([call['operation'] for call in calls], operations)

    def test_arm_cache_restores_only_untrusted_archive_bytes(self):
        action = yaml.load((ROOT / '.github/actions/setup-build/action.yml')
                           .read_text(), Loader=yaml.BaseLoader)
        steps = action['runs']['steps']
        cached = [step for step in steps if step.get('uses', '').startswith(
            'actions/cache@')]
        self.assertEqual(len(cached), 1)
        cache = cached[0]
        self.assertRegex(cache['uses'], r'^actions/cache@[0-9a-f]{40}$')
        self.assertEqual(cache['with']['path'],
                         '${{ runner.temp }}/nexus-arm-archives')
        self.assertNotIn('restore-keys', cache['with'])
        for identity in ('runner.os', 'runner.arch',
                         'steps.arm-archive.outputs.sha256'):
            self.assertIn('${{ ' + identity + ' }}', cache['with']['key'])
        installer = next(step for step in steps if
                         'scripts/ci/install_arm_toolchain.py' in
                         step.get('run', ''))
        self.assertEqual(installer['if'], "inputs.arm == 'true'")
        self.assertIn('--cache-dir "$RUNNER_TEMP/nexus-arm-archives"',
                      installer['run'])
        self.assertLess(steps.index(cache), steps.index(installer))

    def test_tool_contracts_have_one_automatic_workflow_caller(self):
        events = workflow('enterprise-tools.yml')['on']
        self.assertEqual(set(events), {'workflow_call', 'workflow_dispatch'})
        # Work branches enter automatic CI through their unrestricted PR.
        ci_events = workflow('ci.yml')['on']
        self.assertEqual(set(ci_events['push']['branches']),
                         {'main', 'develop'})
        self.assertIn('pull_request', ci_events)
        callers = []
        for path in (ROOT / '.github/workflows').glob('*.yml'):
            document = workflow(path.name)
            if set(document['on']) & {'push', 'pull_request', 'schedule'}:
                callers += [(path.name, name)
                            for name, job in document['jobs'].items()
                            if job.get('uses') ==
                            './.github/workflows/enterprise-tools.yml']
        self.assertEqual(callers, [('ci.yml', 'tool-contracts')])

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

    def test_h743_provider_stage_binds_canonical_inputs_to_one_project(self):
        job = workflow('build-matrix.yml')['jobs']['controller-routes']
        expected = {
            'fixture': 'h743_assembly',
            'project': 'tests/contracts/h743_assembly',
            'assemblies': (
                'tools/configure/assemblies/h743-spi-dma.toml\n'
                'tools/configure/assemblies/h743-spi-dma-cache-on.toml\n'
                'tools/configure/assemblies/h743-spi-dma-uart-tx.toml\n'
                'tools/configure/assemblies/h743-spi-dma-uart-rx.toml\n'
                'tools/configure/assemblies/h743-spi-dma-runtime-clock.toml\n'
                'tools/configure/assemblies/h743-uart-continuous.toml\n'
                'tools/configure/assemblies/h743-uart-continuous-cache-on.toml\n')}
        self.assertEqual([row for row in job['strategy']['matrix'].get('include', [])
                          if row.get('fixture') == 'h743_assembly'], [expected])
        canonical = expected['assemblies'].splitlines()
        original = sorted((ROOT / expected['project']).glob('*.toml'))
        self.assertEqual(len(original), 26)
        selected = original + [ROOT / value for value in canonical]
        self.assertEqual(len(selected), 33)
        self.assertEqual(len(set(selected)), 33)
        self.assertTrue(all(path.is_file() for path in selected))
        step = next(step for step in job['steps'] if step.get('name') ==
                    'Link and audit each authored software assembly')
        self.assertEqual(step['env']['PROVIDER_PROJECT'], '${{ matrix.project }}')
        self.assertEqual(step['env']['PROVIDER_ASSEMBLIES'],
                         '${{ matrix.assemblies }}')
        self.assertEqual(step['run'].count('scripts/ci/provider_assemblies.py'), 1)
        self.assertIn('arguments+=(--project "$PROVIDER_PROJECT")', step['run'])
        self.assertIn('IFS= read -r assembly', step['run'])

    def test_u575_provider_stage_admits_chain_and_lptim_canonical_inputs(self):
        job = workflow('build-matrix.yml')['jobs']['controller-routes']
        expected = {
            'fixture': 'u575_assembly',
            'project': 'tests/contracts/u575_assembly',
            'assemblies': (
                'tools/configure/assemblies/u575-uart-chain.toml\n'
                'tools/configure/assemblies/u575-lptim-run.toml\n')}
        self.assertEqual([row for row in job['strategy']['matrix'].get('include', [])
                          if row.get('fixture') == 'u575_assembly'], [expected])
        original = sorted((ROOT / expected['project']).glob('*.toml'))
        selected = original + [ROOT / value for value in
                               expected['assemblies'].splitlines()]
        self.assertEqual(len(selected), len(set(selected)))
        self.assertTrue(all(path.is_file() for path in selected))

    def test_provider_shell_passes_each_authored_and_canonical_input_once(self):
        job = workflow('build-matrix.yml')['jobs']['controller-routes']
        step = next(step for step in job['steps'] if step.get('name') ==
                    'Link and audit each authored software assembly')
        selections = {
            'h743_assembly': [
                'tools/configure/assemblies/h743-spi-dma.toml',
                'tools/configure/assemblies/h743-spi-dma-cache-on.toml',
                'tools/configure/assemblies/h743-spi-dma-uart-tx.toml',
                'tools/configure/assemblies/h743-spi-dma-uart-rx.toml',
                'tools/configure/assemblies/h743-spi-dma-runtime-clock.toml',
                'tools/configure/assemblies/h743-uart-continuous.toml',
                'tools/configure/assemblies/h743-uart-continuous-cache-on.toml'],
            'u575_assembly': [
                'tools/configure/assemblies/u575-uart-chain.toml',
                'tools/configure/assemblies/u575-lptim-run.toml'],
            'gd32_assembly': []}
        for family, canonical in selections.items():
            with self.subTest(family=family), tempfile.TemporaryDirectory() as name:
                root = Path(name) / 'repository with spaces'
                project = root / 'tests/contracts' / family
                project.mkdir(parents=True)
                fixture_names = sorted(path.name for path in
                    (ROOT / 'tests/contracts' / family).glob('*.toml'))
                for fixture in fixture_names:
                    (project / fixture).write_text('authored fixture port\n')
                for relative in canonical:
                    path = root / relative
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text('canonical fixture port\n')
                tool = root / '.venv/bin/python'
                tool.parent.mkdir(parents=True)
                tool.write_text(
                    '#!' + sys.executable + '\n'
                    'import json,os,sys\n'
                    'with open(os.environ["RECORDED_PROVIDER_ARGS"], "a") as f:\n'
                    '    f.write(json.dumps(sys.argv[1:]) + "\\n")\n')
                tool.chmod(0o700)
                record = root / 'actual-arguments.jsonl'
                selected_project = ('tests/contracts/' + family
                                    if canonical else '')
                env = dict(os.environ, FIXTURE=family,
                           GITHUB_WORKSPACE=str(root),
                           PROVIDER_PROJECT=selected_project,
                           PROVIDER_ASSEMBLIES='\n'.join(canonical),
                           RECORDED_PROVIDER_ARGS=str(record))
                result = subprocess.run(
                    ['bash', '-euo', 'pipefail', '-c', step['run']],
                    cwd=root, env=env, capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)
                calls = [json.loads(line)
                         for line in record.read_text().splitlines()]
                self.assertEqual(len(calls), 1)
                argv = calls[0]
                self.assertEqual(argv[0], 'scripts/ci/provider_assemblies.py')
                self.assertEqual(argv[argv.index('--root') + 1], str(root))
                actual = [argv[i + 1] for i, value in enumerate(argv)
                          if value == '--fixture']
                expected = ['tests/contracts/' + family + '/' + name
                            for name in fixture_names]
                if canonical:
                    expected += canonical
                    self.assertEqual(argv.count('--project'), 1)
                    self.assertEqual(argv[argv.index('--project') + 1],
                                     selected_project)
                else:
                    self.assertNotIn('--project', argv)
                self.assertEqual(actual, expected)


    def test_named_pin_state_ci_passes_only_the_canonical_input_without_local_toml(self):
        job = workflow('build-matrix.yml')['jobs']['controller-routes']
        canonical = 'tools/configure/assemblies/g474-pin-states.toml'
        project = 'tests/contracts/pin_state_fixture'
        expected = {'fixture': 'pin_state_fixture', 'project': project,
                    'assemblies': canonical + '\n'}
        self.assertEqual([row for row in job['strategy']['matrix'].get('include', [])
                          if row.get('fixture') == 'pin_state_fixture'], [expected])
        self.assertFalse(list((ROOT / project).glob('*.toml')))
        step = next(step for step in job['steps'] if step.get('name') ==
                    'Link and audit each authored software assembly')
        with tempfile.TemporaryDirectory(prefix='Named state CI ') as value:
            root = Path(value) / 'repository with spaces'
            (root / project).mkdir(parents=True)
            assembly = root / canonical
            assembly.parent.mkdir(parents=True)
            assembly.write_text('canonical input port\n')
            tool = root / '.venv/bin/python'
            tool.parent.mkdir(parents=True)
            tool.write_text('#!' + sys.executable + '\n'
                'import json,os,sys\n'
                'with open(os.environ["RECORDED_PROVIDER_ARGS"], "a") as f:\n'
                '    f.write(json.dumps(sys.argv[1:]) + "\\n")\n')
            tool.chmod(0o700)
            record = root / 'arguments.jsonl'
            env = dict(os.environ, FIXTURE='pin_state_fixture',
                       GITHUB_WORKSPACE=str(root), PROVIDER_PROJECT=project,
                       PROVIDER_ASSEMBLIES=canonical + '\n',
                       RECORDED_PROVIDER_ARGS=str(record))
            actual = subprocess.run(['bash', '-euo', 'pipefail', '-c', step['run']],
                                    cwd=root, env=env, capture_output=True, text=True,
                                    timeout=30)
            self.assertEqual(actual.returncode, 0, actual.stdout + actual.stderr)
            calls = [json.loads(line) for line in record.read_text().splitlines()]
            self.assertEqual(len(calls), 1)
            argv = calls[0]
            selected = [argv[index + 1] for index, arg in enumerate(argv)
                        if arg == '--fixture']
            self.assertEqual(selected, [canonical])
            self.assertEqual(argv[argv.index('--project') + 1], project)
            self.assertEqual(argv[0], 'scripts/ci/provider_assemblies.py')

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

    def test_u575_pair_context_capture_is_required_on_original_gate(self):
        steps = workflow('build-matrix.yml')['jobs']['arch']['steps']
        gates = [step for step in steps if
                 'scripts/ci/u575_trustzone.py' in step.get('run', '')]
        self.assertEqual(len(gates), 1)
        self.assertIn('--capture-pair-context', gates[0]['run'])
        paired = [step for step in steps if step.get('name') ==
                  'Seal and independently verify the U575 software pair']
        self.assertEqual(len(paired), 1)
        self.assertNotIn('if', paired[0])
        self.assertIn('--expected-context-sha256', paired[0]['run'])
        self.assertIn('--expected-manifest-sha256', paired[0]['run'])
        self.assertIn('"$RUNNER_TEMP/', paired[0]['run'])
        self.assertNotIn('candidate.py', paired[0]['run'])

    def test_u575_pair_upload_binds_verified_transport_and_original_raw(self):
        steps = workflow('build-matrix.yml')['jobs']['arch']['steps']
        for name in ('Preserve compiler evidence',
                     'Bind the upload to observed execution identity'):
            step = next(item for item in steps if item.get('name') == name)
            values = '\n'.join(str(value) for value in step['with'].values())
            with self.subTest(step=name):
                self.assertIn('build/u575-pair-evidence/', values)
                self.assertIn('build/u575-trustzone/', values)
                self.assertNotIn('RUNNER_TEMP', values)
                self.assertNotIn('/payload', values)

    def test_u575_pair_shell_requires_verify_before_copying_opaque_artifacts(self):
        steps = workflow('build-matrix.yml')['jobs']['arch']['steps']
        selected = [step for step in steps if step.get('name') ==
                    'Seal and independently verify the U575 software pair']
        self.assertEqual(len(selected), 1)
        fake = r'''#!/usr/bin/env python3
import hashlib,json,os,pathlib,sys
args=sys.argv[1:]
assert args[0]=='tools/evidence/u575_pair.py'
operation=args[1]
def value(flag): return args[args.index(flag)+1]
with open(os.environ['FAKE_PAIR_CALLS'],'a') as stream:
    stream.write(json.dumps(args)+'\n')
if operation=='seal':
    context=pathlib.Path(value('--context'))
    assert hashlib.sha256(context.read_bytes()).hexdigest()==value('--expected-context-sha256')
    output=pathlib.Path(value('--output'))
    assert not output.is_relative_to(pathlib.Path.cwd())
    (output/'store').mkdir(parents=True)
    (output/'store'/('a'*64+'.tar.gz')).write_bytes(b'opaque transport fixture')
    (output/'manifest.json').write_text('{"pair":"fixture"}\n')
elif operation=='verify':
    manifest=pathlib.Path(value('--manifest'))
    assert hashlib.sha256(manifest.read_bytes()).hexdigest()==value('--expected-manifest-sha256')
    assert pathlib.Path(value('--archive')).read_bytes()==b'opaque transport fixture'
    if os.environ['FAKE_PAIR_VERIFY_FAILURE']=='1': sys.exit(31)
else: raise AssertionError(operation)
print('{"status":"fixture-only"}')
'''
        for fail in ('0', '1'):
            with self.subTest(verify_failure=fail), tempfile.TemporaryDirectory() as directory:
                root = Path(directory) / 'source'
                tools = root / '.venv/bin'
                tools.mkdir(parents=True)
                executable = tools / 'python'
                executable.write_text(fake.replace('#!/usr/bin/env python3',
                                                    '#!' + sys.executable))
                executable.chmod(0o755)
                for name in ('nm', 'objdump'):
                    path = tools / ('arm-none-eabi-' + name)
                    path.write_bytes(b'external reader fixture')
                    path.chmod(0o755)
                control = root / 'build/u575-trustzone/pair-context'
                control.mkdir(parents=True)
                (control / 'context.json').write_bytes(b'context fixture')
                (control / 'original.raw').write_bytes(b'original raw')
                calls = Path(directory) / 'calls.jsonl'
                temporary = Path(directory) / 'runner temp'
                temporary.mkdir()
                env = dict(os.environ, RUNNER_TEMP=str(temporary),
                           GITHUB_RUN_ID='123', GITHUB_RUN_ATTEMPT='2',
                           PATH=str(tools) + os.pathsep + os.environ['PATH'],
                           FAKE_PAIR_CALLS=str(calls),
                           FAKE_PAIR_VERIFY_FAILURE=fail)
                result = subprocess.run(['bash', '-euo', 'pipefail', '-c',
                                         selected[0]['run']], cwd=root,
                                        env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0 if fail == '0' else 31,
                                 result.stderr)
                self.assertEqual([json.loads(line)[1] for line in
                                  calls.read_text().splitlines()], ['seal', 'verify'])
                artifact = root / 'build/u575-pair-evidence'
                if fail == '1':
                    self.assertFalse(artifact.exists())
                else:
                    self.assertEqual({p.suffix for p in artifact.iterdir()},
                                     {'.json', '.gz'})
                    self.assertEqual(len(list(artifact.iterdir())), 2)
                self.assertEqual((control / 'original.raw').read_bytes(),
                                 b'original raw')

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


PROFILE_BASELINE_SHA = '921ef15385b75a0a9e91dc4be22db7362dcf0f342cfd473008222c431d15d715'
POLICY_BASELINE_SHAS = {'.github/workflows/ci.yml': 'e2ad98367ccdfe5a2292599dfce90a367555a75900717e263b9bd546b9f01f3e', '.github/workflows/release.yml': '7bd91f314401d686e4c6935bfb29279b4a123c09786262e36f5be64da872e010'}


class SoftwareQualificationContracts(unittest.TestCase):
    def document(self):
        path = ROOT / '.github/workflows/software-qualification.yml'
        self.assertTrue(path.is_file(), 'Explicit nonpublishing reusable workflow required')
        return workflow(path.name)

    def steps(self):
        return self.document()['jobs']['qualify']['steps']

    def step(self, identifier):
        return next(step for step in self.steps() if step.get('id') == identifier)

    def fixture(self, parent):
        root = parent / 'source with spaces'
        root.mkdir()
        assembly = root / 'tools/configure/assemblies/space board.toml'
        assembly.parent.mkdir(parents=True)
        assembly.write_text('schema = 2\n')
        (root / '.gitignore').write_text('build/\n')
        for name in ('tools/release/delivery.py', 'tools/configure/metadata.py'):
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        subprocess.run(['git', 'init', '-q', str(root)], check=True)
        self.commit(root)
        return root

    def commit(self, root):
        subprocess.run(['git', '-C', str(root), 'add', '.'], check=True)
        subprocess.run(['git', '-C', str(root), '-c', 'user.name=Fixture',
                        '-c', 'user.email=fixture@example.invalid',
                        'commit', '-qm', 'tracked fixture'], check=True)

    def environment(self, root, parent):
        commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse',
                                          'HEAD'], text=True).strip()
        return dict(os.environ, GITHUB_WORKSPACE=str(root), RUNNER_TEMP=str(parent),
                    GITHUB_SHA=commit, NEXUS_WORKFLOW_SHA=commit,
                    NEXUS_EXPECTED_COMMIT=commit,
                    NEXUS_QUALIFICATION_ASSEMBLY=
                        'tools/configure/assemblies/space board.toml',
                    NEXUS_QUALIFICATION_WORKLOAD='gpio',
                    E=str(parent / 'nexus-reproduction-123-1'),
                    GITHUB_OUTPUT=str(parent / 'outputs'),
                    GITHUB_REPOSITORY='fixture/nexus',
                    GITHUB_WORKFLOW_REF='fixture/nexus/performance.yml@refs/heads/test',
                    GITHUB_EVENT_NAME='workflow_dispatch', GITHUB_REF='refs/heads/test',
                    GITHUB_RUN_ID='123', GITHUB_RUN_ATTEMPT='1')

    def execute(self, identifier, root, env):
        script = self.step(identifier)['run']
        # Execute the maintained guard in the fixture's actual clean checkout.
        script = script.replace('python3 tools/release/delivery.py',
                                shlex.quote(sys.executable) + ' ' +
                                shlex.quote(str(root / 'tools/release/delivery.py')))
        if env.get('REAL_EVIDENCE_STORE') == 'true':
            script = script.replace('.venv/bin/python tools/evidence/store.py',
                                    shlex.quote(sys.executable) + ' ' +
                                    shlex.quote(str(ROOT / 'tools/evidence/store.py')))
        if env.get('REAL_QUALIFICATION_TRANSFER') == 'true':
            for prefix in ('python3', '.venv/bin/python'):
                script = script.replace(
                    prefix + ' tools/evidence/qualification_transfer.py',
                    shlex.quote(sys.executable) + ' ' + shlex.quote(str(
                        ROOT / 'tools/evidence/qualification_transfer.py')))
        return subprocess.run(['bash', '-euo', 'pipefail', '-c', script],
                              cwd=root, env=env, capture_output=True, text=True)

    def test_default_profiles_and_required_policies_are_preserved(self):
        import hashlib
        document = workflow('performance.yml')
        self.assertIsInstance(document['on']['workflow_dispatch'], dict,
                              'Explicit dispatch mode input required')
        mode = document['on']['workflow_dispatch']['inputs']['mode']
        self.assertEqual(mode['default'], 'profiles')
        self.assertEqual(mode['options'], ['profiles', 'qualification-only'])
        self.assertEqual(document['on']['schedule'], [{'cron': '0 2 * * 1'}])
        profile = dict(document['jobs']['profiles'])
        self.assertEqual(profile.pop('if'),
                         "github.event_name == 'schedule' || inputs.mode == 'profiles'")
        canonical = json.dumps(profile, sort_keys=True, separators=(',', ':')).encode()
        self.assertEqual(hashlib.sha256(canonical).hexdigest(), PROFILE_BASELINE_SHA)
        caller = document['jobs']['qualification']
        self.assertEqual(caller['if'],
                         "github.event_name == 'workflow_dispatch' && inputs.mode == 'qualification-only'")
        self.assertEqual(caller['uses'], './.github/workflows/software-qualification.yml')
        self.assertNotIn('secrets', caller)
        for name, expected in POLICY_BASELINE_SHAS.items():
            self.assertEqual(hashlib.sha256((ROOT / name).read_bytes()).hexdigest(), expected)

    def test_reusable_is_readonly_same_source_and_has_no_publication(self):
        document = self.document()
        self.assertEqual(set(document['on']), {'workflow_call'})
        self.assertEqual(document['permissions'], {'contents': 'read'})
        self.assertEqual(set(document['jobs']), {'qualify'})
        steps = self.steps()
        setup = next(step for step in steps
                     if step.get('uses') == './.github/actions/setup-build')
        self.assertLess(steps.index(self.step('resources')), steps.index(setup),
                        'Measure runner headroom before online preparation')
        checkout = self.step('checkout')
        self.assertEqual(checkout['with']['ref'], '${{ github.sha }}')
        self.assertEqual(checkout['with']['persist-credentials'], 'false')
        self.assertEqual(checkout['with']['fetch-depth'], '0')
        for name in ('expected_commit', 'qualification_assembly', 'qualification_workload'):
            self.assertEqual(document['on']['workflow_call']['inputs'][name]['required'], 'true')
        uses = '\n'.join(step.get('uses', '') for step in self.steps())
        self.assertNotIn('attest', uses)
        self.assertNotIn('secrets', document)
        script = '\n'.join(step.get('run', '') for step in self.steps())
        self.assertNotIn('docker run ', script)
        self.assertNotIn('--development-fixture', script)
        self.assertNotIn('candidate.py', script)
        self.assertNotIn('${{ inputs.', script)

    def test_request_guard_rejects_all_sha_mismatches_without_identity(self):
        self.document()
        variants = [('NEXUS_EXPECTED_COMMIT', ''),
                    ('NEXUS_EXPECTED_COMMIT', 'A' * 40),
                    ('NEXUS_EXPECTED_COMMIT', '$(touch injected)'),
                    ('GITHUB_SHA', '1' * 40), ('NEXUS_WORKFLOW_SHA', '2' * 40)]
        for key, value in variants:
            with self.subTest(key=key, value=value), tempfile.TemporaryDirectory() as temporary:
                parent = Path(temporary)
                root = self.fixture(parent)
                env = self.environment(root, parent)
                env[key] = value
                result = self.execute('request', root, env)
                self.assertNotEqual(result.returncode, 0)
                reason = parent / 'nexus-qualification-early-123-1/identity-unavailable.json'
                self.assertTrue(reason.is_file())
                self.assertEqual(json.loads(reason.read_text())['identity_available'], False)
                self.assertFalse((root / 'injected').exists())
                self.assertFalse(list(root.rglob('identity.json')))
                self.assertFalse((parent / 'nexus-source-sdk').exists())

    def test_source_guard_accepts_tracked_spaces_and_records_exact_tuple(self):
        self.document()
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = self.fixture(parent)
            env = self.environment(root, parent)
            self.assertEqual(self.execute('request', root, env).returncode, 0)
            result = self.execute('source', root, env)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            receipt = json.loads((root / 'build/software-qualification/source.json').read_text())
            self.assertEqual(receipt['commit'], env['NEXUS_EXPECTED_COMMIT'])
            self.assertEqual(receipt['assembly'], env['NEXUS_QUALIFICATION_ASSEMBLY'])
            self.assertEqual(receipt['workload'], 'gpio')
            self.assertIn('validated=true', (parent / 'outputs').read_text())

    def test_canonical_h743_flash_path_keeps_the_existing_source_guard(self):
        self.document()
        canonical = 'tools/configure/assemblies/h743-high-peripherals.toml'
        catalogue = json.loads((ROOT /
            'tools/configure/assemblies/catalogue.json').read_text())
        self.assertEqual(catalogue['assemblies'].count(Path(canonical).name), 1)
        actual = ROOT / canonical
        self.assertTrue(actual.is_file())
        previous = ('cmake/package/verification/io_evolution/'
                    'h743-high-peripherals.toml')
        self.assertFalse((ROOT / previous).exists())
        for relative, admitted in ((canonical, True), (previous, False)):
            with self.subTest(path=relative), \
                    tempfile.TemporaryDirectory() as temporary:
                parent = Path(temporary)
                root = self.fixture(parent)
                selected = root / relative
                selected.parent.mkdir(parents=True, exist_ok=True)
                selected.write_bytes(actual.read_bytes())
                self.commit(root)
                env = self.environment(root, parent)
                env['NEXUS_QUALIFICATION_ASSEMBLY'] = relative
                self.assertEqual(self.execute('request', root, env).returncode, 0)
                result = self.execute('source', root, env)
                if admitted:
                    self.assertEqual(result.returncode, 0,
                                     result.stdout + result.stderr)
                    record = json.loads((root /
                        'build/software-qualification/source.json').read_text())
                    self.assertEqual(record['assembly'], canonical)
                    self.assertEqual(record['workload'], 'gpio')
                else:
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse((parent / 'outputs').exists())

    def test_source_guard_rejects_head_dirty_path_and_workload(self):
        self.document()
        for kind in ('head', 'dirty', 'alias', 'absolute', 'symlink', 'untracked', 'backslash', 'workload'):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as temporary:
                parent = Path(temporary)
                root = self.fixture(parent)
                env = self.environment(root, parent)
                if kind == 'head':
                    env['NEXUS_EXPECTED_COMMIT'] = '3' * 40
                elif kind == 'dirty':
                    (root / '.gitignore').write_text('changed\n')
                elif kind == 'alias':
                    env['NEXUS_QUALIFICATION_ASSEMBLY'] = 'tools/configure/assemblies/../assemblies/space board.toml'
                elif kind == 'absolute':
                    env['NEXUS_QUALIFICATION_ASSEMBLY'] = str(root / env['NEXUS_QUALIFICATION_ASSEMBLY'])
                elif kind == 'symlink':
                    link = root / 'tools/configure/assemblies/link.toml'
                    link.symlink_to('space board.toml')
                    self.commit(root)
                    env = self.environment(root, parent)
                    env['NEXUS_QUALIFICATION_ASSEMBLY'] = 'tools/configure/assemblies/link.toml'
                elif kind == 'backslash':
                    name = 'tools/configure/assemblies/odd\\name.toml'
                    (root / name).write_text('schema=2\n')
                    self.commit(root)
                    env = self.environment(root, parent)
                    env['NEXUS_QUALIFICATION_ASSEMBLY'] = name
                elif kind == 'untracked':
                    (root / 'tools/configure/assemblies/new.toml').write_text('schema=2\n')
                    env['NEXUS_QUALIFICATION_ASSEMBLY'] = 'tools/configure/assemblies/new.toml'
                else:
                    env['NEXUS_QUALIFICATION_WORKLOAD'] = '$(touch injected)'
                result = self.execute('source', root, env)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((parent / 'outputs').exists())
                self.assertFalse((root / 'injected').exists())
                self.assertFalse(list(root.rglob('identity.json')))

    def test_complete_sdk_and_actual_observed_image_delegation(self):
        sdk = self.step('sdk')['run']
        for command in ('dependencies.py prepare --scope sdk',
                        'package_source_sdk.py', 'verify_consumers.py',
                        'transport.py pack', 'delivery.py stage'):
            self.assertIn(command, sdk)
        self.assertIn('--parallel 1', sdk)
        self.assertIn('$RUNNER_TEMP/nexus-source-sdk', sdk)
        self.assertNotIn('--development-fixture', sdk)
        image = self.step('image')['run']
        for command in ('recipe.py verify', 'docker build', 'container.py seal',
                        'container.py verify', 'container.py load'):
            self.assertIn(command, image)
        self.assertLess(image.index('container.py seal'), image.index('container.py load'))
        self.assertIn('--secret id=proxy_ca,src=/etc/ssl/certs/ca-certificates.crt', image)
        self.assertIn('dependencies/python-tools-linux-cp312.txt', image)
        self.assertIn('sha256:[0-9a-f]{64}', image)
        formal = self.step('reproduce')['run']
        self.assertEqual(formal.count('tools/evidence/reproduce.py'), 2)
        self.assertIn('--prepare-spec', formal)
        self.assertIn('--workload "$NEXUS_QUALIFICATION_WORKLOAD"', formal)
        self.assertNotIn('--docker', formal)
        self.assertNotIn('--qualification', formal)

    def test_setup_failure_preserves_raw_and_does_not_create_identity(self):
        self.document()
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = self.fixture(parent)
            env = self.environment(root, parent)
            env['NEXUS_EXPECTED_COMMIT'] = 'bad'
            self.assertNotEqual(self.execute('request', root, env).returncode, 0)
            env.update(NEXUS_CHECKOUT_VALIDATED='', NEXUS_JOB_RESULT='failure')
            self.assertEqual(self.execute('collect', root, env).returncode, 0)
            self.assertEqual(self.execute('readiness', root, env).returncode, 0)
            evidence = root / 'build/software-qualification'
            self.assertTrue((evidence / 'setup/identity-unavailable.json').is_file())
            self.assertTrue((evidence / 'setup/request.raw').is_file())
            self.assertFalse(list(root.rglob('identity.json')))
            self.assertNotIn('ready=true', (parent / 'outputs').read_text()
                             if (parent / 'outputs').exists() else '')

    def command_port(self, root, parent):
        port = root / '.venv/bin/python'
        port.parent.mkdir(parents=True)
        port.write_text('#!' + sys.executable + '\n' +
                        'import json,os,sys\n' +
                        'with open(os.environ["PORT_LOG"], "a") as f:\n' +
                        ' f.write(json.dumps(sys.argv[1:])+"\\n")\n' +
                        'if sys.argv[1]==os.environ.get("FAIL_PORT"):\n' +
                        ' print("external port failed",file=sys.stderr);sys.exit(37)\n')
        port.chmod(0o700)
        return dict(PORT_LOG=str(parent / 'ports.jsonl'),
                    Q=str(root / 'build/software-qualification'))

    def test_sdk_external_failure_keeps_exit_raw_and_stops_later_commands(self):
        self.document()
        for failed in ('tools/dev/dependencies.py', 'cmake/package/verify_consumers.py'):
            with self.subTest(failed=failed), tempfile.TemporaryDirectory() as temporary:
                parent = Path(temporary)
                root = self.fixture(parent)
                env = self.environment(root, parent)
                env.update(self.command_port(root, parent), FAIL_PORT=failed)
                Path(env['Q']).mkdir(parents=True)
                result = self.execute('sdk', root, env)
                self.assertEqual(result.returncode, 37)
                calls = [json.loads(line) for line in (parent / 'ports.jsonl').read_text().splitlines()]
                self.assertEqual(calls[-1][0], failed)
                self.assertFalse(any('transport.py' in call[0] for call in calls))
                self.assertIn('external port failed', (Path(env['Q']) / 'sdk.raw').read_text())
                self.assertFalse(list(root.rglob('identity.json')))

    def test_image_ports_and_invalid_iid_keep_real_validator_fail_closed(self):
        self.document()
        for outcome in ('docker-failure', 'bad-id', 'valid-model-id'):
            with self.subTest(outcome=outcome), tempfile.TemporaryDirectory() as temporary:
                parent = Path(temporary)
                root = self.fixture(parent)
                env = self.environment(root, parent)
                env.update(self.command_port(root, parent))
                Path(env['Q']).mkdir(parents=True)
                requirements = root / 'dependencies/python-tools-linux-cp312.txt'
                requirements.parent.mkdir()
                requirements.write_text('model context only\n')
                binary = parent / 'ports'
                binary.mkdir()
                docker = binary / 'docker'
                docker.write_text('#!' + sys.executable + '\n' +
                    'import os,sys\nfrom pathlib import Path\n' +
                    'if os.environ["IMAGE_PORT_OUTCOME"]=="docker-failure":\n' +
                    ' print("model Docker failed",file=sys.stderr);sys.exit(41)\n' +
                    'p=Path(sys.argv[sys.argv.index("--iidfile")+1])\n' +
                    'p.write_text("bad\\n" if os.environ["IMAGE_PORT_OUTCOME"]=="bad-id" else "sha256:"+"1"*64+"\\n")\n')
                docker.chmod(0o700)
                env.update(PATH=str(binary) + os.pathsep + env['PATH'],
                           IMAGE_PORT_OUTCOME=outcome)
                result = self.execute('image', root, env)
                calls = [json.loads(line) for line in (parent / 'ports.jsonl').read_text().splitlines()]
                sealed = any(call[:2] == ['tools/evidence/container.py', 'seal'] for call in calls)
                if outcome == 'valid-model-id':
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertTrue(sealed)
                    self.assertEqual([call[1] for call in calls
                                      if call[0] == 'tools/evidence/container.py'],
                                     ['seal', 'verify', 'load'])
                else:
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse(sealed)
                    if outcome == 'docker-failure':
                        self.assertEqual(result.returncode, 41)
                context = parent / 'nexus-oci-context'
                self.assertEqual([str(p.relative_to(context)) for p in context.rglob('*')
                                  if p.is_file()], ['dependencies/python-tools-linux-cp312.txt'])
                self.assertFalse(list(root.rglob('identity.json')))

    def test_failed_validated_checkout_uses_actual_failed_job_identity(self):
        self.document()
        from tools.maintenance.job_identity import capture
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = self.fixture(parent)
            env = self.environment(root, parent)
            evidence = root / 'build/software-qualification'
            evidence.mkdir(parents=True)
            (evidence / 'failure.raw').write_text('actual fixture failure bytes\n')
            record = capture(root=root, job='qualification-fixture', result='failure',
                             expected_commit=env['NEXUS_EXPECTED_COMMIT'],
                             artifacts=[evidence], tools={'python': sys.executable},
                             output=root / 'build/identity')
            self.assertEqual(record['kind'], 'ci_job_identity')
            self.assertEqual(record['job_result'], 'failure')
            self.assertEqual(record['source']['commit'], env['NEXUS_EXPECTED_COMMIT'])
            self.assertEqual(record['qualification'], 'not_a_software_candidate')
            self.assertEqual(record['artifacts'][0]['relative_path'],
                             'build/software-qualification/failure.raw')

    def test_checkout_cleanup_preserves_bound_early_failure_evidence(self):
        self.document()
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = self.fixture(parent)
            env = self.environment(root, parent)
            env['NEXUS_EXPECTED_COMMIT'] = 'bad'
            request = self.execute('request', root, env)
            self.assertNotEqual(request.returncode, 0)
            early = parent / 'nexus-qualification-early-123-1'
            self.assertTrue((early / 'request.raw').is_file())
            context = (early / 'context.json').read_bytes()
            # An external checkout may clean/init the entire workspace.
            shutil.rmtree(root)
            root.mkdir()
            env.update(NEXUS_CHECKOUT_VALIDATED='', NEXUS_JOB_RESULT='failure')
            self.assertEqual(self.execute('collect', root, env).returncode, 0)
            self.assertEqual(self.execute('readiness', root, env).returncode, 0)
            copied = root / 'build/software-qualification/setup'
            self.assertEqual((copied / 'context.json').read_bytes(), context)
            self.assertTrue((copied / 'request.raw').is_file())
            self.assertEqual(json.loads((copied / 'identity-unavailable.json').read_text())['identity_available'], False)
            self.assertFalse(list(root.rglob('identity.json')))

    def test_failed_partial_archive_keeps_failure_and_original_setup_bytes(self):
        self.document()
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = self.fixture(parent)
            env = self.environment(root, parent)
            env.update(self.command_port(root, parent),
                       NEXUS_CHECKOUT_VALIDATED='true', NEXUS_JOB_RESULT='failure',
                       FAIL_PORT='tools/evidence/store.py')
            early = parent / 'nexus-qualification-early-123-1'
            early.mkdir()
            (early / 'request.raw').write_text('original failure bytes\n')
            partial = parent / 'nexus-sdk-consumers'
            partial.mkdir()
            (partial / 'failure.raw').write_text('partial consumer bytes\n')
            result = self.execute('collect', root, env)
            self.assertEqual(result.returncode, 37)
            copied = root / 'build/software-qualification/setup/request.raw'
            self.assertEqual(copied.read_bytes(), (early / 'request.raw').read_bytes())
            self.assertFalse(list(root.rglob('identity.json')))
            self.assertFalse((root / 'build/delivery').exists())

    def test_unavailable_identity_tool_after_checkout_cannot_claim_ready(self):
        self.document()
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = self.fixture(parent)
            env = self.environment(root, parent)
            env.pop('PYTHONPATH', None)
            env['NEXUS_CHECKOUT_VALIDATED'] = 'true'
            result = self.execute('readiness', root, env)
            self.assertNotEqual(result.returncode, 0)
            reason = root / 'build/software-qualification/setup/identity-unavailable.json'
            self.assertEqual(json.loads(reason.read_text())['reason'],
                             'identity_tool_unavailable')
            self.assertFalse((parent / 'outputs').exists())
            self.assertFalse(list(root.rglob('identity.json')))

    def test_offline_navigation_is_sealed_opaque_before_actual_host_identity(self):
        from tools.configure import configure
        from tools.evidence import store
        from tools.evidence.common import EvidenceError
        from tools.maintenance.job_identity import capture
        self.document()
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = self.fixture(parent)
            env = self.environment(root, parent)
            q = root / 'build/software-qualification'
            q.mkdir(parents=True)
            env.update(Q=str(q), NEXUS_CHECKOUT_VALIDATED='true',
                       NEXUS_JOB_RESULT='success', REAL_EVIDENCE_STORE='true')
            # Seal the real resolver's one-way serialized artifact projection.
            navigation = {}
            resolved = configure.resolve_ir(
                ROOT / 'tools/configure/assemblies/qiming-baremetal.toml',
                ROOT, input_paths=navigation).to_dict()
            report_argument = re.search(r'--report "([^"\n]+)"',
                                        self.step('reproduce')['run']).group(1)
            report_path = Path(report_argument.replace('$Q', str(q)).replace('$E', env['E']))
            report_path.parent.mkdir(parents=True, exist_ok=True)
            report_path.write_text('{"kind":"navigation-transport-fixture"}\n')
            runs = report_path.parent / 'reproduction-runs'
            for directory, mount in (('run-a', '/inputs/sdk-a'),
                                     ('different-path-b', '/other/sdk-b')):
                generated = runs / directory / 'build/generated'
                generated.mkdir(parents=True)
                (generated / 'resolved.json').write_text(json.dumps(resolved))
                container_paths = {label: mount + '/share/nexus/src/' + label
                                   for label in navigation}
                self.assertTrue(container_paths)
                self.assertTrue(all(not Path(path).exists()
                                    for path in container_paths.values()))
                (generated / 'input_paths.json').write_text(json.dumps(container_paths))
                artifacts = generated.parent / 'bin'
                artifacts.mkdir()
                (artifacts / 'image.elf').write_bytes(b'fixture ELF transport bytes\0')
                (artifacts / 'image.bin').write_bytes(b'fixture BIN transport bytes\0')
                (runs / directory / 'command-0.log').write_bytes(b'original raw command bytes\n')
                (runs / directory / 'command.json').write_text(json.dumps(
                    {'argv': ['cmake', '-S', mount + '/share/nexus/src']}))
            original = store.inventory(report_path.parent)
            delivery = root / 'build/delivery'
            delivery.mkdir()
            (delivery / 'fixture.raw').write_text('host identity fixture only\n')
            collection = self.execute('collect', root, env)
            self.assertEqual(collection.returncode, 0,
                             collection.stdout + collection.stderr)
            from tools.evidence.qualification_transfer import ROOTS
            selected = [root / path for _, path in ROOTS[:2]]
            try:
                record = capture(root=root, job='offline-navigation-fixture',
                                 result='success', expected_commit=env['NEXUS_EXPECTED_COMMIT'],
                                 artifacts=selected, tools={'python': sys.executable},
                                 output=root / 'build/actual-host-identity')
            except (EvidenceError, OSError, ValueError) as error:
                self.fail('Host identity must bind the opaque offline closure: ' + str(error))
            package_path = q / 'reproduction-package.json'
            self.assertTrue(package_path.is_file())
            package = json.loads(package_path.read_text())
            store.verify_package(package)
            restored = parent / 'restored-offline-execution'
            store.restore_tree(package, restored)
            self.assertEqual(store.inventory(restored), original)
            self.assertEqual(store.inventory(report_path.parent), original)
            self.assertEqual(record['job_result'], 'success')
            self.assertEqual(record['qualification'], 'not_a_software_candidate')
            self.assertFalse(any(item['relative_path'].endswith('/resolved.json')
                                 for item in record['artifacts']))
            self.assertTrue(any(item['relative_path'].endswith('.tar.gz')
                                for item in record['artifacts']))

    def test_job_environment_contexts_and_step_directory_scope_are_valid(self):
        # Official context-availability table: jobs.<job_id>.env excludes runner.
        # https://docs.github.com/en/actions/reference/workflows-and-actions/contexts#context-availability
        allowed = {'github', 'needs', 'strategy', 'matrix', 'vars', 'secrets', 'inputs'}
        document = self.document()
        for job in document['jobs'].values():
            for key, value in job.get('env', {}).items():
                for expression in re.findall(r'\$\{\{(.*?)\}\}', value):
                    # Ignore quoted literals; inspect actual context references.
                    expression = re.sub(r"'(?:[^']|'')*'", "''", expression)
                    roots = re.findall(r'(?<![\w.])([A-Za-z_]\w*)\s*\.', expression)
                    self.assertTrue(roots, 'Maintained env expression needs a context')
                    for context in roots:
                        self.assertIn(context, allowed, key + ': invalid job.env context')
        self.assertNotIn('E', document['jobs']['qualify']['env'])
        with tempfile.TemporaryDirectory(prefix='runner temp scope ') as temporary:
            expected = str(Path(temporary) / 'nexus-reproduction-123-2')
            for identifier in ('reproduce', 'collect'):
                statements = [line for line in self.step(identifier)['run'].splitlines()
                              if line.startswith('E=')]
                self.assertEqual(len(statements), 1, identifier)
                env = dict(os.environ, RUNNER_TEMP=temporary, GITHUB_RUN_ID='123',
                           GITHUB_RUN_ATTEMPT='2', E='/inherited/wrong-location')
                result = subprocess.run(['bash', '-euo', 'pipefail', '-c',
                                         statements[0] + '\nprintf \'%s\' "$E"'],
                                        env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, expected)

    def test_transport_plan_uses_the_single_role_authority(self):
        from tools.evidence.qualification_transfer import ROOTS
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = self.fixture(parent)
            env = self.environment(root, parent)
            env['REAL_QUALIFICATION_TRANSFER'] = 'true'
            result = self.execute('transport_plan', root, env)
            self.assertEqual(result.returncode, 0, result.stderr)
            actual = Path(env['GITHUB_OUTPUT']).read_text()
            self.assertIn('identity_inputs<<NEXUS_ROOTS\n' +
                          '\n'.join(path for _, path in ROOTS[:2]) +
                          '\nNEXUS_ROOTS\n', actual)
            self.assertIn('identity_output=' + dict(ROOTS)['job_identity'],
                          actual)

    def test_failed_prepare_preserves_original_diagnostics_without_ready(self):
        from tools.evidence import store
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = self.fixture(parent)
            env = self.environment(root, parent)
            env['REAL_QUALIFICATION_TRANSFER'] = 'true'
            original = root / 'build/software-qualification'
            original.mkdir(parents=True)
            (original / 'failure.raw').write_bytes(b'original failure raw\n')
            before = store.inventory(original)
            result = self.execute('prepare', root, env)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(store.inventory(original), before)
            self.assertFalse(Path(env['GITHUB_OUTPUT']).exists())
            self.assertFalse((root / 'build/qualification-transfer/core').exists())

    def test_real_finalizer_preserves_partial_upload_failure(self):
        from unittest.mock import patch
        from tools.evidence.qualification_transfer import ROOTS
        from tools.maintenance.job_identity import capture
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = self.fixture(parent)
            env = self.environment(root, parent)
            env['REAL_QUALIFICATION_TRANSFER'] = 'true'
            for _, relative in ROOTS[:2]:
                directory = root / relative
                directory.mkdir(parents=True)
                (directory / 'original.raw').write_bytes(b'opaque raw\n')
            with patch.dict(os.environ, env):
                capture(root=root, job='software-qualification', result='success',
                        expected_commit=env['NEXUS_EXPECTED_COMMIT'],
                        artifacts=[root / p for _, p in ROOTS[:2]],
                        tools={'python': sys.executable},
                        output=root / dict(ROOTS)['job_identity'])
            prepared = self.execute('prepare', root, env)
            self.assertEqual(prepared.returncode, 0, prepared.stderr)
            env['NEXUS_UPLOAD_STEPS'] = json.dumps({
                'part0': {'outcome': 'failure', 'outputs': {}}})
            failed = self.execute('transfer_result', root, env)
            self.assertNotEqual(failed.returncode, 0)
            receipt = json.loads((root / 'build/qualification-transfer/core/'
                                  'uploads.json').read_text())
            self.assertEqual(receipt['status'], 'partial')
            self.assertFalse(receipt['qualified'])
            self.assertEqual(receipt['uploads'][0]['outcome'], 'failure')

    def test_multipart_uploads_are_individually_connector_bounded(self):
        steps = self.steps()
        parts = [step for step in steps
                 if re.fullmatch(r'part[0-7]', step.get('id', ''))]
        self.assertEqual(len(parts), 8,
                         'One oversized ZIP cannot be independently downloaded')
        for index, step in enumerate(parts):
            self.assertEqual(step['id'], 'part' + str(index))
            self.assertIn('always()', step['if'])
            self.assertNotIn('continue-on-error', step)
            self.assertEqual(step['with']['compression-level'], '0')
            self.assertTrue(step['with']['path'].endswith(
                '/parts/part-' + str(index).zfill(3)))
        core = self.step('core')
        self.assertEqual(core['with']['compression-level'], '0')
        self.assertGreater(steps.index(core), steps.index(parts[-1]))
        self.assertIn("steps.prepare.outcome != 'success'",
                      self.step('upload')['if'])

    def test_identity_and_failure_upload_are_explicit_and_exact(self):
        steps = self.steps()
        identity = self.step('identity')
        self.assertEqual(identity['uses'], './.github/actions/record-identity')
        self.assertEqual(identity['if'], "always() && steps.readiness.outputs.ready == 'true'")
        self.assertEqual(identity['with']['result'], '${{ job.status }}')
        self.assertEqual(identity['with']['require-dependencies'], 'true')
        upload = self.step('upload')
        self.assertEqual(upload['if'],
                         "always() && steps.prepare.outcome != 'success'")
        self.assertEqual(upload['with']['include-hidden-files'], 'true')
        self.assertEqual(upload['with']['if-no-files-found'], 'error')
        self.assertEqual(upload['with']['retention-days'], '30')
        self.assertLess(steps.index(identity), steps.index(self.step('prepare')))
        self.assertEqual(identity['with']['output'],
                         '${{ steps.transport_plan.outputs.identity_output }}')
        self.assertEqual(identity['with']['artifacts'],
                         '${{ steps.transport_plan.outputs.identity_inputs }}')
        from tools.evidence.qualification_transfer import ROOTS
        self.assertEqual(upload['with']['path'].splitlines(),
                         [path + '/' for _, path in ROOTS])
        self.assertEqual(self.step('collect')['if'], 'always()')
        collect = self.step('collect')['run']
        self.assertIn('tools/evidence/store.py export', collect)
        self.assertNotIn('transport.py pack', collect)
        self.assertIn('NEXUS_JOB_RESULT', collect)


class FormalSoftwareWorkflow(unittest.TestCase):
    """Keep the manual plan entry finite, complete and separate from release."""

    def document(self):
        return workflow('formal-software.yml')

    def step(self, identifier):
        return next(step for step in self.document()['jobs']['formal']['steps']
                    if step.get('id') == identifier)

    def python_body(self, identifier):
        script = self.step(identifier)['run']
        return script.split("<<'PY'\n", 1)[1].rsplit('\nPY', 1)[0]

    def test_dispatch_is_explicit_read_only_and_source_bound(self):
        document = self.document()
        self.assertEqual(set(document['on']), {'workflow_dispatch'})
        inputs = document['on']['workflow_dispatch']['inputs']
        self.assertEqual(set(inputs), {'mode', 'expected_commit', 'run_id',
                                      'attempt', 'expected_source'})
        self.assertEqual(inputs['mode']['default'], 'formal')
        self.assertEqual(inputs['mode']['options'], ['formal', 'diagnostics'])
        self.assertEqual(inputs['expected_commit']['required'], 'false')
        self.assertEqual(document['permissions'], {'contents': 'read'})

        self.assertEqual(document['concurrency']['cancel-in-progress'], 'false')
        steps = document['jobs']['formal']['steps']
        self.assertEqual(steps[0]['id'], 'request')
        self.assertEqual(self.step('request')['env']['F'],
                         '${{ runner.temp }}/formal')
        self.assertIn('echo "F=$F" >> "$GITHUB_ENV"',
                      self.step('request')['run'])
        checkout = self.step('checkout')
        self.assertEqual(checkout['with']['ref'], '${{ github.sha }}')
        self.assertEqual(checkout['with']['fetch-depth'], '0')
        self.assertEqual(checkout['with']['persist-credentials'], 'false')
        self.assertNotIn('${{ inputs.', self.step('request')['run'])

    def diagnostic_step(self, identifier):
        steps = self.document()['jobs']['diagnostics']['steps']
        return next(step for step in steps if step.get('id') == identifier)

    def test_diagnostics_is_separate_readonly_without_plan_or_build(self):
        jobs = self.document()['jobs']
        self.assertEqual(jobs['formal']['if'],
                         "inputs.mode == 'formal' || inputs.mode == ''")
        job = jobs['diagnostics']
        self.assertEqual(job['if'], "inputs.mode == 'diagnostics'")
        self.assertEqual(job['permissions'],
                         {'contents': 'read', 'actions': 'read'})
        self.assertEqual(job['timeout-minutes'], '30')
        commands = '\n'.join(step.get('run', '') for step in job['steps'])
        for forbidden in ('qualification.py', 'candidate.py', 'cmake',
                          'setup-build', 'git push', 'gh release'):
            self.assertNotIn(forbidden, commands)
        self.assertNotIn('${{ inputs.', commands)
        checkout = self.diagnostic_step('checkout')
        self.assertEqual(checkout['with']['ref'], '${{ github.sha }}')
        self.assertEqual(checkout['with']['persist-credentials'], 'false')

    def test_diagnostics_checks_actual_run_attempt_and_artifact_before_download(self):
        from unittest.mock import patch
        script = self.diagnostic_step('request')['run']
        body = script.split("<<'PY'\n", 1)[1].rsplit('\nPY', 1)[0]
        source = 'a' * 40
        run = {'id': 123, 'run_attempt': 2, 'head_sha': source,
               'head_branch': 'main', 'status': 'completed',
               'conclusion': 'failure', 'event': 'workflow_dispatch',
               'path': '.github/workflows/formal-software.yml',
               'repository': {'full_name': 'X-Gen-Lab/nexus'}}
        artifact = {'id': 789, 'name': f'formal-software-{source}-123-2',
                    'expired': False, 'size_in_bytes': 12345,
                    'workflow_run': {'id': 123, 'head_sha': source}}
        cases = [('valid', {}, {}, {}, False),
                 ('malformed_run', {'NEXUS_TARGET_RUN': '../123'}, {}, {}, True),
                 ('malformed_attempt', {'NEXUS_TARGET_ATTEMPT': '0'}, {}, {}, True),
                 ('malformed_source', {'NEXUS_EXPECTED_SOURCE': 'short'}, {}, {}, True),
                 ('api_failure', {}, {}, {}, True),
                 ('duplicate_artifact', {}, {}, {'_copies': 2}, True),
                 ('wrong_source', {}, {'head_sha': 'b' * 40}, {}, True),
                 ('wrong_attempt', {}, {'run_attempt': 1}, {}, True),
                 ('wrong_run', {}, {'id': 124}, {}, True),
                 ('wrong_repo', {}, {'repository': {'full_name': 'else/repo'}}, {}, True),
                 ('wrong_workflow', {}, {'path': '.github/workflows/ci.yml'}, {}, True),
                 ('running', {}, {'status': 'in_progress'}, {}, True),
                 ('success', {}, {'conclusion': 'success'}, {}, False),
                 ('cancelled', {}, {'conclusion': 'cancelled'}, {}, True),
                 ('expired', {}, {}, {'expired': True}, True),
                 ('artifact_source', {}, {}, {'workflow_run': {'id': 123, 'head_sha': 'b' * 40}}, True),
                 ('artifact_attempt', {}, {}, {'name': f'formal-software-{source}-123-1'}, True)]
        for name, changed, run_change, artifact_change, rejected in cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                env = {'D': str(root / 'diagnostic'),
                       'GITHUB_EVENT_NAME': 'workflow_dispatch',
                       'GITHUB_REF': 'refs/heads/fix/diagnostics',
                       'GITHUB_REPOSITORY': 'X-Gen-Lab/nexus',
                       'GITHUB_SHA': 'c' * 40,
                       'NEXUS_WORKFLOW_SHA': 'c' * 40,
                       'GITHUB_OUTPUT': str(root / 'outputs'),
                       'NEXUS_TARGET_RUN': '123',
                       'NEXUS_TARGET_ATTEMPT': '2',
                       'NEXUS_EXPECTED_SOURCE': source, **changed}
                calls = []

                def api(argv, **kwargs):
                    calls.append(argv)
                    self.assertEqual(argv[:2], ['gh', 'api'])
                    self.assertEqual(kwargs['timeout'], 60)
                    if name == 'api_failure':
                        raise subprocess.CalledProcessError(1, argv)
                    if '/attempts/' in argv[-1]:
                        return json.dumps({**run, **run_change})
                    self.assertIn('--paginate', argv)
                    self.assertIn('--slurp', argv)
                    copies = artifact_change.get('_copies', 1)
                    changed_artifact = {key: value for key, value in
                                        artifact_change.items() if key != '_copies'}
                    return json.dumps([{'artifacts':
                        [{**artifact, **changed_artifact}] * copies}])

                with patch.dict(os.environ, env), \
                        patch('subprocess.check_output', side_effect=api):
                    if rejected:
                        with self.assertRaises(SystemExit):
                            exec(compile(body, '<diagnostics-request>', 'exec'), {})
                    else:
                        exec(compile(body, '<diagnostics-request>', 'exec'), {})
                selected = json.loads((root / 'diagnostic/request/selection.json').read_text())
                self.assertEqual(selected['accepted'], not rejected)
                self.assertFalse(selected['software_qualified'])
                if not rejected:
                    self.assertEqual(selected['source_conclusion'],
                                     run_change.get('conclusion', 'failure'))
                    self.assertIn('artifact_id=789\n', (root / 'outputs').read_text())
                    self.assertEqual(calls[0][-1],
                                     'repos/X-Gen-Lab/nexus/actions/runs/123/attempts/2')
                else:
                    self.assertFalse((root / 'outputs').exists())

    def test_diagnostics_downloads_only_selected_official_artifact(self):
        step = self.diagnostic_step('download')
        self.assertEqual(step['uses'],
                         'actions/download-artifact@9000827ccba6bdab643e8b6fd33ac0654aef8333')
        self.assertEqual(step['with']['artifact-ids'],
                         '${{ steps.request.outputs.artifact_id }}')
        self.assertEqual(step['with']['run-id'], '${{ inputs.run_id }}')
        self.assertEqual(step['with']['repository'], '${{ github.repository }}')
        self.assertEqual(step['with']['github-token'], '${{ github.token }}')
        extract = self.diagnostic_step('extract')['run']
        self.assertIn('tools/evidence/formal_diagnostics.py', extract)
        self.assertIn('--run-attempt "$NEXUS_TARGET_ATTEMPT"', extract)
        self.assertIn('--expected-commit "$NEXUS_EXPECTED_SOURCE"', extract)

    def test_diagnostics_keeps_each_bounded_part_in_separate_artifact(self):
        steps = self.document()['jobs']['diagnostics']['steps']
        uploads = [step for step in steps
                   if step.get('uses', '').startswith('actions/upload-artifact@')]
        self.assertEqual(len(uploads), 18)
        for index in range(16):
            step = self.diagnostic_step(f'part_{index:03}')
            self.assertEqual(step['with']['path'],
                             '${{ runner.temp }}/formal-inspection/result/parts/' +
                             f'part-{index:03}')
            self.assertEqual(step['if'],
                             "always() && steps.extract.outcome == 'success' && "
                             f'fromJSON(steps.extract.outputs.part_count) > {index}')
            self.assertEqual(step['with']['compression-level'], '0')
            self.assertEqual(step['with']['if-no-files-found'], 'error')
        for step in uploads:
            self.assertNotIn('continue-on-error', step)
            self.assertNotIn('/artifact/', step['with']['path'])
            self.assertNotIn('result/\n', step['with']['path'])

    def test_actual_guard_rejects_wrong_event_branch_and_sha(self):
        source = self.python_body('request')
        good = {'GITHUB_EVENT_NAME': 'workflow_dispatch',
                'GITHUB_REF': 'refs/heads/main', 'GITHUB_SHA': 'a' * 40,
                'NEXUS_WORKFLOW_SHA': 'a' * 40,
                'NEXUS_EXPECTED_COMMIT': 'a' * 40}
        cases = [({}, 0), ({'GITHUB_EVENT_NAME': 'push'}, 1),
                 ({'GITHUB_REF': 'refs/heads/feature'}, 1),
                 ({'NEXUS_EXPECTED_COMMIT': 'short'}, 1),
                 ({'NEXUS_EXPECTED_COMMIT': 'b' * 40}, 1),
                 ({'NEXUS_WORKFLOW_SHA': 'b' * 40}, 1)]
        for changed, expected in cases:
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as directory:
                env = {**os.environ, **good, **changed, 'F': directory}
                result = subprocess.run([sys.executable, '-B', '-c', source],
                                        env=env, capture_output=True, timeout=10)
                self.assertEqual(result.returncode, expected, result.stderr)
                record = json.loads((Path(directory) / 'request.json').read_text())
                self.assertEqual(record['accepted'], expected == 0)
                self.assertFalse(record['software_qualified'])

    def test_setup_reuses_complete_native_dependencies_and_existing_analyzers(self):
        job = self.document()['jobs']['formal']
        self.assertEqual(job['runs-on'], 'ubuntu-24.04')
        self.assertEqual(job['timeout-minutes'], '360')
        self.assertEqual(job['env']['NEXUS_BUILD_JOBS'], '2')
        self.assertEqual(job['env']['CMAKE_BUILD_PARALLEL_LEVEL'], '2')
        self.assertEqual(job['env']['CTEST_PARALLEL_LEVEL'], '1')
        self.assertNotIn('NEXUS_COMMAND_TIMEOUT', job['env'])
        setup = self.step('setup')
        self.assertEqual(setup['uses'], './.github/actions/setup-build')
        self.assertEqual(setup['with'], {'preset': 'native-debug', 'arm': 'true'})
        analyzers = self.step('analyzers')['run']
        self.assertIn('sudo apt-get install -y clang-tidy-14 cppcheck', analyzers)
        self.assertIn('echo /usr/lib/llvm-14/bin >> "$GITHUB_PATH"', analyzers)
        resources = self.step('resources')['run']
        self.assertIn('binary_directory(root, preset).exists()', resources)
        self.assertIn('df -B1 "$GITHUB_WORKSPACE" "$RUNNER_TEMP"', resources)

    def test_plan_uses_original_bytes_and_propagates_actual_failures(self):
        from unittest.mock import patch
        from tools.dev import process
        body = self.python_body('software')
        real_run = process.run
        for code in (0, 17, 124, 130):
            with self.subTest(exit_code=code), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                observed = []

                def execute(argv, **kwargs):
                    observed.append((argv, kwargs))
                    return real_run([sys.executable, '-c',
                                     "print('actual child receipt'); raise SystemExit(" +
                                     str(code) + ')'], cwd=root,
                                    env=os.environ.copy(),
                                    log=root / 'actual.raw', timeout=10,
                                    stage='formal-workflow-contract')

                environment = {'GITHUB_WORKSPACE': str(root),
                               'F': str(root / 'formal'),
                               'NEXUS_COMMAND_TIMEOUT': '11'}
                with patch.dict(os.environ, environment), \
                        patch('tools.dev.process.run', side_effect=execute), \
                        self.assertRaises(SystemExit) as stopped:
                    exec(compile(body, '<formal-software-step>', 'exec'), {})
                self.assertEqual(stopped.exception.code, code)
                self.assertEqual(len(observed), 1)
                argv, kwargs = observed[0]
                self.assertEqual(argv, [sys.executable, '-B', 'tools/dev/qualification.py',
                    '--root', str(root), '--plan', 'tools/dev/plans/software.json',
                    '--output', str(root / 'formal/software')])
                self.assertEqual(kwargs['timeout'], 18000)
                self.assertEqual(kwargs['cwd'], root)
                self.assertEqual(kwargs['stage'], 'formal-software')
                self.assertEqual(kwargs['env'].get('NEXUS_COMMAND_TIMEOUT'), '11')
                receipt = json.loads((root / 'actual.command.json').read_text())
                self.assertEqual(receipt['exit_code'], code)
                self.assertEqual(receipt['child_returncode'], code)

    def test_archives_preserve_both_complete_trees_without_recursive_copy(self):
        formal = self.step('formal_archive')
        build = self.step('build_archive')
        for step in (formal, build):
            self.assertEqual(step['if'], "always() && steps.source.outputs.validated == 'true'")
            self.assertNotIn('continue-on-error', step)
            self.assertIn('tools/evidence/store.py export', step['run'])
        self.assertIn('--source "$F" --store build/formal-transfer/store', formal['run'])
        self.assertIn('--source build --store "$RUNNER_TEMP/formal-build-store"', build['run'])
        identity = self.step('identity')
        self.assertEqual(identity['uses'], './.github/actions/record-identity')
        self.assertEqual(identity['with']['artifacts'], 'build/')
        self.assertEqual(identity['with']['result'], '${{ job.status }}')
        self.assertEqual(identity['with']['require-dependencies'], 'true')
        self.assertIn('tidy=clang-tidy', identity['with']['tools'])
        steps = self.document()['jobs']['formal']['steps']
        self.assertLess(steps.index(formal), steps.index(identity))
        self.assertLess(steps.index(identity), steps.index(build))
        commands = '\n'.join(step.get('run', '') for step in steps)
        self.assertNotIn('shutil.copytree', commands)
        self.assertNotIn('qualification_transfer.py', commands)
        self.assertNotIn('candidate.py', commands)

    def test_failure_and_cancel_preserve_unqualified_diagnostics_and_fallback(self):
        diagnostics = self.step('diagnostics')
        self.assertEqual(diagnostics['if'], 'always()')
        self.assertIn("'software_qualified': False", diagnostics['run'])
        self.assertIn("'physical_status': 'not_executed'", diagnostics['run'])
        fallback = self.step('fallback_upload')
        self.assertIn('always()', fallback['if'])
        self.assertIn("steps.formal_archive.outcome != 'success'", fallback['if'])
        self.assertIn("steps.build_archive.outcome != 'success'", fallback['if'])
        self.assertEqual(fallback['with']['path'].splitlines(),
                         ['${{ runner.temp }}/formal/', 'build/'])
        upload = self.step('archive_upload')
        self.assertEqual(upload['if'], "always() && steps.build_archive.outcome == 'success'")
        self.assertEqual(upload['with']['compression-level'], '0')
        for step in self.document()['jobs']['formal']['steps']:
            if step.get('uses', '').startswith('actions/upload-artifact@'):
                self.assertEqual(step['with']['include-hidden-files'], 'true')
                self.assertEqual(step['with']['if-no-files-found'], 'error')
                self.assertEqual(step['with']['retention-days'], '30')
                self.assertNotIn('continue-on-error', step)

    def test_stage_metadata_upload_is_early_original_and_only_after_execution(self):
        observe = self.step('stage_metadata')
        upload = self.step('stage_metadata_upload')
        condition = ("always() && (steps.software.outcome == 'success' || "
                     "steps.software.outcome == 'failure' || "
                     "steps.software.outcome == 'cancelled')")
        self.assertEqual(observe['if'], condition)
        self.assertEqual(upload['if'], condition)
        self.assertNotIn('continue-on-error', observe)
        self.assertNotIn('continue-on-error', upload)
        self.assertEqual(upload['with']['path'].splitlines(), [
            '${{ runner.temp }}/formal/software/report.json',
            '${{ runner.temp }}/formal/software/*.command.json',
            '${{ runner.temp }}/formal-stage-metadata.json'])
        self.assertEqual(upload['with']['if-no-files-found'], 'error')
        steps = self.document()['jobs']['formal']['steps']
        self.assertLess(steps.index(self.step('software')), steps.index(observe))
        self.assertLess(steps.index(observe), steps.index(upload))
        self.assertLess(steps.index(upload), steps.index(self.step('formal_archive')))

    def stage_metadata_fixture(self, outcome, *, missing=None, malformed=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            software = root / 'formal/software'
            software.mkdir(parents=True)
            status = {'success': 'executed_passed', 'failure': 'failed',
                      'cancelled': 'running'}[outcome]
            report = json.dumps({'status': status, 'stages': [
                {'name': 'quality', 'duration_s': 12.5,
                 'status': 'passed' if outcome == 'success' else 'failed'}]})
            originals = {'report.json': b'{' if malformed else report.encode(),
                         'quality.command.json': b'{"exit_code":17}\n'}
            for name, data in originals.items():
                if name != missing:
                    (software / name).write_bytes(data)
            nested = software / 'quality.attempts/one'
            nested.mkdir(parents=True)
            (nested / 'unexpected.command.json').write_bytes(b'nested')
            env = {**os.environ, 'F': str(root / 'formal'),
                   'RUNNER_TEMP': str(root), 'GITHUB_SHA': 'a' * 40,
                   'GITHUB_RUN_ID': '123', 'GITHUB_RUN_ATTEMPT': '1',
                   'NEXUS_SOFTWARE_OUTCOME': outcome}
            result = subprocess.run([
                sys.executable, '-B', '-c', self.python_body('stage_metadata')],
                env=env, capture_output=True, timeout=10)
            observed = json.loads((root / 'formal-stage-metadata.json').read_text())
            for name, data in originals.items():
                if name != missing:
                    self.assertEqual((software / name).read_bytes(), data)
            return result, observed

    def test_stage_metadata_retains_success_failure_and_partial_cancel_bytes(self):
        for outcome in ('success', 'failure', 'cancelled'):
            with self.subTest(outcome=outcome):
                result, record = self.stage_metadata_fixture(outcome)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(record['software_step_outcome'], outcome)
                self.assertEqual(record['metadata_status'], 'complete')
                self.assertFalse(record['software_qualified'])
                self.assertEqual(record['missing'], [])
                self.assertEqual({item['path'] for item in record['files']},
                                 {'report.json', 'quality.command.json'})
                self.assertTrue(all(re.fullmatch('[0-9a-f]{64}', item['sha256'])
                                    for item in record['files']))

    def test_stage_metadata_missing_or_invalid_files_remain_explicit_failures(self):
        for missing in ('report.json', 'quality.command.json'):
            with self.subTest(missing=missing):
                result, record = self.stage_metadata_fixture('failure', missing=missing)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(record['metadata_status'], 'incomplete')
                self.assertIn(missing, record['missing'])
                self.assertFalse(record['software_qualified'])
        result, record = self.stage_metadata_fixture('failure', malformed=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(record['metadata_status'], 'incomplete')
        self.assertTrue(record['errors'])

    def test_upload_failure_or_cancel_selects_complete_raw_fallback(self):
        condition = self.step('fallback_upload')['if']
        prefix, alternatives = condition.split('&&', 1)
        self.assertEqual(prefix.strip(), 'always()')
        terms = alternatives.strip().removeprefix('(').removesuffix(')').split('||')
        selectors = []
        for term in terms:
            match = re.fullmatch(r"steps\.(\w+)\.outcome != 'success'", term.strip())
            self.assertIsNotNone(match, 'Unsupported maintained fallback condition')
            selectors.append(match.group(1))
        for outcome, expected in (('success', False), ('failure', True),
                                  ('cancelled', True), ('skipped', True)):
            with self.subTest(archive_upload=outcome):
                state = {'formal_archive': 'success', 'build_archive': 'success',
                         'archive_upload': outcome}
                selected = any(state[name] != 'success' for name in selectors)
                self.assertEqual(selected, expected)

    def test_no_report_cache_restore_or_qualification_promotion(self):
        document = self.document()
        steps = document['jobs']['formal']['steps']
        self.assertFalse(any('download-artifact@' in step.get('uses', '') or
                             'actions/cache@' in step.get('uses', '') for step in steps))
        commands = '\n'.join(step.get('run', '') for step in steps)
        self.assertNotIn('git push', commands)
        self.assertNotIn('gh release', commands)
        self.assertNotIn('--require-candidate', commands)
        self.assertEqual(document['permissions'], {'contents': 'read'})


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
