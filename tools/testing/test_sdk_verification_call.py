"""Every source-consuming CMake entry rechecks actual SDK bytes."""

from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class SDKVerificationCallTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.prefix = self.root / "prefix"
        self.source = self.prefix / "share/nexus/src"
        self.package = self.prefix / "lib/cmake/Nexus"
        self.package.mkdir(parents=True)
        (self.source / "cmake/package").mkdir(parents=True)
        (self.source / "cmake/platform").mkdir(parents=True)
        (self.source / ".nexus-source-sdk.json").write_text(json.dumps(
            {"source_revision": "fixture", "publishable": True}) + "\n")
        (self.source / "payload").write_text("approved")
        (self.source / "cmake/package/package_source_sdk.py").write_text(
            "import json,sys\nfrom pathlib import Path\n"
            "root=Path(sys.argv[2])\n"
            "count=root.parent/'count'\n"
            "count.write_text(str(int(count.read_text())+1) if count.exists() else '1')\n"
            "if (root/'payload').read_text() != 'approved':\n"
            "    sys.exit('actual payload changed')\n"
            "print(json.dumps({'source_revision':'fixture','publishable':True}))\n")
        shutil.copyfile(ROOT / "cmake/platform/SDK.cmake",
                        self.source / "cmake/platform/SDK.cmake")
        (self.source / "CMakeLists.txt").write_text(
            'set(NEXUS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")\n'
            'include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform/SDK.cmake")\n'
            'add_library(nexus_config INTERFACE)\n'
            'add_library(Nexus::Config ALIAS nexus_config)\n'
            'set_target_properties(nexus_config PROPERTIES NEXUS_SOURCE_DIR '
            '"${CMAKE_CURRENT_SOURCE_DIR}")\n')
        template = (ROOT / "cmake/package/NexusConfig.cmake.in").read_text()
        (self.package / "NexusConfig.cmake").write_text(
            template.replace("@NEXUS_SDK_VERSION@", "1.0.0"))

    def tearDown(self):
        self.temporary.cleanup()

    def configure(self, extra=""):
        (self.root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(verify_call NONE)\n'
            f'set(Python3_EXECUTABLE "{sys.executable}")\n'
            f'set(Nexus_DIR "{self.package}")\n'
            'find_package(Nexus CONFIG REQUIRED)\n' + extra)
        return subprocess.run([shutil.which("cmake"), "-S", str(self.root),
                               "-B", str(self.root / "build"), "-G", "Ninja"],
                              capture_output=True, text=True, timeout=30,
                              check=False)

    def test_package_and_provider_entries_each_verify(self):
        result = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.source.parent / "count").read_text(), "2")

    def test_repeated_find_and_later_sdk_include_each_verify_again(self):
        result = self.configure(
            'find_package(Nexus CONFIG REQUIRED)\n'
            f'set(NEXUS_SOURCE_DIR "{self.source}")\n'
            f'include("{self.source}/cmake/platform/SDK.cmake")\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.source.parent / "count").read_text(), "4")

    def test_modified_payload_rejected_by_next_find_even_same_manifest(self):
        result = self.configure(
            f'file(WRITE "{self.source}/payload" "changed")\n'
            'find_package(Nexus CONFIG REQUIRED)\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("actual payload changed", result.stdout + result.stderr)
        self.assertEqual((self.source.parent / "count").read_text(), "3")


if __name__ == "__main__":
    unittest.main()
