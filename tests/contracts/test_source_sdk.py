"""Relocate the real source SDK and compile/run independent public consumers."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "cmake/package"))
import verify_consumers as consumer_gate
import package_source_sdk as source_sdk


class SourceSDKSelectionTests(unittest.TestCase):
    def test_metadata_and_memory_generation_are_required_sdk_payload(self):
        required = {"dependencies/platform-compatibility.json",
                    "tools/configure/metadata.py", "tools/configure/memory.py"}
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)))

    def test_test_framework_payload_is_not_a_firmware_sdk_dependency(self):
        for name in ("dependencies/source/googletest-1.16.0.tar.gz",
                     "dependencies/source/googletest-LICENSE.txt",
                     "tests/contracts/factory_test.cpp"):
            with self.subTest(name=name):
                self.assertFalse(source_sdk.selected(name))
        self.assertTrue(source_sdk.selected("dependencies/googletest.lock.json"))
        self.assertTrue(source_sdk.selected("io/include/nexus/io/factory.h"))
        self.assertTrue(source_sdk.selected("tools/configure/assemblies/native.toml"))

    def test_audit_archives_stay_outside_sdk_but_operational_tools_remain(self):
        for name in ("tools/testing/evidence/report.json",
                     "tools/testing/evidence/nested/results.xml",
                     "tools/testing/evidence/sources/provider.c.raw",
                     "tools/testing/evidence/firmware.elf"):
            with self.subTest(name=name):
                self.assertFalse(source_sdk.selected(name))
        for name in ("tools/testing/evidence.py",
                     "tools/testing/evidence_extra/report.json",
                     "tools/testing/test_source_identity.py",
                     "tools/evidence/verify.py",
                     "tools/maintenance/stm32g4_lock.py",
                     "dependencies/googletest.lock.json"):
            with self.subTest(name=name):
                self.assertTrue(source_sdk.selected(name))

    def test_consumer_uses_authored_toml_for_each_maintained_family(self):
        with tempfile.TemporaryDirectory(prefix="SDK consumer inputs ") as value:
            directory = Path(value)
            for name, arm, chip in (("native", False, "native"),
                                    ("stm32", True, "stm32"),
                                    ("gd32", True, "gd32")):
                _, _, command = consumer_gate.prepare_consumer(
                    directory / "prefix", directory, name,
                    lambda arguments: None, arm=arm, chip=chip)
                assembly = next(argument for argument in command
                                if argument.startswith("-DNEXUS_ASSEMBLY_FILE="))
                self.assertTrue(assembly.endswith(".toml"), assembly)

    def test_exposed_io_core_clock_and_provider_closures_are_required(self):
        required = {
            "arch/CMakeLists.txt", "cmake/toolchains/arm-gcc.cmake",
            "os/wait.c", "os/include/nexus/os/wait.h",
            "os/baremetal/notify.c", "os/native/native.c",
            "components/CMakeLists.txt",
            "core/include/nexus/core/fault.h", "core/src/fault.c",
            "core/include/nexus/core/stack_observation.h",
            "core/src/stack_observation.c", "io/CMakeLists.txt",
            "io/include/nexus/io/usb_device.h", "io/src/usb_device.c",
            "io/src/usb_device_controller.c", "io/private/usb_device_controller.h",
            "io/native/usb_device_access.h", "io/native/usb_device.c",
            "io/include/nexus/io/ethernet.h", "io/include/nexus/io/mdio.h",
            "io/src/eth_mac.c", "io/private/eth_mac.h",
            "io/native/ethernet_access.h", "io/native/ethernet.c",
            "io/src/can_controller.c", "io/private/can_controller.h",
            "io/include/nexus/io/timer_capture.h",
            "io/include/nexus/io/advanced_pwm.h",
            "io/private/timer_advanced_controller.h",
            "tools/configure/bindings.py", "tools/configure/emission.py",
            "tools/configure/usb_device.py", "tools/configure/ethernet.py",
            "tools/configure/timer_advanced.py",
            "cmake/platform/Configure.cmake", "cmake/platform/ControllerModes.cmake",
            "os/freertos/timebase_lowpower.c",
            "os/freertos/private/timebase_lowpower.h",
            "os/freertos/stack_snapshot.c",
            "os/freertos/include/nexus/os/freertos_stack.h",
            "components/clock_domain/src/clock_domain.c",
            "components/user_io_broker/src/user_io_broker.c",
            "components/dp83848/src/dp83848.c",
            "scripts/ci/vendor_import_identity.py", "scripts/ci/verify_vendor_source.py",
        }
        for family in ("stm32f407", "gd32f470"):
            required.update(f"soc/{family}/" + name for name in (
                "system.c", "timebase_lowpower.c", "usb_clock.c",
                "drivers/usb_device.c", "drivers/ethernet.c", "drivers/can.c",
                "drivers/timer_capture.c", "drivers/advanced_pwm.c",
                "CMakeLists.txt", "soc.json", "routes.json"))
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))


class SourceSDKThreadXSelectionTests(unittest.TestCase):
    def test_threadx_dependency_snapshot_is_exact_reviewed_source_closure(self):
        lock = json.loads((ROOT / "dependencies/threadx.lock.json").read_text())
        selected = source_sdk.dependency_entries(ROOT, "ext/threadx")
        self.assertEqual(set(selected), set(lock["files"]) | {lock["license"]["path"]})
        self.assertEqual(len(selected), 215)
        self.assertNotIn("ports/cortex_m7/gnu/src/tx_thread_schedule.S", selected)

    def test_approved_origin_declaration_is_required_and_packaged(self):
        self.assertTrue(source_sdk.selected(".gitmodules"))
        self.assertIn(".gitmodules", source_sdk.REQUIRED_FILES)

    def test_all_reviewed_threadx_sources_headers_license_and_owned_composition_are_required(self):
        lock = json.loads((ROOT / "dependencies/threadx.lock.json").read_text())
        official = {"ext/threadx/" + name for name in lock["files"]}
        official.add("ext/threadx/" + lock["license"]["path"])
        owned = {"dependencies/threadx.lock.json", "tools/maintenance/threadx_lock.py",
                 "tools/maintenance/source_catalogue.py", "tools/configure/threadx.py",
                 "tools/configure/lowpower.py", "os/threadx/CMakeLists.txt",
                 "os/threadx/Kernel.cmake", "os/threadx/threadx.c", "os/threadx/m4.c",
                 "os/threadx/m4_vectors.S", "os/threadx/prepare_linux.py",
                 "os/threadx/config/tx_user.h", "os/threadx/include/nexus/os/threadx.h"}
        self.assertTrue((official | owned).issubset(set(source_sdk.REQUIRED_FILES)))
        self.assertIn("ext/threadx", source_sdk.REQUIRED_DEPENDENCIES)


class SourceSDKG4SelectionTests(unittest.TestCase):
    def test_g4_snapshot_contains_only_five_reviewed_sources_and_original_notice(self):
        lock = json.loads((ROOT / "dependencies/stm32g4.lock.json").read_text())
        entries = source_sdk.dependency_entries(ROOT, "vendors/st/cmsis_device_g4")
        self.assertEqual(set(entries), set(lock["files"]))
        self.assertEqual(len(entries), 5)
        self.assertNotIn("Include/stm32g491xx.h", entries)

    def test_g4_lock_verifier_and_all_five_official_files_are_required(self):
        lock = json.loads((ROOT / "dependencies/stm32g4.lock.json").read_text())
        required = {"dependencies/stm32g4.lock.json",
                    "tools/maintenance/stm32g4_lock.py"}
        required.update("vendors/st/cmsis_device_g4/" + name
                        for name in lock["files"])
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)))
        self.assertIn("vendors/st/cmsis_device_g4", source_sdk.REQUIRED_DEPENDENCIES)

    def test_closed_g4_owned_platform_and_reference_sources_are_required(self):
        required = {
            "soc/stm32g474/CMakeLists.txt", "soc/stm32g474/system.c",
            "soc/stm32g474/drivers/gpio.c", "soc/stm32g474/drivers/uart.c",
            "soc/stm32g474/private/stm32g474_system.h",
            "soc/stm32g474/private/stm32g474_provider.h",
            "soc/stm32g474/soc.json", "soc/stm32g474/routes.json",
            "boards/stm32g474re_hse24_reference/board.json",
            "tools/configure/providers/stm32g474/__init__.py",
            "tools/configure/providers/stm32g474/bindings.py",
            "tools/configure/providers/stm32g474/emission.py"}
        self.assertTrue(required.issubset(set(source_sdk.REQUIRED_FILES)),
                        sorted(required - set(source_sdk.REQUIRED_FILES)))


class SourceSDKDevelopmentSnapshotTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="SDK staged source ")
        self.addCleanup(self.temporary.cleanup)
        self.repository = Path(self.temporary.name)
        self.environment = {
            key: value for key, value in os.environ.items()
            if not key.startswith("GIT_")}
        self.git("init", "--quiet")
        self.write("core/committed.c", "int committed_source;\n")
        self.write("docs/committed.txt", "external documentation\n")
        self.git("add", ".")
        self.git("-c", "user.name=SDK test", "-c",
                 "user.email=sdk-test@local.invalid", "commit", "--quiet",
                 "-m", "committed snapshot baseline")
        self.entries = source_sdk.tree_entries(self.repository)
        self.write("core/committed.c", "int working_source;\n")
        self.write("arch/cortex_m/staged.c", "int staged_source;\n")
        self.write("docs/staged.txt", "nonselected staged source\n")
        self.git("add", "arch/cortex_m/staged.c", "docs/staged.txt")
        self.write("tools/untracked.py", "untracked_source = True\n")
        self.write("docs/untracked.txt", "nonselected untracked source\n")
        self.write("dependencies/source/host-framework.tar.gz", "host only\n")

    def git(self, *arguments):
        result = subprocess.run(
            ["git", "-C", str(self.repository), *arguments],
            env=self.environment, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0,
                         result.stdout.decode() + result.stderr.decode())
        return result.stdout

    def write(self, name, data):
        file = self.repository / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(data)

    def test_development_snapshot_includes_staged_and_untracked_sources(self):
        before = self.git("write-tree")
        names = source_sdk.snapshot_files(
            self.repository, self.entries, development_fixture=True)
        self.assertEqual(names, {
            "core/committed.c", "arch/cortex_m/staged.c",
            "tools/untracked.py"})
        self.assertEqual(self.git("write-tree"), before)

    def test_publishable_snapshot_uses_only_original_committed_blobs(self):
        names = source_sdk.snapshot_files(
            self.repository, self.entries, development_fixture=False)
        self.assertEqual(names, {"core/committed.c"})
        blobs = source_sdk.committed_blobs(
            self.repository, {name: self.entries[name] for name in names})
        self.assertEqual(blobs["core/committed.c"], b"int committed_source;\n")
        self.assertNotEqual(blobs["core/committed.c"],
                            (self.repository / "core/committed.c").read_bytes())

    def test_real_git_snapshots_exclude_archives_in_both_package_modes(self):
        committed = ("tools/testing/evidence/committed/report.json",
                     "tools/testing/evidence.py",
                     "tools/testing/evidence_extra/committed.json")
        for name in committed:
            self.write(name, "committed SDK selection fixture\n")
        self.git("add", *committed)
        self.git("-c", "user.name=SDK test", "-c",
                 "user.email=sdk-test@local.invalid", "commit", "--quiet",
                 "-m", "committed archive and operational tool boundaries")
        entries = source_sdk.tree_entries(self.repository)
        staged = ("tools/testing/evidence/staged/results.xml",
                  "tools/testing/evidence_extra/staged.py")
        for name in staged:
            self.write(name, "staged SDK selection fixture\n")
        self.git("add", *staged)
        untracked = ("tools/testing/evidence/untracked/provider.c.raw",
                     "tools/evidence/verify.py")
        for name in untracked:
            self.write(name, "untracked SDK selection fixture\n")
        index_before = self.git("write-tree")
        for development in (False, True):
            with self.subTest(development=development):
                names = source_sdk.snapshot_files(
                    self.repository, entries, development_fixture=development)
                self.assertNotIn(committed[0], names)
                self.assertNotIn(staged[0], names)
                self.assertNotIn(untracked[0], names)
                self.assertIn(committed[1], names)
                self.assertIn(committed[2], names)
                self.assertEqual(staged[1] in names, development)
                self.assertEqual(untracked[1] in names, development)
        self.assertEqual(self.git("write-tree"), index_before)


class SourceSDKTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="nexus installed source sdk ")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.directory = Path(cls.temporary.name)
        cls.run_ok(["git", "init", "--quiet", str(cls.directory)])
        (cls.directory / "parent-identity.txt").write_text("consumer owner\n")
        cls.run_ok(["git", "-C", str(cls.directory), "add", "parent-identity.txt"])
        cls.run_ok(["git", "-C", str(cls.directory), "-c", "user.name=SDK test", "-c", "user.email=sdk-test@local.invalid",
                    "commit", "--quiet", "-m", "unrelated consumer owner"])
        original = cls.directory / "initial package"
        cls.prefix = cls.directory / "relocated prefix with spaces"
        cls.run_ok([sys.executable, "-B", str(ROOT / "cmake/package/package_source_sdk.py"),
                    "--source", str(ROOT), "--output", str(original), "--development-fixture"])
        original.rename(cls.prefix)
        cls.sdk = cls.prefix / "share/nexus/src"
        cls.identity = json.loads((cls.sdk / ".nexus-source-sdk.json").read_text())

    @staticmethod
    def run_process(arguments, **kwargs):
        environment = os.environ.copy()
        for key in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                    "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH", "LIBRARY_PATH"):
            environment.pop(key, None)
        kwargs.setdefault("env", environment)
        return subprocess.run(arguments, text=True, capture_output=True, timeout=180, **kwargs)

    @classmethod
    def run_ok(cls, arguments, **kwargs):
        result = cls.run_process(arguments, **kwargs)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        return result

    def verify(self):
        return self.run_process([sys.executable, "-B", str(self.sdk / "cmake/package/package_source_sdk.py"),
                         "--verify", str(self.sdk)])

    def test_cli_summary_cannot_be_confused_with_complete_manifest(self):
        result = self.verify()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        summary = json.loads(result.stdout)
        self.assertEqual(summary["kind"], "nexus_source_sdk_summary")
        self.assertEqual(summary["sdk_kind"], self.identity["kind"])
        self.assertNotEqual(summary["kind"], self.identity["kind"])
        self.assertEqual(summary["snapshot_sha256"],
                         self.identity["snapshot_sha256"])
        self.assertEqual(summary["files"], len(self.identity["files_sha256"]))
        self.assertNotIn("files_sha256", summary)

    def consumer(self, name, **options):
        source, build, command = consumer_gate.prepare_consumer(
            self.prefix, self.directory, name, self.run_ok, **options)
        return source, build, self.run_process(command)

    def test_relocated_c_and_cpp_consumer_build_and_run_without_original_source(self):
        source, build, result = self.consumer("native", fixture=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((build / "resolved-sdk.txt").read_text(), str(self.sdk))
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4"])
        for application in ("installed_c", "installed_cpp"):
            self.run_ok([str(build / "bin" / application)])
        self.run_ok([str(build / "parent_plain")])
        compile_commands = json.loads((build / "compile_commands.json").read_text())
        self.assertGreater(len(compile_commands), 0)
        for entry in compile_commands:
            self.assertNotIn(str(ROOT) + "/", entry.get("command", ""))

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_arm_consumer_compiles_real_startup_and_linker(self):
        _, build, result = self.consumer("arm", arm=True, fixture=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4", "--target", "installed_c", "installed_cpp"])
        for application in ("installed_c", "installed_cpp"):
            self.assertGreater((build / "bin" / (application + ".elf")).stat().st_size, 0)
            resources = json.loads((build / "bin" / (application + ".resources.json")).read_text())
            self.assertEqual(resources["status"], "passed")
        self.check_private_includes(build)

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_gd32_consumer_compiles_real_startup_and_linker(self):
        _, build, result = self.consumer("gd32 arm", arm=True, chip="gd32", fixture=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4", "--target", "installed_c", "installed_cpp"])
        for application in ("installed_c", "installed_cpp"):
            self.assertGreater((build / "bin" / (application + ".elf")).stat().st_size, 0)
            resources = json.loads((build / "bin" / (application + ".resources.json")).read_text())
            self.assertEqual(resources["status"], "passed")
        self.check_private_includes(build)

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_g474_selected_baremetal_and_freertos_consumers_link(self):
        for backend, filename in (("baremetal", "io.toml"),
                                  ("freertos", "io-freertos.toml")):
            with self.subTest(backend=backend):
                source, build, arguments = consumer_gate.prepare_consumer(
                    self.prefix, self.directory, "g474 " + backend,
                    self.run_ok, fixture=True, arm=True)
                kernel = " LIBRARIES Nexus::OSFreeRTOS" if backend == "freertos" else ""
                definitions = "NX_G474_IO_FIXTURE"
                if backend == "freertos":
                    definitions += " NX_G474_FREERTOS_FIXTURE"
                (source / "CMakeLists.txt").write_text(
                    'cmake_minimum_required(VERSION 3.31)\n'
                    'project(installed_g474_consumer LANGUAGES C ASM)\n'
                    'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
                    'find_package(Nexus 1.0.0 EXACT CONFIG REQUIRED)\n'
                    f'nexus_add_firmware(installed_g474 SOURCES main.c{kernel})\n'
                    f'target_compile_definitions(installed_g474 PRIVATE {definitions})\n'
                    'get_target_property(sdk_root Nexus::Config NEXUS_SOURCE_DIR)\n'
                    'get_target_property(config_root Nexus::Platform NEXUS_CONFIG_DIR)\n'
                    'file(WRITE "${CMAKE_BINARY_DIR}/resolved-sdk.txt" "${sdk_root}")\n'
                    'file(WRITE "${CMAKE_BINARY_DIR}/resolved-config.txt" "${config_root}")\n')
                fixture = ROOT / "tests/contracts/g474_assembly"
                shutil.copyfile(fixture / "main.c", source / "main.c")
                assembly = source / "g474.toml"
                text = (fixture / filename).read_text()
                text = text.replace(
                    'board_package = "../../../boards/stm32g474re_hse24_reference"',
                    f'board_package = "{self.sdk}/boards/stm32g474re_hse24_reference"')
                assembly.write_text(text)
                arguments = [argument for argument in arguments
                             if not argument.startswith("-DNEXUS_ASSEMBLY_FILE=")]
                arguments.append(f"-DNEXUS_ASSEMBLY_FILE={assembly}")
                self.run_ok(arguments)
                self.assertEqual((build / "resolved-sdk.txt").read_text(), str(self.sdk))
                self.run_ok(["cmake", "--build", str(build), "--parallel", "4",
                             "--target", "installed_g474"])
                image = build / "bin/installed_g474.elf"
                resources = json.loads(
                    (build / "bin/installed_g474.resources.json").read_text())
                self.assertEqual(resources["status"], "passed")
                self.assertGreater(image.stat().st_size, 0)
                config = Path((build / "resolved-config.txt").read_text())
                self.run_ok([sys.executable, "-B",
                    str(self.sdk / "tools/measurement/retention.py"),
                    "--elf", str(image), "--bindings", str(config / "bindings.c"),
                    "--output", str(build / "provider-retention.json")])
                retention = json.loads((build / "provider-retention.json").read_text())
                self.assertEqual(retention["status"], "passed")
                commands = json.loads((build / "compile_commands.json").read_text())
                products = [entry for entry in commands
                            if Path(entry["file"]).resolve() == source / "main.c"]
                self.assertEqual(len(products), 1)
                for entry in commands:
                    self.assertNotIn(str(ROOT) + "/", entry["command"])
                for private in ("/vendors/", "/soc/", "-DSTM32G474"):
                    self.assertNotIn(private, products[0]["command"])

    def check_private_includes(self, build):
        commands = json.loads((build / "compile_commands.json").read_text())
        product = [entry for entry in commands if Path(entry["file"]).name in {"main.c", "main.cpp"}]
        self.assertEqual(len(product), 2)
        for entry in product:
            command = entry["command"]
            self.assertNotIn("/vendors/", command)
            self.assertNotIn("/soc/", command)
            self.assertNotIn(str(ROOT) + "/", command)
            self.assertNotIn("-DSTM32F407", command)
            self.assertNotIn("-DGD32F470", command)

    def modern_consumer(self, family, board, backend):
        source, build, arguments = consumer_gate.prepare_modern_consumer(
            self.prefix, self.directory, family, backend, self.run_ok,
            fixture=True)
        self.assertIn(str(self.sdk / "boards" / board),
                      (source / "assembly.toml").read_text())
        self.run_ok(arguments)
        self.assertEqual((build / "resolved-sdk.txt").read_text(), str(self.sdk))
        self.run_ok(["cmake", "--build", str(build), "--parallel", "2",
                     "--target", "installed_modern"])
        image = build / "bin/installed_modern.elf"
        resources = json.loads(
            (build / "bin/installed_modern.resources.json").read_text())
        self.assertEqual(resources["status"], "passed")
        self.assertGreater(image.stat().st_size, 0)
        config = Path((build / "resolved-config.txt").read_text())
        self.run_ok([sys.executable, "-B",
            str(self.sdk / "tools/measurement/retention.py"),
            "--elf", str(image), "--bindings", str(config / "bindings.c"),
            "--output", str(build / "provider-retention.json")])
        retention = json.loads((build / "provider-retention.json").read_text())
        self.assertEqual(retention["status"], "passed")
        commands = json.loads((build / "compile_commands.json").read_text())
        products = [entry for entry in commands
                    if Path(entry["file"]).resolve() == source / "main.c"]
        self.assertEqual(len(products), 1)
        for entry in commands:
            self.assertNotIn(str(ROOT) + "/", entry["command"])
        for private in ("/vendors/", "/soc/", "-DSTM32H743", "-DSTM32U575"):
            self.assertNotIn(private, products[0]["command"])
        # The SDK must supply the selected original vector table, exactly once.
        part = "stm32h743xx" if family == "h743" else "stm32u575xx"
        startup = [entry for entry in commands
                   if Path(entry["file"]).name == f"startup_{part}.s"]
        self.assertEqual(len(startup), 1)
        self.assertTrue(Path(startup[0]["file"]).is_relative_to(self.sdk))

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_h743_selected_baremetal_and_freertos_consumers_link(self):
        for backend in ("baremetal", "freertos"):
            with self.subTest(backend=backend):
                self.modern_consumer("h743", "stm32h743_nucleo_zi2", backend)

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_u575_selected_baremetal_and_freertos_consumers_link(self):
        for backend in ("baremetal", "freertos"):
            with self.subTest(backend=backend):
                self.modern_consumer("u575", "stm32u575_nucleo_zi_q", backend)

    def threadx_consumer(self, name, board, clock, *, arm):
        source, build, arguments = consumer_gate.prepare_consumer(
            self.prefix, self.directory, name, self.run_ok,
            fixture=True, arm=arm)
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(installed_threadx_consumer LANGUAGES C ASM)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            'find_package(Nexus 1.0.0 EXACT CONFIG REQUIRED)\n'
            'nexus_add_firmware(installed_threadx SOURCES main.c '
            'LIBRARIES Nexus::OSThreadX)\n'
            'get_target_property(sdk_root Nexus::Config NEXUS_SOURCE_DIR)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/resolved-sdk.txt" "${sdk_root}")\n')
        # The fixture deliberately retains a permanent owner task: link only.
        shutil.copyfile(ROOT / "tests/contracts/os_threadx_runtime/main.c",
                        source / "main.c")
        assembly = source / "threadx.toml"
        assembly.write_text(
            f'schema = 2\nboard = "{board}"\nbackend = "threadx"\n'
            f'clock = "{clock}"\noptimization = "Os"\n\n'
            '[memory]\nmain_stack_bytes = 2048\n'
            'flash_load_limit_bytes = 65536\n'
            'static_ram_limit_bytes = 32768\nlibc_heap_bytes = 0\n')
        arguments = [argument for argument in arguments
                     if not argument.startswith("-DNEXUS_ASSEMBLY_FILE=")]
        arguments.append(f"-DNEXUS_ASSEMBLY_FILE={assembly}")
        self.run_ok(arguments)
        self.assertEqual((build / "resolved-sdk.txt").read_text(), str(self.sdk))
        self.run_ok(["cmake", "--build", str(build), "--parallel", "4",
                     "--target", "installed_threadx"])
        commands = json.loads((build / "compile_commands.json").read_text())
        product = [entry for entry in commands
                   if Path(entry["file"]).resolve() == source / "main.c"]
        self.assertEqual(len(product), 1)
        for entry in commands:
            self.assertNotIn(str(ROOT) + "/", entry["command"])
        for private in ("/vendors/", "/soc/", "-DSTM32F407", "-DGD32F470"):
            self.assertNotIn(private, product[0]["command"])
        if arm:
            image = build / "bin/installed_threadx.elf"
            resources = json.loads(
                (build / "bin/installed_threadx.resources.json").read_text())
            self.assertEqual(resources["status"], "passed")
        else:
            image = build / "bin/installed_threadx"
        self.assertGreater(image.stat().st_size, 0)

    def test_relocated_threadx_native_consumer_links_actual_official_kernel(self):
        self.threadx_consumer("threadx native", "native_reference", "model",
                              arm=False)

    @unittest.skipUnless(shutil.which("arm-none-eabi-gcc"), "ARM GNU toolchain unavailable: no ARM execution claim")
    def test_relocated_threadx_three_board_consumers_link_actual_official_kernel(self):
        for name, board, clock in (
                ("qiming", "stm32f407_qiming_v31", "hse8-pll168"),
                ("sky", "stm32f407ve_sky_qingchun", "hse8-pll168"),
                ("liangshan", "gd32f470_liangshan", "hxtal25-pll200")):
            with self.subTest(board=board):
                self.threadx_consumer("threadx " + name, board, clock, arm=True)

    def test_fixture_requires_explicit_consumer_opt_in(self):
        self.assertFalse(self.identity["publishable"])
        _, _, result = self.consumer("fixture rejection", fixture=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("nonpublishable development fixture", result.stdout + result.stderr)

    def test_formal_consumer_cli_rejects_nonpublishable_fixture(self):
        output = self.directory / "formal consumer rejection"
        result = self.run_process([sys.executable, "-B",
            str(ROOT / "cmake/package/verify_consumers.py"),
            "--prefix", str(self.prefix), "--output", str(output)])
        self.assertNotEqual(result.returncode, 0)
        report = json.loads((output / "verification.json").read_text())
        self.assertEqual(report["status"], "failed")
        self.assertIn("publishable", report["error"])
        self.assertEqual(len(report["commands"]), 1)
        self.assertEqual(report["commands"][0]["exit_code"], 0)

    def test_expected_revision_conflict_is_rejected(self):
        _, _, result = self.consumer("revision rejection", fixture=True, expected_revision="0" * 40)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("expected revision", " ".join((result.stdout + result.stderr).split()))

    def test_different_source_package_version_is_rejected(self):
        _, _, result = self.consumer("version rejection", fixture=True, version="99.0.0")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("version", result.stdout + result.stderr)

    def test_development_contracts_are_explicitly_rejected(self):
        _, _, result = self.consumer("contract rejection", contracts=True, fixture=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("excludes development tests/contracts", " ".join((result.stdout + result.stderr).split()))

    def test_modified_required_file_is_rejected(self):
        target = self.sdk / "core/src/request.c"
        original = target.read_bytes()
        try:
            target.write_bytes(original + b"\n/* changed after packaging */\n")
            result = self.verify()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("file identity changed", result.stderr)
        finally:
            target.write_bytes(original)

    def test_modified_exported_config_is_rejected(self):
        target = self.prefix / "lib/cmake/Nexus/NexusConfig.cmake"
        original = target.read_bytes()
        try:
            target.write_bytes(original + b"\n# changed after export\n")
            result = self.verify()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Exported CMake package identity changed", result.stderr)
        finally:
            target.write_bytes(original)

    def test_missing_license_is_rejected(self):
        target = self.sdk / "ext/freertos/LICENSE.md"
        original = target.read_bytes()
        try:
            target.unlink()
            result = self.verify()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Missing source SDK file", result.stderr)
        finally:
            target.write_bytes(original)

    def test_rehashed_missing_arch_source_is_rejected(self):
        identity = self.sdk / source_sdk.MANIFEST
        original_identity = identity.read_bytes()
        for name in ("arch/include/nexus/arch/arch.h",
                     "arch/cortex_m/nx_arch_cortex_m.c",
                     "arch/cortex_m/private/compiler.h",
                     "arch/native/nx_arch_native.c"):
            with self.subTest(name=name):
                target = self.sdk / name
                original_source = target.read_bytes()
                try:
                    target.unlink()
                    manifest = json.loads(original_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("missing required sources/licenses",
                                  result.stderr)
                finally:
                    target.write_bytes(original_source)
                    identity.write_bytes(original_identity)

    def test_rehashed_default_target_source_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        for name in ("arch/CMakeLists.txt", "os/wait.c",
                     "components/CMakeLists.txt", "cmake/toolchains/arm-gcc.cmake",
                     "tools/measurement/resources.py", "tools/evidence/common.py"):
            with self.subTest(name=name):
                target = self.sdk / name
                old_source = target.read_bytes()
                try:
                    target.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0,
                                        "Rehashed default target omission accepted: " + name)
                    self.assertIn("missing required sources/licenses", result.stderr)
                finally:
                    target.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_secondary_controller_generator_omission_rejects(self):
        name = "tools/configure/secondary_controllers.py"
        target = self.sdk / name
        identity = self.sdk / source_sdk.MANIFEST
        old_source = target.read_bytes()
        old_identity = identity.read_bytes()
        try:
            target.unlink()
            manifest = json.loads(old_identity)
            manifest["files_sha256"].pop(name)
            manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                manifest["files_sha256"], sort_keys=True,
                separators=(",", ":")).encode())
            identity.write_text(json.dumps(manifest) + "\n")
            result = self.verify()
            self.assertNotEqual(result.returncode, 0,
                                "Rehashed controller generator omission accepted")
            self.assertIn("missing required sources/licenses", result.stderr)
        finally:
            target.write_bytes(old_source)
            identity.write_bytes(old_identity)

    def test_rehashed_exposed_io_clock_and_cold_source_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        names = (
            "core/src/fault.c", "core/include/nexus/core/fault.h",
            "io/include/nexus/io/usb_device.h", "io/src/usb_device_controller.c",
            "io/private/usb_device_controller.h", "io/native/usb_device_access.h",
            "io/include/nexus/io/ethernet.h", "io/src/eth_mac.c",
            "io/private/eth_mac.h", "io/include/nexus/io/mdio.h",
            "soc/stm32f407/drivers/usb_device.c",
            "soc/gd32f470/drivers/ethernet.c",
            "soc/stm32f407/private/stm32f407_can.h",
            "io/src/can_controller.c", "io/private/can_controller.h",
            "io/include/nexus/io/advanced_pwm.h",
            "io/private/timer_advanced_controller.h",
            "soc/gd32f470/drivers/timer_capture.c",
            "os/freertos/timebase_lowpower.c",
            "os/freertos/private/timebase_lowpower.h",
            "soc/stm32f407/timebase_lowpower.c", "soc/gd32f470/usb_clock.c",
            "tools/configure/usb_device.py", "tools/configure/ethernet.py",
            "tools/configure/timer_advanced.py", "tools/configure/bindings.py",
            "cmake/platform/ControllerModes.cmake",
            "components/clock_domain/private/clock_domain_impl.h",
            "components/user_io_broker/include/nexus/components/user_io_broker.h",
            "components/dp83848/include/nexus/dp83848/dp83848.h",
            "soc/stm32g474/system.c", "soc/stm32g474/drivers/uart.c",
            "soc/stm32g474/private/stm32g474_provider.h",
            "soc/stm32g474/soc.json", "soc/stm32g474/routes.json",
            "boards/stm32g474re_hse24_reference/board.json",
            "tools/configure/providers/stm32g474/__init__.py",
            "tools/configure/providers/stm32g474/bindings.py",
            "tools/configure/providers/stm32g474/emission.py")
        for name in names:
            with self.subTest(name=name):
                target = self.sdk / name
                old_source = target.read_bytes()
                try:
                    target.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0,
                                        "Rehashed exposed source omission accepted: " + name)
                    self.assertIn("missing required sources/licenses", result.stderr)
                finally:
                    target.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_threadx_kernel_header_license_and_owned_source_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        for name in ("ext/threadx/common/src/tx_thread_create.c",
                     "ext/threadx/ports/linux/gnu/src/tx_timer_interrupt.c",
                     "ext/threadx/ports/cortex_m4/gnu/src/tx_thread_schedule.S",
                     "ext/threadx/common/inc/tx_api.h", "ext/threadx/LICENSE.txt",
                     "os/threadx/Kernel.cmake", "tools/configure/threadx.py",
                     "tools/maintenance/threadx_lock.py"):
            with self.subTest(name=name):
                target = self.sdk / name
                old_source = target.read_bytes()
                try:
                    target.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("missing required sources/licenses", result.stderr)
                finally:
                    target.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_threadx_wrong_commit_or_source_identity_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        try:
            manifest = json.loads(old_identity)
            next(x for x in manifest["dependencies"] if x["path"] == "ext/threadx")["commit"] = "a" * 40
            identity.write_text(json.dumps(manifest) + "\n")
            self.assertNotEqual(self.verify().returncode, 0)
        finally:
            identity.write_bytes(old_identity)
        source = self.sdk / "ext/threadx/common/src/tx_thread_create.c"
        old_source = source.read_bytes()
        try:
            source.write_bytes(old_source + b"\n/* unreviewed source */\n")
            manifest = json.loads(old_identity)
            manifest["files_sha256"][source.relative_to(self.sdk).as_posix()] = source_sdk.digest(source.read_bytes())
            manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                manifest["files_sha256"], sort_keys=True, separators=(",", ":")).encode())
            identity.write_text(json.dumps(manifest) + "\n")
            self.assertNotEqual(self.verify().returncode, 0)
        finally:
            source.write_bytes(old_source)
            identity.write_bytes(old_identity)

    def test_installed_threadx_cli_does_not_write_into_readonly_sdk(self):
        before = {p.relative_to(self.sdk).as_posix() for p in self.sdk.rglob("*") if p.is_file()}
        command = [sys.executable, str(self.sdk / "tools/maintenance/threadx_lock.py"),
                   "--source", str(self.sdk / "ext/threadx"),
                   "--lock", str(self.sdk / "dependencies/threadx.lock.json"),
                   "--port", "linux"]
        result = self.run_process(command)
        self.assertEqual(result.returncode, 0, result.stderr)
        after = {p.relative_to(self.sdk).as_posix() for p in self.sdk.rglob("*") if p.is_file()}
        self.assertEqual(after, before)

    def test_installed_threadx_cli_requires_verified_manifest_not_missing_git_fallback(self):
        command = [sys.executable, str(self.sdk / "tools/maintenance/threadx_lock.py"),
                   "--source", str(self.sdk / "ext/threadx"),
                   "--lock", str(self.sdk / "dependencies/threadx.lock.json"),
                   "--port", "linux"]
        result = self.run_process(command)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["source_kind"], "installed_source_sdk")
        source = self.sdk / "ext/threadx/common/src/tx_thread_create.c"
        old_source = source.read_bytes()
        try:
            source.write_bytes(b"corrupted")
            result = self.run_process(command)
            self.assertNotEqual(result.returncode, 0)
        finally:
            source.write_bytes(old_source)

    def test_rehashed_g4_source_notice_lock_and_verifier_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        lock = json.loads((self.sdk / "dependencies/stm32g4.lock.json").read_text())
        names = ["vendors/st/cmsis_device_g4/" + name for name in lock["files"]]
        names.extend(("dependencies/stm32g4.lock.json",
                      "tools/maintenance/stm32g4_lock.py"))
        for name in names:
            with self.subTest(name=name):
                file = self.sdk / name
                old_source = file.read_bytes()
                try:
                    file.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    self.assertNotEqual(self.verify().returncode, 0)
                finally:
                    file.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_g4_wrong_commit_changed_source_or_reduced_graph_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        source = self.sdk / "vendors/st/cmsis_device_g4/Include/stm32g474xx.h"
        lock_path = self.sdk / "dependencies/stm32g4.lock.json"
        original_source = source.read_bytes()
        original_lock = lock_path.read_bytes()
        try:
            manifest = json.loads(old_identity)
            next(x for x in manifest["dependencies"]
                 if x["path"] == "vendors/st/cmsis_device_g4")["commit"] = "a" * 40
            identity.write_text(json.dumps(manifest))
            self.assertNotEqual(self.verify().returncode, 0)
            for file, content in ((source, original_source + b"\n/* unreviewed */\n"),
                                  (lock_path, json.dumps({**json.loads(original_lock),
                                      "files": {}}).encode())):
                file.write_bytes(content)
                manifest = json.loads(old_identity)
                manifest["files_sha256"][file.relative_to(self.sdk).as_posix()] = source_sdk.digest(content)
                manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                    manifest["files_sha256"], sort_keys=True,
                    separators=(",", ":")).encode())
                identity.write_text(json.dumps(manifest))
                self.assertNotEqual(self.verify().returncode, 0)
                source.write_bytes(original_source)
                lock_path.write_bytes(original_lock)
        finally:
            source.write_bytes(original_source)
            lock_path.write_bytes(original_lock)
            identity.write_bytes(old_identity)

    def test_installed_g4_cli_verifies_manifest_readonly_without_git_fallback(self):
        before = {p.relative_to(self.sdk).as_posix()
                  for p in self.sdk.rglob("*") if p.is_file()}
        command = [sys.executable, str(self.sdk / "tools/maintenance/stm32g4_lock.py"),
                   "--source", str(self.sdk / "vendors/st/cmsis_device_g4"),
                   "--lock", str(self.sdk / "dependencies/stm32g4.lock.json")]
        result = self.run_process(command)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stdout)["verified_files"], 5)
        self.assertEqual(json.loads(result.stdout)["source_kind"], "installed_source_sdk")
        after = {p.relative_to(self.sdk).as_posix()
                 for p in self.sdk.rglob("*") if p.is_file()}
        self.assertEqual(after, before)
        source = self.sdk / "vendors/st/cmsis_device_g4/Include/stm32g474xx.h"
        old_source = source.read_bytes()
        try:
            source.write_bytes(b"corrupted")
            result = self.run_process(command)
            self.assertNotEqual(result.returncode, 0)
        finally:
            source.write_bytes(old_source)

    def rehash_manifest_file(self, manifest, file, data):
        file.write_bytes(data)
        manifest["files_sha256"][file.relative_to(self.sdk).as_posix()] = (
            source_sdk.digest(data))
        manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
            manifest["files_sha256"], sort_keys=True,
            separators=(",", ":")).encode())
        (self.sdk / source_sdk.MANIFEST).write_text(json.dumps(manifest) + "\n")

    def test_rehashed_modern_soc_owned_firmware_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        names = ["soc/stm32h743/power_boot.S",
                 "soc/stm32h743/private/boot_registers.h"]
        for family, board in (("stm32h743", "stm32h743_nucleo_zi2"),
                              ("stm32u575", "stm32u575_nucleo_zi_q")):
            names += [f"soc/{family}/" + name for name in (
                "CMakeLists.txt", "system.c", "drivers/gpio.c", "drivers/uart.c",
                f"private/{family}_system.h", f"private/{family}_provider.h",
                "soc.json", "routes.json")]
            names.append(f"boards/{board}/board.json")
        for name in names:
            with self.subTest(name=name):
                file = self.sdk / name
                old_source = file.read_bytes()
                try:
                    file.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    result = self.verify()
                    self.assertNotEqual(result.returncode, 0,
                                        "Rehashed actual firmware omission accepted")
                    self.assertIn("missing required sources/licenses", result.stderr)
                finally:
                    file.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_modern_device_source_notice_lock_omissions_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        names = ["tools/maintenance/cmsis_device_lock.py"]
        for review in source_sdk.cmsis_device_lock.REVIEWED_DEVICES.values():
            names += [review["directory"] + "/" + name
                      for name in sorted(review["files"])]
            names.append(review["lock_name"])
        for name in names:
            with self.subTest(name=name):
                file = self.sdk / name
                old_source = file.read_bytes()
                try:
                    file.unlink()
                    manifest = json.loads(old_identity)
                    manifest["files_sha256"].pop(name)
                    manifest["snapshot_sha256"] = source_sdk.digest(json.dumps(
                        manifest["files_sha256"], sort_keys=True,
                        separators=(",", ":")).encode())
                    identity.write_text(json.dumps(manifest) + "\n")
                    self.assertNotEqual(self.verify().returncode, 0,
                                        "Rehashed original device input omission accepted")
                finally:
                    file.write_bytes(old_source)
                    identity.write_bytes(old_identity)

    def test_rehashed_modern_device_wrong_commit_changed_source_and_reduced_lock_reject(self):
        identity = self.sdk / source_sdk.MANIFEST
        old_identity = identity.read_bytes()
        for review in source_sdk.cmsis_device_lock.REVIEWED_DEVICES.values():
            with self.subTest(part=review["directory"]):
                source = self.sdk / review["directory"] / "LICENSE.md"
                lock = self.sdk / review["lock_name"]
                old_source, old_lock = source.read_bytes(), lock.read_bytes()
                try:
                    manifest = json.loads(old_identity)
                    next(record for record in manifest["dependencies"]
                         if record["path"] == review["directory"])["commit"] = "a" * 40
                    identity.write_text(json.dumps(manifest) + "\n")
                    self.assertNotEqual(self.verify().returncode, 0,
                                        "Wrong reviewed device commit accepted")
                    self.rehash_manifest_file(json.loads(old_identity), source,
                        old_source + b"\n/* unreviewed original notice */\n")
                    self.assertNotEqual(self.verify().returncode, 0,
                                        "Rehashed changed original notice accepted")
                    source.write_bytes(old_source)
                    reduced = {**json.loads(old_lock), "files": {}}
                    self.rehash_manifest_file(json.loads(old_identity), lock,
                                              json.dumps(reduced).encode())
                    self.assertNotEqual(self.verify().returncode, 0,
                                        "Rehashed reduced reviewed graph accepted")
                finally:
                    source.write_bytes(old_source)
                    lock.write_bytes(old_lock)
                    identity.write_bytes(old_identity)

    def test_installed_modern_device_cli_is_readonly_and_requires_complete_sdk(self):
        before = {p.relative_to(self.sdk).as_posix()
                  for p in self.sdk.rglob("*") if p.is_file()}
        for review in source_sdk.cmsis_device_lock.REVIEWED_DEVICES.values():
            with self.subTest(part=review["directory"]):
                command = [sys.executable,
                    str(self.sdk / "tools/maintenance/cmsis_device_lock.py"),
                    "--source", str(self.sdk / review["directory"]),
                    "--lock", str(self.sdk / review["lock_name"])]
                result = self.run_process(command)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                report = json.loads(result.stdout)
                self.assertEqual(report["verified_files"], 5)
                self.assertEqual(report["source_kind"], "installed_source_sdk")
                source = self.sdk / "core/src/request.c"
                old_source = source.read_bytes()
                try:
                    source.write_bytes(b"corrupted unrelated SDK owner source")
                    self.assertNotEqual(self.run_process(command).returncode, 0,
                                        "Device CLI ignored complete SDK proof")
                finally:
                    source.write_bytes(old_source)
        after = {p.relative_to(self.sdk).as_posix()
                 for p in self.sdk.rglob("*") if p.is_file()}
        self.assertEqual(after, before)

    def test_sdk_manifest_sources_exports_and_file_set_stay_unchanged_during_verify(self):
        # Inject at the external vendor-verifier boundary after the SDK has
        # read all hashes. The actual SDK verification runs without a fake map.
        external = source_sdk.threadx_lock.verify
        paths = (self.sdk / source_sdk.MANIFEST,
                 self.sdk / "core/src/request.c",
                 self.prefix / "lib/cmake/Nexus/NexusConfig.cmake")
        for file in paths:
            old = file.read_bytes()
            old_mode = file.stat().st_mode
            for change in ("rewrite-identical", "changed-bytes", "changed-mode"):
                with self.subTest(file=str(file), change=change):
                    def mutate(*args, **kwargs):
                        result = external(*args, **kwargs)
                        if change == "rewrite-identical":
                            replacement = file.with_name(file.name + ".replacement")
                            replacement.write_bytes(old)
                            replacement.replace(file)
                        elif change == "changed-bytes":
                            file.write_bytes(old + b"\n")
                        else:
                            file.chmod(old_mode ^ 0o100)
                        return result
                    try:
                        with mock.patch.object(source_sdk.threadx_lock, "verify",
                                               side_effect=mutate):
                            with self.assertRaisesRegex(ValueError, "changed during"):
                                source_sdk.verify(self.sdk)
                    finally:
                        file.write_bytes(old)
                        file.chmod(old_mode)
        extra = self.sdk / "late-added.c"
        def add_file(*args, **kwargs):
            result = external(*args, **kwargs)
            extra.write_text("int late_added;\n")
            return result
        try:
            with mock.patch.object(source_sdk.threadx_lock, "verify",
                                   side_effect=add_file):
                with self.assertRaisesRegex(ValueError, "changed during"):
                    source_sdk.verify(self.sdk)
        finally:
            extra.unlink(missing_ok=True)

    def test_final_file_read_cannot_hide_a_late_change_to_an_earlier_checked_input(self):
        last_export = self.prefix / "lib/cmake/Nexus/NexusConfigVersion.cmake"
        real_read = Path.read_bytes
        for target in (self.sdk / source_sdk.MANIFEST,
                       self.sdk / "core/src/request.c",
                       self.prefix / "lib/cmake/Nexus/NexusConfig.cmake"):
            with self.subTest(target=str(target)):
                old = target.read_bytes()
                calls = 0

                def read_file(path):
                    nonlocal calls
                    data = real_read(path)
                    if path == last_export:
                        calls += 1
                        if calls == 2:
                            # The final content-read sweep has already passed
                            # this earlier file. Same bytes still change inode.
                            replacement = target.with_name(target.name + ".replacement")
                            replacement.write_bytes(old)
                            replacement.replace(target)
                    return data

                try:
                    with mock.patch.object(Path, "read_bytes", new=read_file):
                        with self.assertRaisesRegex(ValueError, "changed during"):
                            source_sdk.verify(self.sdk)
                    self.assertEqual(calls, 2,
                                     "Filesystem injection did not reach final read")
                finally:
                    target.write_bytes(old)

    def test_git_checkout_inside_sdk_cannot_bypass_whole_manifest(self):
        for family, dependency, lock_name, extra in (
                ("G4", "vendors/st/cmsis_device_g4", "stm32g4", []),
                ("ThreadX", "ext/threadx", "threadx", ["--port", "linux"])):
            with self.subTest(dependency=dependency):
                source = self.sdk / dependency
                private = self.directory / ("private " + family + " Git metadata")
                self.run_ok(["git", "clone", "--shared", "--no-checkout", "--quiet",
                             str(ROOT / dependency), str(private)])
                lock_path = self.sdk / ("dependencies/" + lock_name + ".lock.json")
                lock = json.loads(lock_path.read_text())
                self.run_ok(["git", "-C", str(private), "remote", "set-url", "origin",
                             lock["repository"]])
                self.run_ok(["git", "-C", str(private), "config", "core.sparseCheckout", "true"])
                names = set(lock["files"]) | {lock["license"]["path"]}
                (private / ".git/info/sparse-checkout").write_text(
                    "".join("/" + name + "\n" for name in sorted(names)))
                self.run_ok(["git", "-C", str(private), "checkout", "--quiet", "--detach",
                             lock["commit"]])
                (private / ".git").rename(source / ".git")
                other = self.sdk / "CMakeLists.txt"
                original = other.read_bytes()
                try:
                    self.assertEqual(self.run_ok(["git", "-C", str(source), "status",
                                                 "--porcelain"]).stdout, "")
                    other.write_bytes(original + b"\n# altered SDK composition\n")
                    result = self.run_process([
                        sys.executable, "-B", str(self.sdk / ("tools/maintenance/" + lock_name + "_lock.py")),
                        "--source", str(source), "--lock", str(lock_path), *extra])
                    self.assertNotEqual(result.returncode, 0,
                                        "Valid source Git must not bypass SDK manifest verification")
                finally:
                    other.write_bytes(original)
                    shutil.rmtree(source / ".git")

    def test_additional_file_and_symlink_are_rejected(self):
        extra = self.sdk / "unexpected.c"
        try:
            extra.write_text("int unexpected;\n")
            self.assertNotEqual(self.verify().returncode, 0)
            extra.unlink()
            extra.symlink_to(self.sdk / "CMakeLists.txt")
            self.assertNotEqual(self.verify().returncode, 0)
        finally:
            extra.unlink(missing_ok=True)

    def test_default_preparation_rejects_dirty_checkout(self):
        repo = self.directory / "dirty repository"
        repo.mkdir()
        self.run_ok(["git", "init", "--quiet", str(repo)])
        (repo / "README.md").write_text("source\n")
        self.run_ok(["git", "-C", str(repo), "add", "README.md"])
        self.run_ok(["git", "-C", str(repo), "-c", "user.name=SDK test", "-c", "user.email=sdk-test@local.invalid",
                     "commit", "--quiet", "-m", "fixture baseline"])
        (repo / "README.md").write_text("modified\n")
        result = self.run_process([sys.executable, "-B", str(ROOT / "cmake/package/package_source_sdk.py"),
                           "--source", str(repo), "--output", str(self.directory / "must not publish")])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("dirty source checkout", result.stderr)
        self.assertFalse((self.directory / "must not publish").exists())

    def test_output_parent_alias_cannot_bypass_source_boundary(self):
        output = self.directory / ".." / self.directory.name / "aliased package"
        result = self.run_process([sys.executable, "-B", str(ROOT / "cmake/package/package_source_sdk.py"),
                                  "--source", str(ROOT), "--output", str(output), "--development-fixture"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("canonical path", result.stderr)
        self.assertFalse(output.exists())

    def test_clean_checkout_with_missing_dependency_gitlinks_is_rejected(self):
        repo = self.directory / "missing dependencies repository"
        repo.mkdir()
        self.run_ok(["git", "init", "--quiet", str(repo)])
        (repo / "README.md").write_text("source\n")
        (repo / ".gitmodules").write_bytes((ROOT / ".gitmodules").read_bytes())
        self.run_ok(["git", "-C", str(repo), "add", "README.md", ".gitmodules"])
        self.run_ok(["git", "-C", str(repo), "-c", "user.name=SDK test", "-c", "user.email=sdk-test@local.invalid",
                     "commit", "--quiet", "-m", "fixture baseline"])
        result = self.run_process([sys.executable, "-B", str(ROOT / "cmake/package/package_source_sdk.py"),
                                  "--source", str(repo), "--output", str(self.directory / "must not export empty SDK")])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("required dependency Gitlink", result.stderr)
        self.assertFalse((self.directory / "must not export empty SDK").exists())


if __name__ == "__main__":
    unittest.main()
