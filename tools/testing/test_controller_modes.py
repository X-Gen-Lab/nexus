"""CMake consumers query exact controller selections across directory scopes."""
from dataclasses import replace
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools.configure import configure, emission

ROOT = Path(__file__).resolve().parents[2]


class ControllerModesTests(unittest.TestCase):
    def assert_queries(self, selection, queries):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            provider = root / "provider"
            provider.mkdir()
            (provider / "selection.cmake").write_text(selection)
            helper = ROOT / "cmake/platform/ControllerModes.cmake"
            (provider / "CMakeLists.txt").write_text(
                'add_library(nexus_build_options INTERFACE)\n'
                'include("${CMAKE_CURRENT_LIST_DIR}/selection.cmake")\n'
                f'include("{helper.as_posix()}")\n'
                f'include("{helper.as_posix()}")\n')
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.31)\n'
                'project(controller_selection NONE)\n'
                'add_subdirectory(provider)\n' + queries)
            result = subprocess.run(
                ["cmake", "-G", "Ninja", "-S", str(root), "-B", str(root / "b")],
                capture_output=True, text=True, timeout=20, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_mixed_audio_keeps_controller_mode_selection_separate(self):
        self.assert_queries(
            'set(NEXUS_SELECTED_CONTROLLERS '
            '"audio/SAI1/slave-tx;audio/I2S2/slave-duplex-blocks;'
            'audio/SAI1/slave-tx;uart/SAI1/sync")\n',
            'nexus_controller_modes(all audio)\n'
            'nexus_controller_modes(unfiltered audio "")\n'
            'nexus_controller_names(names audio)\n'
            'nexus_controller_modes(sai audio SAI1)\n'
            'nexus_controller_modes(i2s audio I2S2)\n'
            'nexus_controller_modes(absent audio SAI2)\n'
            'nexus_controller_names(empty spi)\n'
            'if(NOT all STREQUAL "slave-tx;slave-duplex-blocks" OR '
            'NOT unfiltered STREQUAL all OR NOT names STREQUAL "SAI1;I2S2" OR NOT sai STREQUAL "slave-tx" OR '
            'NOT i2s STREQUAL "slave-duplex-blocks" OR '
            'NOT absent STREQUAL "" OR NOT empty STREQUAL "")\n'
            '  message(FATAL_ERROR "Selection mismatch: ${all}|${names}|${sai}|'
            '${i2s}|${absent}|${empty}")\n'
            'endif()\n')

    def test_generated_selection_works_without_audit_file(self):
        for profile in ("empty", "uart"):
            with self.subTest(profile=profile), tempfile.TemporaryDirectory() as name:
                source = ROOT / "tools/configure/assemblies"
                ir = configure.resolve_ir(source / f"qiming-baremetal-{profile}.toml")
                output = Path(name)
                configure.generate(ir, output)
                selection = (output / "selection.cmake").read_text()
                expected = ";".join(item.mode for item in ir.controllers)
                self.assert_queries(selection,
                    'nexus_controller_modes(modes uart)\n'
                    f'if(NOT modes STREQUAL "{expected}")\n'
                    '  message(FATAL_ERROR "Unexpected UART selection: ${modes}")\n'
                    'endif()\n')

    def test_projection_rejects_ambiguous_or_executable_cmake_tokens(self):
        source = ROOT / "tools/configure/assemblies/qiming-baremetal-uart.toml"
        item, = configure.resolve_ir(source).controllers
        for field, value in (("kind", "uart/0"), ("controller", "A;B"),
                             ("mode", "${CMAKE_SOURCE_DIR}")):
            with self.subTest(field=field), self.assertRaisesRegex(
                    ValueError, "controller selection"):
                emission.controller_selection([replace(item, **{field: value})])


if __name__ == "__main__":
    unittest.main()
