"""Minimum-image matrix failures retain actual external build logs."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from tools.evidence.common import EvidenceError, atomic_json
from tools.measurement.compare import compare, tool_identity


class MeasurementMatrixTests(unittest.TestCase):
    def test_missing_tool_is_never_a_passing_profile(self):
        with self.assertRaisesRegex(EvidenceError, "unavailable"):
            tool_identity("/does/not/exist/nexus-compiler", ["--version"])

    def test_actual_cmake_failure_retains_nonzero_raw_evidence(self):
        cmake = shutil.which("cmake")
        ninja = shutil.which("ninja")
        if not cmake or not ninja:
            self.skipTest("CMake/Ninja unavailable; actual build fault not exercised")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.mkdir()
            subprocess.run(["git", "init", "-q", str(source)], check=True)
            (source / "tracked.txt").write_text("source without a CMakeLists.txt\n")
            subprocess.run(["git", "-C", str(source), "add", "."], check=True)
            subprocess.run(["git", "-C", str(source), "-c", "user.name=Test",
                "-c", "user.email=test@example.invalid", "commit", "-qm", "fixture"], check=True)
            assembly = root / "fixture.json"
            atomic_json(assembly, {"board_package": "board.json"})
            atomic_json(root / "fixture-empty.json", {"board_package": "board.json"})
            report = compare(source, assembly, root / "build", compiler=str(Path(sys.executable).resolve()),
                cmake=str(Path(cmake).resolve()), ninja=str(Path(ninja).resolve()),
                workloads=("empty",), optimizations=("Os",))
            self.assertEqual(report["status"], "failed")
            execution = report["images"][0]["commands"][0]
            self.assertNotEqual(execution["exit_code"], 0)
            self.assertIn("CMakeLists.txt", Path(execution["raw"]["path"]).read_text())
            self.assertTrue((root / "build/matrix.json").is_file())


if __name__ == "__main__":
    unittest.main()
