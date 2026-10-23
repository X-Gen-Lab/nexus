"""Execute explicit component target selection and rejection through CMake."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.configure.contracts import COMPONENTS


ROOT = Path(__file__).resolve().parents[2]


class ComponentSelectionTests(unittest.TestCase):
    def configure(self, body):
        with tempfile.TemporaryDirectory(
                prefix="component selection ") as value:
            directory = Path(value)
            source = directory / "source"
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.31)\n"
                "project(component_selection NONE)\n"
                f'include("{ROOT / "cmake/platform/Components.cmake"}")\n'
                + body,
                encoding="utf-8")
            return subprocess.run(
                [shutil.which("cmake") or "cmake", "-S", str(source),
                 "-B", str(directory / "build"), "-G", "Ninja"],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, check=False)

    def test_mapping_covers_exact_authored_choices_and_preserves_order(self):
        expected = ";".join(sorted(COMPONENTS))
        result = self.configure(
            "nexus_component_choices(choices)\n"
            "list(SORT choices)\n"
            f'if(NOT choices STREQUAL "{expected}")\n'
            '  message(FATAL_ERROR "Authored component choices drifted")\n'
            "endif()\n"
            "add_library(Nexus::BusOwner INTERFACE IMPORTED)\n"
            "add_library(Nexus::Log INTERFACE IMPORTED)\n"
            "nexus_resolve_components(selected log bus-owner)\n"
            'if(NOT selected STREQUAL "Nexus::Log;Nexus::BusOwner")\n'
            '  message(FATAL_ERROR '
            '"Selection order or target identity differs")\n'
            "endif()\n"
            "nexus_resolve_components(empty)\n"
            'if(NOT empty STREQUAL "")\n'
            '  message(FATAL_ERROR "Empty selection added a component")\n'
            "endif()\n")
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_unknown_component_rejects_without_guessing_a_target(self):
        result = self.configure("nexus_resolve_components(selected unknown)\n")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("No owned target for component unknown", result.stdout)

    def test_missing_selected_target_rejects_before_linking(self):
        result = self.configure("nexus_resolve_components(selected log)\n")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Selected component target is unavailable: log",
                      result.stdout)

    def test_chip_extensions_keep_existing_public_include_paths(self):
        packages = ROOT / "components/soc/stm32"
        includes = [ROOT / layer / "include"
                    for layer in ("core", "io", "arch")]
        includes.append(ROOT / "components/clock_domain/include")
        headers = []
        for package in sorted(path for path in packages.iterdir()
                              if path.is_dir()):
            includes.append(package / "include")
            headers.extend((package / "include/nexus/components").glob("*.h"))
        self.assertTrue(headers, "Chip extensions must retain public headers")
        with tempfile.TemporaryDirectory(prefix="component headers ") as value:
            source = Path(value) / "consumer.c"
            source.write_text("".join(
                f'#include "nexus/components/{header.name}"\n'
                for header in sorted(headers)), encoding="utf-8")
            args = [shutil.which("cc") or "cc", "-std=c11", "-Werror",
                    "-fsyntax-only", str(source)]
            for directory in includes:
                args.extend(["-I", str(directory)])
            result = subprocess.run(args, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True,
                                    check=False)
            self.assertEqual(result.returncode, 0, result.stdout)


if __name__ == "__main__":
    unittest.main()
