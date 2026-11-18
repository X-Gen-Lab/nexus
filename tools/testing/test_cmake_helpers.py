"""Exercise public CMake SDK helpers across real directory scopes."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class CMakeSDKScopeTests(unittest.TestCase):
    def test_shared_catalog_reader_returns_values_and_rejects_nonfiles(self):
        with tempfile.TemporaryDirectory(prefix="Nexus catalogs ") as temporary:
            root = Path(temporary)
            actual = root / "actual input.py"
            actual.write_text("# Exact catalog input.\n")
            catalog = root / "catalog.json"
            script = root / "read.cmake"
            script.write_text(
                f'include("{ROOT.as_posix()}/cmake/platform/Configure.cmake")\n'
                f'_nexus_read_configuration_catalog("{catalog.as_posix()}" paths labels)\n'
                f'file(WRITE "{root.as_posix()}/paths.txt" "${{paths}}")\n')
            for value, accepted in ((str(actual), True), (str(root), False),
                                    ("relative.py", False), (3, False)):
                with self.subTest(value=value):
                    catalog.write_text(json.dumps({"logical/input.py": value}))
                    completed = subprocess.run(["cmake", "-P", str(script)],
                                               capture_output=True, text=True, check=False)
                    self.assertEqual(completed.returncode == 0, accepted,
                                     completed.stdout + completed.stderr)
                    if accepted:
                        self.assertEqual((root / "paths.txt").read_text(), str(actual))

    def test_export_uses_resolved_target_profile_outside_its_directory(self):
        with tempfile.TemporaryDirectory(prefix="Nexus SDK scope ") as temporary:
            root = Path(temporary)
            source = root / "selected source"
            packager = source / "cmake/package/package_source_sdk.py"
            packager.parent.mkdir(parents=True)
            packager.write_text(
                "import json, pathlib, sys\n"
                "output = pathlib.Path(sys.argv[sys.argv.index('--output') + 1])\n"
                "output.mkdir()\n"
                "(output/'arguments.json').write_text(json.dumps(sys.argv[1:]))\n")
            (root / "assembly").mkdir()
            (root / "assembly/CMakeLists.txt").write_text(
                f'set(NEXUS_SOURCE_DIR "{source.as_posix()}")\n'
                f'set(NEXUS_CONFIG_DIR "{root.as_posix()}/generated")\n'
                'set(NEXUS_BACKEND native)\n'
                'set(NEXUS_SELECTED_COMPONENTS "crc;ring_buffer")\n'
                'set(NEXUS_SOC_PACKAGE_ID test-package)\n'
                'set(NEXUS_TEST_SUPPORT ON)\n'
                'set(NEXUS_CPU_ARCH native)\n'
                'set(NEXUS_CPU_COMPILE_OPTIONS "")\n'
                'set(NEXUS_ENUM_ABI native-int)\n'
                'set(NEXUS_OPTIMIZATION Os)\n'
                f'include("{ROOT.as_posix()}/cmake/platform/Options.cmake")\n')
            output = root / "exported SDK"
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.31)\n'
                'project(SDKScope LANGUAGES C)\n'
                'add_subdirectory(assembly)\n'
                'set(NEXUS_BACKEND poison)\n'
                'set(NEXUS_SOC_PACKAGE_ID poison)\n'
                'set(NEXUS_SELECTED_COMPONENTS poison)\n'
                'set(NEXUS_TEST_SUPPORT OFF)\n'
                f'include("{ROOT.as_posix()}/cmake/platform/SDK.cmake")\n'
                f'nexus_package_source_sdk(OUTPUT_DIRECTORY "{output.as_posix()}")\n')
            completed = subprocess.run(
                ["cmake", "-S", str(root), "-B", str(root / "build"), "-G", "Ninja"],
                capture_output=True, text=True, check=False)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            arguments = json.loads((output / "arguments.json").read_text())
            self.assertEqual(arguments[arguments.index("--backend") + 1], "baremetal")
            self.assertEqual(arguments[arguments.index("--package") + 1], "test-package")
            self.assertIn("--host-support", arguments)
            self.assertEqual([arguments[index + 1] for index, value in enumerate(arguments)
                              if value == "--component"], ["crc", "ring_buffer"])


if __name__ == "__main__":
    unittest.main()
