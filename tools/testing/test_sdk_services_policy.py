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
    def test_power_profiles_keep_public_owner_boundary_and_exact_payload(self):
        profiles = tuple(item for item in consumer_profiles.SERVICE_CONSUMERS
                         if item.scenario == "f4_power")
        self.assertEqual([item.assembly for item in profiles],
                         [part + "-provision-empty.toml" for part in
                          ("f411re", "f446ze", "f479ni")])
        prefix = self.prefix()
        sdk = prefix / "share/nexus/src"
        relative = Path("tests/contracts/stm32f4_power")
        shutil.copytree(ROOT / relative, sdk / relative)
        for profile in profiles:
            with self.subTest(profile=profile.name):
                output = self.directory / profile.domain
                output.mkdir()
                source, _, _ = self.consumers.prepare_service_consumer(
                    prefix, output, profile, self.command,
                    expected_revision="a" * 40, fixture=True)
                self.assertEqual((source / "main.c").read_bytes(),
                                 (ROOT / relative / "consumer.c").read_bytes())
                cmake = (source / "CMakeLists.txt").read_text()
                self.assertIn("nexus_add_stm32f4_sleep(selected_power)", cmake)
                self.assertIn("LIBRARIES selected_power", cmake)
                self.assertNotIn("clock_domain/private", cmake)
                self.assertLessEqual({"nx_stm32_standby_enter",
                                      "nx_stm32_power_wake_status",
                                      "nx_clock_domain_construct"},
                                     self.consumers.service_symbols(profile))
                self.assertIn("No physical", self.consumers.service_scope(profile))
        specification = importlib.util.spec_from_file_location(
            "power_source_sdk_policy", ROOT / "cmake/package/package_source_sdk.py")
        package = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(package)
        names = {"CMakeLists.txt", "consumer.c", "README.md"}
        for part in ("f411re", "f446ze", "f479ni"):
            names.add(part + "-board/board.json")
            names.update(part + "-" + mode + ".toml" for mode in
                         ("adopt-retained", "provision-empty"))
        for name in names:
            self.assertIn((relative / name).as_posix(), package.REQUIRED_FILES)
            self.assertTrue(package.selected((relative / name).as_posix()))
        self.assertFalse(package.selected((relative / "unreviewed.c").as_posix()))

    def test_graphics_sdram_profile_reuses_public_composition_and_exports_fixture(self):
        profile = consumer_profiles.ServiceConsumer(
            "f479ni graphics sdram", "combined.toml", "arm-graphics-sdram-f479ni",
            True, "f4_graphics_sdram")
        self.assertIn(profile, consumer_profiles.SERVICE_CONSUMERS)
        prefix = self.prefix()
        sdk = prefix / "share/nexus/src"
        for name in ("stm32f4_graphics", "stm32f4_graphics_sdram"):
            relative = Path("tests/contracts") / name
            shutil.copytree(ROOT / relative, sdk / relative)
        output = self.directory / profile.domain
        output.mkdir()
        source, _, _ = self.consumers.prepare_service_consumer(
            prefix, output, profile, self.command,
            expected_revision="a" * 40, fixture=True)
        self.assertEqual((source / "main.c").read_bytes(),
                         (ROOT / "tests/contracts/stm32f4_graphics/sustained_consumer.c").read_bytes())
        for name in ("sustained_sequence.c", "sustained_sequence.h"):
            self.assertEqual((source / name).read_bytes(),
                             (ROOT / "tests/contracts/stm32f4_graphics" / name).read_bytes())
        self.assertIn("sustained_sequence.c",
                      (source / "CMakeLists.txt").read_text())
        self.assertIn("NX_GRAPHICS_SDRAM_REFERENCE=1",
                      (source / "CMakeLists.txt").read_text())
        specification = importlib.util.spec_from_file_location(
            "graphics_sdram_source_sdk_policy",
            ROOT / "cmake/package/package_source_sdk.py")
        source_sdk = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(source_sdk)
        for name in ("CMakeLists.txt", "combined.toml", "board/board.json"):
            relative = "tests/contracts/stm32f4_graphics_sdram/" + name
            self.assertIn(relative, source_sdk.REQUIRED_FILES)
            self.assertTrue(source_sdk.selected(relative))
        self.assertFalse(source_sdk.selected(
            "tests/contracts/stm32f4_graphics_sdram/consumer.c"))
        for name in ("sustained_consumer.c", "sustained_sequence.c",
                     "sustained_sequence.h"):
            relative = "tests/contracts/stm32f4_graphics/" + name
            self.assertIn(relative, source_sdk.REQUIRED_FILES)
            self.assertTrue(source_sdk.selected(relative))
        self.assertEqual((source / "board/board.json").read_bytes(),
                         (ROOT / "tests/contracts/stm32f4_graphics_sdram/board/board.json").read_bytes())
        required = self.consumers.service_symbols(profile)
        self.assertLessEqual({
            "nx_image_prepare", "nx_image_port_submit", "nx_image_port_service",
            "nx_display_prepare", "nx_display_port_submit",
            "nx_display_port_service", "nx_display_port_stop",
            "nx_stm32_sdram_graphics_attach", "nx_stm32_sdram_graphics_check",
            "nx_stm32_sdram_graphics_detach", "nx_stm32_sdram_graphics_overlaps",
            "nx_graphics_reference_initialize", "nx_graphics_reference_step",
            "nx_graphics_reference_stop",
        }, required)
        scope = self.consumers.service_scope(profile)
        self.assertIn("DMA2D", scope)
        self.assertIn("SDRAM", scope)
        self.assertIn("No physical", scope)

    def test_sdk_required_payload_excludes_the_removed_i2c_long_interface(self):
        specification = importlib.util.spec_from_file_location(
            "i2c_source_sdk_policy", ROOT / "cmake/package/package_source_sdk.py")
        source_sdk = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(source_sdk)
        for relative in ("io/include/nexus/io/i2c_long.h",
                         "io/src/mechanisms/i2c_long.c"):
            with self.subTest(path=relative):
                self.assertNotIn(relative, source_sdk.REQUIRED_FILES)

    def test_sdk_required_payload_retains_current_io_and_f4_compile_closure(self):
        specification = importlib.util.spec_from_file_location(
            "current_io_source_sdk_policy",
            ROOT / "cmake/package/package_source_sdk.py")
        source_sdk = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(source_sdk)
        generic = {"io/include/nexus/io/traits.h", "io/private/dispatch_checks.h",
                   *("io/src/api/" + name + ".c" for name in
                     ("capture", "timer_pulse", "timer_sequence"))}
        private = {"soc/stm32/stm32f4/private/" + name + ".h" for name in (
            "stm32f4_graphics_memory", "stm32f4_irq_wake", "stm32f4_uart",
            "stm32f4_uart_format", "stm32f4_uart_wire_config")}
        self.assertLessEqual(generic | private, set(source_sdk.REQUIRED_FILES))
        self.assertLessEqual(private, set(source_sdk.provider_descriptors
                                         .maintained("stm32f4").owned_payload))
        for name in sorted(generic | private):
            with self.subTest(path=name):
                self.assertTrue(source_sdk.selected(name))
                self.assertTrue((ROOT / name).is_file())

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

    def test_f4_family_profiles_share_one_explicit_consumer_plan(self):
        profiles = tuple(item for item in consumer_profiles.SERVICE_CONSUMERS
                         if item.scenario == "f4_family")
        self.assertEqual([item.assembly for item in profiles], [
            "f405zg.toml", "f415zg.toml", "f417zg.toml",
            "f411re.toml", "f446ze.toml", "f479ni.toml"])
        self.assertEqual([item.domain for item in profiles], [
            "arm-family-f405zg", "arm-family-f415zg", "arm-family-f417zg",
            "arm-family-f411re", "arm-family-f446ze", "arm-family-f479ni"])
        self.assertTrue(all(item.arm for item in profiles))

    def test_new_ip_profiles_copy_exact_public_consumers_and_board_identity(self):
        prefix = self.prefix()
        sdk = prefix / "share/nexus/src"
        for scenario in ("f4_crypto", "f4_sdram", "f4_graphics", "f4_sai", "f4_qspi"):
            relative = Path("tests/contracts") / ("stm32" + scenario)
            shutil.copytree(ROOT / relative, sdk / relative)
        expected = (("f415zg crypto", "f415zg.toml", "arm-crypto-f415zg", "f4_crypto"),
                    ("f417zg crypto", "f417zg.toml", "arm-crypto-f417zg", "f4_crypto"),
                    ("f415zg crypto dma", "f415zg-dma.toml",
                     "arm-crypto-dma-f415zg", "f4_crypto"),
                    ("f417zg crypto dma", "f417zg-dma.toml",
                     "arm-crypto-dma-f417zg", "f4_crypto"),
                    ("f479ni sdram", "f479ni-sdram.toml", "arm-sdram-f479ni", "f4_sdram"),
                    ("f479ni qspi", "f479ni-qspi.toml", "arm-qspi-f479ni", "f4_qspi"),
                    ("f479ni qspi dma", "f479ni-qspi-dma.toml",
                     "arm-qspi-dma-f479ni", "f4_qspi"),
                    ("f479ni qspi quad", "f479ni-qspi-quad.toml",
                     "arm-qspi-quad-f479ni", "f4_qspi"),
                    ("f479ni qspi quad dma", "f479ni-qspi-quad-dma.toml",
                     "arm-qspi-quad-dma-f479ni", "f4_qspi"),
                    ("f479ni graphics", "combined.toml", "arm-graphics-f479ni", "f4_graphics"),
                    ("f479ni image", "image-only.toml", "arm-image-f479ni", "f4_graphics"),
                    *((f"{part} sai {role}", f"{part}-{role}.toml",
                       f"arm-sai-{part}-{role}", "f4_sai")
                      for part in ("f446ze", "f479ni") for role in ("master", "slave")),
                    *((f"{part} {slice_name} {role} blocks",
                       f"{part}-{slice_name}-{role}-blocks.toml",
                       f"arm-{slice_name}-{part}-{role}-blocks", "f4_sai")
                      for part, slice_name in (("f446ze", "sai2"),
                                               ("f479ni", "sparse"))
                      for role in ("master", "slave")))
        for name, assembly, domain, scenario in expected:
            with self.subTest(name=name):
                profile = consumer_profiles.ServiceConsumer(
                    name, assembly, domain, True, scenario)
                self.assertIn(profile, consumer_profiles.SERVICE_CONSUMERS)
                output = self.directory / domain
                output.mkdir()
                source, _, _ = self.consumers.prepare_service_consumer(
                    prefix, output, profile, self.command,
                    expected_revision="a" * 40, fixture=True)
                directory = ROOT / "tests/contracts" / ("stm32" + scenario)
                sustained = scenario == "f4_graphics" and assembly != "image-only.toml"
                consumer = "sustained_consumer.c" if sustained else "consumer.c"
                self.assertEqual((source / "main.c").read_bytes(),
                                 (directory / consumer).read_bytes())
                for helper in ("sustained_sequence.c", "sustained_sequence.h"):
                    self.assertEqual((source / helper).exists(), sustained)
                    if sustained:
                        self.assertEqual((source / helper).read_bytes(),
                                         (directory / helper).read_bytes())
                self.assertEqual("sustained_sequence.c" in
                                 (source / "CMakeLists.txt").read_text(), sustained)
                resolved = configure.resolve_ir(source / "assembly.toml")
                self.assertFalse(resolved.physical_qualified)
                self.assertEqual(resolved.memory_budget.libc_heap_bytes, 0)
                self.assertIn("nx_platform_stop",
                              self.consumers.service_symbols(profile))
                self.assertNotEqual(self.consumers.service_symbols(profile),
                                    self.consumers.SERVICE_SYMBOLS)
                self.assertIn("No physical", self.consumers.service_scope(profile))

    def test_qspi_sdk_payload_retains_one_owned_provider_and_public_reference(self):
        specification = importlib.util.spec_from_file_location(
            "qspi_source_sdk_policy", ROOT / "cmake/package/package_source_sdk.py")
        source_sdk = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(source_sdk)
        owned = {
            "cmake/platform/STM32QSPI.cmake",
            "soc/stm32/stm32f4/drivers/qspi.c",
            "soc/stm32/stm32f4/drivers/qspi_dma.c",
            "soc/stm32/stm32f4/private/stm32f4_qspi.h",
            "soc/stm32/stm32f4/private/qspi_hal/stm32f4xx_hal_conf.h",
        }
        reference = {"tests/contracts/stm32f4_qspi/" + name for name in (
            "consumer.c", "CMakeLists.txt", "f479ni-qspi.toml",
            "f479ni-qspi-dma.toml", "f479ni-qspi-quad.toml",
            "f479ni-qspi-quad-dma.toml", "f479ni-qspi-board/board.json",
            "f479ni-qspi-quad-board/board.json")}
        payload = source_sdk.provider_descriptors.maintained("stm32f4").owned_payload
        self.assertLessEqual(owned, set(payload))
        self.assertLessEqual(owned | reference, set(source_sdk.REQUIRED_FILES))
        for name in sorted(owned | reference):
            with self.subTest(path=name):
                self.assertTrue(source_sdk.selected(name))
                self.assertTrue((ROOT / name).is_file())
        # Host byte/FIFO simulation is not a firmware SDK dependency.
        for name in ("vendor_model.py", "model-config/nexus_config.h"):
            self.assertFalse(source_sdk.selected(
                "tests/contracts/stm32f4_qspi/" + name))

    def test_capture_frames_consumer_preserves_public_session_and_board(self):
        profile = consumer_profiles.ServiceConsumer(
            "f407 capture frames", "ve-frames-raw.toml", "arm-capture-frames-f407",
            True, "f4_capture_audio")
        self.assertIn(profile, consumer_profiles.SERVICE_CONSUMERS)
        prefix = self.prefix()
        sdk = prefix / "share/nexus/src"
        relative = Path("tests/contracts/f407_capture_audio")
        shutil.copytree(ROOT / relative, sdk / relative)
        output = self.directory / profile.domain
        output.mkdir()
        source, _, _ = self.consumers.prepare_service_consumer(
            prefix, output, profile, self.command,
            expected_revision="a" * 40, fixture=True)
        for copied, canonical in (("main.c", "frame_session.c"),
                                  ("ve_board/board.json", "ve_board/board.json")):
            self.assertEqual((source / copied).read_bytes(),
                             (ROOT / relative / canonical).read_bytes())
        resolved = configure.resolve_ir(source / "assembly.toml")
        self.assertFalse(resolved.physical_qualified)
        self.assertEqual(resolved.memory_budget.libc_heap_bytes, 0)
        self.assertLessEqual({
            "nx_capture_port_start_frames", "nx_capture_port_service",
            "nx_capture_port_stop", "nx_frame_stream_initialize",
            "nx_stream_acquire", "nx_stream_release", "nx_platform_stop",
        }, self.consumers.service_symbols(profile))
        self.assertIn("No physical", self.consumers.service_scope(profile))
        specification = importlib.util.spec_from_file_location(
            "capture_source_sdk_policy", ROOT / "cmake/package/package_source_sdk.py")
        source_sdk = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(source_sdk)
        for name in ("CMakeLists.txt", "frame_session.c", "controller_main.c",
                     "ve-frames-raw.toml", "ve_board/board.json"):
            member = (relative / name).as_posix()
            self.assertIn(member, source_sdk.REQUIRED_FILES)
            self.assertTrue(source_sdk.selected(member))

    def test_family_sources_prepare_exact_public_crc_and_board_bytes(self):
        prefix = self.prefix()
        sdk = prefix / "share/nexus/src"
        fixture = Path("tests/contracts/stm32f4_family")
        shutil.copytree(ROOT / fixture, sdk / fixture)
        consumer = Path("tests/contracts/crc_assembly/consumer.c")
        (sdk / consumer).parent.mkdir(parents=True)
        shutil.copyfile(ROOT / consumer, sdk / consumer)
        for stem, part in (("f405zg", "STM32F405ZGT6"),
                           ("f415zg", "STM32F415ZGT6"),
                           ("f417zg", "STM32F417ZGT6"),
                           ("f411re", "STM32F411RET6"),
                           ("f446ze", "STM32F446ZET6"),
                           ("f479ni", "STM32F479NIH6")):
            with self.subTest(part=part):
                profile = consumer_profiles.ServiceConsumer(
                    f"{stem} family", f"{stem}.toml",
                    f"arm-family-{stem}", True, "f4_family")
                output = self.directory / stem
                output.mkdir()
                source, _, command = self.consumers.prepare_service_consumer(
                    prefix, output, profile, self.command,
                    expected_revision="a" * 40, fixture=True)
                self.assertEqual((source / "main.c").read_bytes(),
                                 (ROOT / consumer).read_bytes())
                self.assertEqual((source / "assembly.toml").read_bytes(),
                                 (ROOT / fixture / profile.assembly).read_bytes())
                board = Path(f"{stem}-board/board.json")
                self.assertEqual((source / board).read_bytes(),
                                 (ROOT / fixture / board).read_bytes())
                resolved = configure.resolve_ir(source / "assembly.toml")
                self.assertEqual(resolved.part, part)
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
