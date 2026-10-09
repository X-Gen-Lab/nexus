import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
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


if __name__ == "__main__":
    unittest.main()
