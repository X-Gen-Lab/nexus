"""CMake selects modes for the exact controller rather than a kind cross product."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ControllerModesTests(unittest.TestCase):
    def test_mixed_audio_keeps_controller_mode_selection_separate(self):
        with tempfile.TemporaryDirectory() as name:
            directory = Path(name)
            (directory / "resolved.json").write_text(json.dumps({
                "controllers": [
                    {"kind": "audio", "controller": "SAI1", "mode": "slave-tx"},
                    {"kind": "audio", "controller": "I2S2",
                     "mode": "slave-duplex-blocks"}]}))
            script = directory / "select.cmake"
            script.write_text(
                'set(NEXUS_CONFIG_DIR "' + directory.as_posix() + '")\n'
                'include("' + (ROOT / "cmake/platform/ControllerModes.cmake").as_posix() + '")\n'
                'nexus_controller_modes(all audio)\n'
                'nexus_controller_modes(sai audio SAI1)\n'
                'nexus_controller_modes(i2s audio I2S2)\n'
                'nexus_controller_modes(absent audio SAI2)\n'
                'if(NOT sai STREQUAL "slave-tx" OR NOT i2s STREQUAL '
                '"slave-duplex-blocks" OR NOT absent STREQUAL "")\n'
                '  message(FATAL_ERROR "Controller modes leaked: ${sai}|${i2s}|${absent}")\n'
                'endif()\n'
                'list(LENGTH all count)\n'
                'if(NOT count EQUAL 2)\n'
                '  message(FATAL_ERROR "Aggregate query changed")\n'
                'endif()\n')
            result = subprocess.run(["cmake", "-P", str(script)],
                                    capture_output=True, text=True, timeout=20,
                                    check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
