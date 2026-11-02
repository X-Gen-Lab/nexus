"""Secure source SDK consumers extend one exact reviewed policy."""

from dataclasses import replace
import hashlib
import importlib.util
import json
import shutil
import subprocess
import tempfile
from pathlib import Path
import unittest

from tools.release import transport
from tools.configure import configure
from tools.evidence.common import file_identity

ROOT = Path(__file__).resolve().parents[2]
V12_LAYOUT_SHA256 = (
    "d2bfea74fe3e8eb8e95da84b60d9a6d6076a216d48883e9f4d82ecd65a72ff05")


class SDKSecurePolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        specification = importlib.util.spec_from_file_location(
            "sdk_secure_consumers", ROOT / "cmake/package/verify_consumers.py")
        cls.consumers = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(cls.consumers)

    def test_v13_preserves_the_complete_v12_layout_prefix(self):
        self.assertEqual(transport.CONSUMER_POLICY, "nexus-sdk-consumers-v13")
        layout = transport.consumer_layout()
        encoded = json.dumps(layout[:54], separators=(",", ":")).encode()
        self.assertEqual(hashlib.sha256(encoded).hexdigest(), V12_LAYOUT_SHA256)
        self.assertEqual(len(layout), 56)
        self.assertEqual(len(transport.IMAGES), 59)
        self.assertEqual(len(transport.FIXTURES), 56)

    def test_secure_profiles_have_explicit_reviewed_execution_domains(self):
        profiles = transport.SERVICE_CONSUMERS[-2:]
        self.assertEqual([item.name for item in profiles],
                         ["native secure services", "f407 secure services"])
        self.assertEqual([item.domain for item in profiles],
                         ["native-secure", "arm-secure"])
        self.assertEqual([item.arm for item in profiles], [False, True])
        self.assertEqual([item.scenario for item in profiles],
                         ["secure_services", "secure_services"])
        self.assertEqual(transport.consumer_plan()[-2:],
                         tuple(("services", item) for item in profiles))
        for profile in profiles:
            changed = replace(profile, scenario="unreviewed")
            with self.assertRaisesRegex(ValueError, "canonical|reviewed"):
                self.consumers.service_profile(changed)

    def test_missing_secure_image_is_not_a_complete_v13_transport(self):
        rows = transport.consumer_layout()
        files = {image: {"size": 1} for _, _, images in rows for image in images}
        directories = [name for source, build, _ in rows
                       for name in (source, build, build + "/bin")]
        expected = "native secure services build/bin/installed_services"
        self.assertIn(expected, files)
        transport.consumer_images(files, directories)
        del files[expected]
        with self.assertRaisesRegex(ValueError, "roots and images"):
            transport.consumer_images(files, directories)


    def prefix(self, base):
        prefix = base / "prefix"
        sdk = prefix / "share/nexus/src"
        templates = sdk / "cmake/package/verification/secure_services"
        shutil.copytree(ROOT / "cmake/package/verification/secure_services",
                        templates)
        (sdk / "dependencies").mkdir()
        shutil.copyfile(ROOT / "dependencies/platform-compatibility.json",
                        sdk / "dependencies/platform-compatibility.json")
        return prefix, sdk

    @staticmethod
    def execute(argv):
        result = subprocess.run(argv, capture_output=True, text=True,
                                check=False, timeout=30)
        if result.returncode:
            raise ValueError(result.stdout + result.stderr)
        return result

    def test_preparation_preserves_public_and_composition_boundaries(self):
        for profile in transport.SERVICE_CONSUMERS[-2:]:
            with self.subTest(profile=profile.name), tempfile.TemporaryDirectory() as tmp:
                base = Path(tmp)
                prefix, _ = self.prefix(base)
                source, _, command = self.consumers.prepare_service_consumer(
                    prefix, base, profile, self.execute,
                    expected_revision="a" * 40, fixture=True)
                cmake = (source / "CMakeLists.txt").read_text()
                self.assertIn("find_package(Nexus 0.2.1 EXACT", cmake)
                self.assertIn("nexus_add_tls()", cmake)
                self.assertIn("nexus_add_coremqtt()", cmake)
                self.assertIn("Nexus::TLSStorage", cmake)
                self.assertIn("Nexus::CoreMQTTStorage", cmake)
                self.assertIn("application.c private_headers.c", cmake)
                self.assertIn("SOURCES composition.c", cmake)
                self.assertNotIn("CMAKE_PREFIX_PATH", " ".join(command))
                self.assertIn("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON", command)
                self.assertIn("-DNexus_DIR=" + str(prefix / "lib/cmake/Nexus"), command)
                for source_file in source.glob("*.c"):
                    self.assertNotIn("test_key", source_file.read_text())
                    self.assertNotIn("test_client", source_file.read_text())
                resolved = configure.resolve_ir(source / "assembly.toml", root=ROOT)
                self.assertEqual(resolved.cpu["arch"],
                                 "cortex-m4" if profile.arm else "native")
                if profile.arm:
                    self.assertIn("nexus_add_stm32f407_entropy", cmake)
                    self.assertEqual(resolved.memory_budget.static_ram_limit_bytes,
                                     114688)
                    self.assertEqual(resolved.memory_budget.flash_load_limit_bytes,
                                     524288)

    def image_fixture(self, base, arm=False):
        profile = transport.SERVICE_CONSUMERS[-1 if arm else -2]
        prefix, sdk = self.prefix(base)
        source, build, _ = self.consumers.prepare_service_consumer(
            prefix, base, profile, self.execute,
            expected_revision="a" * 40, fixture=True)
        (sdk / ".nexus-source-sdk.json").write_text("{}\n")
        config = build / "nexus-sdk/generated"
        config.mkdir(parents=True)
        (build / "resolved-sdk.txt").write_text(str(sdk))
        (build / "resolved-config.txt").write_text(str(config))
        (config / "bindings.c").write_text("/* external compiler fixture */\n")
        resolved = configure.resolve_ir(source / "assembly.toml", root=ROOT)
        (config / "resolved.json").write_text(json.dumps(resolved.to_dict()))
        flags = ["-std=c11"]
        if arm:
            flags += ["-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16",
                      "-mfloat-abi=hard", "-fshort-enums"]
        rows = [{"directory": str(build), "file": str(path),
                 "arguments": ["arm-none-eabi-gcc" if arm else "cc", *flags,
                               "-c", str(path)]}
                for path in (source / "application.c", source / "private_headers.c",
                             source / "composition.c", config / "bindings.c")]
        (build / "compile_commands.json").write_text(json.dumps(rows))
        binary = build / "bin"
        binary.mkdir()
        image = binary / ("installed_services.elf" if arm else "installed_services")
        header = bytearray(64)
        header[:6] = b"\x7fELF\x01\x01" if arm else b"\x7fELF\x02\x01"
        header[18:20] = (40 if arm else 62).to_bytes(2, "little")
        image.write_bytes(header)
        if arm:
            (binary / "installed_services.resources.json").write_text(json.dumps({
                "status": "passed", "violations": [], "elf": file_identity(image),
                "resolved": file_identity(config / "resolved.json")}))
        calls = []

        def external(argv):
            calls.append(list(argv))
            if any(str(item).endswith("retention.py") for item in argv):
                Path(argv[argv.index("--output") + 1]).write_text(json.dumps({
                    "status": "passed", "elf": file_identity(image),
                    "kind": "linked_component_owner_retention",
                    "owner_profile": "f407-secure-services",
                    "consumer": file_identity(source / "application.c")}))
            symbols = self.consumers.secure_service_symbols(profile)
            return subprocess.CompletedProcess(argv, 0,
                "".join(name + " T 1000 10\n" for name in sorted(symbols)), "")
        return sdk, source, build, profile, calls, external

    def test_secure_checker_distinguishes_native_execution_and_arm_link(self):
        for arm in (False, True):
            with self.subTest(arm=arm), tempfile.TemporaryDirectory() as tmp:
                sdk, source, build, profile, calls, execute = self.image_fixture(Path(tmp), arm)
                report = self.consumers.check_service_image(sdk, source, build,
                                                           profile, execute)
                self.assertEqual(report["runtime_executed"], not arm)
                self.assertEqual(report["domain"], profile.domain)
                self.assertEqual(report["physical_status"], "not_executed")
                self.assertIn("psa_crypto_init", report["symbols"])
                self.assertIn("MQTT_Init", report["symbols"])
                self.assertIn("nx_entropy_fill", report["symbols"])
                if arm:
                    self.assertIn("nx_stm32f407_entropy_instance_start", report["symbols"])
                    owners = [call for call in calls if "--component-owner" in call]
                    self.assertEqual(len(owners), 1)
                    self.assertIn("f407-secure-services", owners[0])
                    self.assertEqual(owners[0][owners[0].index("--consumer") + 1],
                                     str(source / "application.c"))
                self.assertEqual(any(len(call) == 1 for call in calls), not arm)

    def test_secure_checker_rejects_c99_or_conflicting_mcu_abi(self):
        for fault in ("-std=c99", "-mfloat-abi=soft", "-fno-short-enums"):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as tmp:
                sdk, source, build, profile, calls, execute = self.image_fixture(Path(tmp), True)
                database = build / "compile_commands.json"
                rows = json.loads(database.read_text())
                rows[-1]["arguments"].insert(1, fault)
                database.write_text(json.dumps(rows))
                with self.assertRaisesRegex(ValueError, "C11|ABI"):
                    self.consumers.check_service_image(sdk, source, build,
                                                       profile, execute)
                self.assertEqual(calls, [])

    def test_secure_checker_rejects_composition_mutation_and_vendor_header_leak(self):
        with tempfile.TemporaryDirectory() as tmp:
            sdk, source, build, profile, calls, execute = self.image_fixture(Path(tmp))
            path = source / "composition.c"
            original = path.read_text()
            path.write_text(original + "\n/* changed */\n")
            with self.assertRaisesRegex(ValueError, "canonical scenario"):
                self.consumers.check_service_image(sdk, source, build, profile, execute)
            path.write_text(original)
            database = build / "compile_commands.json"
            rows = json.loads(database.read_text())
            rows[0]["arguments"].insert(1, "-I" + str(sdk / "ext/mbedtls/include"))
            database.write_text(json.dumps(rows))
            with self.assertRaisesRegex(ValueError, "private inputs"):
                self.consumers.check_service_image(sdk, source, build, profile, execute)
            self.assertEqual(calls, [])


    def test_secure_checker_requires_one_real_composition_and_no_cpp(self):
        for fault in ("missing_composition", "cpp"):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as tmp:
                sdk, source, build, profile, calls, execute = self.image_fixture(Path(tmp))
                database = build / "compile_commands.json"
                rows = json.loads(database.read_text())
                if fault == "missing_composition":
                    rows = [row for row in rows
                            if row["file"] != str(source / "composition.c")]
                else:
                    rows.append({"file": str(sdk / "components/tls/src/stray.cpp"),
                                 "arguments": ["c++", "-std=c++17", "-c", "stray.cpp"]})
                database.write_text(json.dumps(rows))
                with self.assertRaisesRegex(ValueError, "composition|C11"):
                    self.consumers.check_service_image(sdk, source, build,
                                                       profile, execute)
                self.assertEqual(calls, [])


    def test_secure_arm_owner_is_explicit_and_heap_cpp_bodies_are_rejected(self):
        for forbidden in ("malloc", "_sbrk_r", "__cxa_guard_acquire", "_Znwm"):
            with self.subTest(symbol=forbidden), tempfile.TemporaryDirectory() as tmp:
                sdk, source, build, profile, calls, execute = self.image_fixture(Path(tmp), True)
                def forbidden_body(argv):
                    result = execute(argv)
                    if str(argv[0]) == "arm-none-eabi-nm":
                        result.stdout += forbidden + " T 2000 10\n"
                    return result
                with self.assertRaisesRegex(ValueError, r"heap|C\+\+"):
                    self.consumers.check_service_image(
                        sdk, source, build, profile, forbidden_body)


if __name__ == "__main__":
    unittest.main()
