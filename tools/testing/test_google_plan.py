"""The real CMake aggregate must execute targets declared after plan setup."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class GooglePlanTests(unittest.TestCase):
    def configure(self, late):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        source = Path(directory.name)
        build = source / "build"
        (source / "case.c").write_text("int main(void) { return 0; }\n")
        plan = (ROOT / "tests/google/plan.cmake").as_posix()
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.24)\n'
            'project(google_plan C)\n'
            'add_custom_target(nexus_google_contracts)\n'
            'add_executable(early case.c)\n'
            'add_dependencies(nexus_google_contracts early)\n'
            'set_property(GLOBAL APPEND PROPERTY NEXUS_GOOGLE_EXECUTABLES '
            '"$<TARGET_FILE:early>")\n'
            f'include("{plan}")\n' + late
        )
        result = subprocess.run(
            [shutil.which("cmake") or "cmake", "-S", str(source),
             "-B", str(build)], capture_output=True, text=True,
        )
        return result, build

    def test_late_executable_in_aggregate_is_in_execution_plan(self):
        result, build = self.configure(
            'add_executable(late case.c)\n'
            'add_dependencies(nexus_google_contracts late)\n'
            'set_property(GLOBAL APPEND PROPERTY NEXUS_GOOGLE_EXECUTABLES '
            '"$<TARGET_FILE:late>")\n'
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        manifest = json.loads((build / "google-contracts.json").read_text())
        self.assertEqual(manifest["schema"], 1)
        self.assertEqual({Path(name).stem for name in manifest["executables"]},
                         {"early", "late"})

    def test_aggregate_rejects_non_executable_dependency(self):
        result, _build = self.configure(
            'add_custom_target(unexecutable)\n'
            'add_dependencies(nexus_google_contracts unexecutable)\n'
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("GoogleTest aggregate requires executable targets",
                      result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
