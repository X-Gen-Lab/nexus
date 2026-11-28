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

from tools.dev.build_lock import LEASES

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
    def test_tool_workflow_keeps_native_execution_in_the_native_matrix(self):
        steps = workflow('enterprise-tools.yml')['jobs']['tooling']['steps']
        commands = '\n'.join(step.get('run', '') for step in steps)
        self.assertNotIn('scripts/ci/tdd_gate.py', commands)
        self.assertNotIn('scripts/ci/native_contracts.py', commands)
        for action in ('doctor', 'configure'):
            self.assertIn('tools/dev/dev.py ' + action, commands)
        self.assertNotIn('tools/dev/dev.py build', commands)
        execute = next(step for step in steps
                       if '--suite testing' in step.get('run', ''))
        self.assertEqual(execute.get('env', {}).get('NEXUS_TOOL_NATIVE_DATABASE'),
                         '${{ github.workspace }}/build/native-debug/compile_commands.json')
        for suite in ('configure', 'ci', 'testing'):
            self.assertIn('--suite ' + suite, commands)

    def test_tool_fixture_preparation_stops_at_configure_and_propagates_failures(self):
        steps = workflow('enterprise-tools.yml')['jobs']['tooling']['steps']
        preparation = next(step for step in steps if step.get('name') ==
                           'Prepare the actual compiler and generated inputs for tool fixtures')
        spy = ('#!' + sys.executable + '\n'
            'import json,os,sys\nfrom pathlib import Path\n'
            'args=sys.argv[1:]\n'
            'with open(os.environ["PREPARE_CALLS"],"a") as output:\n'
            '    output.write(json.dumps(args)+"\\n")\n'
            'if args[1]==os.environ["PREPARE_FAILURE"]:raise SystemExit(37)\n'
            'if args[1]=="configure":\n'
            '    database=Path("build/native-debug/compile_commands.json")\n'
            '    database.parent.mkdir(parents=True)\n'
            '    database.write_text("[{}]\\n")\n')
        expected = [
            ['tools/dev/dev.py', 'doctor', '--target', 'contracts'],
            ['tools/dev/dev.py', 'configure', '--preset', 'native-debug']]
        for failure, count in (('', 2), ('doctor', 1), ('configure', 2)):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as name:
                root = Path(name) / 'repository with spaces'
                executable = root / '.venv/bin/python'
                executable.parent.mkdir(parents=True)
                executable.write_text(spy)
                executable.chmod(0o700)
                calls = root / 'prepare-calls.jsonl'
                result = subprocess.run(
                    ['bash', '-euo', 'pipefail', '-c', preparation['run']],
                    cwd=root, env={**os.environ, 'PREPARE_CALLS': str(calls),
                                   'PREPARE_FAILURE': failure},
                    capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 37 if failure else 0,
                                 result.stdout + result.stderr)
                self.assertEqual([json.loads(line)
                                  for line in calls.read_text().splitlines()],
                                 expected[:count])
                self.assertEqual((root / 'build/native-debug/compile_commands.json').is_file(),
                                 not failure)
                self.assertFalse((root / 'build/native-debug/bin').exists())

    def test_quality_workflow_runs_checks_without_repeating_tool_tests(self):
        jobs = workflow('quality-checks.yml')['jobs']
        commands = '\n'.join(step.get('run', '') for job in jobs.values()
                             for step in job['steps'])
        self.assertNotIn('scripts.ci.test_format_tool', commands)
        self.assertNotIn('test_quality_tools.py', commands)
        self.assertIn('pre_commit run nexus-style', commands)
        self.assertIn('scripts/ci/quality_tools.py tidy', commands)
        self.assertIn('scripts/ci/quality_tools.py cppcheck', commands)

    def test_native_contract_jobs_prepare_the_locked_cross_compiler(self):
        for filename, name in (
                ('build-matrix.yml', 'native'),
                ('quality-checks.yml', 'static-analysis'),
                ('security.yml', 'codeql')):
            with self.subTest(workflow=filename, job=name):
                job = workflow(filename)['jobs'][name]
                setup = next(step for step in job['steps'] if step.get(
                    'uses') == './.github/actions/setup-build')
                self.assertEqual(setup['with'].get('arm'), 'true',
                                 'Native Host C contracts compile real ARM objects')


    def test_production_arm_jobs_are_selected_from_reviewed_presets(self):
        jobs = workflow('build-matrix.yml')['jobs']
        self.assertIn('fromJSON(needs.reference-plan.outputs.matrix)',
                      jobs['arm']['strategy']['matrix'])
        self.assertIn('scripts/ci/reference_matrix.py', str(jobs['reference-plan']))
        arm = jobs['arm']
        self.assertEqual(arm['env']['BUILD_DIR'], '${{ matrix.build_dir }}')
        check = next(step['run'] for step in arm['steps']
                     if 'tools/measurement/resources.py' in step.get('run', ''))
        prepare = next(step['run'] for step in arm['steps']
                       if 'tools/hil/prepare.py' in step.get('run', ''))
        upload = next(step['with']['path'] for step in arm['steps']
                      if 'actions/upload-artifact' in step.get('uses', ''))
        self.assertIn('"$BUILD_DIR/bin/nexus_firmware.elf"', check)
        self.assertIn('"$BUILD_DIR/generated/resolved.json"', prepare)
        self.assertIn('${{ matrix.build_dir }}/bin/', upload)
        fixtures = jobs['controller-routes']['strategy']['matrix']['fixture']
        for name in fixtures:
            with self.subTest(fixture=name):
                self.assertTrue((ROOT / 'tests/integration/assemblies' / name).is_dir())

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

    def test_codeql_trace_keeps_complete_build_with_explicit_finite_budget(self):
        document = workflow('security.yml')
        job = document['jobs']['codeql']
        step = next(item for item in job['steps'] if item.get('name') ==
                    'Trace the actual native build')
        # Exercise the authored shell and real budget parser. Only the external
        # configure/build CLI is a port; no compiler or CodeQL extraction runs.
        spy = ('#!' + sys.executable + '\n'
            'import json,os,sys\nfrom pathlib import Path\n'
            'sys.dont_write_bytecode=True\n'
            'sys.path.insert(0,os.environ["TRACE_PROCESS_ROOT"])\n'
            'from tools.dev.process import command_timeout\n'
            'args=sys.argv[1:]\n'
            'if args[0]!="tools/dev/dev.py" or args[1] not in '
            '("configure","build"):raise SystemExit(97)\n'
            'with open(os.environ["TRACE_CALLS"],"a") as stream:\n'
            '    stream.write(json.dumps({"argv":args,"timeout":'
            'command_timeout(os.environ)})+"\\n")\n'
            'if args[1]=="build":\n'
            '    mode=os.environ["TRACE_OUTPUT"]\n'
            '    if mode!="missing-database":\n'
            '        database=Path("build/native-debug/compile_commands.json")\n'
            '        database.parent.mkdir(parents=True)\n'
            '        database.write_text("" if mode=="empty-database" '
            'else "[{}]\\n")\n'
            '    if mode=="build-failure":raise SystemExit(37)\n')
        for mode, expected in (('complete', 0), ('missing-database', 1),
                               ('empty-database', 1), ('build-failure', 37)):
            with self.subTest(output=mode), tempfile.TemporaryDirectory() as name:
                root = Path(name) / 'repository with spaces'
                executable = root / '.venv/bin/python'
                executable.parent.mkdir(parents=True)
                executable.write_text(spy)
                executable.chmod(0o700)
                calls = root / 'trace-calls.jsonl'
                environment = {key: value for key, value in os.environ.items()
                               if key != 'NEXUS_COMMAND_TIMEOUT'}
                for scope in (document, job, step):
                    environment.update(scope.get('env', {}))
                environment.update(TRACE_PROCESS_ROOT=str(ROOT),
                                   TRACE_CALLS=str(calls), TRACE_OUTPUT=mode)
                result = subprocess.run(
                    ['bash', '-euo', 'pipefail', '-c', step['run']],
                    cwd=root, env=environment, capture_output=True, text=True,
                    timeout=10)
                self.assertEqual(result.returncode, expected,
                                 result.stdout + result.stderr)
                records = [json.loads(line)
                           for line in calls.read_text().splitlines()]
                self.assertEqual([item['argv'] for item in records], [
                    ['tools/dev/dev.py', 'configure', '--preset', 'native-debug'],
                    ['tools/dev/dev.py', 'build', '--preset', 'native-debug',
                     '--parallel', '2']])
                if mode == 'complete':
                    self.assertEqual(records[0]['timeout'], 1800)
                    self.assertEqual(records[1]['timeout'], 3600,
                                     'Traced builds need their own finite budget')

    def test_codeql_scope_only_bypasses_cpp_analysis_on_proven_prose(self):
        document = workflow('security.yml')
        jobs = document['jobs']
        codeql = jobs['codeql']
        self.assertEqual(codeql['name'], 'CodeQL analysis (${{ matrix.language }})')
        self.assertNotIn('if', codeql)
        self.assertEqual(codeql['strategy']['matrix']['language'], ['cpp', 'python'])
        for event in ('push', 'pull_request', 'schedule', 'workflow_dispatch'):
            self.assertIn(event, document['on'])
        for name in ('python-dependency-audit', 'secret-scan'):
            self.assertNotIn('if', jobs[name])
        scope = next(step for step in codeql['steps'] if step.get('id') == 'cpp-scope')
        self.assertIn('matrix.language', scope['if'])
        self.assertIn('pull_request', scope['if'])
        self.assertIn('push', scope['if'])
        self.assertIn('git cat-file blob', scope['run'])
        self.assertIn('--codeql', scope['run'])
        self.assertIn('steps.cpp-scope.outputs.skip_cpp', codeql['steps'][-1]['if'])
        commands = '\n'.join(step.get('run', '') for step in codeql['steps'])
        self.assertNotIn('git submodule update', commands)
        for step in codeql['steps']:
            if ('codeql-action/' in step.get('uses', '') or
                    'setup-build' in step.get('uses', '') or
                    step.get('name') == 'Trace the actual native build'):
                self.assertIn("steps.cpp-scope.outputs.skip_cpp != 'true'", step['if'])

    def test_build_setup_installs_one_hash_locked_tool_closure(self):
        setup = yaml.load((ROOT / '.github/actions/setup-build/action.yml').read_text(),
                          Loader=yaml.BaseLoader)
        steps = setup['runs']['steps']
        commands = '\n'.join(step.get('run', '') for step in steps)
        self.assertIn('install_dev_tools.py --skip-hooks --locked-linux', commands)
        self.assertNotIn('pip install', commands)
        self.assertEqual(steps[0]['with']['cache'], 'pip')
        self.assertEqual(steps[0]['with']['cache-dependency-path'],
                         'dependencies/python-tools-linux-cp311.txt')


    def test_docs_validation_has_no_deployment_permissions(self):
        document = workflow('docs-build.yml')
        self.assertEqual(document['permissions'], {'contents': 'read'})
        caller = workflow('ci.yml')['jobs']['docs']
        self.assertEqual(caller['permissions'], {'contents': 'read'})
        self.assertNotIn('secrets', caller)
        self.assertEqual(set(document['jobs']), {'build'})
        steps = document['jobs']['build']['steps']
        checkout = next(step for step in steps if step.get('uses', '').startswith(
            'actions/checkout@'))
        self.assertEqual(checkout['with']['submodules'], 'false')
        commands = '\n'.join(step.get('run', '') for step in steps)
        self.assertIn('tools/documentation/install.py', commands)
        self.assertIn('tools/documentation/cli.py test', commands)
        self.assertNotIn('tools/documentation/cli.py check', commands)
        self.assertIn('--mode verify', commands)
        self.assertIn('--language all', commands)
        self.assertNotIn('--scope native', commands)
        self.assertIn('tools/documentation/cli.py build', commands)
        self.assertNotIn('sudo apt-get', commands)
        upload = next(step['with']['path'] for step in steps if step.get(
            'uses', '').startswith('actions/upload-artifact@')).splitlines()
        self.assertIn('build/documentation/site/', upload)
        self.assertIn('build/documentation/documentation-build.json', upload)
        self.assertNotIn('build/documentation/', upload)

    def test_source_jobs_prepare_only_the_reviewed_selected_closure(self):
        for name, job, consumer in (
                ('docs-build.yml', 'build', 'tools/documentation/install.py'),):
            with self.subTest(workflow=name):
                steps = workflow(name)['jobs'][job]['steps']
                checkout = next(i for i, step in enumerate(steps) if step.get(
                    'uses', '').startswith('actions/checkout@'))
                self.assertEqual(steps[checkout]['with']['submodules'], 'false')
                preparation = [(i, step['run']) for i, step in enumerate(steps)
                               if 'tools/dev/dependencies.py prepare'
                               in step.get('run', '')]
                self.assertEqual(len(preparation), 2)
                from tools.dev.dependencies import required_submodules
                backends, reports, selected = set(), [], set()
                for position, command in preparation:
                    arguments = shlex.split(command.replace('\\\n', ' '))
                    self.assertEqual(arguments[:4], [
                        'python3', '-B', 'tools/dev/dependencies.py', 'prepare'])
                    self.assertEqual(arguments[arguments.index('--scope') + 1], 'arm')
                    self.assertEqual(arguments[arguments.index('--family') + 1],
                                     'stm32f4')
                    backend = arguments[arguments.index('--backend') + 1]
                    backends.add(backend)
                    selected.update(required_submodules('arm', 'stm32f4', backend))
                    self.assertIn('--allow-network', arguments)
                    report = arguments[arguments.index('--report') + 1]
                    self.assertTrue(report.startswith('build/'))
                    reports.append(report)
                    self.assertLess(checkout, position)
                    self.assertLess(position, next(i for i, step in enumerate(steps)
                                    if consumer in step.get('run', '')))
                self.assertEqual(backends, {'freertos', 'threadx'})
                self.assertTrue({'ext/freertos', 'ext/threadx'} <= selected)
                self.assertNotIn('ext/googletest', selected)
                self.assertFalse(any('/stm32h7' in path for path in selected))
                self.assertFalse(any('--recursive' in step.get('run', '')
                                     for step in steps))
                upload = next(step['with']['path'] for step in steps if step.get(
                    'uses', '').startswith('actions/upload-artifact@'))
                paths = upload.splitlines()
                for report in reports:
                    self.assertTrue(any(Path(report).match(path) or report.startswith(
                        path.rstrip('/') + '/') for path in paths))

    def test_docs_commands_reuse_the_prepared_nondefault_tool_prefix(self):
        steps = workflow('docs-build.yml')['jobs']['build']['steps']
        prepared = next(step['run'] for step in steps
                        if 'tools/documentation/install.py' in step.get('run', ''))
        prepared_arguments = shlex.split(prepared.replace('\\\n', ' '))
        prefix = prepared_arguments[prepared_arguments.index('--prefix') + 1]
        self.assertEqual(prefix, '$RUNNER_TEMP/nexus-docs-tools')
        for action in ('test', 'build'):
            with self.subTest(action=action):
                execute = next(step['run'] for step in steps
                               if 'tools/documentation/cli.py ' + action
                               in step.get('run', ''))
                arguments = shlex.split(execute.replace('\\\n', ' '))
                self.assertEqual(arguments[0], 'python3')
                self.assertIn('--tool-prefix', arguments)
                self.assertEqual(arguments[arguments.index('--tool-prefix') + 1],
                                 prefix)
        self.assertFalse(any('deploy-pages' in step.get('uses', '') for step in steps))

    def test_pages_publication_reuses_successful_trusted_builds(self):
        document = workflow('docs-build.yml')
        option = document['on']['workflow_call']['inputs']['emit_pages']
        self.assertEqual(option['type'], 'boolean')
        self.assertEqual(option['default'], 'false')
        steps = document['jobs']['build']['steps']
        upload = next(step for step in steps if step.get('uses', '').startswith(
            'actions/upload-pages-artifact@'))
        trusted = "github.ref == 'refs/heads/main'"
        self.assertEqual(upload['if'], "success() && inputs.emit_pages && " + trusted
                         + " && (github.event_name == 'push' || github.event_name == 'workflow_dispatch')")
        self.assertEqual(upload['with']['path'], 'build/documentation/site')
        self.assertEqual(upload['with']['name'], 'github-pages')
        ci = workflow('ci.yml')['jobs']
        self.assertEqual(ci['docs']['with']['emit_pages'],
                         "${{ github.event_name == 'push' && " + trusted + " }}")
        caller = ci['publish-docs']
        self.assertEqual(set(caller['needs']), {'docs', 'status-check'})
        # A docs-only push has skipped code ancestors. Explicit status handling
        # permits that trusted publication while rejecting failed/cancelled CI.
        self.assertEqual(caller['if'],
                         "always() && !cancelled() && "
                         "github.event_name == 'push' && " + trusted
                         + " && needs.docs.result == 'success'"
                         + " && needs.status-check.result == 'success'")
        self.assertEqual(caller['uses'], './.github/workflows/docs-publish.yml')
        self.assertEqual(caller['permissions'],
                         {'contents': 'read', 'pages': 'write', 'id-token': 'write'})
        self.assertNotIn('publish-docs', ci['status-check']['needs'])
        publish = workflow('docs-publish.yml')
        self.assertEqual(set(publish['on']), {'workflow_call', 'workflow_dispatch'})
        self.assertEqual(publish['permissions'], {'contents': 'read'})
        reject = publish['jobs']['reject-branch']
        self.assertEqual(reject['if'], "github.event_name == 'workflow_dispatch' && "
                         "github.ref != 'refs/heads/main'")
        self.assertEqual(reject['permissions'], {})
        rejected = subprocess.run(['bash', '-euo', 'pipefail', '-c',
                                  reject['steps'][0]['run']],
                                 capture_output=True, text=True, timeout=10)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn('::error::', rejected.stdout)
        build = publish['jobs']['build']
        self.assertEqual(build['if'], "github.event_name == 'workflow_dispatch' && " + trusted)
        self.assertEqual(build['uses'], './.github/workflows/docs-build.yml')
        self.assertEqual(build['with']['emit_pages'], 'true')
        self.assertEqual(build['permissions'], {'contents': 'read'})
        deploy = publish['jobs']['deploy']
        self.assertEqual(deploy['needs'], 'build')
        self.assertEqual(deploy['if'], "always() && !cancelled() && " + trusted
            + " && ((github.event_name == 'push' && needs.build.result == 'skipped')"
            + " || (github.event_name == 'workflow_dispatch' && needs.build.result == 'success'))")
        self.assertEqual(deploy['permissions'],
                         {'contents': 'read', 'pages': 'write', 'id-token': 'write'})
        self.assertEqual(deploy['environment']['name'], 'github-pages')
        self.assertEqual(deploy['concurrency'],
                         {'group': 'github-pages', 'cancel-in-progress': 'false'})
        self.assertEqual(len(deploy['steps']), 2)
        self.assertTrue(deploy['steps'][-1]['uses'].startswith('actions/deploy-pages@'))
        self.assertEqual(deploy['steps'][-1]['with']['artifact_name'], 'github-pages')

    def test_pages_deployment_rejects_stale_source_and_api_failure(self):
        deploy = workflow('docs-publish.yml')['jobs']['deploy']
        guard = next(step for step in deploy['steps']
                     if step.get('name') == 'Require the current main source')
        self.assertIs(deploy['steps'][0], guard)
        self.assertEqual(guard['env'], {'GH_TOKEN': '${{ github.token }}'})
        spy = ('#!' + sys.executable + '\n'
            'import json,os,sys\n'
            'with open(os.environ["GH_CALLS"],"w") as output:\n'
            '    json.dump(sys.argv[1:],output)\n'
            'if os.environ["GH_MODE"]=="api-failure":raise SystemExit(37)\n'
            'print(os.environ["GH_MAIN_SHA"])\n')
        for mode, latest, expected in (('current', 'a' * 40, 0),
                                      ('stale', 'b' * 40, 1),
                                      ('api-failure', 'a' * 40, 37)):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as name:
                root = Path(name) / 'repository with spaces'
                root.mkdir()
                executable = root / 'gh'
                executable.write_text(spy)
                executable.chmod(0o700)
                calls = root / 'calls.json'
                result = subprocess.run(['bash', '-euo', 'pipefail', '-c', guard['run']],
                    cwd=root, env={**os.environ, 'PATH': str(root) + os.pathsep +
                        os.environ.get('PATH', ''), 'GH_CALLS': str(calls),
                        'GH_MODE': mode, 'GH_MAIN_SHA': latest,
                        'GITHUB_REPOSITORY': 'X-Gen-Lab/nexus', 'GITHUB_SHA': 'a' * 40},
                    capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, expected,
                                 result.stdout + result.stderr)
                self.assertEqual(json.loads(calls.read_text()),
                    ['api', 'repos/X-Gen-Lab/nexus/git/ref/heads/main',
                     '--jq', '.object.sha'])
                if mode == 'stale':
                    self.assertIn('::error::', result.stdout)

    def test_docs_failures_preserve_diagnostics_without_generated_payloads(self):
        steps = workflow('docs-build.yml')['jobs']['build']['steps']
        upload = next((step for step in steps if step.get('with', {}).get(
            'name') == 'documentation-failure-diagnostics'), None)
        self.assertIsNotNone(upload, 'Failed builds need their original receipts')
        self.assertEqual(upload.get('if'), 'failure()')
        self.assertEqual(upload.get('continue-on-error'), 'true')
        self.assertTrue(upload.get('uses', '').startswith('actions/upload-artifact@'))
        self.assertEqual(upload['with']['if-no-files-found'], 'ignore')
        patterns = upload['with']['path'].splitlines()
        required = {
            'build/documentation/documentation-build.json',
            'build/documentation/last-published.json',
            'build/documentation/work/source-check.json',
            'build/documentation/work/html-en.command.json',
            'build/documentation/work/html-en.log',
            'build/documentation/work/html-en.attempts/one/receipt.json',
            'build/documentation/work/html-en.attempts/one/stdout.txt',
            'build/documentation/work/html-en.attempts/one/stderr.txt',
            'build/documentation-attempts/one/tests.command.json',
            'build/documentation-attempts/one/tests.log',
            'build/documentation-attempts/one/tests.attempts/two/receipt.json',
            'build/documentation-attempts/one/tests.attempts/two/stdout.txt',
            'build/documentation-attempts/one/tests.attempts/two/stderr.txt',
            'build/dependencies/docs-build-freertos.json',
            'build/dependencies/docs-build-threadx.json',
            'build/quality/docs-tests.json',
        }
        excluded = {
            'build/documentation/site/en/index.html',
            'build/documentation/cache/html/environment.pickle',
            'build/documentation/work/api/xml/index.xml',
            'build/documentation/work/gettext-work/doctrees/index.doctree',
            'build/documentation/work/gettext/index.pot',
        }
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            for relative in required | excluded:
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('diagnostic fixture\n')
            selected = {path.relative_to(root).as_posix()
                        for pattern in patterns for path in root.glob(pattern)
                        if path.is_file()}
        self.assertEqual(selected, required)






    def test_workflow_dsl_hook_and_existing_quality_job_share_the_tool(self):
        configuration = yaml.safe_load(
            (ROOT / '.pre-commit-config.yaml').read_text())
        hooks = {hook['id']: hook for repo in configuration['repos']
                 for hook in repo['hooks']}
        hook = hooks['nexus-workflow-lint']
        self.assertEqual(hook['entry'],
                         'python scripts/ci/lint_workflows.py --staged --when-changed')
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

    def test_reference_uses_declared_output_and_public_board_fixture_identity(self):
        from scripts.ci.reference_matrix import reference_matrix
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / 'cmake/presets/references.json'
            config.parent.mkdir(parents=True)
            (root / 'CMakePresets.json').write_text(json.dumps({
                'version': 6, 'include': ['cmake/presets/references.json']}))
            cfg = {'name': 'sample-board', 'generator': 'Ninja',
                   'binaryDir': '${sourceDir}/out/arm proof',
                   'cacheVariables': {'NEXUS_ASSEMBLY_FILE': '${sourceDir}/app.toml'}}
            config.write_text(json.dumps({'version': 6,
                'vendor': {'nexus/reference-jobs': True}, 'configurePresets': [cfg]}))
            foreign = root / 'fixtures/boards/same-name'
            foreign.mkdir(parents=True)
            (foreign / 'board.json').write_text('{"id":"different","soc":"part-B"}')
            fixture = root / 'tools/hil/fixtures/same-name.json'
            fixture.parent.mkdir(parents=True)
            fixture.write_text('{"board":"public","part":"part-A"}')
            authored = root / 'app.toml'
            authored.write_text('board_package = "fixtures/boards/same-name"')
            expected = {'preset': 'sample-board', 'build_dir': 'out/arm proof', 'fixture': ''}
            self.assertEqual(reference_matrix(root)['include'], [expected])
            public = root / 'boards/same-name'
            public.mkdir(parents=True)
            (public / 'board.json').write_text('{"id":"public","soc":"part-A"}')
            authored.write_text('board = "same-name"')
            expected['fixture'] = 'tools/hil/fixtures/same-name.json'
            self.assertEqual(reference_matrix(root)['include'], [expected])
            fixture.write_text('{"board":"public","part":"part-B"}')
            with self.assertRaisesRegex(ValueError, 'fixture'):
                reference_matrix(root)
            cfg['binaryDir'] = '${sourceDir}/../outside'
            config.write_text(json.dumps({'version': 6,
                'vendor': {'nexus/reference-jobs': True}, 'configurePresets': [cfg]}))
            with self.assertRaisesRegex(ValueError, 'directory'):
                reference_matrix(root)

    def test_board_backend_link_profiles_have_actual_presets(self):
        jobs = workflow('build-matrix.yml')['jobs']
        from scripts.ci.reference_matrix import reference_matrix
        rows = reference_matrix(ROOT)['include']
        selected = {row['preset'] for row in rows}
        self.assertTrue(all((ROOT / row['fixture']).is_file() for row in rows))
        self.assertEqual(selected, {
            'stm32-qiming-baremetal', 'stm32-qiming-freertos',
            'stm32-sky-baremetal', 'stm32-sky-freertos',
            'stm32-f103rb-baremetal', 'stm32-f103rb-freertos'})
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

    def test_actions_initializer_prepares_active_dependencies_only(self):
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
        from tools.maintenance.source_catalogue import ALL_DEPENDENCIES
        active = {entry.path for entry in ALL_DEPENDENCIES
                  if entry.scope != 'inventory' and entry.parent is None
                  and entry.alias_of is None}
        self.assertLess(active, expected)
        self.assertTrue({'ext/tomlc99', 'ext/cjson'}.isdisjoint(
            dependencies.required_submodules('native')))
        self.assertEqual(set(dependencies.required_submodules('native')),
                         active)

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

    def test_mode_independent_suites_have_one_reported_tool_owner(self):
        tools = workflow('enterprise-tools.yml')['jobs']['tooling']
        commands = '\n'.join(step.get('run', '') for step in tools['steps'])
        selected = re.findall(r'--suite\s+(\w+)', commands)
        suites = ['configure', 'ci', 'testing', 'integration', 'freertos']
        self.assertCountEqual(selected, suites)
        self.assertNotIn('unittest discover -s tests/integration', commands)
        upload = next(step for step in tools['steps'] if step.get(
            'uses', '').startswith('actions/upload-artifact@'))
        for suite in suites:
            report = f'build/quality/tools-{suite}.json'
            with self.subTest(suite=suite):
                self.assertIn('--report ' + report, commands)
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
            'stm32_assembly', 'can_assembly', 'usb_assembly',
            'ethernet_assembly', 'timer_assembly', 'multi_controller_assembly',
            'declared_io_assembly', 'stm32f4_family',
            'stm32f1_family', 'stm32f1_vldiscovery', 'stm32f1_flash', 'stm32f1_watchdog',
            'stm32f1_buses', 'stm32f1_pwm', 'stm32f1_adc',
            'stm32f1_exti', 'stm32f1_stop', 'stm32f1_pvd', 'stm32f1_window_watchdog',
            'stm32f1_can', 'stm32f1_rtc', 'stm32f1_uart_rx_dma', 'stm32f1_dac',
            'stm32f1_spi_dma', 'stm32f1_can_normal', 'stm32f1_fsmc',
            'stm32f1_i2c_10bit', 'stm32f1_i2c_target', 'stm32f1_advanced_pwm', 'stm32f1_pwm_adc', 'stm32f1_can_connectivity',
            'stm32f1_timer_capture', 'stm32f1_i2s2', 'stm32f1_connectivity_dwc',
            'stm32f1_sdio', 'stm32f1_encoder', 'stm32f1_uart9n1',
            'stm32f1_pma_device', 'stm32f1_spi_dual', 'stm32f1_uart_half_duplex',
            'stm32f1_uart_lin', 'stm32f1_uart_flow', 'stm32f1_uart_irda', 'stm32f1_uart_sync', 'stm32f1_uart_remap', 'stm32f1_mdio',
            'stm32f1_connectivity_host'})
        for name in selected:
            self.assertTrue(list((ROOT / 'tests/integration/assemblies' / name).glob('*.toml')))
        commands = '\n'.join(step.get('run', '') for step in job['steps'])
        self.assertEqual(commands.count('scripts/ci/provider_assemblies.py'), 1)
        self.assertIn('arguments+=(--fixture "$assembly")', commands)
        self.assertIn('"${arguments[@]}"', commands)
        self.assertIn('--compiler arm-none-eabi-gcc', commands)
        self.assertNotIn('cmake --build', commands)
        self.assertNotIn('NEXUS_BACKEND', commands)

    def test_family_gpio_matrix_partitions_the_actual_authored_inputs(self):
        job = workflow('build-matrix.yml')['jobs']['controller-routes']
        matrix = job['strategy']['matrix']
        rows = {row['fixture']: row for row in matrix.get('include', [])}
        self.assertEqual(set(rows), {'stm32f4_family', 'stm32f4_gpio'})
        expected_projects = {
            'stm32f4_family': 'tests/integration/assemblies/stm32f4_family',
            'stm32f4_gpio': 'tests/integration/assemblies/stm32f4_family/gpio',
        }
        selected = []
        for name, project in expected_projects.items():
            with self.subTest(fixture=name):
                self.assertEqual(rows[name]['project'], project)
                self.assertTrue((ROOT / project / 'CMakeLists.txt').is_file())
                assemblies = rows[name]['assemblies'].splitlines()
                self.assertEqual(len(assemblies), 2 if name.endswith('gpio') else 7)
                self.assertEqual(any('-gpio.toml' in path for path in assemblies),
                                 name.endswith('gpio'))
                selected.extend(assemblies)
        authored = {path.relative_to(ROOT).as_posix() for path in
                    (ROOT / 'tests/integration/assemblies/stm32f4_family').glob('*.toml')}
        self.assertEqual(set(selected), authored)
        self.assertEqual(len(selected), len(authored))
        step = next(step for step in job['steps'] if step.get('name') ==
                    'Link and audit each authored software assembly')
        self.assertEqual(step['env']['PROVIDER_PROJECT'],
                         "${{ matrix.project || '' }}")
        self.assertEqual(step['env']['PROVIDER_ASSEMBLIES'],
                         "${{ matrix.assemblies || '' }}")



    def test_provider_shell_passes_each_authored_and_canonical_input_once(self):
        job = workflow('build-matrix.yml')['jobs']['controller-routes']
        step = next(step for step in job['steps'] if step.get('name') ==
                    'Link and audit each authored software assembly')
        selections = {'stm32_assembly': [], 'can_assembly': [],
                      'stm32f1_family': [], 'stm32f1_flash': [],
                      'stm32f1_watchdog': [], 'stm32f1_buses': [],
                      'stm32f1_pwm': [], 'stm32f1_adc': [], 'stm32f1_exti': [], 'stm32f1_stop': [],
                      'stm32f1_pvd': [], 'stm32f1_window_watchdog': [],
                      'stm32f1_can': [], 'stm32f1_rtc': [], 'stm32f1_uart_rx_dma': [], 'stm32f1_vldiscovery': [],
                      'stm32f1_dac': [], 'stm32f1_spi_dma': [], 'stm32f1_can_normal': [],
                      'stm32f1_fsmc': [], 'stm32f1_i2c_10bit': [], 'stm32f1_i2c_target': [],
                      'stm32f1_advanced_pwm': [], 'stm32f1_pwm_adc': [], 'stm32f1_can_connectivity': [],
                      'stm32f1_timer_capture': [], 'stm32f1_i2s2': [],
                      'stm32f1_connectivity_dwc': [], 'stm32f1_sdio': [],
                      'stm32f1_encoder': [], 'stm32f1_uart9n1': [], 'stm32f1_pma_device': [],
                      'stm32f1_spi_dual': [], 'stm32f1_uart_half_duplex': [],
                      'stm32f1_uart_lin': [], 'stm32f1_uart_flow': [], 'stm32f1_uart_irda': [],
                      'stm32f1_uart_sync': [], 'stm32f1_uart_remap': [],
                      'stm32f1_mdio': [], 'stm32f1_connectivity_host': []}
        authored_family = sorted('tests/integration/assemblies/stm32f4_family/' + path.name
            for path in (ROOT / 'tests/integration/assemblies/stm32f4_family').glob('*.toml'))
        selections.update(
            stm32f4_family=[path for path in authored_family
                            if not path.endswith('-gpio.toml')],
            stm32f4_gpio=[path for path in authored_family
                          if path.endswith('-gpio.toml')])
        for family, canonical in selections.items():
            with self.subTest(family=family), tempfile.TemporaryDirectory() as name:
                root = Path(name) / 'repository with spaces'
                project = root / 'tests/integration/assemblies' / family
                project.mkdir(parents=True)
                fixture_names = sorted(path.name for path in
                    (ROOT / 'tests/integration/assemblies' / family).glob('*.toml'))
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
                selected_project = (
                    'tests/integration/assemblies/stm32f4_family/gpio'
                    if family == 'stm32f4_gpio' else
                    'tests/integration/assemblies/' + family if canonical else '')
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
                expected = ['tests/integration/assemblies/' + family + '/' + name
                            for name in fixture_names]
                if canonical:
                    expected = canonical
                    self.assertEqual(argv.count('--project'), 1)
                    self.assertEqual(argv[argv.index('--project') + 1],
                                     selected_project)
                else:
                    self.assertNotIn('--project', argv)
                self.assertEqual(actual, expected)



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
        for name in ('Preserve compiler evidence',):
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
        setup = yaml.load((ROOT / '.github/actions/setup-build/action.yml')
                          .read_text(), Loader=yaml.BaseLoader)
        initialization = '\n'.join(step.get('run', '')
                                   for step in setup['runs']['steps'])
        self.assertIn('tools/dev/dependencies.py prepare', initialization)
        from tools.dev import dependencies
        self.assertIn('ext/threadx', dependencies.required_submodules('native'))


