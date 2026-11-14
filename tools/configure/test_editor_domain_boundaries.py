"""The assembly editor describes public grammar without selecting a family."""
from pathlib import Path
import subprocess
import sys
import unittest


class EditorDomainBoundaryTests(unittest.TestCase):
    def test_editor_schema_never_imports_a_family_adapter(self):
        command = (
            "import importlib.abc, sys\n"
            "class NoFamily(importlib.abc.MetaPathFinder):\n"
            " def find_spec(self, fullname, path=None, target=None):\n"
            "  if fullname.startswith('tools.configure.stm32f4'):\n"
            "   raise AssertionError('family selected by editor: ' + fullname)\n"
            "sys.meta_path.insert(0, NoFamily())\n"
            "from tools.configure.authored import editor_schema\n"
            "editor_schema()\n")
        result = subprocess.run([sys.executable, "-c", command],
            cwd=Path(__file__).resolve().parents[2], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_global_grammar_does_not_claim_one_mcus_divider_or_crop_limits(self):
        from tools.configure.authored import editor_schema
        properties = editor_schema()["properties"]
        clocks = properties["clock_parameters"]["properties"]
        self.assertEqual(clocks["source"]["type"], "string")
        self.assertEqual(clocks["source"]["minLength"], 1)
        self.assertNotIn("enum", clocks["source"])
        for field in ("pll_m", "pll_n", "ahb_div", "apb1_div", "apb2_div"):
            with self.subTest(field=field):
                self.assertNotIn("enum", clocks[field])
                self.assertNotIn("maximum", clocks[field])
        crop = properties["capture"]["additionalProperties"]["properties"]["crop"]
        self.assertEqual(crop["properties"]["x_clocks"]["maximum"], 0xffffffff)


if __name__ == "__main__":
    unittest.main()
