"""New SDK service profiles execute explicit, bounded consumer contracts."""

from dataclasses import FrozenInstanceError, replace
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.configure import configure
from tools.evidence.common import file_identity
from tools.maintenance import consumer_profiles


ROOT = Path(__file__).resolve().parents[2]


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

    def test_service_profiles_are_exact_frozen_plan_entries(self):
        profiles = tuple(item for item in consumer_profiles.SERVICE_CONSUMERS
                         if item.scenario == "common_services")
        self.assertEqual([item.name for item in profiles], [
            "native services", "f407 services"])
        self.assertEqual([item.domain for item in profiles], [
            "native-services", "arm-services"])
        self.assertEqual([item.arm for item in profiles],
                         [False, True])
        with self.assertRaises(FrozenInstanceError):
            profiles[0].name = "changed"
        plan = consumer_profiles.consumer_plan()
        self.assertEqual(tuple(entry for entry in plan if entry[0] == "services" and
                               entry[1].scenario == "common_services"), tuple(("services", item)
                                          for item in profiles))
        changed = replace(profiles[0], domain="arm-services")
        with self.assertRaisesRegex(ValueError, "canonical|reviewed"):
            self.consumers.service_profile(changed)


    def test_native_services_prepare_real_success_smoke_and_explicit_libraries(self):
        prefix = self.prefix()
        profile = consumer_profiles.SERVICE_CONSUMERS[0]
        output = self.directory / "prepared"
        output.mkdir()
        source, build, command = self.consumers.prepare_service_consumer(
            prefix, output, profile, self.command,
            expected_revision="a" * 40, fixture=True)
        self.assertEqual(build.name, "native services build")
        self.assertIn("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON", command)
        cmake = (source / "CMakeLists.txt").read_text()
        for target in ("Nexus::Settings", "Nexus::OSWait",
                       "Nexus::LwIPStorage"):
            self.assertIn(target, cmake)
        self.assertIn("LANGUAGES C ASM", cmake)
        smoke = (source / "main.c").read_text()
        for call in ("nx_settings_commit", "nx_settings_open", "nx_wait_select",
                     "nx_lwip_adapter_start", "nx_lwip_adapter_service",
                     "nx_lwip_adapter_stop"):
            self.assertIn(call, smoke)
        resolved = configure.resolve_ir(source / "assembly.toml", root=ROOT, test_support=True)
        self.assertEqual(resolved.soc_family, "native")
        self.assertEqual(resolved.memory_budget.static_ram_limit_bytes, 65536)

    def test_unknown_profile_rejects_before_creating_a_consumer(self):
        prefix = self.prefix()
        output = self.directory / "prepared"
        output.mkdir()
        changed = replace(consumer_profiles.SERVICE_CONSUMERS[0], name="unreviewed")
        with self.assertRaisesRegex(ValueError, "canonical|reviewed"):
            self.consumers.prepare_service_consumer(
                prefix, output, changed, self.command,
                expected_revision="a" * 40, fixture=True)
        self.assertEqual(list(output.iterdir()), [])

    def test_f4_family_profiles_share_one_explicit_twenty_consumer_plan(self):
        profiles = tuple(item for item in consumer_profiles.SERVICE_CONSUMERS
                         if item.scenario == "f4_family")
        self.assertEqual([item.assembly for item in profiles], [
            "f405zg.toml", "f415zg.toml", "f417zg.toml"])
        self.assertEqual([item.domain for item in profiles], [
            "arm-family-f405zg", "arm-family-f415zg", "arm-family-f417zg"])
        self.assertTrue(all(item.arm for item in profiles))
        self.assertEqual(len(consumer_profiles.consumer_plan()), 20)

    def test_family_sources_prepare_exact_public_crc_and_board_bytes(self):
        prefix = self.prefix()
        sdk = prefix / "share/nexus/src"
        fixture = Path("tests/contracts/stm32f4_family")
        shutil.copytree(ROOT / fixture, sdk / fixture)
        consumer = Path("tests/contracts/crc_assembly/consumer.c")
        (sdk / consumer).parent.mkdir(parents=True)
        shutil.copyfile(ROOT / consumer, sdk / consumer)
        for chip in ("405", "415", "417"):
            with self.subTest(chip=chip):
                profile = consumer_profiles.ServiceConsumer(
                    f"f{chip}zg family", f"f{chip}zg.toml",
                    f"arm-family-f{chip}zg", True, "f4_family")
                output = self.directory / chip
                output.mkdir()
                source, _, command = self.consumers.prepare_service_consumer(
                    prefix, output, profile, self.command,
                    expected_revision="a" * 40, fixture=True)
                self.assertEqual((source / "main.c").read_bytes(),
                                 (ROOT / consumer).read_bytes())
                self.assertEqual((source / "assembly.toml").read_bytes(),
                                 (ROOT / fixture / profile.assembly).read_bytes())
                board = Path(f"f{chip}zg-board/board.json")
                self.assertEqual((source / board).read_bytes(),
                                 (ROOT / fixture / board).read_bytes())
                resolved = configure.resolve_ir(source / "assembly.toml")
                self.assertEqual(resolved.part, f"STM32F{chip}ZGT6")
                self.assertFalse(resolved.physical_qualified)
                self.assertEqual(resolved.memory_budget.libc_heap_bytes, 0)
                self.assertIn("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON", command)
                self.assertEqual(self.consumers.service_symbols(profile), {
                    "nx_platform_start", "nx_platform_stop", "nx_chip_info_read",
                    "nx_factory_crc", "nx_crc_port_compute", "nx_stm32_crc_ops"})

    def image_fixture(self, profile_index=0):
        # Model external compiler/runtime ports; actual SDK execution is separate.
        prefix = self.prefix()
        output = self.directory / "images"
        output.mkdir()
        profile = consumer_profiles.SERVICE_CONSUMERS[profile_index]
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
        resolved = configure.resolve_ir(source / "assembly.toml", root=ROOT, test_support=True)
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
        for index in (0, 1):
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
        sdk, source, build, _, profile, calls, external = self.image_fixture(1)
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
