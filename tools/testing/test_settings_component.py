"""Build an authored Settings consumer through the owned component graph."""
from tools.configure.tests.native_fixture import native_fixture

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.configure import configure


ROOT = Path(__file__).resolve().parents[2]


class SettingsComponentTests(unittest.TestCase):
    def test_authored_settings_links_its_storage_dependency(self):
        with tempfile.TemporaryDirectory(prefix="settings consumer ") as name:
            source = Path(name)
            assembly = source / "assembly.toml"
            template = (ROOT / "tests/support/native/assemblies/native-empty.toml")
            assembly.write_text(template.read_text(encoding="utf-8").replace(
                "schema = 2\n", 'schema = 2\ncomponents = ["settings"]\n'),
                encoding="utf-8")
            resolved = configure.resolve_ir(assembly, test_support=native_fixture(assembly))
            self.assertEqual(resolved.components, ("settings",))
            (source / "main.c").write_text(
                '#include "nexus/components/settings.h"\n'
                'int main(void) { nx_settings_close(0); return 0; }\n',
                encoding="utf-8")
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.31)\n'
                'project(settings_consumer C)\n'
                'set(NEXUS_TEST_SUPPORT ON CACHE BOOL "Test model" FORCE)\n'
                f'set(NEXUS_ASSEMBLY_FILE "{assembly.as_posix()}")\n'
                f'add_subdirectory("{ROOT.as_posix()}" nexus)\n'
                'nexus_add_firmware(settings_consumer SOURCES main.c)\n',
                encoding="utf-8")
            cmake = shutil.which("cmake")
            self.assertIsNotNone(cmake, "Prepared CMake is required")
            build = source / "build"
            commands = (
                [cmake, "-S", str(source), "-B", str(build), "-G", "Ninja"],
                [cmake, "--build", str(build), "--target",
                 "settings_consumer", "--parallel", "2"],
                [str(build / "bin/settings_consumer")],
            )
            for command in commands:
                with self.subTest(command=command):
                    result = subprocess.run(
                        command, capture_output=True, text=True,
                        timeout=120, check=False)
                    self.assertEqual(result.returncode, 0,
                                     result.stdout + result.stderr)
                    if result.returncode:
                        return


if __name__ == "__main__":
    unittest.main()
