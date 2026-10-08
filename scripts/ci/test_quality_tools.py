import json
import os
from pathlib import Path
import shlex
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
        self.source = self.root / "services" / "sample.c"
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

    def test_explicit_production_host_models_retain_actual_compilation_scope(self):
        modeled = []
        for fixture, production in (
            ('tests/drivers/gd32f470/test_uart.c', 'platforms/gd32f470/src/uart.c'),
            ('tests/drivers/gd32f470/test_spi.c', 'platforms/gd32f470/src/spi.c'),
            ('tests/drivers/gd32f470/test_timebase.c', 'soc/gd32f470zg/interrupt.c'),
        ):
            source = self.root / production
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text('int production_driver;\n')
            model = self.root / fixture
            model.parent.mkdir(parents=True, exist_ok=True)
            model.write_text('#include "' + str(source) + '"\n')
            modeled.append(model)
        ordinary_test = self.root / 'tests/ordinary.c'
        ordinary_test.write_text('int test_fixture;\n')
        self.database([self.source, *modeled, ordinary_test, self.root / 'vendors/sdk.c'])
        selected = commands(self.root, self.build)
        self.assertEqual(len(selected), 4)
        self.assertEqual([entry['file'] for entry in selected[1:]], list(map(str, modeled)))
        self.assertTrue(all(entry['nexus_analysis_scope'] == 'host-model' for entry in selected[1:]))
        self.assertEqual(selected[1]['nexus_production_source'], 'platforms/gd32f470/src/uart.c')
        self.assertEqual(run('tidy', self.root, self.build, self.tool(0), self.report), 0)
        self.assertIn('"kind": "host-model"', self.report.read_text())
        self.assertIn('do not establish ARM execution', self.report.read_text())

    def test_a_host_model_without_its_production_source_fails(self):
        model = self.root / 'tests/drivers/gd32f470/test_spi.c'
        model.parent.mkdir(parents=True)
        model.write_text('#include "missing-driver.c"\n')
        self.database([model])
        with self.assertRaisesRegex(ValueError, 'missing modeled production source'):
            commands(self.root, self.build)

    def test_malformed_database_fails(self):
        (self.build / "compile_commands.json").write_text("{}")
        self.assertEqual(run("tidy", self.root, self.build, self.tool(0), self.report), 1)

    def test_missing_owned_source_fails(self):
        self.source.unlink()
        self.assertEqual(run("tidy", self.root, self.build, self.tool(0), self.report), 1)

    def compiler(self, name="fixture-gcc", identity="17", failure=0, empty=False):
        path = self.root / name
        log = self.root / (name + ".queries.jsonl")
        lines = "" if empty else f"#define __GNUC__ {identity}\n#define __SIZEOF_POINTER__ {{size}}\n#define {{target_macro}} 1\n#define __INT32_C(x) x\n#define PROJECT_ONLY 123\n"
        path.write_text(
            f"#!{sys.executable}\nimport json, sys\n"
            f"with open({str(log)!r}, 'a') as log: log.write(json.dumps(sys.argv[1:]) + '\\n')\n"
            "if '--version' in sys.argv:\n print('fixture compiler identity'); sys.exit(0)\n"
            f"if {failure}: sys.exit({failure})\n"
            "assert '-E' in sys.argv and '-dM' in sys.argv and sys.argv[-1] == '-'\n"
            "assert '-c' not in sys.argv and '-o' not in sys.argv\n"
            "arm = '--target=arm-fixture' in sys.argv\n"
            "size = '4' if arm else '8'\n"
            "target_macro = '__arm__' if arm else '__x86_64__'\n"
            f"print({lines!r}.format(size=size, target_macro=target_macro), end='')\n")
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
        self.assertIn("-D__GNUC__=17", analyzed[0])
        self.assertIn("-D__arm__=1", analyzed[0])
        self.assertIn("-D__SIZEOF_POINTER__=4", analyzed[0])
        self.assertNotIn("-D__x86_64__=1", analyzed[0])
        self.assertIn("-D__GNUC__=23", analyzed[1])
        self.assertIn("-D__SIZEOF_POINTER__=8", analyzed[1])
        self.assertNotIn("-D__arm__=1", analyzed[1])
        query = json.loads(first_log.read_text().splitlines()[-1])
        self.assertIn("-mabi=ilp32", query)
        self.assertIn("--target=arm-fixture", query)
        self.assertIn("Compiler predefined output SHA256:", self.report.read_text())

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
