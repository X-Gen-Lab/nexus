"""An installed platform SDK contains build inputs, not maintainer workflows."""

import importlib.util
import subprocess
import sys
import tempfile
import shutil
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "sdk_build_scope", ROOT / "cmake/package/package_source_sdk.py")
SDK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SDK)


class SDKBuildScopeTests(unittest.TestCase):
    def selected(self, name):
        return SDK.selected(name, root=ROOT, profile=SDK.source_profile())

    def test_build_and_integrity_inputs_are_retained(self):
        for name in ("tools/configure/configure.py",
                     "tools/evidence/common.py", "tools/evidence/identity.py",
                     "tools/measurement/elf.py", "tools/measurement/resources.py",
                     "io/include/nexus/io/spi.h"):
            with self.subTest(name=name):
                self.assertTrue(self.selected(name))

    def test_maintainer_workflows_do_not_enter_the_default_sdk(self):
        for name in ("tools/documentation/cli.py", "tools/hil/admit.py",
                     "scripts/ci/tdd_gate.py", "scripts/ci/native_contracts.py",
                     "tools/measurement/compare.py"):
            with self.subTest(name=name):
                self.assertFalse(self.selected(name))

    def test_documentation_tests_are_not_product_build_inputs(self):
        self.assertFalse(self.selected("tools/documentation/test_cli.py"))

    def test_installed_developer_tools_keep_their_actual_import_closure(self):
        # Copy only the default owned SDK graph: repository-only tooling must
        # not accidentally satisfy imports from the installed developer CLI.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            names = subprocess.check_output(
                ["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0")
            for name in names:
                source = ROOT / name
                if name and source.is_file() and self.selected(name):
                    target = root / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(source, target)
            code = """import sys
sys.path.insert(0, sys.argv[1])
from tools.dev import device, dependencies
for function, arguments, expected in (
        (device._monitor, ("invalid", 0), "Monitor duration"),
        (dependencies.verify_upgrade, ({},), "every maintained gate")):
    try:
        function(*arguments)
    except ValueError as error:
        assert expected in str(error), str(error)
    else:
        raise AssertionError("Invalid input was accepted")
"""
            result = subprocess.run([sys.executable, "-I", "-S", "-c", code,
                                     str(root)], cwd=root, capture_output=True,
                                    text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
