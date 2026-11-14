"""The build bridge carries only the resolver's explicit selection contract."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PackageBuildBridgeTests(unittest.TestCase):
    def configure(self, selection, checks):
        with tempfile.TemporaryDirectory(prefix="package bridge ") as temporary:
            source = Path(temporary)
            resolver = source / "tools/configure/configure.py"
            resolver.parent.mkdir(parents=True)
            resolver.write_text(
                "import pathlib, sys\n"
                "out = pathlib.Path(sys.argv[sys.argv.index('--output') + 1])\n"
                "out.mkdir(parents=True, exist_ok=True)\n"
                f"(out / 'selection.cmake').write_text({selection!r})\n"
                f"(out / 'configuration_code_paths.json').write_text({json.dumps({'resolver': str(resolver)})!r})\n"
                "(out / 'input_paths.json').write_text('{}')\n")
            (source / "assembly.toml").write_text("schema_version = 2\n")
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.31)\n"
                "project(package_bridge NONE)\n"
                f'set(Python3_EXECUTABLE "{sys.executable}")\n'
                'set(NEXUS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")\n'
                'set(NEXUS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")\n'
                'set(NEXUS_ASSEMBLY_FILE "assembly.toml")\n'
                f'include("{ROOT}/cmake/platform/Configure.cmake")\n'
                "nexus_resolve_assembly()\n" + checks)
            executable = shutil.which("cmake")
            self.assertIsNotNone(executable, "Prepared CMake is required")
            return subprocess.run([executable, "-S", str(source), "-B",
                                   str(source / "build"), "-G", "Ninja"],
                                  text=True, capture_output=True, timeout=30)

    def test_private_selection_crosses_function_without_common_whitelist(self):
        result = self.configure(
            'set(NEXUS_REVIEWED_IP_LAYOUT "packed-words")\n'
            'set(NEXUS_SELECTION_VARIABLES "NEXUS_REVIEWED_IP_LAYOUT")\n',
            'if(NOT NEXUS_REVIEWED_IP_LAYOUT STREQUAL "packed-words")\n'
            'message(FATAL_ERROR "Package private selection was lost")\n'
            'endif()\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_existing_family_source_identity_preserves_its_case(self):
        result = self.configure(
            'set(NEXUS_SOC_SOURCE_reviewed "/reviewed/source")\n'
            'set(NEXUS_SELECTION_VARIABLES "NEXUS_SOC_SOURCE_reviewed")\n',
            'if(NOT NEXUS_SOC_SOURCE_reviewed STREQUAL "/reviewed/source")\n'
            'message(FATAL_ERROR "Declared source identity was lost")\n'
            'endif()\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_or_invalid_selection_contract_is_rejected(self):
        cases = (
            '',
            'set(NEXUS_SELECTION_VARIABLES "")\n',
            'set(NEXUS_SELECTION_VARIABLES "CMAKE_C_FLAGS")\n',
            'set(NEXUS_SELECTION_VARIABLES "NEXUS_MISSING")\n',
            'set(NEXUS_VALUE 1)\n'
            'set(NEXUS_SELECTION_VARIABLES "NEXUS_VALUE;NEXUS_VALUE")\n',
        )
        for selection in cases:
            with self.subTest(selection=selection):
                result = self.configure(selection, '')
                self.assertNotEqual(result.returncode, 0,
                                    "Invalid package selection was admitted")
                self.assertIn("selection contract", result.stdout + result.stderr)

    def test_unlisted_private_values_do_not_escape_function_scope(self):
        result = self.configure(
            'set(NEXUS_DECLARED 1)\nset(NEXUS_UNDECLARED 2)\n'
            'set(NEXUS_SELECTION_VARIABLES "NEXUS_DECLARED")\n',
            'if(DEFINED NEXUS_UNDECLARED)\n'
            'message(FATAL_ERROR "Unlisted package value escaped")\n'
            'endif()\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
