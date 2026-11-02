"""New SDK service profiles execute explicit, bounded consumer contracts."""

from dataclasses import FrozenInstanceError, replace
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.configure import configure
from tools.evidence.common import file_identity
from tools.release import transport


ROOT = Path(__file__).resolve().parents[2]
LEGACY_LAYOUT_SHA256 = (
    "f8ffdf097e5379d76b49b97caae526b9d9f25a006407cf8a9f5e72a344ed2e99")


class SDKServicesPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        specification = importlib.util.spec_from_file_location(
            "sdk_services_consumers", ROOT / "cmake/package/verify_consumers.py")
        cls.consumers = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(cls.consumers)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="SDK services ")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def prefix(self):
        # This is a preparation fixture, not an installed or qualified SDK.
        prefix = self.directory / "prefix"
        sdk = prefix / "share/nexus/src"
        destination = sdk / "cmake/package/verification/common_services"
        shutil.copytree(ROOT / "cmake/package/verification/common_services",
                        destination)
        (sdk / "dependencies").mkdir()
        shutil.copyfile(ROOT / "dependencies/platform-compatibility.json",
                        sdk / "dependencies/platform-compatibility.json")
        return prefix

    def command(self, argv):
        execution = subprocess.run(argv, capture_output=True, text=True,
                                   check=False, timeout=30)
        self.assertEqual(execution.returncode, 0,
                         execution.stdout + execution.stderr)
        return execution

    def test_v12_retains_the_complete_legacy_layout_prefix(self):
        self.assertEqual(transport.CONSUMER_POLICY, "nexus-sdk-consumers-v12")
        layout = transport.consumer_layout()
        encoded = json.dumps(layout[:50], separators=(",", ":")).encode()
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         LEGACY_LAYOUT_SHA256)
        self.assertEqual(len(layout), 54)
        self.assertEqual(len(transport.IMAGES), 57)
        self.assertEqual(len(transport.FIXTURES), 54)

    def test_service_profiles_are_exact_frozen_plan_entries(self):
        profiles = transport.SERVICE_CONSUMERS
        self.assertEqual([item.name for item in profiles], [
            "native services", "external services", "f407 services",
            "gd470 services"])
        self.assertEqual([item.domain for item in profiles], [
            "native-services", "external-package", "arm-services",
            "arm-services"])
        self.assertEqual([item.arm for item in profiles],
                         [False, False, True, True])
        with self.assertRaises(FrozenInstanceError):
            profiles[0].name = "changed"
        plan = transport.consumer_plan()
        self.assertEqual(plan[-4:], tuple(("services", item)
                                          for item in profiles))
        changed = replace(profiles[0], domain="arm-services")
        with self.assertRaisesRegex(ValueError, "canonical|reviewed"):
            self.consumers.service_profile(changed)

    def test_transport_requires_new_images_without_guessing_name_prefixes(self):
        rows = transport.consumer_layout()
        files = {image: {"size": 1} for _, _, images in rows for image in images}
        directories = [name for source, build, _ in rows
                       for name in (source, build, build + "/bin")]
        transport.consumer_images(files, directories)
        self.assertIn("external services build/bin/installed_services", files)
        self.assertIn("f407 services build/bin/installed_services.elf", files)
        del files["external services build/bin/installed_services"]
        with self.assertRaisesRegex(ValueError, "roots and images"):
            transport.consumer_images(files, directories)

    def test_native_services_prepare_real_success_smoke_and_explicit_libraries(self):
        prefix = self.prefix()
        profile = transport.SERVICE_CONSUMERS[0]
        output = self.directory / "prepared"
        output.mkdir()
        source, build, command = self.consumers.prepare_service_consumer(
            prefix, output, profile, self.command,
            expected_revision="a" * 40, fixture=True)
        self.assertEqual(build.name, "native services build")
        self.assertIn("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON", command)
        cmake = (source / "CMakeLists.txt").read_text()
        for target in ("Nexus::Settings", "Nexus::OSWait", "Nexus::LwIP"):
            self.assertIn(target, cmake)
        self.assertIn("LANGUAGES C ASM", cmake)
        smoke = (source / "main.c").read_text()
        for call in ("nx_settings_commit", "nx_settings_open", "nx_wait_select",
                     "nx_lwip_adapter_start", "nx_lwip_adapter_service",
                     "nx_lwip_adapter_stop"):
            self.assertIn(call, smoke)
        resolved = configure.resolve_ir(source / "assembly.toml", root=ROOT)
        self.assertEqual(resolved.soc_family, "native")
        self.assertEqual(resolved.memory_budget.static_ram_limit_bytes, 65536)

    def test_external_package_is_copied_outside_sdk_and_relocates(self):
        prefix = self.prefix()
        output = self.directory / "a product"
        output.mkdir()
        source, _, _ = self.consumers.prepare_service_consumer(
            prefix, output, transport.SERVICE_CONSUMERS[1], self.command,
            expected_revision="a" * 40, fixture=True)
        original = configure.resolve_ir(source / "assembly.toml", root=ROOT)
        self.assertEqual(original.soc_family, "sdk_services_model")
        self.assertTrue(original.provider_context.provider.MODEL)
        self.assertTrue(original.provider_context.directory.is_relative_to(source))
        relocated = self.directory / "z relocated product"
        shutil.copytree(source, relocated)
        moved = configure.resolve_ir(relocated / "assembly.toml", root=ROOT)
        self.assertEqual(original.to_dict(), moved.to_dict())

    def test_unknown_profile_rejects_before_creating_a_consumer(self):
        prefix = self.prefix()
        output = self.directory / "prepared"
        output.mkdir()
        changed = replace(transport.SERVICE_CONSUMERS[0], name="unreviewed")
        with self.assertRaisesRegex(ValueError, "canonical|reviewed"):
            self.consumers.prepare_service_consumer(
                prefix, output, changed, self.command,
                expected_revision="a" * 40, fixture=True)
        self.assertEqual(list(output.iterdir()), [])

    def image_fixture(self, profile_index=0):
        # Model external compiler/runtime ports; actual SDK execution is separate.
        prefix = self.prefix()
        output = self.directory / "images"
        output.mkdir()
        profile = transport.SERVICE_CONSUMERS[profile_index]
        source, build, _ = self.consumers.prepare_service_consumer(
            prefix, output, profile, self.command,
            expected_revision="a" * 40, fixture=True)
        sdk = prefix / "share/nexus/src"
        (sdk / ".nexus-source-sdk.json").write_text("{}\n")
        config = build / "nexus-sdk/generated"
        config.mkdir(parents=True)
        (build / "resolved-sdk.txt").write_text(str(sdk))
        (build / "resolved-config.txt").write_text(str(config))
        (config / "bindings.c").write_text("/* compiler port fixture */\n")
        resolved = configure.resolve_ir(source / "assembly.toml", root=ROOT)
        (config / "resolved.json").write_text(json.dumps(resolved.to_dict()))
        rows = [{"directory": str(build), "file": str(path),
                 "arguments": ["cc", "-c", str(path)]}
                for path in (source / "main.c", source / "private_headers.c",
                             config / "bindings.c")]
        (build / "compile_commands.json").write_text(json.dumps(rows))
        binary = build / "bin"
        binary.mkdir()
        image = binary / ("installed_services" + (".elf" if profile.arm else ""))
        header = bytearray(64)
        header[:6] = b"\x7fELF\x01\x01" if profile.arm else b"\x7fELF\x02\x01"
        header[18:20] = (40 if profile.arm else 62).to_bytes(2, "little")
        image.write_bytes(header)
        if profile.arm:
            (binary / "installed_services.resources.json").write_text(json.dumps({
                "status": "passed", "violations": [],
                "elf": file_identity(image),
                "resolved": file_identity(config / "resolved.json")}))
        calls = []

        def external(argv):
            calls.append(list(argv))
            if any(str(item).endswith("retention.py") for item in argv):
                Path(argv[argv.index("--output") + 1]).write_text(json.dumps({
                    "status": "passed", "elf": file_identity(image)}))
            symbols = "".join(name + " T 1000 10\n"
                              for name in sorted(self.consumers.SERVICE_SYMBOLS))
            return subprocess.CompletedProcess(argv, 0, symbols, "")
        return sdk, source, build, config, profile, calls, external

    def test_service_checker_runs_only_native_and_binds_arm_resources(self):
        for index in (0, 2):
            with self.subTest(profile=index):
                fixture = self.image_fixture(index)
                sdk, source, build, _, profile, calls, external = fixture
                report = self.consumers.check_service_image(
                    sdk, source, build, profile, external)
                self.assertEqual(report["runtime_executed"], not profile.arm)
                self.assertEqual(report["domain"], profile.domain)
                self.assertEqual(set(report["symbols"]),
                                 self.consumers.SERVICE_SYMBOLS)
                self.assertEqual(any(len(call) == 1 for call in calls),
                                 not profile.arm)
                self.assertEqual(any(str(call[0]) == "arm-none-eabi-nm"
                                     for call in calls), profile.arm)
                shutil.rmtree(self.directory / "prefix")
                shutil.rmtree(self.directory / "images")

    def test_service_checker_rejects_changed_source_before_execution(self):
        sdk, source, build, _, profile, calls, external = self.image_fixture()
        (source / "main.c").write_text("int main(void) { return 0; }\n")
        with self.assertRaisesRegex(ValueError, "canonical scenario"):
            self.consumers.check_service_image(sdk, source, build, profile, external)
        self.assertEqual(calls, [])

    def test_service_checker_rejects_compile_operand_and_config_redirection(self):
        sdk, source, build, config, profile, calls, external = self.image_fixture()
        database = build / "compile_commands.json"
        original = database.read_text()
        entries = json.loads(original)
        entries[0]["arguments"] = ["cc", "-c", str(source / "another.c")]
        database.write_text(json.dumps(entries))
        with self.assertRaisesRegex(ValueError, "source and -c"):
            self.consumers.check_service_image(sdk, source, build, profile, external)
        database.write_text(original)
        redirected = build / "redirected"
        shutil.copytree(config, redirected)
        (build / "resolved-config.txt").write_text(str(redirected))
        with self.assertRaisesRegex(ValueError, "configuration directory"):
            self.consumers.check_service_image(sdk, source, build, profile, external)
        self.assertEqual(calls, [])

    def test_service_checker_rejects_wrong_domain_and_unreviewed_compile_input(self):
        sdk, source, build, config, profile, calls, external = self.image_fixture()
        path = config / "resolved.json"
        original = path.read_text()
        data = json.loads(original)
        data["cpu"]["arch"] = "cortex-m4"
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, "reviewed domain"):
            self.consumers.check_service_image(sdk, source, build, profile, external)
        path.write_text(original)
        database = build / "compile_commands.json"
        entries = json.loads(database.read_text())
        entries.append({"file": str(self.directory / "outside.c"),
                        "arguments": ["cc", "-c", "outside.c"]})
        database.write_text(json.dumps(entries))
        with self.assertRaisesRegex(ValueError, "unreviewed source"):
            self.consumers.check_service_image(sdk, source, build, profile, external)
        self.assertEqual(calls, [])

    def test_service_checker_rejects_missing_symbol_and_runtime_failure(self):
        sdk, source, build, _, profile, calls, external = self.image_fixture()

        def missing(argv):
            result = external(argv)
            result.stdout = "\n".join(line for line in result.stdout.splitlines()
                                      if not line.startswith("nx_settings_commit "))
            return result
        with self.assertRaisesRegex(ValueError, "retention is incomplete"):
            self.consumers.check_service_image(sdk, source, build, profile, missing)

        def failed(argv):
            result = external(argv)
            if len(argv) == 1:
                result.returncode = 7
            return result
        with self.assertRaisesRegex(ValueError, "runtime failed"):
            self.consumers.check_service_image(sdk, source, build, profile, failed)

    def test_service_checker_rejects_arm_resource_identity_before_retention(self):
        sdk, source, build, _, profile, calls, external = self.image_fixture(2)
        path = build / "bin/installed_services.resources.json"
        data = json.loads(path.read_text())
        data["elf"]["sha256"] = "f" * 64
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, "resource/configuration"):
            self.consumers.check_service_image(sdk, source, build, profile, external)
        self.assertEqual(calls, [])

    def test_service_checker_rechecks_inputs_after_actual_ports_return(self):
        sdk, source, build, _, profile, calls, external = self.image_fixture()

        def changing(argv):
            result = external(argv)
            if str(argv[0]) == "nm":
                (source / "main.c").write_text("changed during observation\n")
            return result
        with self.assertRaisesRegex(ValueError, "changed during execution"):
            self.consumers.check_service_image(sdk, source, build, profile, changing)


if __name__ == "__main__":
    unittest.main()