PROFILE_BASELINE_SHA = '921ef15385b75a0a9e91dc4be22db7362dcf0f342cfd473008222c431d15d715'
class CTestAdmission(unittest.TestCase):
    def execute_fixture(self, kind):
        self.assertIsNotNone(shutil.which('cmake'))
        self.assertIsNotNone(shutil.which('ctest'))
        with tempfile.TemporaryDirectory(prefix='nexus real ctest ') as temporary:
            root = Path(temporary)
            for relative in ('tools/dev/dev.py', 'tools/dev/process.py',
                             'tools/dev/build_lock.py',
                             'tools/dev/presets.py', 'tools/dev/provenance.py',
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
            # This independent temporary build owns its own kernel lock. A
            # raw subprocess closes parent descriptors, so do not retain claims
            # to the outer gate's unrelated build lease.
            environment = dict(os.environ)
            environment.pop(LEASES, None)
            configured = subprocess.run(['cmake', '--preset', 'fixture'], cwd=root,
                                        env=environment,
                                        capture_output=True, text=True, timeout=30)
            self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
            report = root / 'build/fixture/ctest-results.xml'
            # A stale success must never admit a later failed/empty test run.
            report.write_text('<testsuite tests="1"><testcase name="stale"/></testsuite>')
            os.utime(report, (1, 1))
            result = subprocess.run([sys.executable, root / 'tools/dev/dev.py',
                                     'test', '--preset', 'fixture'], cwd=root,
                                    env=environment, capture_output=True, text=True,
                                    timeout=30)
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
