import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
import quality_tools
from quality_tools import commands, compiler_query, run


@unittest.skipUnless(os.name == "posix", "analyzer process fixtures require POSIX; CI analysis runs on Linux")
class RequiredAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        self.build = self.root / "build"
        self.build.mkdir()
        self.source = self.root / "components" / "sample.c"
        self.source.parent.mkdir()
        self.source.write_text("int sample(void) { return 1; }\n")
        self.report = self.root / "reports" / "analysis.txt"
        self.database([self.source])

    def database(self, paths):
        (self.build / "compile_commands.json").write_text(json.dumps([
            {"file": str(p), "directory": str(self.root), "arguments": ["cc", "-c", str(p)]}
            for p in paths]))

    def tool(self, code, version=0):
        path = self.root / "analyzer"
        path.write_text(f"#!{sys.executable}\nimport sys\nprint('model analyzer executed')\nsys.exit({version} if '--version' in sys.argv else {code})\n")
        path.chmod(0o700)
        return str(path)

    def test_success_requires_nonempty_scope(self):
        self.assertEqual(run("tidy", self.root, self.build, self.tool(0), self.report), 0)
        self.assertIn("Exit code: 0", self.report.read_text())

    def test_equivalent_output_entries_have_one_analysis_context(self):
        entries = [
            {"file": str(self.source), "directory": str(self.root),
             "arguments": ["cc", "-DFEATURE=1", "-c", str(self.source),
                           "-o", "first.o", "-MMD", "-MF", "first.d", "-MT", "first.o"]},
            {"file": str(self.source), "directory": str(self.root),
             "command": shlex.join(["cc", "-DFEATURE=1", "-c", str(self.source),
                                    "-o", "second.o", "-MD", "-MFsecond.d", "-MQsecond.o"])},
        ]
        (self.build / "compile_commands.json").write_text(json.dumps(entries))
        tool = self.root / "counting-analyzer"
        count = self.root / "invocations.jsonl"
        tool.write_text(
            f"#!{sys.executable}\nimport json,sys\n"
            "if '--version' in sys.argv: print('fixture'); sys.exit(0)\n"
            f"with open({str(count)!r}, 'a') as stream: stream.write(json.dumps(sys.argv) + '\\n')\n")
        tool.chmod(0o700)
        self.assertEqual(run("tidy", self.root, self.build, str(tool), self.report), 0)
        self.assertEqual(len(count.read_text().splitlines()), 1)
        self.assertIn('Unique analysis contexts: 1', self.report.read_text())
        self.assertIn('"compilation_indexes": [0, 1]', self.report.read_text())
        self.assertEqual(json.loads((self.build / "compile_commands.json").read_text()), entries)

    def test_context_identity_preserves_every_semantic_argument_and_order(self):
        base = {"file": str(self.source), "directory": str(self.root),
                "arguments": ["cc", "-c", str(self.source)]}
        flags = (['-D', 'FEATURE=1'], ['-U', 'FEATURE'], ['-mcpu=cortex-m4'],
                 ['-mfloat-abi=soft'], ['-std=c99'], ['-x', 'c++'], ['-I', 'first'],
                 ['-isystem', 'system'], ['-include', 'forced.h'], ['--sysroot=/sdk'],
                 ['-target', 'arm-none-eabi'], ['-pthread'], ['-O2'],
                 ['-Xclang', '-unknown-option'])
        entries = [base, *[{**base, "arguments": ["cc", *part, "-c", str(self.source)]}
                           for part in flags]]
        entries.extend([{**base, "arguments": ["cc", '-I', 'first', '-I', 'second', '-c', str(self.source)]},
                        {**base, "arguments": ["cc", '-I', 'second', '-I', 'first', '-c', str(self.source)]},
                        {**base, "arguments": ["clang", '-c', str(self.source)]},
                        {**base, "directory": str(self.build)}])
        contexts = quality_tools.analysis_contexts(entries)
        self.assertEqual(len(contexts), len(entries))

    def test_context_option_values_are_not_treated_as_output_flags(self):
        base = {"file": str(self.source), "directory": str(self.root)}
        entries = [{**base, "arguments": ['cc', '-D', value, '-c', str(self.source), '-o', 'out.o']}
                   for value in ('-oone', '-otwo', '-MFone', '-MFtwo')]
        self.assertEqual(len(quality_tools.analysis_contexts(entries)), 4)

    def test_unknown_option_grammar_is_retained_without_deduplication(self):
        base = {'file': str(self.source), 'directory': str(self.root)}
        entries = [{**base, 'arguments': ['cc', '-unknown-mode', '-o', output,
                                         '-c', str(self.source)]}
                   for output in ('first.o', 'second.o')]
        self.assertEqual(len(quality_tools.analysis_contexts(entries)), 2)
        for entry in entries:
            self.assertEqual(quality_tools.analysis_arguments(entry), entry['arguments'])

    def test_compiler_and_known_launcher_prefixes_are_never_output_options(self):
        base = {'file': str(self.source), 'directory': str(self.root)}
        for launcher in ([], ['ccache'], ['sccache'], ['distcc']):
            with self.subTest(launcher=launcher):
                entries = [{**base, 'arguments': [*launcher, compiler, '-c',
                                                 str(self.source), '-o', 'out.o']}
                           for compiler in ('-MFfirst-gcc', '-MFsecond-gcc')]
                self.assertEqual(len(quality_tools.analysis_contexts(entries)), 2)
                for entry in entries:
                    self.assertEqual(quality_tools.analysis_arguments(entry),
                                     [*entry['arguments'][:len(launcher) + 1], str(self.source)])

    def test_unknown_compiler_or_launcher_keeps_the_complete_argv(self):
        base = {'file': str(self.source), 'directory': str(self.root)}
        for prefix in (['unknown-compiler'], ['env', 'cc'],
                       ['ccache', 'unknown-compiler']):
            with self.subTest(prefix=prefix):
                entries = [{**base, 'arguments': [*prefix, '-c', str(self.source),
                                                 '-o', output]}
                           for output in ('first.o', 'second.o')]
                self.assertEqual(len(quality_tools.analysis_contexts(entries)), 2)
                for entry in entries:
                    self.assertEqual(quality_tools.analysis_arguments(entry), entry['arguments'])

    def test_precompiled_header_operands_remain_distinct_contexts(self):
        base = {'file': str(self.source), 'directory': str(self.root)}
        for option in ('-include-pch', '-include-pth'):
            with self.subTest(option=option):
                entries = [{**base, 'arguments': ['clang', option, header,
                                                 '-c', str(self.source), '-o', 'out.o']}
                           for header in ('-MFfirst.pch', '-MFsecond.pch')]
                self.assertEqual(len(quality_tools.analysis_contexts(entries)), 2)
                for entry in entries:
                    self.assertIn(entry['arguments'][2], quality_tools.analysis_arguments(entry))

    def test_unknown_include_family_options_do_not_guess_operand_grammar(self):
        base = {'file': str(self.source), 'directory': str(self.root)}
        for option in ('-include-new-mode', '-isystem-new-mode'):
            with self.subTest(option=option):
                entries = [{**base, 'arguments': ['clang', option, '-MF' + value,
                                                 '-c', str(self.source), '-o', 'out.o']}
                           for value in ('first', 'second')]
                self.assertEqual(len(quality_tools.analysis_contexts(entries)), 2)
                for entry in entries:
                    self.assertEqual(quality_tools.analysis_arguments(entry), entry['arguments'])

    def test_query_preserves_output_like_option_operands(self):
        argv = ['cc', '-I', '-oheaders', '-isystem', '-MFheaders',
                '-D', '-oMACRO', '-include', str(self.source),
                '-c', str(self.source), '-o', 'discard.o']
        entry = {'file': str(self.source), 'directory': str(self.root),
                 'arguments': argv}
        _, query = compiler_query(entry)
        self.assertEqual(query, ['cc', *argv[1:9], '-x', 'c', '-E', '-dM', '-'])

    def test_query_uses_language_at_translation_unit_not_trailing_language(self):
        entry = {'file': str(self.source), 'directory': str(self.root),
                 'arguments': ['cc', '-x', 'c++', '-c', str(self.source), '-x', 'c']}
        _, query = compiler_query(entry)
        self.assertEqual(query, ['cc', '-x', 'c++', '-E', '-dM', '-'])

    def test_query_language_none_restores_translation_unit_extension(self):
        entry = {'file': str(self.source), 'directory': str(self.root),
                 'arguments': ['cc', '-x', 'c++', '-x', 'none', '-c', str(self.source)]}
        _, query = compiler_query(entry)
        self.assertEqual(query, ['cc', '-x', 'c', '-E', '-dM', '-'])

    def test_invalid_execution_budgets_fail_before_analyzer_launch(self):
        tool = self.tool(0)
        for budget in (0, -1, '2.0'):
            with self.subTest(parallel=budget):
                self.assertEqual(run('tidy', self.root, self.build, tool,
                                     self.report, parallel=budget), 1)
        for budget in (0, -1, float('inf'), float('nan')):
            with self.subTest(timeout=budget):
                self.assertEqual(run('tidy', self.root, self.build, tool,
                                     self.report, timeout=budget), 1)

    def test_parallel_execution_is_bounded_and_reports_are_deterministic(self):
        sources = [self.source.with_name(f"sample{index}.c") for index in range(4)]
        for source in sources:
            source.write_text('int sample(void) { return 1; }\n')
        self.database(sources)
        tool = self.root / 'concurrent-analyzer'
        activity = self.root / 'activity.jsonl'
        tool.write_text(
            f'#!{sys.executable}\nimport json,pathlib,sys,time\n'
            "if '--version' in sys.argv: print('fixture'); sys.exit(0)\n"
            f"with open({str(activity)!r}, 'a') as stream: stream.write(json.dumps(['start', time.monotonic()])+'\\n')\n"
            "source = pathlib.Path(sys.argv[1]); index = int(source.stem.removeprefix('sample'))\n"
            "source.with_name('ready' + str(index)).touch()\n"
            "partner = source.with_name('ready' + str(index ^ 1)); deadline = time.monotonic() + 5\n"
            "while not partner.exists():\n"
            " if time.monotonic() >= deadline: sys.exit(9)\n"
            " time.sleep(0.005)\n"
            "print(sys.argv[1])\n"
            f"with open({str(activity)!r}, 'a') as stream: stream.write(json.dumps(['end', time.monotonic()])+'\\n')\n")
        tool.chmod(0o700)
        self.assertEqual(run('tidy', self.root, self.build, str(tool), self.report,
                             parallel=2), 0, self.report.read_text())
        active = maximum = 0
        for event, _ in sorted((json.loads(line) for line in activity.read_text().splitlines()), key=lambda item: item[1]):
            active += 1 if event == 'start' else -1
            maximum = max(maximum, active)
        self.assertEqual(active, 0)
        self.assertEqual(maximum, 2)
        report = self.report.read_text()
        invocation_sources = [json.loads(line[len('Invocation: '):])[1]
                              for line in report.splitlines() if line.startswith('Invocation: ')
                              and '--version' not in line]
        self.assertEqual(invocation_sources, list(map(str, sources)))

    def test_parallel_timeout_keeps_failure_and_other_context_results(self):
        other = self.source.with_name('other.c')
        other.write_text('int other;\n')
        self.database([self.source, other])
        tool = self.root / 'timeout-analyzer'
        tool.write_text(
            f'#!{sys.executable}\nimport sys,time\n'
            "if '--version' in sys.argv: print('fixture'); sys.exit(0)\n"
            f"if sys.argv[1] == {str(self.source)!r}: time.sleep(30)\n"
            "print('other-context-result')\n")
        tool.chmod(0o700)
        self.assertEqual(run('tidy', self.root, self.build, str(tool), self.report,
                             parallel=2, timeout=0.2), 1)
        report = self.report.read_text()
        self.assertIn('Exit code: 124', report)
        self.assertIn('other-context-result', report)

    def test_sigterm_cancels_active_trees_without_launching_queued_commands(self):
        import signal
        import time
        sources = [self.source.with_name(f'cancel{index}.c') for index in range(4)]
        for source in sources:
            source.write_text('int cancellation_fixture;\n')
        self.database(sources)
        tool = self.root / 'cancel-analyzer'
        tool.write_text(
            f'#!{sys.executable}\nimport pathlib,subprocess,sys,time,os\n'
            "if '--version' in sys.argv: print('fixture'); sys.exit(0)\n"
            "source = pathlib.Path(sys.argv[1])\n"
            "parent = source.with_suffix('.parent.pid'); child = source.with_suffix('.child.pid')\n"
            "parent.write_text(str(os.getpid()))\n"
            "code = 'import os,pathlib,time; pathlib.Path(' + repr(str(child)) + ').write_text(str(os.getpid())); time.sleep(30)'\n"
            "subprocess.Popen([sys.executable, '-c', code])\n"
            "while not child.exists(): time.sleep(0.005)\n"
            "source.with_suffix('.ready').touch()\ntime.sleep(30)\n")
        tool.chmod(0o700)
        command = [sys.executable, str(Path(quality_tools.__file__).resolve()),
                   'tidy', '--root', str(self.root), '--build-dir', str(self.build),
                   '--tool', str(tool), '--report', str(self.report), '--parallel', '2']
        launched = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        identities = []
        try:
            deadline = time.monotonic() + 5
            while (len(list(self.source.parent.glob('cancel*.ready'))) < 2
                   and launched.poll() is None and time.monotonic() < deadline):
                time.sleep(0.01)
            self.assertEqual(len(list(self.source.parent.glob('cancel*.ready'))), 2)
            if Path('/proc/self/stat').is_file():
                identities = [quality_tools.process._linux_process(int(path.read_text()))
                              for path in self.source.parent.glob('cancel*.pid')]
            launched.send_signal(signal.SIGTERM)
            out, err = launched.communicate(timeout=8)
            self.assertEqual(launched.returncode, 130, out + err)
            receipts = list(self.report.parent.glob('analysis.contexts/run-*/*/analysis.command.json'))
            records = [json.loads(path.read_text()) for path in receipts]
            self.assertEqual(len(records), 4)
            self.assertTrue(all(record['outcome'] == 'cancelled' for record in records))
            self.assertTrue(all(record['withdrawal_complete'] for record in records))
            self.assertEqual(sum(record['child_returncode'] is not None for record in records), 2)
            self.assertEqual(len(list(self.source.parent.glob('cancel*.ready'))), 2)
        finally:
            if launched.poll() is None:
                launched.kill()
                launched.communicate(timeout=5)
            for identity in identities:
                if identity is not None:
                    current = quality_tools.process._linux_process(identity['pid'])
                    if current is not None and current['start_time'] == identity['start_time'] and current['state'] not in ('Z', 'X'):
                        os.kill(identity['pid'], signal.SIGKILL)

    def test_diagnostic_exit_fails(self):
        self.assertEqual(run("cppcheck", self.root, self.build, self.tool(2), self.report), 1)
        self.assertIn("Exit code: 2", self.report.read_text())

    def test_crashed_tool_fails(self):
        self.assertEqual(run("tidy", self.root, self.build, self.tool(70), self.report), 1)

    def test_tool_version_failure_cannot_pass(self):
        self.assertEqual(run("tidy", self.root, self.build, self.tool(0, 1), self.report), 1)

    def test_missing_tool_fails_and_retains_reason(self):
        self.assertEqual(run("tidy", self.root, self.build, str(self.root / "missing"), self.report), 1)
        self.assertIn("Analysis execution failed", self.report.read_text())

    def test_zero_owned_scope_fails(self):
        external = self.root / "ext" / "vendor.c"
        external.parent.mkdir()
        external.write_text("int vendor;\n")
        self.database([external])
        self.assertEqual(run("cppcheck", self.root, self.build, self.tool(0), self.report), 1)
        self.assertIn("zero owned translation units", self.report.read_text())

    def test_external_vendor_commands_excluded(self):
        self.database([self.source, Path("/tmp/foreign.c"), self.root / "vendors" / "sdk.c"])
        self.assertEqual(len(commands(self.root, self.build)), 1)

    def test_core_is_owned_and_external_product_is_not(self):
        runtime = self.root / "core" / "bootstrap.c"
        runtime.parent.mkdir()
        runtime.write_text("int bootstrap(void) { return 0; }\n")
        product = self.root / "products" / "private.c"
        product.parent.mkdir()
        product.write_text("int product_private;\n")
        self.database([self.source, runtime, product])
        self.assertEqual([entry['file'] for entry in commands(self.root, self.build)],
                         [str(self.source), str(runtime)])

    def test_owned_posix_event_support_is_analyzed(self):
        support = self.root / "tests/contracts/os_freertos_runtime/posix_event.c"
        support.parent.mkdir(parents=True)
        support.write_text("int event_support;\n")
        self.database([support])
        selected = commands(self.root, self.build)
        self.assertEqual(len(selected), 1)
        self.assertEqual(selected[0]["nexus_analysis_scope"], "host-model")
        self.assertEqual(selected[0]["nexus_production_source"],
                         "tests/contracts/os_freertos_runtime/posix_event.c")

    def test_owned_posix_idle_signal_progress_is_analyzed(self):
        support = self.root / "tests/contracts/os_freertos_runtime/posix_idle.c"
        support.parent.mkdir(parents=True)
        support.write_text("int idle_support;\n")
        self.database([support])
        selected = commands(self.root, self.build)
        self.assertEqual(len(selected), 1)
        self.assertEqual(selected[0]["nexus_analysis_scope"], "host-model")
        self.assertEqual(selected[0]["nexus_production_source"],
                         "tests/contracts/os_freertos_runtime/posix_idle.c")

    def test_register_models_use_actual_production_compilation_entries(self):
        source = self.root / "soc/gd32f470/drivers/uart.c"
        source.parent.mkdir(parents=True)
        source.write_text("int production_driver;\n")
        model = self.root / "tests/contracts/gd32_io_test.c"
        model.parent.mkdir(parents=True)
        model.write_text("int model_test;\n")
        entries = [
            {"file": str(source), "directory": str(self.root),
             "arguments": ["cc", "-DNX_REGISTER_MODEL=1", "-c", str(source)]},
            {"file": str(source), "directory": str(self.root),
             "arguments": ["cc", "-DGD32F470=1", "-c", str(source)]},
            {"file": str(model), "directory": str(self.root),
             "arguments": ["cc", "-c", str(model)]},
        ]
        (self.build / "compile_commands.json").write_text(json.dumps(entries))
        selected = commands(self.root, self.build)
        self.assertEqual(len(selected), 2)
        self.assertEqual(selected[0]["arguments"][1], "-DNX_REGISTER_MODEL=1")
        self.assertEqual(selected[1]["arguments"][1], "-DGD32F470=1")
        self.assertTrue(all(e["nexus_analysis_scope"] == "production" for e in selected))

    def test_malformed_database_fails(self):
        (self.build / "compile_commands.json").write_text("{}")
        self.assertEqual(run("tidy", self.root, self.build, self.tool(0), self.report), 1)

    def test_missing_owned_source_fails(self):
        self.source.unlink()
        self.assertEqual(run("tidy", self.root, self.build, self.tool(0), self.report), 1)

    def compiler(self, name="fixture-gcc", identity="17", failure=0, empty=False):
        path = self.root / name
        log = self.root / (name + ".queries.jsonl")
        lines = "" if empty else f"#define __GNUC__ {identity}\n#define __SIZEOF_POINTER__ {{size}}\n#define {{target_macro}} 1\n#define __INTPTR_MAX__ {{signed_max}}\n#define __UINTPTR_MAX__ {{unsigned_max}}\n#define __INT32_C(x) x\n#define PROJECT_ONLY 123\n"
        path.write_text(
            f"#!{sys.executable}\nimport json, sys\n"
            f"with open({str(log)!r}, 'a') as log: log.write(json.dumps(sys.argv[1:]) + '\\n')\n"
            "if '--version' in sys.argv:\n print('fixture compiler identity'); sys.exit(0)\n"
            f"if {failure}: sys.exit({failure})\n"
            "assert '-E' in sys.argv and '-dM' in sys.argv and sys.argv[-1] == '-'\n"
            "assert '-c' not in sys.argv and '-o' not in sys.argv\n"
            "query_input = sys.stdin.read()\n"
            "arm = '--target=arm-fixture' in sys.argv\n"
            "size = '4' if arm else '8'\n"
            "signed_max = '2147483647' if arm else '9223372036854775807L'\n"
            "unsigned_max = '4294967295U' if arm else '18446744073709551615UL'\n"
            "target_macro = '__arm__' if arm else '__x86_64__'\n"
            f"print({lines!r}.format(size=size, target_macro=target_macro, signed_max=signed_max, unsigned_max=unsigned_max), end='')\n"
            "if '#include <stdint.h>' in query_input:\n"
            " print('#define INTPTR_MAX __INTPTR_MAX__\\n#define UINTPTR_MAX __UINTPTR_MAX__')\n")
        path.chmod(0o700)
        return str(path), log

    def recording_analyzer(self):
        path = self.root / "recording-analyzer"
        self.analyzer_log = self.root / "analyzer.jsonl"
        path.write_text(
            f"#!{sys.executable}\nimport json, sys\n"
            "if '--version' in sys.argv: print('fixture cppcheck'); sys.exit(0)\n"
            f"with open({str(self.analyzer_log)!r}, 'a') as log: log.write(json.dumps(sys.argv[1:]) + '\\n')\n"
            "assert any(a.startswith('-D__GNUC__=') for a in sys.argv)\n"
            "assert not any(a.startswith('-DPROJECT_ONLY=') or a.startswith('-D__INT32_C') for a in sys.argv)\n"
            "print('fixture analysis executed')\n")
        path.chmod(0o700)
        return str(path)

    def test_cppcheck_imports_actual_compiler_and_target_per_duplicate_source(self):
        first, first_log = self.compiler("one-gcc", "17")
        second, second_log = self.compiler("two-gcc", "23")
        entries = [
            {"directory": str(self.root), "file": str(self.source), "arguments": [first, "--target=arm-fixture", "-mabi=ilp32", "-std=c11", "-o", "discard.o", "-c", str(self.source)]},
            {"directory": str(self.root), "file": str(self.source), "arguments": [second, "--target=x86-fixture", "-std=c11", "-c", str(self.source)]},
        ]
        (self.build / "compile_commands.json").write_text(json.dumps(entries))
        self.assertEqual(run("cppcheck", self.root, self.build, self.recording_analyzer(), self.report), 0)
        analyzed = [json.loads(line) for line in self.analyzer_log.read_text().splitlines()]
        self.assertEqual(len(analyzed), 2)
        self.assertIn("--check-level=exhaustive", analyzed[0])
        self.assertIn("-D__GNUC__=17", analyzed[0])
        self.assertIn("-D__arm__=1", analyzed[0])
        self.assertIn("-D__SIZEOF_POINTER__=4", analyzed[0])
        self.assertIn("-D__UINTPTR_MAX__=4294967295U", analyzed[0])
        self.assertIn("-D__INTPTR_MAX__=2147483647", analyzed[0])
        self.assertIn("-DUINTPTR_MAX=__UINTPTR_MAX__", analyzed[0])
        self.assertIn("-DINTPTR_MAX=__INTPTR_MAX__", analyzed[0])
        self.assertNotIn("-D__x86_64__=1", analyzed[0])
        self.assertIn("-D__GNUC__=23", analyzed[1])
        self.assertIn("-D__SIZEOF_POINTER__=8", analyzed[1])
        self.assertIn("-D__UINTPTR_MAX__=18446744073709551615UL", analyzed[1])
        self.assertIn("-D__INTPTR_MAX__=9223372036854775807L", analyzed[1])
        self.assertNotIn("-D__arm__=1", analyzed[1])
        query = json.loads(first_log.read_text().splitlines()[-1])
        self.assertIn("-mabi=ilp32", query)
        self.assertIn("--target=arm-fixture", query)
        self.assertIn("Compiler predefined output SHA256:", self.report.read_text())
        self.assertIn("Compiler stdint output SHA256:", self.report.read_text())

    def test_missing_standard_pointer_limits_fail_before_analysis(self):
        driver, _ = self.compiler()
        path = Path(driver)
        path.write_text(path.read_text().replace("if '#include <stdint.h>' in query_input:", "if False:"))
        entries = [{"directory": str(self.root), "file": str(self.source),
                    "arguments": [driver, "-c", str(self.source)]}]
        (self.build / "compile_commands.json").write_text(json.dumps(entries))
        self.assertEqual(run("cppcheck", self.root, self.build, self.recording_analyzer(), self.report), 1)
        self.assertFalse(self.analyzer_log.exists())
        self.assertIn("compiler stdint pointer limit scope is incomplete", self.report.read_text())

    def test_unobserved_pointer_limit_dependency_fails_before_analysis(self):
        driver, _ = self.compiler()
        path = Path(driver)
        path.write_text(path.read_text().replace("UINTPTR_MAX __UINTPTR_MAX__", "UINTPTR_MAX UNKNOWN_ABI_LIMIT"))
        entries = [{"directory": str(self.root), "file": str(self.source),
                    "arguments": [driver, "-c", str(self.source)]}]
        (self.build / "compile_commands.json").write_text(json.dumps(entries))
        self.assertEqual(run("cppcheck", self.root, self.build, self.recording_analyzer(), self.report), 1)
        self.assertFalse(self.analyzer_log.exists())
        self.assertIn("unobserved dependency", self.report.read_text())

    def test_command_string_with_spaces_uses_argv_without_shell(self):
        driver, log = self.compiler()
        spaced = self.source.with_name("source with spaces.c")
        spaced.write_text("int sample;\n")
        marker = self.root / "shell-marker"
        argv = [driver, "--target=arm-fixture", "-DOPAQUE=a;touch " + str(marker), "-c", str(spaced)]
        (self.build / "compile_commands.json").write_text(json.dumps([
            {"directory": str(self.root), "file": str(spaced), "command": shlex.join(argv)}]))
        self.assertEqual(run("cppcheck", self.root, self.build, self.recording_analyzer(), self.report), 0)
        self.assertFalse(marker.exists())
        self.assertIn(argv[2], json.loads(log.read_text().splitlines()[-1]))

    def test_analyzer_databases_preserve_two_actual_cmake_compilation_contexts(self):
        self.assertIsNotNone(shutil.which('cmake'), 'CMake is required for the real database contract')
        self.assertIsNotNone(shutil.which('cc'), 'A C compiler is required for the real consumer')
        for profile, value in (('first', 17), ('second', 23)):
            directory = self.root / ('headers ' + profile) / 'owned'
            directory.mkdir(parents=True)
            (directory / 'context.h').write_text(f'#define EXPECTED_VALUE {value}\n')
        self.source.write_text(
            '#include "owned/context.h"\n'
            '_Static_assert(PROFILE_VALUE == EXPECTED_VALUE, "wrong include/define context");\n'
            'int sample(void) { return PROFILE_VALUE; }\n')
        (self.root / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.21)\nproject(quality_database_contract C)\n'
            'set(CMAKE_C_STANDARD 11)\nset(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            'add_library(first OBJECT components/sample.c)\n'
            'target_include_directories(first PRIVATE "${CMAKE_SOURCE_DIR}/headers first")\n'
            'target_compile_definitions(first PRIVATE PROFILE_VALUE=17)\n'
            'add_library(second OBJECT components/sample.c)\n'
            'target_include_directories(second PRIVATE "${CMAKE_SOURCE_DIR}/headers second")\n'
            'target_compile_definitions(second PRIVATE PROFILE_VALUE=23)\n')
        for argv in (['cmake', '-S', str(self.root), '-B', str(self.build)],
                     ['cmake', '--build', str(self.build), '--parallel', '2']):
            result = subprocess.run(argv, text=True, capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        tool = self.root / 'strict-database-consumer'
        log = self.root / 'database-consumer.jsonl'
        tool.write_text(
            f'#!{sys.executable}\nimport json, pathlib, shlex, subprocess, sys\n'
            "if '--version' in sys.argv: print('process fixture with real compiler'); sys.exit(0)\n"
            "args = sys.argv[1:]\n"
            "database = (pathlib.Path(args[args.index('-p') + 1]) / 'compile_commands.json' "
            "if '-p' in args else pathlib.Path(next(a.split('=', 1)[1] for a in args if a.startswith('--project='))))\n"
            "entries = json.loads(database.read_text())\nassert len(entries) == 1\n"
            "entry = entries[0]\n"
            "assert set(entry) <= {'directory', 'file', 'command', 'arguments', 'output'}, 'unknown compilation database key'\n"
            "argv = entry.get('arguments') or shlex.split(entry['command'])\n"
            # Consume the original compile invocation, rather than reconstructing
            # flags in the fixture. Header lookup and the C static assertion
            # independently verify that both target contexts remain intact.
            "result = subprocess.run(argv, cwd=entry['directory'])\n"
            f"with open({str(log)!r}, 'a') as output: output.write(json.dumps(entry) + '\\n')\n"
            "sys.exit(result.returncode)\n")
        tool.chmod(0o700)
        for kind in ('tidy', 'cppcheck'):
            with self.subTest(kind=kind):
                log.unlink(missing_ok=True)
                self.assertEqual(run(kind, self.root, self.build, str(tool), self.report), 0,
                                 self.report.read_text())
                consumed = [json.loads(line) for line in log.read_text().splitlines()]
                self.assertEqual(len(consumed), 2)
                self.assertIn('PROFILE_VALUE=17', consumed[0].get('command', ''))
                self.assertIn('PROFILE_VALUE=23', consumed[1].get('command', ''))
                self.assertIn('Source scope:', self.report.read_text())

    def test_compiler_query_failure_never_runs_analysis(self):
        driver, _ = self.compiler(failure=71)
        self.database([self.source])
        entries = json.loads((self.build / "compile_commands.json").read_text())
        entries[0]["arguments"][0] = driver
        (self.build / "compile_commands.json").write_text(json.dumps(entries))
        tool = self.recording_analyzer()
        self.assertEqual(run("cppcheck", self.root, self.build, tool, self.report), 1)
        self.assertFalse(self.analyzer_log.exists())
        self.assertIn("Compiler query exit code: 71", self.report.read_text())

    def test_empty_compiler_predefines_never_pass(self):
        driver, _ = self.compiler(empty=True)
        entries = [{"directory": str(self.root), "file": str(self.source), "arguments": [driver, "-c", str(self.source)]}]
        (self.build / "compile_commands.json").write_text(json.dumps(entries))
        self.assertEqual(run("cppcheck", self.root, self.build, self.recording_analyzer(), self.report), 1)
        self.assertIn("query failed or empty", self.report.read_text())

    def test_missing_compiler_never_passes(self):
        entries = [{"directory": str(self.root), "file": str(self.source), "arguments": [str(self.root / "missing-gcc"), "-c", str(self.source)]}]
        (self.build / "compile_commands.json").write_text(json.dumps(entries))
        self.assertEqual(run("cppcheck", self.root, self.build, self.recording_analyzer(), self.report), 1)
        self.assertFalse(self.analyzer_log.exists())

    def test_language_and_target_flags_preserved_without_compile_outputs(self):
        entry = {"directory": str(self.root), "file": str(self.source),
                 "arguments": ["arm-none-eabi-gcc", "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-x", "c++", "-DREAL=1", "-UOTHER", "-Iinclude", "-MMD", "-MF", "deps.d", "-MT", "target", "-oresult.o", "-c", str(self.source)]}
        version, query = compiler_query(entry)
        self.assertEqual(version, ["arm-none-eabi-gcc", "--version"])
        for flag in ("-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-DREAL=1", "-UOTHER", "-Iinclude", "c++"):
            self.assertIn(flag, query)
        for flag in ("-MMD", "-MF", "deps.d", "-MT", "target", "-oresult.o", "-c", str(self.source)):
            self.assertNotIn(flag, query)

    def test_unsupported_response_file_fails_closed(self):
        entry = {"directory": str(self.root), "file": str(self.source), "arguments": ["cc", "@unexpanded.rsp", "-c", str(self.source)]}
        with self.assertRaises(ValueError):
            compiler_query(entry)

    def cppcheck_importer_fixture(self):
        """Model the observed 2.13 importer boundary, use the real compiler."""
        tool = self.root / 'system-import-consumer'
        self.analyzer_log = self.root / 'system-consumer.jsonl'
        tool.write_text(
            f'#!{sys.executable}\nimport json, pathlib, shlex, subprocess, sys\n'
            "if '--version' in sys.argv: print('Cppcheck importer boundary fixture'); sys.exit(0)\n"
            "database = pathlib.Path(next(a.split('=', 1)[1] for a in sys.argv if a.startswith('--project=')))\n"
            "entries = json.loads(database.read_text()); assert len(entries) == 1\n"
            "entry = entries[0]; argv = entry.get('arguments') or shlex.split(entry['command'])\n"
            # Actual Cppcheck 2.13 ignores these compilation-database arguments.
            # All other flags/configuration remain intact in this process model.
            "imported = []; index = 0\n"
            "while index < len(argv):\n"
            " if argv[index] == '-isystem': index += 2; continue\n"
            " if argv[index].startswith('-isystem'): index += 1; continue\n"
            " if argv[index] == '-include': index += 2; continue\n"
            " if argv[index].startswith('-include'): index += 1; continue\n"
            " if argv[index] in ('-I', '-D', '-U', '-o', '-x'):\n"
            "  imported.extend(argv[index:index + 2]); index += 2; continue\n"
            " imported.append(argv[index]); index += 1\n"
            # Simplecpp 2.13 pushes --include files onto a LIFO stack. The
            # production wrapper must preserve GNU's original include order.
            " forced = [arg.split('=', 1)[1] for arg in sys.argv if arg.startswith('--include=')]\n"
            " imported[1:1] = [part for header in reversed(forced) for part in ('-include', header)]\n"
            "result = subprocess.run(imported, cwd=entry['directory'])\n"
            f"with open({str(self.analyzer_log)!r}, 'a') as log: log.write(json.dumps(entry) + '\\n')\n"
            "sys.exit(result.returncode)\n")
        tool.chmod(0o700)
        return str(tool)

    def test_cppcheck_forced_headers_preserve_order_cwd_and_duplicate_context(self):
        self.source.write_text(
            '#if VALUE != EXPECTED || ORDER != 2\n'
            '#error "forced header context lost"\n'
            '#endif\nint sample(void) { return VALUE; }\n')
        entries = []
        for label, value in [('first', 17), ('second', 23)]:
            directory = self.root / ('working ' + label)
            directory.mkdir()
            (directory / 'first header.h').write_text(
                f'#define VALUE {value}\n#define ORDER 1\n')
            (directory / 'second header.h').write_text(
                '#if ORDER != 1\n#error "forced include order changed"\n'
                '#endif\n#undef ORDER\n#define ORDER 2\n')
            argv = ['cc', f'-DEXPECTED={value}', '-includefirst header.h',
                    '-include', 'second header.h', '-std=c11',
                    '-c', str(self.source), '-o', str(directory / 'sample.o')]
            real = subprocess.run(argv, cwd=directory, capture_output=True,
                                  text=True, timeout=30)
            self.assertEqual(real.returncode, 0, real.stdout + real.stderr)
            entries.append({'directory': str(directory),
                            'file': str(self.source), 'arguments': argv})
        (self.build / 'compile_commands.json').write_text(json.dumps(entries))
        self.assertEqual(run('cppcheck', self.root, self.build,
                             self.cppcheck_importer_fixture(), self.report), 0,
                         self.report.read_text())
        self.assertEqual(json.loads((self.build / 'compile_commands.json').read_text()), entries)

    def test_cppcheck_forced_header_uses_actual_compiler_search_order(self):
        regular = self.root / 'regular headers'
        system = self.root / 'system headers'
        regular.mkdir(); system.mkdir()
        (regular / 'context.h').write_text('#define VALUE 17\n')
        (system / 'context.h').write_text('#error "wrong include priority"\n')
        self.source.write_text(
            '#if VALUE != 17\n#error "forced header missing"\n'
            '#endif\nint sample(void) { return VALUE; }\n')
        argv = ['cc', '-isystem', str(system), '-I', str(regular),
                '-include', 'context.h', '-std=c11', '-c', str(self.source),
                '-o', str(self.root / 'sample.o')]
        real = subprocess.run(argv, cwd=self.root, capture_output=True,
                              text=True, timeout=30)
        self.assertEqual(real.returncode, 0, real.stdout + real.stderr)
        (self.build / 'compile_commands.json').write_text(json.dumps([
            {'directory': str(self.root), 'file': str(self.source),
             'command': shlex.join(argv)}]))
        self.assertEqual(run('cppcheck', self.root, self.build,
                             self.cppcheck_importer_fixture(), self.report), 0,
                         self.report.read_text())

    def test_cppcheck_forced_header_diagnostic_is_retained(self):
        header = self.root / 'owned.h'
        header.write_text('#ifndef __GNUC__\n#error "compiler identity lost"\n'
                          '#endif\n#define OWNED_ENABLED 1\n')
        self.source.write_text(
            '#ifdef OWNED_ENABLED\n#error "owned diagnostic retained"\n'
            '#endif\nint sample(void) { return 0; }\n')
        (self.build / 'compile_commands.json').write_text(json.dumps([
            {'directory': str(self.root), 'file': str(self.source),
             'arguments': ['cc', '-include' + str(header),
                           '-c', str(self.source)]}]))
        self.assertEqual(run('cppcheck', self.root, self.build,
                             self.cppcheck_importer_fixture(), self.report), 1)
        self.assertIn('owned diagnostic retained', self.report.read_text())

    def test_cppcheck_forced_header_keeps_nested_macro_include_and_relative_search(self):
        headers = self.root / 'relative headers'
        headers.mkdir()
        (headers / 'entry.h').write_text(
            '#define NESTED_HEADER "nested.h"\n#include NESTED_HEADER\n')
        (headers / 'nested.h').write_text('#define VALUE 41\n')
        self.source.write_text(
            '#if VALUE != 41\n#error "nested forced dependency lost"\n'
            '#endif\nint sample(void) { return VALUE; }\n')
        (self.build / 'compile_commands.json').write_text(json.dumps([
            {'directory': str(self.root), 'file': str(self.source),
             'arguments': ['cc', '-Irelative headers', '-includeentry.h',
                           '-c', str(self.source)]}]))
        self.assertEqual(run('cppcheck', self.root, self.build,
                             self.cppcheck_importer_fixture(), self.report), 0,
                         self.report.read_text())

    def test_cppcheck_unprojected_preprocessor_forms_fail_closed(self):
        for flags in (['-imacros', 'macros.h'], ['-imacrosmacros.h'],
                      ['-include-pch', 'cache.pch'],
                      ['-include-pth', 'cache.pth'],
                      ['-Xpreprocessor', '-include', '-Xpreprocessor', 'forced.h'],
                      ['-Wp,-include,forced.h']):
            with self.subTest(flags=flags):
                entry = {'directory': str(self.root), 'file': str(self.source),
                         'arguments': ['cc', *flags, '-c', str(self.source)]}
                with self.assertRaisesRegex(ValueError, 'explicit analyzer projection'):
                    quality_tools.cppcheck_compilation_entry(entry)

    def test_cppcheck_forced_include_option_values_are_not_reparsed(self):
        argv = ['cc', '-D', 'OPAQUE=-includecontext.h', '-U', 'IGNORED',
                '-includeactual.h', '-c', str(self.source)]
        entry = {'directory': str(self.root), 'file': str(self.source),
                 'arguments': argv}
        original = json.loads(json.dumps(entry))
        normal, headers = quality_tools.forced_include_arguments(entry)
        self.assertEqual(headers, ['actual.h'])
        self.assertEqual(normal, argv[:5] + argv[6:])
        self.assertEqual(entry, original)

    def test_cppcheck_forced_header_preserves_system_path_option_value(self):
        headers = self.root / '-includeheaders'
        headers.mkdir()
        (headers / 'entry.h').write_text('#define VALUE 17\n')
        self.source.write_text(
            '#if VALUE != 17\n#error "system option value reparsed"\n'
            '#endif\nint sample(void) { return VALUE; }\n')
        argv = ['cc', '-isystem', '-includeheaders', '-include', 'entry.h',
                '-c', str(self.source), '-o', str(self.root / 'sample.o')]
        real = subprocess.run(argv, cwd=self.root, capture_output=True,
                              text=True, timeout=30)
        self.assertEqual(real.returncode, 0, real.stdout + real.stderr)
        (self.build / 'compile_commands.json').write_text(json.dumps([
            {'directory': str(self.root), 'file': str(self.source),
             'arguments': argv}]))
        self.assertEqual(run('cppcheck', self.root, self.build,
                             self.cppcheck_importer_fixture(), self.report), 0,
                         self.report.read_text())

    def test_cppcheck_implicit_precompiled_header_sidecar_fails_closed(self):
        import io
        header = self.root / 'forced.h'
        header.write_text('#define VALUE 17\n')
        sidecar = self.root / 'forced.h.gch'
        entry = {'directory': str(self.root), 'file': str(self.source),
                 'arguments': ['cc', '-includeforced.h', '-c', str(self.source)]}
        for directory in (False, True):
            with self.subTest(directory=directory):
                if directory:
                    sidecar.mkdir()
                else:
                    sidecar.write_bytes(b'opaque PCH candidate')
                with self.assertRaisesRegex(ValueError, 'precompiled'):
                    quality_tools.observed_forced_includes(entry, self.build,
                                                          io.StringIO())
                if directory:
                    sidecar.rmdir()
                else:
                    sidecar.unlink()

    def test_cppcheck_precompiled_header_flags_fail_closed(self):
        for flag in ('-fpch-preprocess', '-fpch-deps'):
            with self.subTest(flag=flag):
                entry = {'directory': str(self.root), 'file': str(self.source),
                         'arguments': ['cc', flag, '-c', str(self.source)]}
                with self.assertRaisesRegex(ValueError, 'explicit analyzer projection'):
                    quality_tools.cppcheck_compilation_entry(entry)

    def test_cppcheck_restores_each_actual_system_header_context(self):
        self.assertIsNotNone(shutil.which('cc'), 'Real compiler required')
        entries = []
        self.source.write_text(
            '#include <context.h>\n'
            '#if PROFILE_VALUE != EXPECTED_VALUE\n'
            '#error "different compilation header context"\n'
            '#endif\nint sample(void) { return PROFILE_VALUE; }\n')
        for label, value in [('first', 17), ('second', 23)]:
            headers = self.root / ('system headers ' + label)
            headers.mkdir()
            (headers / 'context.h').write_text(f'#define EXPECTED_VALUE {value}\n')
            argv = ['cc', '-isystem', str(headers), f'-DPROFILE_VALUE={value}',
                    '-std=c11', '-c', str(self.source), '-o', str(self.root / (label + '.o'))]
            real = subprocess.run(argv, capture_output=True, text=True, timeout=30)
            self.assertEqual(real.returncode, 0, real.stdout + real.stderr)
            entries.append({'directory': str(self.root), 'file': str(self.source),
                            'arguments': argv})
        (self.build / 'compile_commands.json').write_text(json.dumps(entries))
        self.assertEqual(run('cppcheck', self.root, self.build,
                             self.cppcheck_importer_fixture(), self.report), 0,
                         self.report.read_text())
        consumed = [json.loads(line) for line in self.analyzer_log.read_text().splitlines()]
        self.assertEqual(len(consumed), 2)
        for index, value in enumerate([17, 23]):
            self.assertIn(f'-DPROFILE_VALUE={value}', consumed[index]['arguments'])
            self.assertIn(str(self.root / ('system headers ' + ['first', 'second'][index])),
                          consumed[index]['arguments'])
        self.assertEqual(json.loads((self.build / 'compile_commands.json').read_text()), entries)

    def test_cppcheck_preserves_gnu_regular_before_system_header_priority(self):
        regular = self.root / 'regular headers'
        system = self.root / 'system headers'
        regular.mkdir(); system.mkdir()
        (regular / 'shared.h').write_text('#define SHARED_VALUE 17\n')
        (system / 'shared.h').write_text('#define SHARED_VALUE 23\n')
        (system / 'system_only.h').write_text('#define SYSTEM_VALUE 41\n')
        self.source.write_text(
            '#include <shared.h>\n#include <system_only.h>\n'
            '#if SHARED_VALUE != 17 || SYSTEM_VALUE != 41\n'
            '#error "GNU include priority changed"\n'
            '#endif\nint sample(void) { return SHARED_VALUE; }\n')
        # GNU searches -I directories before -isystem irrespective of argv order.
        argv = ['cc', '-isystem', str(system), '-I', str(regular), '-std=c11',
                '-c', str(self.source), '-o', str(self.root / 'priority.o')]
        real = subprocess.run(argv, capture_output=True, text=True, timeout=30)
        self.assertEqual(real.returncode, 0, real.stdout + real.stderr)
        (self.build / 'compile_commands.json').write_text(json.dumps([
            {'directory': str(self.root), 'file': str(self.source),
             'command': shlex.join(argv)}]))
        self.assertEqual(run('cppcheck', self.root, self.build,
                             self.cppcheck_importer_fixture(), self.report), 0,
                         self.report.read_text())

    def test_system_projection_preserves_joined_paths_abi_and_literal_defines(self):
        argv = ['arm-none-eabi-gcc', '-isystemsystem headers', '-mcpu=cortex-m4',
                '-mthumb', '-DOPAQUE=-isystem;$(literal)', '-Iregular',
                '-UOTHER', '-c', str(self.source), '-o', 'discard.o']
        entry = {'directory': str(self.root), 'file': str(self.source),
                 'command': shlex.join(argv)}
        original = json.loads(json.dumps(entry))
        projected = quality_tools.cppcheck_compilation_entry(entry)
        self.assertEqual(projected['arguments'], [*argv[:1], *argv[2:],
                                                  '-I', 'system headers'])
        self.assertEqual(entry, original)
        self.assertNotIn('command', projected)

    def test_missing_system_include_argument_fails_before_analysis(self):
        entry = {'directory': str(self.root), 'file': str(self.source),
                 'arguments': ['cc', '-c', str(self.source), '-isystem']}
        with self.assertRaisesRegex(ValueError, 'system include'):
            quality_tools.cppcheck_compilation_entry(entry)

    def test_sysroot_relative_system_paths_require_explicit_expansion(self):
        for directory in ('=/usr/include', '$SYSROOT/usr/include'):
            with self.subTest(directory=directory):
                entry = {'directory': str(self.root), 'file': str(self.source),
                         'arguments': ['cc', '-isystem', directory, '-c', str(self.source)]}
                with self.assertRaisesRegex(ValueError, 'system include'):
                    quality_tools.cppcheck_compilation_entry(entry)

    def header_scope_case(self, header, *, system=True):
        header.parent.mkdir(parents=True, exist_ok=True)
        if not header.exists():
            header.write_text('extern int first, last;\n'
                              'static inline int compare(void) {return &first < &last;}\n')
        self.source.write_text(f'#include "{header}"\nint sample(void) {{return compare();}}\n')
        entry = {'directory': str(self.root), 'file': str(self.source),
                 'arguments': ['cc', '-isystem' if system else '-I',
                               str(header.parent), '-c', str(self.source)]}
        (self.build / 'compile_commands.json').write_text(json.dumps([entry]))
        tool = self.root / 'header-ownership-analyzer'
        tool.write_text(
            f'#!{sys.executable}\nimport sys\n'
            "if '--version' in sys.argv: print('header ownership fixture'); sys.exit(0)\n"
            f"if {'--suppress=*:' + str(header)!r} in sys.argv: sys.exit(0)\n"
            "print('<results><errors><error id=\"comparePointers\" severity=\"error\">')\n"
            f"print('<location file=\"{header}\" line=\"2\"/></error></errors></results>')\n"
            'sys.exit(2)\n')
        tool.chmod(0o700)
        return run('cppcheck', self.root, self.build, str(tool), self.report)

    def test_exact_declared_external_system_header_is_outside_owned_checks(self):
        header = self.root / 'vendors/fixture/include/header.h'
        self.assertEqual(self.header_scope_case(header), 0, self.report.read_text())
        self.assertIn('External system header exclusions:', self.report.read_text())
        self.assertIn(str(header), self.report.read_text())

    def test_owned_headers_are_checked_even_with_system_include_flags(self):
        for system in (False, True):
            with self.subTest(system=system):
                header = self.root / 'os/include/header.h'
                self.assertEqual(self.header_scope_case(header, system=system), 1)
                self.assertIn('Exit code: 2', self.report.read_text())
                self.assertIn('comparePointers', self.report.read_text())
                self.assertNotIn('--suppress=*:' + str(header), self.report.read_text())

    def test_external_implementation_file_is_not_a_header_exclusion(self):
        header = self.root / 'ext/fixture/include/body.c'
        self.assertEqual(self.header_scope_case(header), 1)
        self.assertIn('Exit code: 2', self.report.read_text())
        self.assertNotIn('--suppress=*:' + str(header), self.report.read_text())

    def test_vendor_header_without_declared_system_scope_is_checked(self):
        header = self.root / 'vendors/fixture/include/header.h'
        self.assertEqual(self.header_scope_case(header, system=False), 1)
        self.assertIn('Exit code: 2', self.report.read_text())
        self.assertNotIn('--suppress=*:' + str(header), self.report.read_text())

    def test_external_header_symlink_cannot_hide_owned_code(self):
        owned = self.root / 'os/owned.h'
        owned.parent.mkdir()
        owned.write_text('extern int first, last;\n'
                         'static inline int compare(void) {return &first < &last;}\n')
        header = self.root / 'vendors/fixture/include/alias.h'
        header.parent.mkdir(parents=True)
        header.symlink_to(owned)
        self.assertEqual(self.header_scope_case(header), 1)
        self.assertIn('Exit code: 2', self.report.read_text())
        self.assertNotIn('--suppress=*:' + str(header), self.report.read_text())


if __name__ == "__main__":
    unittest.main()
