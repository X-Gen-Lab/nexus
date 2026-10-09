"""Release behavior tests with local Git dependencies and generated build bundles."""

import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

import package_release as release
from test_validate_firmware_elf import fixture as firmware_fixture


def elf(machine=62, elf_class=2):
    value = bytearray(64)
    value[:6] = b"\x7fELF" + bytes((elf_class, 1))
    value[16:18] = (2).to_bytes(2, "little")
    value[18:20] = machine.to_bytes(2, "little")
    return bytes(value)


ELF = elf()
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
        self.dependency = self.source.parent / "dependency"
        self.dependency.mkdir()
        self.dep_git("init", "--quiet")
        self.dep_git("config", "user.name", "Dependency Test")
        self.dep_git("config", "user.email", "dependency-test@example.invalid")
        (self.dependency / "library.c").write_text("/* Local dependency fixture. */\n")
        self.dep_git("add", ".")
        self.dep_git("commit", "--quiet", "-m", "Dependency fixture")
        for dependency in release.RELEASE_PROFILES["linux-gcc-release"]["dependencies"]:
            self.add_dependency(dependency)
        self.fragment = self.source / "platforms/native/defconfig"
        self.fragment.parent.mkdir(parents=True)
        self.fragment.write_text("CONFIG_PLATFORM_NATIVE=y\n")
        runtime_source = self.source / "runtime/main.c"
        runtime_source.parent.mkdir(parents=True)
        runtime_source.write_text("int main(void) { return 0; }\n")
        self.create_board_sources()
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "Fixture")
        self.git("tag", "v1.2.3")
        self.commit = self.git("rev-parse", "HEAD")
        self.preset = "linux-gcc-release"
        self.artifact = "nexus-linux-gcc"
        self.build = self.source / "build" / self.preset
        self.build.mkdir(parents=True)
        self.elf = ELF
        self.values = {
            "CONFIG_BUILD_TYPE": "Release", "CONFIG_BUILD_TYPE_RELEASE": True,
            "CONFIG_BUILD_TYPE_DEBUG": False, "CONFIG_BUILD_TESTS": True,
            "CONFIG_BUILD_CONTRACTS": True, "CONFIG_ENABLE_COVERAGE": False,
            "CONFIG_ENABLE_SANITIZERS": False, "CONFIG_PLATFORM_NAME": "native",
            "CONFIG_PLATFORM_NATIVE": True, "CONFIG_PLATFORM_STM32": False,
            "CONFIG_BOARD_NAME": "native-reference", "CONFIG_OSAL_BACKEND_NAME": "native",
            "CONFIG_TOOLCHAIN_NAME": "gcc", "CONFIG_TEST_BUDGET": 4096,
        }
        self.write_bundle()
        self.write_cache()
        self.write_compile_commands()
        self.write_test_report()
        self.write_board_bundle()

    def git(self, *args):
        return subprocess.check_output(
            ["git", "-C", str(self.source), *args], text=True, stderr=subprocess.PIPE
        ).strip()

    def dep_git(self, *args):
        return subprocess.check_output(
            ["git", "-C", str(self.dependency), *args], text=True, stderr=subprocess.PIPE,
        ).strip()

    def add_dependency(self, path):
        self.git("-c", "protocol.file.allow=always", "submodule", "add", "--quiet", str(self.dependency), path)

    def write_bundle(self):
        generated = self.build / "generated"
        generated.mkdir(exist_ok=True)
        effective, header, cmake = [], [], []
        for key, value in self.values.items():
            if isinstance(value, bool):
                effective.append(f"{key}=y" if value else f"# {key} is not set")
                header.append(f"#define NX_{key} 1" if value else f"/* #undef NX_{key} */")
                resolved = "ON" if value else "OFF"
            else:
                effective.append(f"{key}={json.dumps(value)}")
                if value != "":
                    header.append(f"#define NX_{key} {json.dumps(value)}")
                resolved = str(value)
            delimiter = "="
            while f"]{delimiter}]" in resolved:
                delimiter += "="
            cmake.append(f"set({key} [{delimiter}[{resolved}]{delimiter}])")
        for name, lines in (("effective.config", effective), ("nexus_config.h", header), ("config.cmake", cmake)):
            (generated / name).write_text("\n".join(lines) + "\n")

    @staticmethod
    def board_soc(profile):
        return {"native-reference": "native", "stm32f4discovery-mb997": "stm32f407vg",
                "stm32f407zg-qiming-v31": "stm32f407zg", "stm32f407ve-sky-qingchun": "stm32f407ve",
                "gd32f470zg-liangshan": "gd32f470zg"}[profile["board"]]

    def create_board_sources(self):
        for profile in release.RELEASE_PROFILES.values():
            directory = self.source / release.BOARD_DIRECTORIES[profile["board"]]
            directory.mkdir(parents=True, exist_ok=True)
            (directory / "CMakeLists.txt").write_text("# Board fixture\n")
            (directory / "nexus_board.h").write_text("/* Board input fixture. */\n")
            platform = profile["platform"]
            resources = []
            if platform != "native":
                pins = {"tx": ("A", 2, 7), "rx": ("A", 3, 7)} if platform == "stm32" else {"tx": ("A", 9, 7), "rx": ("A", 10, 7)}
                resources.append({"id": "console", "kind": "uart", "instance": 1 if platform == "stm32" else 0,
                    "when": "CONFIG_PLATFORM_STM32" if platform == "stm32" else "CONFIG_GD32F470ZG",
                    "clock": "USART2" if platform == "stm32" else "USART0", "irq": 38 if platform == "stm32" else 37,
                    "priority": 6, "pins": [{"signal": signal, "port": port, "pin": pin, "af": af}
                                               for signal, (port, pin, af) in pins.items()]})
            manifest = {"schema": 1, "id": profile["board"], "soc": self.board_soc(profile),
                        "hse_hz": 0 if platform == "native" else (8000000 if platform == "stm32" else 25000000),
                        "interface_target": "board_fixture", "object_targets": [],
                        "inputs": ["CMakeLists.txt", "nexus_board.h"], "resources": resources}
            (directory / "board.json").write_text(json.dumps(manifest))
        for profile in (release.RELEASE_PROFILES["stm32-armgcc-release"],
                        release.RELEASE_PROFILES["gd32f470-armgcc-baremetal-release"]):
            path = self.source / release.linker_sections(profile)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("/* Linker section fixture. */\n")

    def write_board_bundle(self, profile=None, layout_input=None):
        profile = profile or release.RELEASE_PROFILES["linux-gcc-release"]
        generator = release.board_package_module()
        manifest, soc, identity, active = generator.validate_manifest(
            self.source / release.BOARD_DIRECTORIES[profile["board"]], self.values)
        self.layout = generator.validate_layout(layout_input, manifest["soc"])
        generator.emit(self.build / "generated", manifest, soc, identity, active, self.layout, self.source)
        self.board_identity = json.loads((self.build / "generated/board-identity.json").read_text())

    def write_compile_commands(self, flags=()):
        value = [{"directory": str(self.build), "file": str(self.source / "runtime/main.c"),
                  "arguments": ["gcc", f"-I{self.build / 'generated'}", *flags, "-c",
                                str(self.source / "runtime/main.c")]}]
        (self.build / "compile_commands.json").write_text(json.dumps(value))

    def write_test_report(self, contents='<testsuite tests="1" failures="0"><testcase name="fixture" status="run"/></testsuite>'):
        (self.build / "ctest-results.xml").write_text(contents)

    def write_cache(self, build_type="Release", config=None):
        (self.build / "CMakeCache.txt").write_text(
            f"CMAKE_BUILD_TYPE:STRING={build_type}\n"
            "CMAKE_GENERATOR:INTERNAL=Ninja\n"
            f"CMAKE_HOME_DIRECTORY:INTERNAL={self.source}\n"
            "CMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc\n"
            "NEXUS_PLATFORM:STRING=native\n"
            "NEXUS_OSAL_BACKEND:STRING=\n"
            "NEXUS_BUILD_TESTS:BOOL=ON\n"
            "NEXUS_BUILD_CONTRACTS:BOOL=ON\n"
            "NEXUS_ENABLE_COVERAGE:BOOL=OFF\n"
            "NEXUS_ENABLE_SANITIZERS:BOOL=OFF\n"
            f"NEXUS_CONFIG_FILE:FILEPATH={config or self.fragment}\n",
            encoding="utf-8",
        )

    def add_output(self, name=None, content=None):
        if name is None:
            name = ("bin/runtime_native_smoke" if self.values["CONFIG_PLATFORM_NAME"] == "native"
                    else "bin/nexus_contract_firmware.elf")
        path = self.build / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(self.elf if content is None else content)
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

    def test_missing_generated_board_identity_rejects_candidate(self):
        self.add_output()
        (self.build / "generated/board-identity.json").unlink()
        with self.assertRaisesRegex(release.ReleaseError, "board-identity"):
            self.package()

    def test_arm_generated_board_layout_header_and_linker_are_required(self):
        self.arm_fixture()
        self.add_output()
        for name in release.BOARD_FILES + release.LAYOUT_FILES:
            path = self.build / "generated" / name
            content = path.read_bytes()
            path.unlink()
            with self.subTest(name=name), self.assertRaises(release.ReleaseError):
                self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")
            path.write_bytes(content)

    def test_generated_header_linker_and_board_identity_must_match_inputs(self):
        self.arm_fixture()
        self.add_output()
        for name in ("board-identity.json", "board.cmake", "nx_flash_layout.h", "firmware.ld"):
            path = self.build / "generated" / name
            content = path.read_bytes()
            path.write_bytes(content + b"\n/* injected */\n")
            with self.subTest(name=name), self.assertRaises(release.ReleaseError):
                self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")
            path.write_bytes(content)

    def test_platform_contract_cannot_be_substituted_by_unrelated_application(self):
        for name in ("bin/unrelated-app", "bin/nexus_contract_firmware.elf"):
            with self.subTest(name=name):
                output = self.add_output(name)
                with self.assertRaisesRegex(release.ReleaseError, "platform contract"):
                    self.package()
                output.unlink()

    def test_contract_name_directory_and_type_are_specific_to_each_platform(self):
        for preset, profile in release.RELEASE_PROFILES.items():
            native = profile["platform"] == "native"
            valid_name = "runtime_native_smoke" if native else "nexus_contract_firmware.elf"
            wrong_name = "nexus_contract_firmware.elf" if native else "runtime_native_smoke"
            header = elf() if native else elf(40, 1)
            relocatable = bytearray(header)
            relocatable[16:18] = (1).to_bytes(2, "little")
            for name, content in ((f"bin/{wrong_name}", header),
                                  ("bin/runtime_native_smoke.elf" if native else "bin/nexus_contract_firmware", header),
                                  (f"bin/{valid_name}.other", header),
                                  (f"lib/{valid_name}", header),
                                  (f"bin/{valid_name}", bytes(relocatable))):
                with self.subTest(preset=preset, name=name, elf_type=int.from_bytes(content[16:18], "little")):
                    with self.assertRaisesRegex(release.ReleaseError, "platform contract"):
                        release.validate_target_outputs([(name, content)], profile)

    def test_native_candidate_has_source_bound_board_identity_without_flash_layout(self):
        self.add_output()
        archive = self.package()
        with zipfile.ZipFile(archive) as bundle:
            names = bundle.namelist()
            provenance = json.loads(bundle.read(f"{self.artifact}/provenance.json"))
            self.assertEqual(provenance["target"]["board_sha256"], self.board_identity["sha256"])
            self.assertIsNone(provenance["target"]["layout_sha256"])
            for name in ("board.json", "CMakeLists.txt", "nexus_board.h"):
                self.assertIn(f"{self.artifact}/configuration/board/{name}", names)
            self.assertNotIn(f"{self.artifact}/configuration/layout.json", names)
        self.verify()

    def test_missing_archived_board_input_rejected_after_manifest_refresh(self):
        self.add_output()
        archive = self.package()
        self.rewrite_archive(archive, removals=[f"{self.artifact}/configuration/board/nexus_board.h"],
                             refresh_manifests=True)
        with self.assertRaisesRegex(release.ReleaseError, "Board/layout"):
            self.verify()

    def test_coherently_rewritten_board_is_still_bound_to_source_git_objects(self):
        self.add_output()
        archive = self.package()
        prefix = self.artifact + "/"
        (self.source / "boards/native_reference/nexus_board.h").write_text("/* Rewritten board. */\n")
        self.write_board_bundle()
        with zipfile.ZipFile(archive) as bundle:
            provenance = json.loads(bundle.read(prefix + "provenance.json"))
        provenance["target"]["board_sha256"] = self.board_identity["sha256"]
        provenance["validation"]["board_sha256"] = self.board_identity["sha256"]
        changes = {prefix + "configuration/board/nexus_board.h":
                   (self.source / "boards/native_reference/nexus_board.h").read_bytes()}
        for name in release.BOARD_FILES:
            changes[prefix + "configuration/" + name] = (self.build / "generated" / name).read_bytes()
        changes[prefix + "provenance.json"] = json.dumps(provenance).encode()
        self.rewrite_archive(archive, changes=changes, refresh_manifests=True)
        # Local worktree matches the forged bytes; committed Git objects do not.
        release.verify_bundle(archive, self.artifact, self.preset, "v1.2.3", self.commit)
        with self.assertRaisesRegex(release.ReleaseError, "source commit"):
            self.verify()

    def test_archived_arm_layout_header_linker_and_source_section_mutations_fail(self):
        self.arm_fixture(); self.add_output()
        archive = self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")
        prefix = "nexus-stm32f407-baremetal/"
        names = [prefix + "configuration/" + name for name in
                 (*release.LAYOUT_FILES, "linker-sections.ld")]
        with zipfile.ZipFile(archive) as bundle:
            original = {name: bundle.read(name) for name in names}
        for name in names:
            self.rewrite_archive(archive, changes=original | {name: original[name] + b"\n/* rewritten */\n"},
                                 refresh_manifests=True)
            with self.subTest(name=name), self.assertRaises(release.ReleaseError):
                self.verify(expected=["nexus-stm32f407-baremetal=stm32-armgcc-release"])

    def test_native_archive_cannot_add_a_physical_layout(self):
        self.add_output(); archive = self.package()
        self.rewrite_archive(archive, additions={f"{self.artifact}/configuration/layout.json": b"{}"},
                             refresh_manifests=True)
        with self.assertRaisesRegex(release.ReleaseError, "physical Flash layout"):
            self.verify()

    def test_explicit_layout_is_archived_and_controls_the_real_arm_gate(self):
        self.arm_fixture()
        profile = release.RELEASE_PROFILES["stm32-armgcc-release"]
        layout_input = self.source / "layouts/reference.json"
        layout_input.parent.mkdir()
        layout_input.write_text(json.dumps({"schema": 1, "soc": "stm32f407vg",
            "image": {"offset": 0, "size": 0xe0000},
            "regions": [{"name": "config", "offset": 0xe0000, "size": 0x20000}]}))
        self.git("add", "."); self.git("commit", "--quiet", "-m", "Explicit layout input")
        self.git("tag", "-f", "v1.2.3"); self.commit = self.git("rev-parse", "HEAD")
        with (self.build / "CMakeCache.txt").open("a") as cache:
            cache.write(f"NEXUS_FLASH_LAYOUT_FILE:FILEPATH={layout_input}\n")
        self.write_board_bundle(profile, layout_input)
        self.elf = firmware_fixture(config=self.values, layout=self.layout, board_identity=self.board_identity)
        self.add_output()
        archive = self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")
        with zipfile.ZipFile(archive) as bundle:
            prefix = "nexus-stm32f407-baremetal/"
            provenance = json.loads(bundle.read(prefix + "provenance.json"))
            self.assertEqual(provenance["target"]["layout_sha256"], self.layout["sha256"])
            self.assertEqual(provenance["validation"]["images"][0]["image_end"], 0x080e0000)
            self.assertEqual(bundle.read(prefix + "configuration/layout.input.json"), layout_input.read_bytes())
        self.verify(expected=["nexus-stm32f407-baremetal=stm32-armgcc-release"])

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
            self.assertEqual({m["path"] for m in provenance["submodules"]}, {"ext/googletest", "ext/freertos", "vendors/arm/CMSIS_5",
                              "vendors/st/cmsis_device_f4", "vendors/st/stm32f4xx_hal_driver"})
            self.assertEqual(provenance["target"]["board"], "native-reference")
            self.assertEqual(provenance["validation"]["executed"], 1)
            self.assertEqual(set(provenance["compiled_outputs"]), {"bin/runtime_native_smoke", "lib/libnexus.a"})
            self.assertIn("unsigned provenance", provenance["limitations"])
            for name in release.CONFIGURATION_FILES:
                self.assertIn(f"{self.artifact}/configuration/{name}", bundle.namelist())
            self.assertIn(f"{self.artifact}/configuration/input.fragment", bundle.namelist())
            self.assertIn(f"{self.artifact}/build/compile_commands.json", bundle.namelist())
            self.assertIn(f"{self.artifact}/validation/ctest-results.xml", bundle.namelist())
            self.assertNotIn("NEXUS_CONFIG_HEADER", provenance["configuration_paths"])
        self.verify()
        self.assertIn(archive.name, (archive.parent / "SHA256SUMS").read_text())

    def test_multi_configuration_nested_outputs_include_only_requested_configuration(self):
        self.add_output("bin/Release/runtime_native_smoke", ELF)
        self.add_output("bin/Release/plugins/device.so", ELF)
        self.add_output("lib/Release/hal.lib", LIBRARY)
        self.add_output("bin/Debug/blinky", ELF)
        archive = self.package()
        with zipfile.ZipFile(archive) as bundle:
            names = bundle.namelist()
            self.assertIn(f"{self.artifact}/bin/Release/plugins/device.so", names)
            self.assertIn(f"{self.artifact}/lib/Release/hal.lib", names)
            self.assertNotIn(f"{self.artifact}/bin/Debug/blinky", names)
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
        for name in release.CONFIGURATION_FILES:
            path = self.build / "generated" / name
            original = path.read_bytes()
            path.unlink()
            with self.subTest(name=name), self.assertRaises(release.ReleaseError):
                self.package()
            path.write_bytes(original)

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
            self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")

    def test_wrong_compiler_cannot_use_gcc_release_label(self):
        self.add_output()
        cache = self.build / "CMakeCache.txt"
        cache.write_text(cache.read_text().replace("/usr/bin/gcc", "/usr/bin/clang"))
        with self.assertRaisesRegex(release.ReleaseError, "compiler"):
            self.package()

    def arm_fixture(self, osal="baremetal"):
        toolchain = self.source / "cmake" / "toolchains" / "arm-gcc.cmake"
        toolchain.parent.mkdir(parents=True)
        toolchain.write_text("# fixture ARM GCC toolchain\n")
        for path in ("vendors/arm/CMSIS_5", "vendors/st/cmsis_device_f4", "vendors/st/stm32f4xx_hal_driver"):
            if not (self.source / path).exists():
                self.add_dependency(path)
        self.fragment = self.source / ("configs/stm32f407_freertos_defconfig" if osal == "freertos"
                                       else "configs/stm32f407_baremetal_defconfig")
        self.fragment.parent.mkdir(exist_ok=True)
        self.fragment.write_text(f"CONFIG_PLATFORM_STM32=y\nCONFIG_OSAL_{osal.upper()}=y\n")
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "ARM toolchain fixture")
        self.git("tag", "-f", "v1.2.3")
        self.commit = self.git("rev-parse", "HEAD")
        self.configure_arm_bundle(toolchain, osal)

    def configure_arm_bundle(self, toolchain, osal="baremetal", profile=None):
        profile = profile or release.RELEASE_PROFILES["stm32-armgcc-release"]
        cache = self.build / "CMakeCache.txt"
        value = cache.read_text().replace("/usr/bin/gcc", "/usr/bin/arm-none-eabi-gcc")
        value = value.replace("NEXUS_PLATFORM:STRING=native", "NEXUS_PLATFORM:STRING=" + profile["platform"])
        value = value.replace("NEXUS_BUILD_TESTS:BOOL=ON", "NEXUS_BUILD_TESTS:BOOL=OFF")
        value = re.sub(r"NEXUS_CONFIG_FILE:FILEPATH=.*", f"NEXUS_CONFIG_FILE:FILEPATH={self.fragment}", value)
        cache.write_text(value + (
            f"CMAKE_TOOLCHAIN_FILE:FILEPATH={toolchain}\n"
        ))
        self.values.update({"CONFIG_PLATFORM_NAME": profile["platform"], "CONFIG_PLATFORM_NATIVE": False,
                            "CONFIG_PLATFORM_STM32": profile["platform"] == "stm32",
                            "CONFIG_BOARD_NAME": profile["board"], "CONFIG_OSAL_BACKEND_NAME": osal,
                            "CONFIG_TOOLCHAIN_NAME": "arm-none-eabi-gcc", "CONFIG_CPU_ARCH": "cortex-m4",
                            "CONFIG_FPU_TYPE": "fpv4-sp-d16", "CONFIG_FLOAT_ABI": "hard",
                            "CONFIG_BUILD_TESTS": False})
        self.values.update(profile.get("expected_config", {}))
        self.values.update({"CONFIG_LINKER_RAM_START": 0x20000000,
                            "CONFIG_LINKER_RAM_SIZE": 0x20000 if profile["platform"] == "stm32" else 0x30000,
                            "CONFIG_LINKER_FLASH_START": 0x08000000,
                            "CONFIG_LINKER_FLASH_SIZE": self.values.get("CONFIG_STM32_FLASH_SIZE", 0x100000),
                            "CONFIG_INSTANCE_STM32_UART_1": profile["platform"] == "stm32",
                            "CONFIG_GD32_UART_ENABLE": profile["platform"] == "gd32f470",
                            "CONFIG_STM32_HSE_VALUE": 8000000, "CONFIG_GD32_HXTAL_VALUE": 25000000,
                            "CONFIG_OSAL_FREERTOS": osal == "freertos"})
        self.values["CONFIG_" + self.board_soc(profile).upper()] = True
        self.write_bundle()
        self.write_compile_commands(("-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"))
        self.write_board_bundle(profile)
        self.elf = firmware_fixture(config=self.values, layout=self.layout, board_identity=self.board_identity)

    def test_arm_effective_target_profile_can_be_packaged_and_verified(self):
        self.arm_fixture()
        self.add_output()
        self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")
        self.verify(expected=["nexus-stm32f407-baremetal=stm32-armgcc-release"])

    def test_arm_package_cli_executes_static_gate_and_download_verification(self):
        self.arm_fixture()
        self.add_output()
        result = self.cli("package", "--source-commit", self.commit,
                          "--preset", "stm32-armgcc-release", "--artifact", "nexus-stm32f407-baremetal",
                          "--build-dir", str(self.build))
        self.assertEqual(result.returncode, 0, result.stderr)
        result = self.cli("verify-assets", "--source-commit", self.commit,
                          "--expected", "nexus-stm32f407-baremetal=stm32-armgcc-release")
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads((self.build / "firmware-static-contract.json").read_text())
        self.assertEqual(report["kind"], "arm-static-link-contract")

    def test_arm_package_reruns_actual_elf_checks_instead_of_trusting_a_pass_report(self):
        self.arm_fixture()
        for defect in ("missing_vectors", "weak_irq", "rwx", "storage_overlap", "partial_descriptor"):
            with self.subTest(defect=defect):
                (self.build / "firmware-static-contract.json").write_text('{"status":"forged-pass"}')
                self.add_output(content=firmware_fixture(defect, config=self.values, layout=self.layout, board_identity=self.board_identity))
                with self.assertRaisesRegex(release.ReleaseError, "static contract"):
                    self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")
                self.assertFalse((self.build / "firmware-static-contract.json").exists())
        self.assertFalse((self.source / "release").exists())

    def test_arm_report_is_recomputed_archived_and_verified_against_firmware(self):
        self.arm_fixture()
        self.add_output()
        archive = self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")
        prefix = "nexus-stm32f407-baremetal"
        with zipfile.ZipFile(archive) as bundle:
            report = json.loads(bundle.read(prefix + "/validation/firmware-static-contract.json"))
            provenance = json.loads(bundle.read(prefix + "/provenance.json"))
        self.assertEqual(report, provenance["validation"])
        self.assertEqual(report["kind"], "arm-static-link-contract")
        self.assertFalse(report["hardware_verified"])
        self.assertEqual(report["images"][0]["device_descriptor_bytes"], 32)
        self.assertEqual(report["images"][0]["vector_bytes"], 0x188)
        self.assertIn("USART2_IRQHandler", report["images"][0]["strong_interrupts"])
        report["images"][0]["vector_bytes"] = 0
        self.rewrite_archive(archive, changes={prefix + "/validation/firmware-static-contract.json":
                                             json.dumps(report).encode()}, refresh_manifests=True)
        with self.assertRaisesRegex(release.ReleaseError, "real firmware"):
            self.verify(expected=["nexus-stm32f407-baremetal=stm32-armgcc-release"])

    def test_arm_archive_firmware_mutation_rejected_after_transfer_manifests_refreshed(self):
        self.arm_fixture()
        self.add_output()
        archive = self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")
        self.rewrite_archive(archive, changes={"nexus-stm32f407-baremetal/bin/nexus_contract_firmware.elf":
                                             firmware_fixture("missing_vectors", config=self.values, layout=self.layout, board_identity=self.board_identity)},
                             refresh_manifests=True)
        with self.assertRaisesRegex(release.ReleaseError, "static contract"):
            self.verify(expected=["nexus-stm32f407-baremetal=stm32-armgcc-release"])

    def test_release_workflow_matrix_and_asset_gate_match_reviewed_profiles(self):
        workflow = (Path(release.__file__).resolve().parents[2] / ".github/workflows/release.yml").read_text()
        pairs = re.findall(r"^            preset: (\S+)\n            artifact: (\S+)$", workflow, re.MULTILINE)
        expected = {preset: profile["artifact"] for preset, profile in release.RELEASE_PROFILES.items()}
        self.assertEqual(dict(pairs), expected)
        self.assertEqual(len(pairs), len(expected))
        asset_pairs = re.findall(r"--expected (\S+)=([^\s\\]+)", workflow)
        self.assertEqual({preset: artifact for artifact, preset in asset_pairs}, expected)
        self.assertIn("if: ${{ !matrix.host_tests }}", workflow)
        self.assertIn("python scripts/ci/validate_firmware_elf.py", workflow)
        self.assertIn("build/${{ matrix.preset }}/firmware-static-contract.json", workflow)

    def test_arm_config_toolchain_or_cpu_override_fails_closed(self):
        self.arm_fixture()
        self.add_output()
        original = self.values.copy()
        for field, value in (("CONFIG_CPU_ARCH", "cortex-m7"), ("CONFIG_TOOLCHAIN_NAME", "armclang"),
                             ("CONFIG_FPU_TYPE", "fpv5-d16"), ("CONFIG_FLOAT_ABI", "soft")):
            self.values = original | {field: value}
            self.write_bundle()
            with self.subTest(field=field), self.assertRaisesRegex(release.ReleaseError, "Effective"):
                self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")

    def test_source_commit_mismatch_fails(self):
        self.add_output()
        with self.assertRaisesRegex(release.ReleaseError, "differs"):
            self.package(commit="0" * 40)

    def test_modified_tracked_source_cannot_claim_clean_provenance(self):
        self.add_output()
        (self.source / "README.md").write_text("Uncommitted\n")
        with self.assertRaisesRegex(release.ReleaseError, "Tracked source"):
            self.package()

    def test_untracked_production_source_cannot_claim_source_commit_identity(self):
        self.add_output()
        source = self.source / "runtime/untracked.c"
        source.write_text("int untracked_feature(void) { return 1; }\n")
        with self.assertRaisesRegex(release.ReleaseError, "Untracked source"):
            self.package()

    def test_cache_cannot_point_to_a_different_source_tree(self):
        self.add_output()
        cache = self.build / "CMakeCache.txt"
        cache.write_text(cache.read_text().replace(f"CMAKE_HOME_DIRECTORY:INTERNAL={self.source}",
                                                  f"CMAKE_HOME_DIRECTORY:INTERNAL={self.dependency}"))
        with self.assertRaisesRegex(release.ReleaseError, "outside"):
            self.package()

    def test_production_compile_command_cannot_reference_unrelated_source(self):
        self.add_output()
        commands = self.build / "compile_commands.json"
        contents = json.loads(commands.read_text())
        contents[0]["file"] = str(self.source.parent / "runtime/unrelated.c")
        commands.write_text(json.dumps(contents))
        with self.assertRaisesRegex(release.ReleaseError, "outside"):
            self.package()

    def test_production_namespace_uses_only_first_component_below_recorded_root(self):
        root = Path("/not-present/osal/nexus")
        generated = root / "build/generated"
        commands = [{"file": str(root / "hal/device.c"), "arguments": ["gcc", f"-I{generated}", "-c"]}]
        # The ancestor root itself also contains 'osal'. No substring can
        # establish component ownership relative to that root.
        commands.extend({"file": str(root / path), "arguments": ["gcc", "-c"]} for path in
                        ("tests/osal/freertos_runtime/wait_for_event.c", "tests/hal/model.c",
                         "ext/vendor/services/library.c", "build/generated/runtime/model.c"))
        release.validate_compile_commands(json.dumps(commands), release.RELEASE_PROFILES[self.preset],
                                          generated, root, require_local_sources=False)

    def test_nested_owned_names_cannot_supply_a_production_command(self):
        root = Path("/not-present/hal/nexus")
        generated = root / "build/generated"
        for path in ("tests/osal/freertos_runtime/wait_for_event.c", "tests/arch/model.c",
                     "ext/vendor/hal/driver.c", "build/runtime/fake.c", "fake.c"):
            command = {"file": str(root / path), "arguments": ["gcc", f"-I{generated}", "-c"]}
            with self.subTest(path=path), self.assertRaisesRegex(release.ReleaseError, "no Nexus production"):
                release.validate_compile_commands(json.dumps([command]), release.RELEASE_PROFILES[self.preset],
                                                  generated, root, require_local_sources=False)

    def test_owned_relative_compile_path_uses_recorded_command_directory(self):
        directory = self.source / "hal"
        directory.mkdir(); (directory / "device.c").write_text("int fixture;\n")
        generated = self.build / "generated"
        command = {"file": "device.c", "directory": str(directory),
                   "arguments": ["gcc", f"-I{generated}", "-c"]}
        release.validate_compile_commands(json.dumps([command]), release.RELEASE_PROFILES[self.preset], generated, self.source)
        for bad_directory in ("hal", str(self.source.parent / "other")):
            command["directory"] = bad_directory
            with self.subTest(directory=bad_directory), self.assertRaises(release.ReleaseError):
                release.validate_compile_commands(json.dumps([command]), release.RELEASE_PROFILES[self.preset], generated, self.source)

    def test_recorded_source_identity_and_owned_configuration_remain_required_offline(self):
        root = Path("/not-present/nexus")
        generated = root / "build/generated"
        for directory in ("hal", "osal", "framework", "services", "platforms", "boards", "soc", "arch", "runtime"):
            commands = [{"file": str(root / directory / "source.c"), "arguments": ["gcc", "-c"]}]
            with self.subTest(directory=directory), self.assertRaisesRegex(release.ReleaseError, "generated configuration"):
                release.validate_compile_commands(json.dumps(commands), release.RELEASE_PROFILES[self.preset], generated, root,
                                                  require_local_sources=False)
        commands = [{"file": str(root / "hal/source.c"), "arguments": ["gcc", f"-I{generated}", "-c"]}]
        for invalid_root in (None, Path("relative/root"), root / "../nexus"):
            with self.subTest(root=invalid_root), self.assertRaises(release.ReleaseError):
                release.validate_compile_commands(json.dumps(commands), release.RELEASE_PROFILES[self.preset], generated, invalid_root,
                                                  require_local_sources=False)
        commands.append({"file": "/outside/vendor.c", "arguments": ["gcc", "-c"]})
        with self.assertRaisesRegex(release.ReleaseError, "outside"):
            release.validate_compile_commands(json.dumps(commands), release.RELEASE_PROFILES[self.preset], generated, root,
                                              require_local_sources=False)

    def test_bundle_compile_identity_does_not_require_build_machine_source_on_verifier(self):
        self.add_output(); archive = self.package()
        foreign = self.source.parent / "not-present-build-machine/nexus"
        self.assertFalse(foreign.exists())
        prefix = self.artifact + "/"
        with zipfile.ZipFile(archive) as bundle:
            cache = bundle.read(prefix + "build/CMakeCache.txt").decode().replace(str(self.source), str(foreign))
            commands = bundle.read(prefix + "build/compile_commands.json").decode().replace(str(self.source), str(foreign))
            provenance = json.loads(bundle.read(prefix + "provenance.json"))
        provenance["cmake"]["CMAKE_HOME_DIRECTORY"] = str(foreign)
        self.rewrite_archive(archive, changes={prefix + "build/CMakeCache.txt": cache.encode(),
                prefix + "build/compile_commands.json": commands.encode(),
                prefix + "provenance.json": json.dumps(provenance).encode()}, refresh_manifests=True)
        release.verify_bundle(archive, self.artifact, self.preset, "v1.2.3", self.commit)

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
                      "bin/file\nchecksum", "bin/./blinky", "bin//blinky", "bin/nexus_contract_firmware.elf."):
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
                                  "nexus-stm32f407-baremetal=stm32-armgcc-release"])
        self.assertFalse((self.source / "release_notes.md").exists())

    def test_corrupt_archive_transfer_is_rejected(self):
        self.add_output()
        archive = self.package()
        with archive.open("ab") as handle:
            handle.write(b"tampered transfer")
        with self.assertRaisesRegex(release.ReleaseError, "checksum mismatch"):
            self.verify()

    def rewrite_archive(self, archive, additions=None, changes=None, refresh_manifests=False, removals=()):
        with zipfile.ZipFile(archive) as bundle:
            content = {name: bundle.read(name) for name in bundle.namelist()}
        content.update(additions or {})
        content.update(changes or {})
        for name in removals:
            content.pop(name)
        if refresh_manifests:
            prefix = archive.name.split("-v", 1)[0] + "/"
            provenance_name = prefix + "provenance.json"
            checksums_name = prefix + "SHA256SUMS"
            provenance = json.loads(content[provenance_name])
            provenance["files"] = {name[len(prefix):]: {"sha256": hashlib.sha256(value).hexdigest(), "size": len(value)}
                                   for name, value in content.items() if name not in (provenance_name, checksums_name)}
            content[provenance_name] = json.dumps(provenance).encode()
            content[checksums_name] = "".join(
                f"{hashlib.sha256(value).hexdigest()}  {name[len(prefix):]}\n"
                for name, value in sorted(content.items()) if name != checksums_name
            ).encode()
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
        self.rewrite_archive(archive, changes={f"{self.artifact}/bin/runtime_native_smoke": ELF + b"changed"})
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
        archive = self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")
        with zipfile.ZipFile(archive) as bundle:
            value = json.loads(bundle.read("nexus-stm32f407-baremetal/provenance.json"))
        value["cmake"]["NEXUS_PLATFORM"] = "native"
        self.rewrite_archive(archive, changes={"nexus-stm32f407-baremetal/provenance.json": json.dumps(value).encode()})
        with self.assertRaisesRegex(release.ReleaseError, "platform"):
            self.verify(expected=["nexus-stm32f407-baremetal=stm32-armgcc-release"])

    def test_configuration_fragment_is_input_and_legacy_header_cache_is_ignored(self):
        self.add_output()
        # This small fragment omits Release, backend, board and resource defaults.
        # Those identities must come from the generated bundle.
        cache = self.build / "CMakeCache.txt"
        cache.write_text(cache.read_text() + "NEXUS_CONFIG_HEADER:FILEPATH=/stale/root/nexus_config.h\n")
        archive = self.package()
        with zipfile.ZipFile(archive) as bundle:
            effective = bundle.read(f"{self.artifact}/configuration/effective.config")
            fragment = bundle.read(f"{self.artifact}/configuration/input.fragment")
            self.assertIn(b'CONFIG_BUILD_TYPE="Release"', effective)
            self.assertNotIn(b"CONFIG_BUILD_TYPE", fragment)
            self.assertEqual(fragment, self.fragment.read_bytes())
        self.verify()

    def test_configuration_consumers_must_agree_for_each_effective_symbol(self):
        self.add_output()
        for name, before, after in (
            ("nexus_config.h", "NX_CONFIG_TEST_BUDGET 4096", "NX_CONFIG_TEST_BUDGET 8192"),
            ("config.cmake", "CONFIG_TEST_BUDGET [=[4096]=]", "CONFIG_TEST_BUDGET [=[8192]=]"),
            ("effective.config", "CONFIG_TEST_BUDGET=4096", "CONFIG_TEST_BUDGET=8192"),
        ):
            self.write_bundle()
            path = self.build / "generated" / name
            path.write_text(path.read_text().replace(before, after))
            with self.subTest(name=name), self.assertRaisesRegex(release.ReleaseError, "disagree"):
                self.package()
        self.assertFalse((self.source / "release").exists())

    def test_generated_configuration_rejects_duplicate_malformed_and_unrecorded_symbols(self):
        self.add_output()
        for name, addition in (
            ("effective.config", "CONFIG_TEST_BUDGET=4096\n"),
            ("effective.config", "not a configuration\n"),
            ("config.cmake", "set(CONFIG_TEST_BUDGET [=[4096]=])\n"),
            ("config.cmake", "set(CONFIG_UNRECORDED [=[ON]=])\n"),
            ("nexus_config.h", "#define NX_CONFIG_TEST_BUDGET 4096\n"),
        ):
            self.write_bundle()
            path = self.build / "generated" / name
            path.write_text(path.read_text() + addition)
            with self.subTest(name=name, addition=addition), self.assertRaises(release.ReleaseError):
                self.package()

    def test_generated_helpers_cannot_override_effective_symbols(self):
        self.add_output()
        header = self.build / "generated/nexus_config.h"
        header.write_text(header.read_text() + "/* Peripheral Instance Traversal Macros */\n"
                          "#define NX_CONFIG_BUILD_TYPE_RELEASE 0\n")
        with self.assertRaisesRegex(release.ReleaseError, "redefine"):
            self.package()

    def test_escaped_configuration_strings_are_data_not_executed_cmake(self):
        self.values["CONFIG_MANIFEST_LABEL"] = 'line ]=] "quoted"; ${ENV_VAR} \\ label'
        self.write_bundle()
        self.add_output()
        self.package()
        self.verify()

    def test_effective_mode_and_cmake_option_conflicts_fail_even_with_release_cache(self):
        self.add_output()
        original = self.values.copy()
        for changes in (
            {"CONFIG_BUILD_TYPE": "Debug", "CONFIG_BUILD_TYPE_RELEASE": False, "CONFIG_BUILD_TYPE_DEBUG": True},
            {"CONFIG_BUILD_TESTS": False}, {"CONFIG_ENABLE_SANITIZERS": True},
        ):
            self.values = original | changes
            self.write_bundle()
            with self.subTest(changes=changes), self.assertRaises(release.ReleaseError):
                self.package()

    def test_arm_chip_board_backend_and_toolchain_file_must_match_profile(self):
        self.arm_fixture()
        self.add_output()
        original = self.values.copy()
        for key, value in (("CONFIG_BOARD_NAME", "another-board"), ("CONFIG_STM32_CHIP_NAME", "STM32F429xx"),
                           ("CONFIG_OSAL_BACKEND_NAME", "freertos")):
            self.values = original | {key: value}
            self.write_bundle()
            with self.subTest(key=key), self.assertRaisesRegex(release.ReleaseError, "Effective"):
                self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")
        self.values = original
        self.write_bundle()
        cache = self.build / "CMakeCache.txt"
        cache.write_text(cache.read_text().replace("arm-gcc.cmake", "armclang.cmake"))
        with self.assertRaisesRegex(release.ReleaseError, "toolchain file"):
            self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")

    def test_freertos_candidate_has_distinct_rtos_and_support_profile_identity(self):
        self.arm_fixture("freertos")
        self.add_output()
        archive = self.package(preset="stm32-armgcc-freertos-release", artifact="nexus-stm32f407-freertos")
        with zipfile.ZipFile(archive) as bundle:
            provenance = json.loads(bundle.read("nexus-stm32f407-freertos/provenance.json"))
            self.assertEqual(provenance["target"]["osal"], "freertos")
            self.assertEqual(provenance["target"]["support_profile"], "stm32f407-discovery-freertos")
            self.assertEqual(provenance["validation"]["kind"], "arm-static-link-contract")
            self.assertIs(provenance["target"]["hardware_verified"], False)
            self.assertIn("ext/freertos", {m["path"] for m in provenance["submodules"]})
        self.verify(expected=["nexus-stm32f407-freertos=stm32-armgcc-freertos-release"])

    def test_unmaintained_host_release_profiles_are_rejected(self):
        self.add_output()
        for preset, artifact in (("windows-msvc-release", "nexus-windows-msvc"),
                                 ("macos-clang-release", "nexus-macos-clang")):
            with self.subTest(preset=preset), self.assertRaisesRegex(release.ReleaseError, "Unsupported"):
                self.package(preset=preset, artifact=artifact)

    def test_compiled_elf_architecture_must_match_effective_target(self):
        self.add_output(content=elf(40, 1))
        with self.assertRaisesRegex(release.ReleaseError, "ELF architecture"):
            self.package()
        self.arm_fixture()
        self.add_output(content=ELF)
        with self.assertRaisesRegex(release.ReleaseError, "ELF architecture"):
            self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")

    def test_compile_database_must_consume_generated_configuration_and_arm_flags(self):
        native_output = self.add_output()
        commands = self.build / "compile_commands.json"
        commands.write_text(commands.read_text().replace(str(self.build / "generated"), str(self.source)))
        with self.assertRaisesRegex(release.ReleaseError, "generated configuration"):
            self.package()
        native_output.unlink()
        self.arm_fixture()
        self.add_output()
        commands.write_text(commands.read_text().replace('"-mfloat-abi=hard", ', ""))
        with self.assertRaisesRegex(release.ReleaseError, "ARM target flags"):
            self.package(preset="stm32-armgcc-release", artifact="nexus-stm32f407-baremetal")

    def test_native_requires_nonzero_successful_test_report(self):
        self.add_output()
        for contents in (
            '<testsuite tests="0"/>',
            '<testsuite tests="1" failures="1"><testcase status="fail"><failure/></testcase></testsuite>',
            '<testsuite tests="1"><testcase status="run"><error/></testcase></testsuite>',
            '<testsuite tests="1"><testcase name="skip" status="notrun"><skipped/></testcase></testsuite>',
            '<testsuite tests="9"><testcase name="pass" status="run"/></testsuite>',
            '<testsuite tests="invalid"><testcase name="pass" status="run"/></testsuite>',
            'invalid XML',
        ):
            self.write_test_report(contents)
            with self.subTest(contents=contents), self.assertRaises(release.ReleaseError):
                self.package()
        (self.build / "ctest-results.xml").unlink()
        with self.assertRaises(release.ReleaseError):
            self.package()

    def test_test_skips_are_reported_without_inventing_execution(self):
        self.write_test_report('<testsuite tests="2" failures="0" skipped="1"><testcase name="pass" status="run"/>'
                               '<testcase name="skip" status="notrun"><skipped/></testcase></testsuite>')
        self.add_output()
        archive = self.package()
        with zipfile.ZipFile(archive) as bundle:
            validation = json.loads(bundle.read(f"{self.artifact}/provenance.json"))["validation"]
            self.assertEqual((validation["executed"], validation["skipped"]), (1, 1))
        self.verify()
        self.assertIn("1 host tests passed, 1 skipped", (self.source / "release_notes.md").read_text())

    def test_native_candidate_requires_each_sdk_dependency_used_by_its_contracts(self):
        self.add_output()
        for dependency in ("vendors/arm/CMSIS_5", "vendors/st/cmsis_device_f4", "vendors/st/stm32f4xx_hal_driver"):
            self.git("submodule", "deinit", "--force", dependency)
            with self.subTest(dependency=dependency), self.assertRaisesRegex(release.ReleaseError, "initialized"):
                self.package()
            self.git("-c", "protocol.file.allow=always", "submodule", "update", "--init", dependency)
        self.package()
        self.verify()

    def test_missing_required_dependency_fails_but_unused_sdk_does_not_block(self):
        self.add_dependency("vendors/unused-sdk")
        self.git("commit", "--quiet", "-am", "Unused SDK fixture")
        self.git("tag", "-f", "v1.2.3")
        self.commit = self.git("rev-parse", "HEAD")
        self.git("submodule", "deinit", "--force", "vendors/unused-sdk")
        self.add_output()
        archive = self.package()
        self.verify()
        archive.unlink()
        archive.with_suffix(".zip.sha256").unlink()
        self.git("submodule", "deinit", "--force", "ext/freertos")
        with self.assertRaisesRegex(release.ReleaseError, "initialized"):
            self.package()

    def test_bundle_verifier_checks_configuration_after_transfer_manifests_are_recomputed(self):
        self.add_output()
        archive = self.package()
        with zipfile.ZipFile(archive) as bundle:
            name = f"{self.artifact}/configuration/nexus_config.h"
            changed = bundle.read(name).replace(b"NX_CONFIG_TEST_BUDGET 4096", b"NX_CONFIG_TEST_BUDGET 8192")
        self.rewrite_archive(archive, changes={name: changed}, refresh_manifests=True)
        with self.assertRaisesRegex(release.ReleaseError, "header disagree"):
            self.verify()

    def test_bundle_verifier_rejects_hardware_claim_and_false_test_identity(self):
        self.add_output()
        archive = self.package()
        name = f"{self.artifact}/provenance.json"
        with zipfile.ZipFile(archive) as bundle:
            original = json.loads(bundle.read(name))
        for field in ("hardware", "tests"):
            value = json.loads(json.dumps(original))
            if field == "hardware":
                value["target"]["hardware_verified"] = True
            else:
                value["validation"]["executed"] = 9999
            self.rewrite_archive(archive, changes={name: json.dumps(value).encode()}, refresh_manifests=True)
            with self.subTest(field=field), self.assertRaisesRegex(release.ReleaseError, "identity"):
                self.verify()

    def test_bundle_verifier_binds_dependencies_to_the_source_gitlinks(self):
        self.add_output()
        archive = self.package()
        name = f"{self.artifact}/provenance.json"
        with zipfile.ZipFile(archive) as bundle:
            value = json.loads(bundle.read(name))
        value["submodules"][0]["commit"] = "0" * 40
        self.rewrite_archive(archive, changes={name: json.dumps(value).encode()}, refresh_manifests=True)
        with self.assertRaisesRegex(release.ReleaseError, "dependency identity"):
            self.verify()

    def add_import(self):
        root = self.source / "vendors/gigadevice/gd32f4xx"
        (root / "Firmware").mkdir(parents=True, exist_ok=True)
        (root / "LICENSES").mkdir(exist_ok=True)
        data = b"/* SDK identity fixture, not real vendor code. */\n"
        (root / "Firmware/sdk.c").write_bytes(data)
        (root / "README.md").write_text("Reviewed source import origin fixture\n")
        (root / "LICENSES/BSD-3-Clause.txt").write_text("License notice fixture\n")
        lock = {"vendor": "GigaDevice", "package": "GD32F4xx Firmware Library", "version": "3.3.3",
                "source_url": "https://example.invalid/sdk.zip", "download_sha256": "a" * 64,
                "nested_archive_sha256": "b" * 64,
                "files": [{"path": "Firmware/sdk.c", "sha256": hashlib.sha256(data).hexdigest(),
                           "bytes": len(data), "license": "BSD-3-Clause"}]}
        (root / "source.lock.json").write_text(json.dumps(lock))
        return root

    def all_profiles_fixture(self):
        self.arm_fixture()
        root = self.add_import()
        for profile in release.RELEASE_PROFILES.values():
            fragment = self.source / profile["fragment"]
            fragment.parent.mkdir(parents=True, exist_ok=True)
            if not fragment.exists():
                fragment.write_text("# Platform profile fixture\n")
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "Reviewed profile and imported SDK fixture")
        self.git("tag", "-f", "v1.2.3")
        self.commit = self.git("rev-parse", "HEAD")
        return root

    def select_profile(self, preset):
        profile = release.RELEASE_PROFILES[preset]
        self.preset, self.artifact = preset, profile["artifact"]
        self.fragment = self.source / profile["fragment"]
        self.build = self.source / "build" / preset
        self.build.mkdir(exist_ok=True)
        self.values = {"CONFIG_BUILD_TYPE": "Release", "CONFIG_BUILD_TYPE_RELEASE": True,
                       "CONFIG_BUILD_TESTS": True, "CONFIG_BUILD_CONTRACTS": True,
                       "CONFIG_ENABLE_COVERAGE": False, "CONFIG_ENABLE_SANITIZERS": False,
                       "CONFIG_PLATFORM_NAME": "native", "CONFIG_PLATFORM_NATIVE": True,
                       "CONFIG_BOARD_NAME": "native-reference", "CONFIG_OSAL_BACKEND_NAME": "native",
                       "CONFIG_TOOLCHAIN_NAME": "gcc", "CONFIG_TEST_BUDGET": 4096}
        self.elf = ELF
        self.write_bundle(); self.write_cache(); self.write_compile_commands(); self.write_test_report()
        self.write_board_bundle()
        if profile["platform"] != "native":
            self.configure_arm_bundle(self.source / "cmake/toolchains/arm-gcc.cmake", profile["osal"], profile)
        return profile

    def test_complete_maintained_matrix_uses_one_source_and_verified_distinct_candidates(self):
        self.all_profiles_fixture()
        expected = []
        for preset in release.RELEASE_PROFILES:
            self.select_profile(preset)
            self.add_output()
            self.package()
            expected.append(f"{self.artifact}={preset}")
        self.verify(expected=expected)
        sums = (self.source / "release/SHA256SUMS").read_text().splitlines()
        self.assertEqual(len(sums), len(release.RELEASE_PROFILES))
        notes = (self.source / "release_notes.md").read_text()
        for profile in release.RELEASE_PROFILES.values():
            self.assertIn(profile["artifact"], notes)
        self.assertEqual(notes.count("ARM static link contract passed; HIL pending"), 8)

    def test_gd32_import_is_recorded_as_source_lock_and_notices_not_vendor_git(self):
        self.all_profiles_fixture()
        self.select_profile("gd32f470-armgcc-baremetal-release")
        self.add_output()
        archive = self.package()
        with zipfile.ZipFile(archive) as bundle:
            provenance = json.loads(bundle.read(f"{self.artifact}/provenance.json"))
            self.assertEqual(provenance["submodules"], [])
            record = provenance["imported_sources"][0]
            self.assertEqual(record["kind"], "vendor-source-import")
            self.assertEqual(record["upstream"]["version"], "3.3.3")
            self.assertNotIn("commit", record)
            self.assertIn(f"{self.artifact}/configuration/imports/{record['path']}/LICENSES/BSD-3-Clause.txt", bundle.namelist())
        self.verify()

    def test_new_board_part_and_capacity_must_match_reviewed_profile(self):
        self.all_profiles_fixture()
        for preset in ("stm32-qiming-armgcc-baremetal-release", "stm32-sky-armgcc-freertos-release"):
            self.select_profile(preset); self.add_output()
            original = self.values.copy()
            for key, value in (("CONFIG_BOARD_NAME", "wrong-board"),
                               ("CONFIG_STM32_PART_NAME", "STM32F407VGT6"),
                               ("CONFIG_STM32_FLASH_SIZE", 0x200000)):
                self.values = original | {key: value}; self.write_bundle()
                with self.subTest(preset=preset, key=key), self.assertRaisesRegex(release.ReleaseError, "Effective"):
                    self.package()

    def test_all_arm_platforms_reject_flag_override_including_arch_and_runtime_units(self):
        self.all_profiles_fixture()
        self.select_profile("gd32f470-armgcc-baremetal-release"); self.add_output()
        for directory in ("arch", "runtime"):
            file = self.source / directory / "source.c"; file.parent.mkdir(exist_ok=True); file.write_text("int fixture;\n")
            # Identity is already committed before packaging; only command verification is needed.
            for override in ("-mcpu=cortex-m7", "-mfpu=fpv5-d16", "-mfloat-abi=soft", "-marm", "@hidden.rsp"):
                commands = [{"file": str(file), "arguments": ["arm-none-eabi-gcc", f"-I{self.build / 'generated'}",
                    "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", override]}]
                with self.subTest(directory=directory, override=override), self.assertRaisesRegex(release.ReleaseError, "ARM"):
                    release.validate_compile_commands(json.dumps(commands), release.RELEASE_PROFILES[self.preset], self.build / "generated", self.source)

    def test_import_firmware_notice_and_lock_tamper_fail_before_packaging(self):
        root = self.all_profiles_fixture()
        self.select_profile("gd32f470-armgcc-baremetal-release"); self.add_output()
        for relative in ("Firmware/sdk.c", "source.lock.json", "LICENSES/BSD-3-Clause.txt"):
            file = root / relative; original = file.read_bytes(); file.write_bytes(original + b"tampered")
            with self.subTest(relative=relative), self.assertRaisesRegex(release.ReleaseError, "import"):
                self.package()
            file.write_bytes(original)
        (root / "Firmware/extra.c").write_text("unreviewed")
        with self.assertRaisesRegex(release.ReleaseError, "import"):
            self.package()

    def test_archive_verifier_rejects_refreshed_manifest_import_metadata_forgery(self):
        self.all_profiles_fixture(); self.select_profile("gd32f470-armgcc-baremetal-release")
        self.add_output(); archive = self.package()
        name = f"{self.artifact}/provenance.json"
        with zipfile.ZipFile(archive) as bundle:
            provenance = json.loads(bundle.read(name))
        provenance["imported_sources"][0]["upstream"]["version"] = "unreviewed"
        self.rewrite_archive(archive, changes={name: json.dumps(provenance).encode()}, refresh_manifests=True)
        with self.assertRaisesRegex(release.ReleaseError, "import"):
            self.verify()

    def test_unknown_new_board_profile_still_fails_closed(self):
        self.add_output()
        with self.assertRaisesRegex(release.ReleaseError, "Unsupported"):
            self.package(preset="stm32-unknown-armgcc-release", artifact="nexus-unknown")


