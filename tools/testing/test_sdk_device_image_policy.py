"""Installed device and image-window profiles retain their exact contracts."""

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


class SDKDeviceImagePolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for name, path in (("consumers", "verify_consumers.py"),
                           ("packager", "package_source_sdk.py")):
            spec = importlib.util.spec_from_file_location(
                "device_image_" + name, ROOT / "cmake/package" / path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            setattr(cls, name, module)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="device image SDK ")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def profile(self, name):
        return next(item for item in transport.SERVICE_CONSUMERS
                    if item.name == name)

    def prepared(self, name):
        profile = self.profile(name)
        prefix = self.directory / "prefix"
        sdk = prefix / "share/nexus/src"
        shutil.copytree(ROOT / "cmake/package/verification" / profile.scenario,
                        sdk / "cmake/package/verification" / profile.scenario)
        (sdk / "dependencies").mkdir()
        shutil.copyfile(ROOT / "dependencies/platform-compatibility.json",
                        sdk / "dependencies/platform-compatibility.json")
        (sdk / ".nexus-source-sdk.json").write_text("{}\n")
        def execute(argv):
            return subprocess.run(argv, check=True, capture_output=True, text=True)
        source, build, command = self.consumers.prepare_service_consumer(
            prefix, self.directory, profile, execute,
            expected_revision="a" * 40, fixture=True)
        return sdk, source, build, command, profile

    def image_fixture(self, name):
        sdk, source, build, command, profile = self.prepared(name)
        config = build / "nexus-sdk/generated"
        config.mkdir(parents=True)
        (build / "resolved-sdk.txt").write_text(str(sdk))
        (build / "resolved-config.txt").write_text(str(config))
        (config / "bindings.c").write_text("/* test double */\n")
        resolved = configure.resolve_ir(source / "assembly.toml", root=ROOT, test_support=True).to_dict()
        (config / "resolved.json").write_text(json.dumps(resolved))
        rows = []
        flags = ["-std=c11"] + (["-mcpu=cortex-m4", "-mthumb",
                 "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-fshort-enums"]
                if profile.arm else [])
        for path in (source / "main.c", source / "private_headers.c",
                     config / "bindings.c"):
            rows.append({"file": str(path), "directory": str(build),
                         "arguments": ["cc", *flags, "-c", str(path)]})
        (build / "compile_commands.json").write_text(json.dumps(rows))
        image = build / "bin" / ("installed_services.elf" if profile.arm
                                 else "installed_services")
        image.parent.mkdir()
        header = bytearray(64)
        header[:6] = (b"\x7fELF\x01\x01" if profile.arm else b"\x7fELF\x02\x01")
        header[18:20] = (40 if profile.arm else 62).to_bytes(2, "little")
        image.write_bytes(header)
        if profile.arm:
            (image.parent / "installed_services.resources.json").write_text(
                json.dumps({"status": "passed", "violations": [],
                            "elf": file_identity(image),
                            "resolved": file_identity(config / "resolved.json")}))
        calls = []
        def execute(argv):
            calls.append(argv)
            if "retention.py" in " ".join(argv):
                output = Path(argv[argv.index("--output") + 1])
                output.write_text(json.dumps({"status": "passed",
                                              "elf": file_identity(image)}))
            symbols = self.consumers.service_symbols(profile)
            return subprocess.CompletedProcess(argv, 0,
                "".join(name + " T 1000 10\n" for name in sorted(symbols)), "")
        return sdk, source, build, config, profile, calls, execute

    def test_device_payload_is_required_before_sdk_payload_reads(self):
        required = {"components/tmp117/CMakeLists.txt",
                    "components/tmp117/include/nexus/components/tmp117.h",
                    "components/tmp117/include/nexus/components/tmp117_i2c.h",
                    "components/tmp117/src/tmp117.c",
                    "components/tmp117/src/i2c_transport.c"}
        self.assertLessEqual(required, set(self.packager.REQUIRED_FILES))

    def test_device_preparation_is_c11_public_and_selects_real_component(self):
        for name in ("native tmp117", "f407 tmp117"):
            with self.subTest(name=name):
                # One clean temporary directory per canonical preparation.
                with tempfile.TemporaryDirectory() as temporary:
                    self.directory = Path(temporary)
                    _, source, _, command, profile = self.prepared(name)
                    cmake = (source / "CMakeLists.txt").read_text()
                    self.assertIn("Nexus::TMP117I2C", cmake)
                    self.assertIn("set(CMAKE_C_STANDARD 11)", cmake)
                    self.assertNotIn("Nexus::LwIP", cmake)
                    self.assertNotIn("LANGUAGES CXX", cmake)
                    self.assertEqual(any("CMAKE_TOOLCHAIN_FILE" in arg
                                         for arg in command), profile.arm)
                    resolved = configure.resolve_ir(source / "assembly.toml", root=ROOT, test_support=True).to_dict()
                    self.assertEqual(resolved["soc_family"],
                                     "stm32f4" if profile.arm else "native")

    def test_image_window_is_copied_with_one_authored_layout(self):
        _, source, _, _, _ = self.prepared("f407 image window")
        self.assertTrue((source / "layout.json").is_file())
        resolved = configure.resolve_ir(source / "assembly.toml", root=ROOT, test_support=True).to_dict()
        self.assertEqual(resolved["memory_budgets"]["image_prefix_bytes"], 512)
        self.assertEqual(resolved["memory_budgets"]["image_suffix_bytes"], 1024)
        self.assertEqual(resolved["layout"]["origin"], 0x08020000)

    def test_changed_layout_rejects_before_external_observation(self):
        sdk, source, build, _, profile, calls, execute = self.image_fixture(
            "f407 image window")
        (source / "layout.json").write_text("{}\n")
        with self.assertRaisesRegex(ValueError, "canonical scenario"):
            self.consumers.check_service_image(sdk, source, build, profile, execute)
        self.assertEqual(calls, [])

    def test_native_device_observation_executes_and_binds_public_sources(self):
        sdk, source, build, _, profile, calls, execute = self.image_fixture(
            "native tmp117")
        report = self.consumers.check_service_image(sdk, source, build, profile, execute)
        self.assertTrue(report["runtime_executed"])
        self.assertEqual(report["physical_status"], "not_executed")
        self.assertNotIn("forbidden_symbols", report)
        self.assertTrue(any(len(call) == 1 for call in calls))
        self.assertEqual(set(report["symbols"]), self.consumers.service_symbols(profile))

    def test_device_compile_c99_rejects_before_observation(self):
        sdk, source, build, _, profile, calls, execute = self.image_fixture(
            "native tmp117")
        path = build / "compile_commands.json"
        rows = json.loads(path.read_text())
        rows[0]["arguments"][1] = "-std=c99"
        path.write_text(json.dumps(rows))
        with self.assertRaisesRegex(ValueError, "C11"):
            self.consumers.check_service_image(sdk, source, build, profile, execute)
        self.assertEqual(calls, [])

    def test_missing_device_implementation_symbol_rejects(self):
        sdk, source, build, _, profile, _, execute = self.image_fixture(
            "native tmp117")
        def missing(argv):
            result = execute(argv)
            if argv[0] == "nm":
                result.stdout = "\n".join(line for line in result.stdout.splitlines()
                    if not line.startswith("nx_tmp117_poll "))
            return result
        with self.assertRaisesRegex(ValueError, "retention is incomplete"):
            self.consumers.check_service_image(sdk, source, build, profile, missing)

    def test_arm_device_conflicting_abi_rejects_before_observation(self):
        sdk, source, build, _, profile, calls, execute = self.image_fixture(
            "f407 tmp117")
        path = build / "compile_commands.json"
        rows = json.loads(path.read_text())
        rows[0]["arguments"].append("-mfloat-abi=soft")
        path.write_text(json.dumps(rows))
        with self.assertRaisesRegex(ValueError, "ABI differs"):
            self.consumers.check_service_image(sdk, source, build, profile, execute)
        self.assertEqual(calls, [])

    def test_arm_image_rejects_retained_heap_and_cpp_bodies(self):
        sdk, source, build, _, profile, _, execute = self.image_fixture(
            "f407 image window")
        for symbol in ("malloc", "__cxa_allocate_exception"):
            with self.subTest(symbol=symbol):
                def forbidden(argv):
                    result = execute(argv)
                    if argv[0] == "arm-none-eabi-nm":
                        result.stdout += symbol + " T 2000 10\n"
                    return result
                with self.assertRaisesRegex(ValueError, r"heap or C\+\+ runtime"):
                    self.consumers.check_service_image(
                        sdk, source, build, profile, forbidden)

    def test_layout_mutation_during_symbol_observation_is_terminal(self):
        sdk, source, build, _, profile, _, execute = self.image_fixture(
            "f407 image window")
        def changed(argv):
            result = execute(argv)
            if argv[0] == "arm-none-eabi-nm":
                with (source / "layout.json").open("a") as stream:
                    stream.write(" ")
            return result
        with self.assertRaisesRegex(ValueError, "inputs changed"):
            self.consumers.check_service_image(sdk, source, build, profile, changed)

    def test_image_observation_binds_layout_and_prohibits_heap(self):
        sdk, source, build, _, profile, _, execute = self.image_fixture(
            "f407 image window")
        report = self.consumers.check_service_image(sdk, source, build, profile, execute)
        self.assertFalse(report["runtime_executed"])
        self.assertEqual(report["inputs"]["consumer/layout.json"],
                         file_identity(source / "layout.json"))
        self.assertEqual(report["forbidden_symbols"],
                         {"libc_heap": [], "cpp_runtime": []})
        self.assertEqual(report["scope"],
                         "Executable window only; no bootloader, signing, activation "
                         "or physical execution. No measured stack or timing.")


if __name__ == "__main__":
    unittest.main()
