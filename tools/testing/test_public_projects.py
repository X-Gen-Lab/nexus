"""Create one public GPIO consumer without private templates or optional SDKs."""

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import shutil
import tomllib
import unittest

from tools.dev import projects
from tools.testing import test_dev_projects as fixtures

ROOT = Path(__file__).resolve().parents[2]
CANONICAL = Path("cmake/package/verification/public_gpio")


class PublicProjectTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.ProjectCreationTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.output = self.fixture.output

    def source(self, *, mutation=""):
        source = self.fixture.selected_source(family="stm32f4", mutation=mutation)
        resolver = source / "tools/configure/configure.py"
        resolver.write_text(resolver.read_text().replace(
            '"backend": "native"', '"backend": "baremetal"'))
        shutil.copytree(ROOT / CANONICAL, source / CANONICAL)
        return source

    def create(self, **selection):
        return projects.create_public(self.output,
                                      board="stm32f407_qiming_v31",
                                      clock="hse8-pll168", gpio="led0",
                                      **selection)

    def test_source_project_uses_its_selected_resolver_and_public_canonical_bytes(self):
        source = self.source()
        result = self.create(source_root=source)
        self.assertTrue((source / "execution.json").exists())
        assembly = tomllib.loads((self.output / "assembly.toml").read_text())
        self.assertEqual(assembly["board"], "stm32f407_qiming_v31")
        self.assertEqual(assembly["clock"], "hse8-pll168")
        self.assertEqual(assembly["backend"], "baremetal")
        self.assertEqual(assembly["gpio"], {
            "status": {"binding": "led0", "mode": "output"}})
        for name in ("main.c", "CMakeLists.txt", "README.md"):
            self.assertEqual((self.output / name).read_bytes(),
                             (source / CANONICAL / name).read_bytes())
        self.assertIn("NX_GPIO_REF_STATUS", (self.output / "main.c").read_text())
        self.assertNotIn("test", result["commands"])
        command = result["commands"]["configure"]
        self.assertIn("-DNEXUS_SOURCE=" + str(source), command)
        self.assertIn("-DCMAKE_TOOLCHAIN_FILE=" +
                      str(source / "cmake/toolchains/arm-gcc.cmake"), command)
        self.assertNotIn("-DNEXUS_TEST_SUPPORT=ON", command)
        self.assertEqual(result["template"], "public-gpio")
        self.assertNotIn("examples_commit", result)
        self.assertFalse(result["qualification"])

    def test_sdk_project_verifies_its_exact_prefix_before_resolution_and_pins_it(self):
        prefix, source = self.fixture.sdk_prefix()
        resolver = source / "tools/configure/configure.py"
        resolver.write_text(resolver.read_text().replace(
            '"soc_family": \'native\'', '"soc_family": \'stm32f4\'').replace(
            '"arch": \'native\'', '"arch": \'cortex-m4\'').replace(
            '"backend": "native"', '"backend": "baremetal"'))
        shutil.copytree(ROOT / CANONICAL, source / CANONICAL)
        result = self.create(sdk_prefix=prefix)
        self.assertTrue((source / "verified.json").exists())
        self.assertTrue((source / "execution.json").exists())
        command = result["commands"]["configure"]
        self.assertIn("-DNexus_DIR=" + str(prefix / "lib/cmake/Nexus"), command)
        self.assertIn("-DNEXUS_EXPECTED_SOURCE_REVISION=" + "d" * 40, command)
        self.assertNotIn("test", result["commands"])

    def test_public_cli_needs_no_templates_repository(self):
        source = self.source()
        stream = io.StringIO()
        with redirect_stdout(stream):
            result = projects.main([
                "--public", "--board", "stm32f407_qiming_v31",
                "--clock", "hse8-pll168", "--gpio", "led0",
                "--source", str(source), "--output", str(self.output), "--json"])
        self.assertEqual(result, 0)
        self.assertEqual(json.loads(stream.getvalue())["template"], "public-gpio")

    def test_real_resolver_rejects_an_unknown_binding_and_rolls_back_only_its_output(self):
        with self.assertRaisesRegex(ValueError, "rejected|binding"):
            projects.create_public(self.output, board="stm32f407_qiming_v31",
                                   clock="hse8-pll168", gpio="not_a_binding",
                                   source_root=ROOT)
        self.assertFalse(self.output.exists())

    def test_selected_canonical_change_during_resolution_rolls_back(self):
        source = self.source(mutation=(
            "path=root/'cmake/package/verification/public_gpio/main.c'\n"
            "path.write_text(path.read_text()+'/* changed */\\n')\n"))
        with self.assertRaisesRegex(ValueError, "changed"):
            self.create(source_root=source)
        self.assertFalse(self.output.exists())

    def test_existing_product_and_source_selection_conflicts_are_preserved(self):
        source = self.source()
        self.output.mkdir()
        retained = self.output / "owned.c"
        retained.write_text("user source\n")
        with self.assertRaisesRegex(ValueError, "exists"):
            self.create(source_root=source)
        self.assertEqual(retained.read_text(), "user source\n")
        self.assertEqual(list(self.output.iterdir()), [retained])
        shutil.rmtree(self.output)
        with self.assertRaisesRegex(ValueError, "mutually|source.*SDK"):
            self.create(source_root=source, sdk_prefix=source)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
