"""The real CMake version bootstrap consumes one reviewed source policy."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ReleaseVersionTests(unittest.TestCase):
    def test_cmake_project_version_follows_policy_without_another_literal(self):
        with tempfile.TemporaryDirectory(prefix="Nexus version policy ") as value:
            directory = Path(value)
            policy = json.loads((ROOT /
                                 "dependencies/platform-compatibility.json").read_text())
            policy["semantic_version"] = "0.1.73"
            path = directory / "dependencies/platform-compatibility.json"
            path.parent.mkdir()
            path.write_text(json.dumps(policy))
            for name in ("TargetPolicy.cmake", "SDK.cmake"):
                relative = "cmake/platform/" + name
                target = directory / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / relative, target)
            metadata = directory / "tools/configure/metadata.py"
            metadata.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / "tools/configure/metadata.py", metadata)
            # Execute the production bootstrap through project(), independently
            # of hardware resolution and the rest of the source target graph.
            bootstrap = (ROOT / "CMakeLists.txt").read_text().split(
                "# A consumer selects one assembly;", 1)[0]
            (directory / "CMakeLists.txt").write_text(
                bootstrap + '\nfile(WRITE "${CMAKE_BINARY_DIR}/version.txt" '
                '"${CMAKE_PROJECT_VERSION}")\n')
            build = directory / "build"
            result = subprocess.run(
                ["cmake", "-S", str(directory), "-B", str(build), "-G", "Ninja"],
                capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual((build / "version.txt").read_text(),
                             policy["semantic_version"])
            invalid = json.dumps(policy).replace('"0.1.73"', '"not-a-version"')
            duplicate = json.dumps(policy).replace(
                '"semantic_version": "0.1.73"',
                '"semantic_version": "0.1.73", "semantic_version": "0.1.74"')
            for name, content in (("invalid", invalid), ("duplicate", duplicate)):
                with self.subTest(policy=name):
                    path.write_text(content)
                    result = subprocess.run(
                        ["cmake", "-S", str(directory), "-B",
                         str(directory / name), "-G", "Ninja"],
                        capture_output=True, text=True, timeout=60)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("version policy rejected", result.stderr)


if __name__ == "__main__":
    unittest.main()