@unittest.skipUnless(os.environ.get("NEXUS_RELEASE_TEST_BUILD"),
                     "actual compile database requires NEXUS_RELEASE_TEST_BUILD")
class ActualReleaseCompileDatabaseTests(unittest.TestCase):
    """Read a real maintained build; no toy report substitutes for its commands."""

    def test_actual_effective_build_and_compile_database(self):
        build = Path(os.environ["NEXUS_RELEASE_TEST_BUILD"]).resolve()
        preset = os.environ.get("NEXUS_RELEASE_TEST_PRESET", "linux-gcc-release")
        profile = release.RELEASE_PROFILES[preset]
        cache = release.cmake_cache(build / "CMakeCache.txt")
        contents = {name: (build / "generated" / name).read_text() for name in release.CONFIGURATION_FILES}
        config = release.validate_configuration_bundle(contents)
        release.validate_effective_build(cache, profile, "Release", config)
        commands = (build / "compile_commands.json").read_text()
        generated = build / "generated"
        release.validate_compile_commands(commands, profile, generated, Path(cache["CMAKE_HOME_DIRECTORY"]))
        paths, board_identity, layout = release.board_configuration(
            Path(cache["CMAKE_HOME_DIRECTORY"]), build, config, profile, cache)
        self.assertGreater(len(paths), 0)
        outputs = release.build_outputs(build, Path(cache["CMAKE_HOME_DIRECTORY"]), "Release")
        release.validate_target_outputs(((name, file.read_bytes()[:64]) for name, file in outputs), profile)
        if profile["platform"] == "native":
            self.assertIsNone(layout)
            self.assertEqual(board_identity["id"], "native-reference")
            result = release.test_report((build / "ctest-results.xml").read_bytes())
            self.assertGreater(result["executed"], 0)
            nested_kernel = [entry for entry in json.loads(commands)
                             if entry["file"].endswith("/tests/osal/freertos_runtime/wait_for_event.c")]
            self.assertGreater(len(nested_kernel), 0)
            without_generated = 0
            for entry in nested_kernel:
                arguments = entry.get("arguments")
                if arguments is None:
                    import shlex
                    arguments = shlex.split(entry["command"])
                without_generated += f"-I{generated}" not in arguments
            self.assertGreater(without_generated, 0)
        else:
            images = [(file.name, file.read_bytes()) for file in sorted((build / "bin").glob("*.elf"))]
            result = release.validate_arm_artifacts(config, contents["effective.config"].encode(), images,
                                                   layout=layout, board_identity=board_identity)
            self.assertGreater(len(result["images"]), 0)
            self.assertFalse(result["hardware_verified"])


if __name__ == "__main__":
    unittest.main()
