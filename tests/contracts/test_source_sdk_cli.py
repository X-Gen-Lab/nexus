"""Direct SDK commands preserve payload bytes and caller import policy.

These small relocated Python-source fixtures exercise real entrypoints/imports;
they are not installed source SDKs and claim no package or consumer admission.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class SourceSDKCLIBytecodeTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="SDK CLI sources ")
        self.addCleanup(self.directory.cleanup)
        self.sources = Path(self.directory.name) / "relocated sources"
        for directory in ("tools", "soc"):
            for source in (ROOT / directory).rglob("*.py"):
                relative = source.relative_to(ROOT)
                if source.name.startswith("test_") or "tests" in relative.parts:
                    continue
                target = self.sources / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
        shutil.copytree(ROOT / "dependencies", self.sources / "dependencies")
        for name in ("package_source_sdk.py", "verify_consumers.py"):
            target = self.sources / "cmake/package" / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / "cmake/package" / name, target)
        self.environment = os.environ.copy()
        for name in ("PYTHONDONTWRITEBYTECODE", "PYTHONPYCACHEPREFIX",
                     "PYTHONPATH"):
            self.environment.pop(name, None)

    def snapshot(self):
        return {
            path.relative_to(self.sources).as_posix():
                hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file()
                else None
            for path in self.sources.rglob("*")
        }

    def check_help(self, name):
        before = self.snapshot()
        result = subprocess.run(
            [sys.executable, str(self.sources / name if "/" in name else
                                      self.sources / "cmake/package" / name),
             "--help"], cwd=self.directory.name, env=self.environment,
            capture_output=True, text=True, timeout=30, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("usage:", result.stdout)
        after = self.snapshot()
        self.assertEqual(set(after) - set(before), set(),
                         "Direct CLI must not create payload bytecode")
        self.assertEqual(after, before)

    def check_library_import(self, name):
        environment = {**self.environment, "PYTHONPYCACHEPREFIX": str(
            Path(self.directory.name) / "caller bytecode")}
        code = "\n".join((
            "import importlib.util, json, sys",
            "assert sys.dont_write_bytecode is False",
            "spec = importlib.util.spec_from_file_location("
            "'sdk_tool', sys.argv[1])",
            "module = importlib.util.module_from_spec(spec)",
            "spec.loader.exec_module(module)",
            "print(json.dumps({'dont_write_bytecode': "
            "sys.dont_write_bytecode}))",
        ))
        result = subprocess.run(
            [sys.executable, "-c", code,
             str(self.sources / name if "/" in name else
                                      self.sources / "cmake/package" / name)],
            cwd=self.directory.name, env=environment, capture_output=True,
            text=True, timeout=30, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout),
                         {"dont_write_bytecode": False})

    def test_mcuboot_help_preserves_relocated_sources(self):
        self.check_help("tools/maintenance/mcuboot_lock.py")

    def test_mcuboot_import_preserves_caller_policy(self):
        self.check_library_import("tools/maintenance/mcuboot_lock.py")

    def test_consumer_help_preserves_relocated_sources(self):
        self.check_help("verify_consumers.py")

    def test_package_help_preserves_relocated_sources(self):
        self.check_help("package_source_sdk.py")

    def test_consumer_library_import_preserves_caller_policy(self):
        self.check_library_import("verify_consumers.py")

    def test_package_library_import_preserves_caller_policy(self):
        self.check_library_import("package_source_sdk.py")


if __name__ == "__main__":
    unittest.main()
