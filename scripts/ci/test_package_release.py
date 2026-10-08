"""Release safety tests with temporary local Git repos; no network/build tools."""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

import package_release as release


ELF = b"\x7fELF" + bytes(60)
LIBRARY = b"!<arch>\n" + b"fixture.o/      0           0     0     100644  64        `\n" + ELF


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name).resolve() / "source"
        self.source.mkdir()
        self.git("init", "--quiet")
        self.git("config", "user.name", "Release Test")
        self.git("config", "user.email", "release-test@example.invalid")
        for name, content in (
            ("CMakePresets.json", "{}\n"), ("README.md", "Nexus fixture\n"),
            ("LICENSE", "Fixture license\n"), (".gitignore", "build/\nrelease/\nrelease_notes.md\n"),
        ):
            (self.source / name).write_text(content, encoding="utf-8")
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "Fixture")
        self.git("tag", "v1.2.3")
        self.commit = self.git("rev-parse", "HEAD")
        self.preset = "linux-gcc-release"
        self.artifact = "nexus-linux-gcc"
        self.build = self.source / "build" / self.preset
        self.build.mkdir(parents=True)
        (self.build / ".config").write_text('CONFIG_PLATFORM_NAME="native"\n')
        (self.build / "nexus_config.h").write_text("#define CONFIG_PLATFORM_NATIVE 1\n")
        self.write_cache()

    def git(self, *args):
        return subprocess.check_output(
            ["git", "-C", str(self.source), *args], text=True, stderr=subprocess.PIPE
        ).strip()

    def write_cache(self, build_type="Release", config=None):
        (self.build / "CMakeCache.txt").write_text(
            f"CMAKE_BUILD_TYPE:STRING={build_type}\n"
            "CMAKE_GENERATOR:INTERNAL=Ninja\n"
            "CMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc\n"
            "NEXUS_PLATFORM:STRING=native\n"
            f"NEXUS_CONFIG_FILE:FILEPATH={config or self.build / '.config'}\n"
            f"NEXUS_CONFIG_HEADER:FILEPATH={self.build / 'nexus_config.h'}\n",
            encoding="utf-8",
        )

    def add_output(self, name="bin/blinky", content=ELF):
        path = self.build / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def package(self, **overrides):
        args = dict(source=self.source, version="v1.2.3", commit=self.commit,
                    preset=self.preset, artifact=self.artifact, build=self.build,
                    output=self.source / "release", configuration="Release")
        args.update(overrides)
        return release.package(**args)

    def verify(self, **overrides):
        args = dict(source=self.source, version="v1.2.3", commit=self.commit,
                    assets=self.source / "release", expected=[f"{self.artifact}={self.preset}"],
                    notes=self.source / "release_notes.md")
        args.update(overrides)
        return release.verify_assets(**args)

    def cli(self, *args):
        env = os.environ.copy()
        env.pop("GITHUB_OUTPUT", None)
        return subprocess.run(
            [sys.executable, str(Path(release.__file__).resolve()), *args,
             "--source", str(self.source), "--version", "v1.2.3"],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env,
        )

    def test_metadata_validates_existing_matching_tag(self):
        self.assertEqual(release.metadata(self.source, "v1.2.3"),
                         {"version": "v1.2.3", "commit": self.commit, "prerelease": False})

    def test_prerelease_metadata_identifies_rc_tag(self):
        self.git("tag", "v1.2.3-rc.1")
        info = release.metadata(self.source, "v1.2.3-rc.1")
        self.assertTrue(info["prerelease"])
        output = self.source / "github-output"
        with mock.patch.dict(os.environ, {"GITHUB_OUTPUT": str(output)}):
            with mock.patch("builtins.print"):
                code = release.main(["metadata", "--source", str(self.source),
                                     "--version", "v1.2.3-rc.1"])
        self.assertEqual(code, 0)
        self.assertIn("prerelease=true\n", output.read_text())

    def test_metadata_rejects_missing_tag(self):
        with self.assertRaises(release.ReleaseError):
            release.metadata(self.source, "v9.9.9")

    def test_metadata_rejects_different_tag_commit(self):
        (self.source / "README.md").write_text("A new commit\n")
        self.git("commit", "--quiet", "-am", "Changed fixture")
        with self.assertRaisesRegex(release.ReleaseError, "does not point"):
            release.metadata(self.source, "v1.2.3")

    def test_metadata_rejects_untrusted_or_invalid_version(self):
        for value in ("v1.2.3;touch hacked", "v1.2.3\ncommit=hacked", "../v1.2.3",
                      "v01.2.3", "1.2.3", "v1.2.3-01"):
            with self.subTest(value=value), self.assertRaises(release.ReleaseError):
                release.metadata(self.source, value)
        self.assertEqual(release.validate_version("v1.2.3-rc.1"), "v1.2.3-rc.1")

    def test_cli_metadata_returns_commit_and_failure_is_nonzero(self):
        result = self.cli("metadata")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["commit"], self.commit)
        result = self.cli("package", "--source-commit", self.commit, "--preset", self.preset,
                          "--artifact", self.artifact, "--build-dir", str(self.build))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No compiled", result.stderr)
        self.assertFalse((self.source / "release").exists())

    def test_cli_package_and_complete_asset_verification(self):
        self.add_output()
        result = self.cli("package", "--source-commit", self.commit, "--preset", self.preset,
                          "--artifact", self.artifact, "--build-dir", str(self.build))
        self.assertEqual(result.returncode, 0, result.stderr)
        result = self.cli("verify-assets", "--source-commit", self.commit,
                          "--expected", f"{self.artifact}={self.preset}")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.source / "release" / "SHA256SUMS").is_file())

    def test_archive_contains_compiled_outputs_config_and_unsigned_provenance(self):
        self.add_output()
        self.add_output("lib/libnexus.a", LIBRARY)
        archive = self.package()
        with zipfile.ZipFile(archive) as bundle:
            provenance = json.loads(bundle.read(f"{self.artifact}/provenance.json"))
            self.assertEqual(provenance["commit"], self.commit)
            self.assertEqual(provenance["preset"], self.preset)
            self.assertEqual(provenance["configuration"], "Release")
            self.assertEqual(provenance["submodules"], [])
            self.assertEqual(set(provenance["compiled_outputs"]), {"bin/blinky", "lib/libnexus.a"})
            self.assertIn("unsigned provenance", provenance["limitations"])
            self.assertIn(f"{self.artifact}/configuration/.config", bundle.namelist())
            self.assertIn(f"{self.artifact}/configuration/nexus_config.h", bundle.namelist())
        self.verify()
        self.assertIn(archive.name, (archive.parent / "SHA256SUMS").read_text())

    def test_multi_configuration_nested_outputs_include_only_requested_configuration(self):
        pe = b"MZ" + bytes(58) + (64).to_bytes(4, "little") + b"PE\x00\x00" + bytes(64)
        self.add_output("bin/Release/plugins/device.dll", pe)
        self.add_output("lib/Release/hal.lib", LIBRARY)
        self.add_output("bin/Debug/blinky.exe", pe)
        archive = self.package()
        with zipfile.ZipFile(archive) as bundle:
            names = bundle.namelist()
            self.assertIn(f"{self.artifact}/bin/Release/plugins/device.dll", names)
            self.assertIn(f"{self.artifact}/lib/Release/hal.lib", names)
            self.assertNotIn(f"{self.artifact}/bin/Debug/blinky.exe", names)
        self.verify()

    def test_debug_only_outputs_cannot_form_release(self):
        self.add_output("bin/Debug/blinky", ELF)
        with self.assertRaisesRegex(release.ReleaseError, "No compiled"):
            self.package()

    def test_documentation_or_raw_blobs_cannot_form_release(self):
        for name, content in (("bin/README.md", b"Docs only\n"),
                              ("lib/hal.a", b"Not a library"),
                              ("bin/firmware.bin", bytes(128))):
            self.add_output(name, content)
        with self.assertRaisesRegex(release.ReleaseError, "No compiled"):
            self.package()

    def test_missing_build_or_cmake_cache_fails(self):
        with self.assertRaises(release.ReleaseError):
            self.package(build=self.source / "build" / "missing")
        (self.build / "CMakeCache.txt").unlink()
        with self.assertRaises(release.ReleaseError):
            self.package()

    def test_missing_config_or_header_fails(self):
        self.add_output()
        for name in (".config", "nexus_config.h"):
            original = (self.build / name).read_bytes()
            (self.build / name).unlink()
            with self.subTest(name=name), self.assertRaises(release.ReleaseError):
                self.package()
            (self.build / name).write_bytes(original)

    def test_effective_debug_or_unknown_cache_cannot_form_release(self):
        self.add_output()
        for build_type in ("Debug", ""):
            self.write_cache(build_type)
            with self.subTest(build_type=build_type), self.assertRaisesRegex(
                    release.ReleaseError, "configuration does not match"):
                self.package()

    def test_explicit_debug_configuration_cannot_use_release_preset(self):
        self.add_output()
        self.write_cache("Debug")
        with self.assertRaisesRegex(release.ReleaseError, "Release build"):
            self.package(configuration="Debug")

    def test_native_cache_cannot_be_labeled_as_arm_target(self):
        self.add_output()
        with self.assertRaisesRegex(release.ReleaseError, "platform"):
            self.package(preset="linux-stm32-armgcc-release", artifact="nexus-arm-cortex-m4")

    def test_wrong_compiler_cannot_use_gcc_release_label(self):
        self.add_output()
        cache = self.build / "CMakeCache.txt"
        cache.write_text(cache.read_text().replace("/usr/bin/gcc", "/usr/bin/clang"))
        with self.assertRaisesRegex(release.ReleaseError, "compiler"):
            self.package()

    def arm_fixture(self):
        toolchain = self.source / "cmake" / "toolchains" / "arm-gcc.cmake"
        toolchain.parent.mkdir(parents=True)
        toolchain.write_text("# fixture ARM GCC toolchain\n")
        self.git("add", "cmake")
        self.git("commit", "--quiet", "-m", "ARM toolchain fixture")
        self.git("tag", "-f", "v1.2.3")
        self.commit = self.git("rev-parse", "HEAD")
        cache = self.build / "CMakeCache.txt"
        value = cache.read_text().replace("/usr/bin/gcc", "/usr/bin/arm-none-eabi-gcc")
        value = value.replace("NEXUS_PLATFORM:STRING=native", "NEXUS_PLATFORM:STRING=stm32")
        cache.write_text(value + (
            "NEXUS_CPU_ARCH:STRING=cortex-m4\n"
            "NEXUS_FPU_TYPE:STRING=fpv4-sp-d16\n"
            "NEXUS_FLOAT_ABI:STRING=hard\n"
            "NEXUS_TOOLCHAIN_NAME:STRING=arm-gcc\n"
            f"CMAKE_TOOLCHAIN_FILE:FILEPATH={toolchain}\n"
        ))

    def test_arm_effective_target_profile_can_be_packaged_and_verified(self):
        self.arm_fixture()
        self.add_output()
        self.package(preset="linux-stm32-armgcc-release", artifact="nexus-arm-cortex-m4")
        self.verify(expected=["nexus-arm-cortex-m4=linux-stm32-armgcc-release"])

    def test_arm_config_toolchain_or_cpu_override_fails_closed(self):
        self.arm_fixture()
        self.add_output()
        cache = self.build / "CMakeCache.txt"
        original = cache.read_text()
        for before, after in (("cortex-m4", "cortex-m7"), ("arm-gcc\n", "armclang\n"),
                              ("fpv4-sp-d16", "fpv5-d16"), ("=hard\n", "=soft\n")):
            cache.write_text(original.replace(before, after))
            with self.subTest(after=after), self.assertRaisesRegex(release.ReleaseError, "Effective"):
                self.package(preset="linux-stm32-armgcc-release", artifact="nexus-arm-cortex-m4")

    def test_source_commit_mismatch_fails(self):
        self.add_output()
        with self.assertRaisesRegex(release.ReleaseError, "differs"):
            self.package(commit="0" * 40)

    def test_modified_tracked_source_cannot_claim_clean_provenance(self):
        self.add_output()
        (self.source / "README.md").write_text("Uncommitted\n")
        with self.assertRaisesRegex(release.ReleaseError, "Tracked source"):
            self.package()

    def test_path_traversal_and_outside_config_are_rejected(self):
        self.add_output()
        with self.assertRaises(release.ReleaseError):
            self.package(artifact="../escape")
        with self.assertRaises(release.ReleaseError):
            self.package(output=self.source / ".." / "escape")
        external = self.source.parent / "external.config"
        external.write_text("Outside source\n")
        self.write_cache(config=external)
        with self.assertRaisesRegex(release.ReleaseError, "outside"):
            self.package()

    def test_archive_paths_reject_cross_platform_traversal_and_checksum_injection(self):
        for value in ("bin/../../escape", "bin/..\\escape", "bin/C:escape",
                      "bin/file\nchecksum", "bin/./blinky", "bin//blinky", "bin/blinky."):
            with self.subTest(value=value), self.assertRaises(release.ReleaseError):
                release.validate_archive_path(value)
        self.assertEqual(release.validate_archive_path("bin/Release/plugins/device.dll").as_posix(),
                         "bin/Release/plugins/device.dll")

    def make_symlink(self, path, target):
        try:
            path.symlink_to(target, target_is_directory=target.is_dir())
        except (OSError, NotImplementedError) as exc:
            self.skipTest(f"Symlinks unavailable: {exc}")

    def test_symlink_output_file_is_rejected(self):
        self.add_output()
        external = self.source.parent / "external.bin"
        external.write_bytes(ELF)
        self.make_symlink(self.build / "bin" / "escape", external)
        with self.assertRaisesRegex(release.ReleaseError, "Symlinks"):
            self.package()

    def test_symlink_output_directory_is_rejected(self):
        self.add_output()
        self.make_symlink(self.build / "bin" / "escape", self.source.parent)
        with self.assertRaisesRegex(release.ReleaseError, "Symlinks"):
            self.package()

    def test_existing_assets_are_not_overwritten(self):
        self.add_output()
        archive = self.package()
        original = archive.read_bytes()
        with self.assertRaisesRegex(release.ReleaseError, "replace existing"):
            self.package()
        self.assertEqual(archive.read_bytes(), original)

    def test_submodule_status_requires_recorded_initialized_commits(self):
        with mock.patch.object(release, "git", return_value="a" * 40 + " ext/library (heads/main)"):
            self.assertEqual(release.submodules(self.source),
                             [{"path": "ext/library", "commit": "a" * 40}])
        for marker in ("-", "+", "U"):
            with self.subTest(marker=marker), mock.patch.object(
                    release, "git", return_value=marker + "a" * 40 + " ext/library"):
                with self.assertRaises(release.ReleaseError):
                    release.submodules(self.source)

    def test_incomplete_asset_set_is_rejected(self):
        self.add_output()
        self.package()
        with self.assertRaisesRegex(release.ReleaseError, "complete build matrix"):
            self.verify(expected=[f"{self.artifact}={self.preset}",
                                  "nexus-arm-cortex-m4=linux-stm32-armgcc-release"])
        self.assertFalse((self.source / "release_notes.md").exists())

    def test_corrupt_archive_transfer_is_rejected(self):
        self.add_output()
        archive = self.package()
        with archive.open("ab") as handle:
            handle.write(b"tampered transfer")
        with self.assertRaisesRegex(release.ReleaseError, "checksum mismatch"):
            self.verify()

    def rewrite_archive(self, archive, additions=None, changes=None):
        with zipfile.ZipFile(archive) as bundle:
            content = {name: bundle.read(name) for name in bundle.namelist()}
        content.update(additions or {})
        content.update(changes or {})
        with zipfile.ZipFile(archive, "w") as bundle:
            for name, value in content.items():
                bundle.writestr(name, value)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n")

    def test_archive_traversal_is_rejected_even_with_valid_transfer_checksum(self):
        self.add_output()
        archive = self.package()
        self.rewrite_archive(archive, additions={f"{self.artifact}/../../escape": b"bad"})
        with self.assertRaisesRegex(release.ReleaseError, "unsafe entry"):
            self.verify()

    def test_internal_member_checksum_is_verified(self):
        self.add_output()
        archive = self.package()
        self.rewrite_archive(archive, changes={f"{self.artifact}/bin/blinky": ELF + b"changed"})
        with self.assertRaisesRegex(release.ReleaseError, "member checksum mismatch"):
            self.verify()

    def test_wrong_provenance_is_rejected(self):
        self.add_output()
        archive = self.package()
        with self.assertRaisesRegex(release.ReleaseError, "preset"):
            self.verify(expected=[f"{self.artifact}=wrong-preset"])
        self.assertTrue(archive.is_file())

    def test_verifier_rejects_nonrelease_configuration_or_cache_provenance(self):
        self.add_output()
        archive = self.package()
        with zipfile.ZipFile(archive) as bundle:
            original = json.loads(bundle.read(f"{self.artifact}/provenance.json"))
        for field in ("configuration", "cache"):
            value = json.loads(json.dumps(original))
            if field == "configuration":
                value["configuration"] = "Debug"
            else:
                value["cmake"]["CMAKE_BUILD_TYPE"] = "Debug"
            self.rewrite_archive(archive, changes={f"{self.artifact}/provenance.json": json.dumps(value).encode()})
            with self.subTest(field=field), self.assertRaisesRegex(release.ReleaseError, "Release build"):
                self.verify()

    def test_verifier_rejects_native_cache_under_arm_provenance(self):
        self.arm_fixture()
        self.add_output()
        archive = self.package(preset="linux-stm32-armgcc-release", artifact="nexus-arm-cortex-m4")
        with zipfile.ZipFile(archive) as bundle:
            value = json.loads(bundle.read("nexus-arm-cortex-m4/provenance.json"))
        value["cmake"]["NEXUS_PLATFORM"] = "native"
        self.rewrite_archive(archive, changes={"nexus-arm-cortex-m4/provenance.json": json.dumps(value).encode()})
        with self.assertRaisesRegex(release.ReleaseError, "platform"):
            self.verify(expected=["nexus-arm-cortex-m4=linux-stm32-armgcc-release"])


if __name__ == "__main__":
    unittest.main()
