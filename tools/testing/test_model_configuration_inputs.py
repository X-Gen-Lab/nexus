"""Real Ninja models retain both exact generated configuration input catalogs."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ModelConfigurationInputsTests(unittest.TestCase):
    def project(self, root):
        bundle = root / "bundle"
        bundle.mkdir()
        for catalog, filename in (("input_paths", "board.json"),
                                  ("configuration_code_paths", "family.py")):
            source = root / filename
            source.write_text("source\n", encoding="utf-8")
            (bundle / (catalog + ".json")).write_text(
                json.dumps({filename: str(source)}), encoding="utf-8")
        helper = ROOT / "cmake/testing/GeneratedConfiguration.cmake"
        (root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(ModelConfigurationInputs LANGUAGES NONE)\n'
            'include("' + helper.as_posix() + '")\n'
            'nexus_google_configuration_inputs("${CMAKE_CURRENT_SOURCE_DIR}/bundle")\n'
            'file(APPEND "${CMAKE_BINARY_DIR}/observations" "configured\\n")\n',
            encoding="utf-8")
        return bundle

    def execute(self, root, *command, success=True):
        result = subprocess.run(command, cwd=root, capture_output=True,
                                text=True, timeout=60, check=False)
        if success:
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        else:
            self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def test_each_catalog_invalidates_real_ninja_then_no_change_is_no_work(self):
        with tempfile.TemporaryDirectory(prefix="nexus-model-inputs-") as name:
            root = Path(name)
            self.project(root)
            build = root / "build"
            self.execute(root, "cmake", "-S", str(root), "-B", str(build),
                         "-G", "Ninja")
            command = ("cmake", "--build", str(build), "--parallel", "1")
            self.assertIn("no work to do", self.execute(root, *command))
            marker = build / "observations"
            for filename in ("board.json", "family.py"):
                with self.subTest(catalog=filename):
                    before = marker.read_bytes()
                    source = root / filename
                    contents = source.read_bytes()
                    os.utime(source, None)
                    self.execute(root, *command)
                    self.assertEqual(contents, source.read_bytes())
                    self.assertEqual(before + b"configured\n", marker.read_bytes())
                    stable = marker.stat().st_mtime_ns
                    self.assertIn("no work to do", self.execute(root, *command))
                    self.assertEqual(stable, marker.stat().st_mtime_ns)

    def test_missing_or_invalid_catalog_cannot_silently_drop_dependencies(self):
        for invalid in (None, "{", "[]", "{}", '{"input": false}'):
            with self.subTest(invalid=invalid):
                with tempfile.TemporaryDirectory(prefix="nexus-model-invalid-") as name:
                    root = Path(name)
                    bundle = self.project(root)
                    catalog = bundle / "configuration_code_paths.json"
                    if invalid is None:
                        catalog.unlink()
                    else:
                        catalog.write_text(invalid, encoding="utf-8")
                    self.execute(root, "cmake", "-S", str(root), "-B",
                                 str(root / "build"), "-G", "Ninja", success=False)


if __name__ == "__main__":
    unittest.main()
