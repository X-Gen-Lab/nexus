"""Actual CMake factory export recognition stays exact and rejects new types."""
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class FactorySymbolsTests(unittest.TestCase):
    def check(self, declaration, expected):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            header = directory / "factory.h"
            header.write_text(declaration)
            script = directory / "check.cmake"
            script.write_text(
                'cmake_minimum_required(VERSION 3.31)\n'
                f'include("{ROOT.as_posix()}/tests/google/factory_symbols.cmake")\n'
                f'nexus_google_factory_symbols(actual "{header.as_posix()}")\n'
                f'if(NOT actual STREQUAL "{expected or ""}")\n'
                'message(FATAL_ERROR "Incorrect recognized factory identity")\n'
                'endif()\n')
            cmake = (shutil.which("cmake") or shutil.which(
                "cmake", path=str(Path(sys.executable).parent)))
            self.assertIsNotNone(cmake, "Prepared CMake is required")
            result = subprocess.run([cmake, "-P", str(script)], cwd=ROOT,
                                    capture_output=True, text=True, check=False)
            if expected is None:
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Unsupported generated factory export", result.stderr)
            else:
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_readonly_faces_and_labels_keep_their_exact_names(self):
        self.check('const nx_uart_port_t* nx_factory_uart(nx_uart_id_t id);\n'
                   'const char* nx_factory_label(unsigned id);\n',
                   "nx_factory_uart;nx_factory_label")

    def test_only_the_reviewed_uart_admission_callback_type_is_recognized(self):
        self.check('nx_uart_tx_validate_fn nx_factory_uart_tx_validator('
                   'nx_uart_id_t id);\n', "nx_factory_uart_tx_validator")

    def test_unreviewed_returns_remain_rejected_even_with_valid_neighbors(self):
        for return_type in ("void*", "nx_other_callback_fn", "nx_result_t"):
            with self.subTest(return_type=return_type):
                self.check('const nx_gpio_port_t* nx_factory_gpio(unsigned id);\n'
                           + return_type + ' nx_factory_unknown(unsigned id);\n',
                           None)


if __name__ == "__main__":
    unittest.main()
