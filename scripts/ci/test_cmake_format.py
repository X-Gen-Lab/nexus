"""The single CMake formatter preserves list and generator-expression semantics."""

from pathlib import Path
import tempfile
import unittest

from scripts.tools import cmake_format

ROOT = Path(__file__).resolve().parents[2]


class CMakeFormatTests(unittest.TestCase):
    def test_formatting_is_idempotent_and_preserves_rulers_and_arguments(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "CMakeLists.txt"
            ruler = "#" + "-" * 76
            path.write_text(ruler + "\n# Sources\n" + ruler + "\n"
                            'if(TRUE)\nset(value "a;b" "$<IF:$<BOOL:1>,x,y>")\nendif()\n')
            self.assertTrue(cmake_format.check_files(ROOT, [path]))
            cmake_format.format_files(ROOT, [path])
            self.assertEqual(cmake_format.check_files(ROOT, [path]), [])
            self.assertIn(ruler + "\n# Sources\n" + ruler, path.read_text())
            self.assertIn('    set(value "a;b" "$<IF:$<BOOL:1>,x,y>")',
                          path.read_text())

    def assert_ruler_rejected(self, ruler):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "CMakeLists.txt"
            path.write_text(ruler + '\nset(value 1)\n')
            errors = cmake_format.check_files(ROOT, [path])
            self.assertTrue(any("section rulers" in error for error in errors))

    def test_short_ruler_is_rejected(self):
        self.assert_ruler_rejected("#" + "-" * 75)

    def test_long_ruler_is_rejected(self):
        self.assert_ruler_rejected("#" + "-" * 77)

    def test_equals_ruler_is_rejected(self):
        self.assert_ruler_rejected("#" + "=" * 76)

    def test_short_comments_and_literal_rulers_are_not_banners(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "CMakeLists.txt"
            path.write_text('# Sources\n# A short responsibility comment.\n'
                            'set(value "\n#=====\n")\n'
                            'set(value [=[\n#=====\n]=])\n'
                            '#[=[\n#=====\n]=]\n')
            cmake_format.format_files(ROOT, [path])
            self.assertEqual(cmake_format.check_files(ROOT, [path]), [])

    def test_empty_selection_does_not_require_a_tool(self):
        self.assertEqual(cmake_format.check_files(ROOT, []), [])


if __name__ == "__main__":
    unittest.main()
