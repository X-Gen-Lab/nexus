"""Official inline LL headers stay private and never select HAL runtime."""
from __future__ import annotations
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class STM32OfficialCMakeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="official LL CMake ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "empty.c").write_text("int official_fixture(void) { return 0; }\n")

    def configure(self, statement, module_root=ROOT):
        (self.root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(official_ll_fixture C)\n'
            f'set(Python3_EXECUTABLE "{sys.executable}")\n'
            f'include("{module_root}/cmake/platform/STM32Official.cmake")\n'
            'add_library(provider STATIC empty.c)\n' + statement)
        return subprocess.run([
            shutil.which("cmake"), "-S", str(self.root), "-B",
            str(self.root / "build"), "-G", "Ninja"],
            capture_output=True, text=True, timeout=60)

    def test_selected_target_has_private_official_headers_without_hal_source(self):
        result = self.configure(
            'nexus_stm32f4_official(provider)\n'
            'get_target_property(private provider INCLUDE_DIRECTORIES)\n'
            'get_target_property(public provider INTERFACE_INCLUDE_DIRECTORIES)\n'
            'get_target_property(sources provider SOURCES)\n'
            'get_target_property(links provider LINK_LIBRARIES)\n'
            'get_target_property(pin provider NEXUS_STM32F4_OFFICIAL_REVISION)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/properties" '
            '"${private}\n${public}\n${sources}\n${links}\n${pin}\n")\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = (self.root / "build/properties").read_text().splitlines()
        self.assertEqual(lines[0], str(ROOT / "vendors/st/stm32f4_hal/Inc"))
        self.assertEqual(lines[1], "public-NOTFOUND")
        self.assertEqual(lines[2], "empty.c")
        self.assertEqual(lines[3], "links-NOTFOUND")
        self.assertEqual(lines[4], "b6f0ed3829f3829eb358a2e7417d80bba1a42db7")
        built = subprocess.run([
            shutil.which("cmake"), "--build", str(self.root / "build")],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)

    def test_unknown_interface_and_extra_arguments_are_rejected(self):
        for statement in ('nexus_stm32f4_official(missing)\n',
                          'add_library(face INTERFACE)\n'
                          'nexus_stm32f4_official(face)\n',
                          'nexus_stm32f4_official(provider HAL)\n'):
            with self.subTest(statement=statement):
                result = self.configure(statement)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("official", result.stdout + result.stderr)

    def test_repeated_entry_rechecks_current_bytes_without_cached_success(self):
        fixture = self.root / "source"
        (fixture / "cmake/platform").mkdir(parents=True)
        shutil.copyfile(ROOT / "cmake/platform/STM32Official.cmake",
                        fixture / "cmake/platform/STM32Official.cmake")
        (fixture / "dependencies").mkdir()
        (fixture / "dependencies/stm32f4-hal.lock.json").write_text(
            '{"files":{"LICENSE.md":{"bytes":1,"sha256":"fixture"}}}\n')
        (fixture / "tools/maintenance").mkdir(parents=True)
        # Verification is an external subprocess port here. The real locked
        # sources are exercised above and by the source-verifier test suite.
        (fixture / "tools/maintenance/stm32f4_hal_lock.py").write_text(
            'from pathlib import Path\nimport sys\n'
            'root=Path(__file__).resolve().parents[2]\n'
            'count=root/"calls"\n'
            'count.write_text(str(int(count.read_text())+1) '
            'if count.exists() else "1")\n'
            'if (root/"payload").read_text() != "approved":\n'
            '    sys.exit("current source bytes changed")\n')
        (fixture / "payload").write_text("approved")
        result = self.configure(
            'nexus_stm32f4_official(provider)\n'
            f'file(WRITE "{fixture}/payload" "changed")\n'
            'nexus_stm32f4_official(provider)\n', module_root=fixture)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("current source bytes changed", result.stdout + result.stderr)
        self.assertEqual((fixture / "calls").read_text(), "2")


if __name__ == "__main__":
    unittest.main()
