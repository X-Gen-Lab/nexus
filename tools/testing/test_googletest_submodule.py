"""Exercise offline GoogleTest admission against real Git source boundaries."""
from __future__ import annotations
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
ORIGIN = "https://github.com/google/googletest.git"


class GoogleTestSubmoduleTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="google source lock ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "ext/googletest"
        self.source.mkdir(parents=True)
        self.lock_path = self.root / "dependencies/googletest.lock.json"
        self.lock_path.parent.mkdir()
        self.git(self.root, "init", "--quiet")
        self.git(self.source, "init", "--quiet")
        self.git(self.source, "remote", "add", "origin", ORIGIN)
        (self.source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.31)\n"
            "project(googletest-distribution LANGUAGES CXX)\n"
            "set(GOOGLETEST_VERSION 1.16.0)\n"
            "add_library(gmock_main INTERFACE)\n"
            "add_library(GTest::gmock_main ALIAS gmock_main)\n")
        (self.source / "LICENSE").write_text("Fixture notice\n")
        self.git(self.source, "add", ".")
        self.git(self.source, "commit", "--quiet", "-m", "source fixture")
        self.lock = {"schema": 1, "name": "googletest", "version": "1.16.0",
            "commit": self.git(self.source, "rev-parse", "HEAD"),
            "path": "ext/googletest", "url": ORIGIN,
            "license": "BSD-3-Clause",
            "scope": "host GoogleTest/GoogleMock targets only; "
                     "no configure-time network"}
        self.write_lock()
        self.modules = self.root / ".gitmodules"
        self.modules.write_text('[submodule "ext/googletest"]\n'
            '\tpath = ext/googletest\n\turl = ' + ORIGIN + '\n')
        self.git(self.root, "add", ".gitmodules", "dependencies", "ext/googletest")
        self.git(self.root, "commit", "--quiet", "-m", "parent fixture")
        self.git(self.root, "submodule", "absorbgitdirs", "ext/googletest")

    def git(self, directory, *arguments):
        environment = {key: value for key, value in os.environ.items()
                       if not key.startswith("GIT_")}
        environment.update({"GIT_AUTHOR_NAME": "GoogleTest fixture",
            "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
            "GIT_COMMITTER_NAME": "GoogleTest fixture",
            "GIT_COMMITTER_EMAIL": "fixture@example.invalid"})
        result = subprocess.run(["git", "-C", str(directory), *arguments],
            capture_output=True, text=True, env=environment, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout.strip()

    def write_lock(self):
        self.lock_path.write_text(json.dumps(self.lock, indent=2) + "\n")

    def verify(self):
        from tools.maintenance import googletest_lock
        return googletest_lock.verify(self.source, self.lock_path)

    def test_exact_clean_submodule_is_admitted_without_modifying_sources(self):
        before = self.git(self.root, "status", "--porcelain")
        result = self.verify()
        self.assertEqual(result["commit"], self.lock["commit"])
        self.assertEqual(result["source_kind"], "git_submodule")
        self.assertEqual(before, self.git(self.root, "status", "--porcelain"))

    def test_uninitialized_directory_cannot_inherit_parent_git_identity(self):
        (self.source / ".git").unlink()
        with self.assertRaisesRegex(ValueError, "initializ"):
            self.verify()

    def test_wrong_checkout_commit_rejects(self):
        (self.source / "changed.txt").write_text("changed source\n")
        self.git(self.source, "add", ".")
        self.git(self.source, "commit", "--quiet", "-m", "different source")
        with self.assertRaisesRegex(ValueError, "commit"):
            self.verify()

    def test_wrong_parent_gitlink_rejects_even_when_checkout_matches_lock(self):
        self.git(self.root, "update-index", "--cacheinfo", "160000",
                 "a" * 40, "ext/googletest")
        with self.assertRaisesRegex(ValueError, "gitlink"):
            self.verify()

    def test_parent_uses_active_commit_index_without_leaking_it_to_dependency(self):
        alternate = self.root / "active-index"
        shutil.copy2(self.root / ".git/index", alternate)
        self.git(self.root, "update-index", "--cacheinfo", "160000",
                 "a" * 40, "ext/googletest")
        with patch.dict(os.environ, {"GIT_INDEX_FILE": str(alternate)}):
            self.assertEqual(self.verify()["commit"], self.lock["commit"])
        with self.assertRaisesRegex(ValueError, "gitlink"):
            self.verify()

    def test_initial_staged_submodule_works_before_first_parent_commit(self):
        (self.source / "new.txt").write_text("reviewed update\n")
        self.git(self.source, "add", ".")
        self.git(self.source, "commit", "--quiet", "-m", "reviewed update")
        self.lock["commit"] = self.git(self.source, "rev-parse", "HEAD")
        self.write_lock()
        self.git(self.root, "add", "ext/googletest", "dependencies")
        self.assertEqual(self.verify()["commit"], self.lock["commit"])

    def test_tracked_or_untracked_dependency_edits_reject(self):
        for name in ("LICENSE", "untracked.cpp"):
            with self.subTest(name=name):
                path = self.source / name
                before = path.read_bytes() if path.exists() else None
                path.write_text("unreviewed edit\n")
                try:
                    with self.assertRaisesRegex(ValueError, "dirty"):
                        self.verify()
                finally:
                    if before is None:
                        path.unlink()
                    else:
                        path.write_bytes(before)

    def test_changed_declaration_or_checkout_origin_rejects(self):
        original = self.modules.read_text()
        self.modules.write_text(original.replace(ORIGIN, "https://invalid.test/src"))
        with self.assertRaisesRegex(ValueError, "origin"):
            self.verify()
        self.modules.write_text(original)
        self.git(self.source, "remote", "set-url", "origin",
                 "https://invalid.test/src")
        with self.assertRaisesRegex(ValueError, "origin"):
            self.verify()

    def test_unknown_or_duplicate_lock_fields_reject(self):
        self.lock["fallback"] = True
        self.write_lock()
        with self.assertRaisesRegex(ValueError, "fields"):
            self.verify()
        del self.lock["fallback"]
        self.write_lock()
        self.lock_path.write_text(self.lock_path.read_text().replace(
            '"schema": 1', '"schema": 1, "schema": 1'))
        with self.assertRaisesRegex(ValueError, "Duplicate JSON"):
            self.verify()

    def test_unsafe_path_or_different_release_rejects(self):
        self.lock["path"] = "../googletest"
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()
        self.lock["path"] = "ext/googletest"
        self.lock["version"] = "1.17.0"
        self.write_lock()
        with self.assertRaisesRegex(ValueError, "version"):
            self.verify()

    def test_symlinked_sources_reject(self):
        moved = self.source.with_name("real-googletest")
        self.source.rename(moved)
        self.source.symlink_to(moved, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "regular"):
            self.verify()

    def configure(self, *, cross=False):
        helper = self.root / "cmake/testing/GoogleTest.cmake"
        helper.parent.mkdir(parents=True)
        shutil.copy2(ROOT / "cmake/testing/GoogleTest.cmake", helper)
        verifier = self.root / "tools/maintenance/googletest_lock.py"
        verifier.parent.mkdir(parents=True)
        shutil.copy2(ROOT / "tools/maintenance/googletest_lock.py", verifier)
        metadata = self.root / "tools/configure/metadata.py"
        metadata.parent.mkdir(parents=True)
        shutil.copy2(ROOT / "tools/configure/metadata.py", metadata)
        (self.root / "host.cpp").write_text("int main() { return 0; }\n")
        (self.root / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.31)\n"
            "project(host_admission NONE)\n"
            "include(cmake/testing/GoogleTest.cmake)\n"
            "nexus_host_google_test()\n"
            "if(NOT TARGET GTest::gmock_main)\n"
            '  message(FATAL_ERROR "Missing host framework target")\n'
            "endif()\n"
            "add_executable(host_smoke host.cpp)\n"
            "target_compile_features(host_smoke PRIVATE cxx_std_17)\n"
            "target_link_libraries(host_smoke PRIVATE GTest::gmock_main)\n")
        command = [shutil.which("cmake"), "-S", str(self.root),
                   "-B", str(self.root / "build")]
        if cross:
            command.append("-DCMAKE_SYSTEM_NAME=Generic")
        return subprocess.run(command, capture_output=True, text=True, timeout=60)

    def test_shared_cmake_loads_verified_checkout_offline(self):
        result = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("CMAKE_CXX_COMPILER", (self.root /
            "build/CMakeCache.txt").read_text())
        self.assertFalse((self.root / "build/_deps/googletest").exists())
        build = subprocess.run([shutil.which("cmake"), "--build",
            str(self.root / "build")], capture_output=True, text=True,
            timeout=60)
        self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
        binary = self.root / "build/host_smoke"
        if os.name == "nt":
            binary = binary.with_suffix(".exe")
        run = subprocess.run([str(binary)], capture_output=True, text=True,
                             timeout=10)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        # A checkout switch changes Git metadata even when its CMake file is
        # unchanged; the next ordinary build must rerun source admission.
        (self.source / "LICENSE").write_text("different committed source\n")
        self.git(self.source, "add", ".")
        self.git(self.source, "commit", "--quiet", "-m", "unexpected switch")
        rejected = subprocess.run([shutil.which("cmake"), "--build",
            str(self.root / "build")], capture_output=True, text=True,
            timeout=60)
        self.assertNotEqual(rejected.returncode, 0,
                            rejected.stdout + rejected.stderr)
        self.assertIn("checkout commit differs", rejected.stderr)

    def test_cmake_reports_actionable_uninitialized_failure_before_cpp_setup(self):
        (self.source / ".git").unlink()
        result = self.configure()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("prepare --scope native --allow-network", result.stderr)
        self.assertNotIn("CMAKE_CXX_COMPILER:", (self.root /
            "build/CMakeCache.txt").read_text())

    def test_cross_compiling_rejects_before_loading_the_framework(self):
        result = self.configure(cross=True)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("host", result.stderr)
        self.assertNotIn("CMAKE_CXX_COMPILER:", (self.root /
            "build/CMakeCache.txt").read_text())


if __name__ == "__main__":
    unittest.main()
