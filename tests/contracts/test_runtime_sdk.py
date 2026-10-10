"""Build real CPU-only Runtime consumers and reject stale or incomplete SDKs."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
COMMANDS = []

APPLICATION = """#include "nexus/arch/arch.h"
#include "nexus/arch/atomic.h"
#include "nexus/core/request.h"
#include "nexus/io/gpio.h"
#include "nexus/os/baremetal.h"
#include "nexus_config.h"
#include <stdio.h>

nx_time_us_t nx_time_now_us(void) {
    return 42;
}

static uint64_t notification_clock(void* context) {
    (void)context;
    return nx_time_now_us();
}

int main(void) {
    nx_request_t request;
    nx_request_initialize(&request);
    if (nx_request_prepare(&request, nx_deadline_after(42, 100)) != NX_SUCCESS
        || nx_request_admit(&request, NX_REQUEST_ACTIVE) != NX_SUCCESS) {
        return 1;
    }
    nx_request_settle(&request, NX_SUCCESS, 3);
    nx_result_t result = NX_ERROR_STATE;
    size_t transferred = 0;
    if (nx_request_result(&request, &result, &transferred) != NX_SUCCESS
        || result != NX_SUCCESS || transferred != 3) {
        return 2;
    }
    nx_baremetal_notify_t notification;
    if (nx_baremetal_notify_init(&notification, notification_clock, NULL)
        != NX_SUCCESS) {
        return 3;
    }
    nx_wait_port_t port = nx_baremetal_notify_port(&notification);
    uint32_t sequence = port.arm(port.context);
    if (port.wait(port.context, sequence, 100) != NX_ERROR_BUSY
        || port.wake(port.context) != NX_SUCCESS
        || port.wait(port.context, sequence, 100) != NX_SUCCESS) {
        return 4;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    bool masked = nx_arch_irq_masks().primask == 1;
    nx_arch_irq_restore(saved);
    if (!masked || nx_arch_irq_masks().primask != 0
        || nx_arch_exception_number() != 0 || !nx_arch_is_privileged()
        || nx_gpio_port_write(NULL, 1, 0) != NX_ERROR_INVALID) {
        return 5;
    }
    uint32_t word = 7;
    uint32_t expected = 7;
    if (!nx_atomic_u32_compare_exchange_acq_rel(&word, &expected, 9)
        || nx_atomic_u32_load_acquire(&word) != 9) {
        return 6;
    }
    printf("%u\\n", (unsigned)NEXUS_CORE_HZ);
    return 0;
}
"""


def run_process(arguments):
    """Retain real commands when the development evidence path is supplied."""
    environment = os.environ.copy()
    for key in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH",
                "OBJC_INCLUDE_PATH", "CMAKE_INCLUDE_PATH",
                "CMAKE_LIBRARY_PATH", "LIBRARY_PATH"):
        environment.pop(key, None)
    tools = ROOT / (".venv/Scripts" if os.name == "nt" else ".venv/bin")
    environment["PATH"] = str(tools) + os.pathsep + environment["PATH"]
    command = [str(argument) for argument in arguments]
    started = time.monotonic()
    result = subprocess.run(command, env=environment, text=True,
                            capture_output=True, timeout=180)
    evidence = environment.get("NEXUS_RUNTIME_TEST_EVIDENCE")
    if evidence:
        directory = Path(evidence)
        directory.mkdir(parents=True, exist_ok=True)
        index = len(COMMANDS)
        (directory / f"{index:03d}.stdout.txt").write_text(result.stdout)
        (directory / f"{index:03d}.stderr.txt").write_text(result.stderr)
        COMMANDS.append({"command": command, "return_code": result.returncode,
                         "seconds": time.monotonic() - started})
        (directory / "commands.json").write_text(
            json.dumps(COMMANDS, indent=2) + "\n")
    return result


def run_ok(arguments):
    result = run_process(arguments)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result


def native_facts():
    soc = json.loads((ROOT / "soc/native/soc.json").read_text())
    return {"schema_version": 1, "cpu": soc["cpu"], "irq": soc["irq"],
            "clock_hz": soc["clock_profiles"]["model"]["core_hz"]}


def prepare_consumer(directory, entry, call=None):
    """Author only CPU facts and Runtime choices, with no Board or instances."""
    source = directory / "consumer source"
    build = directory / "consumer build"
    source.mkdir(parents=True)
    (source / "cpu.json").write_text(json.dumps(native_facts()) + "\n")
    (source / "runtime.toml").write_text(
        'schema_version = 1\ncpu_facts = "cpu.json"\n'
        'backend = "baremetal"\noptimization = "O2"\n')
    if call is None:
        call = 'nexus_add_runtime(ASSEMBLY "runtime.toml")\n'
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.31)\n"
        "project(runtime_consumer LANGUAGES C CXX)\n"
        "set(CMAKE_C_STANDARD 11)\nset(CMAKE_C_STANDARD_REQUIRED ON)\n"
        "set(CMAKE_C_EXTENSIONS OFF)\nset(CMAKE_CXX_STANDARD 17)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n"
        'set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")\n'
        + entry + call
        + "foreach(forbidden Nexus::Platform Nexus::Factory Nexus::Board "
          "Nexus::NativeModel Nexus::OSNative)\n"
          "    if(TARGET ${forbidden})\n"
          '        message(FATAL_ERROR "Unexpected platform ${forbidden}")\n'
          "    endif()\nendforeach()\n"
          "get_target_property(bundle Nexus::Runtime NEXUS_CONFIG_DIR)\n"
          'file(WRITE "${CMAKE_BINARY_DIR}/runtime-config-dir.txt" "${bundle}")\n'
          "foreach(language c cpp)\n"
          "    add_executable(app_${language} main.${language})\n"
          "    target_link_libraries(app_${language} PRIVATE Nexus::Runtime)\n"
          "endforeach()\n")
    for extension in ("c", "cpp"):
        (source / f"main.{extension}").write_text(APPLICATION)
    return source, build


class RuntimeConsumerAssertions:
    def configure(self, source, build, *arguments):
        return run_process(["cmake", "-S", source, "-B", build,
                            "-G", "Ninja", *arguments])

    def assert_configures(self, source, build, *arguments):
        result = self.configure(source, build, *arguments)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def assert_builds_and_runs(self, build, clock_hz=1000000):
        run_ok(["cmake", "--build", build, "--parallel", "2"])
        for name in ("app_c", "app_cpp"):
            executable = name + (".exe" if os.name == "nt" else "")
            result = run_ok([build / "bin" / executable])
            self.assertEqual(result.stdout.strip(), str(clock_hz))

    def assert_public_graph(self, build, original_source=None):
        commands = json.loads((build / "compile_commands.json").read_text())
        self.assertGreater(len(commands), 4)
        names = {Path(command["file"]).name for command in commands}
        self.assertTrue({"request.c", "nx_arch_native.c", "gpio.c",
                         "notify.c", "main.c", "main.cpp"} <= names)
        for command in commands:
            arguments = command.get("command", "")
            self.assertNotIn("/vendors/", arguments)
            self.assertNotIn("/soc/", arguments)
            self.assertNotIn("/io/native/", arguments)
            self.assertNotIn("/os/native/", arguments)
            self.assertNotIn("googletest", arguments)
            self.assertNotIn("googlemock", arguments)
            if original_source is not None:
                self.assertNotIn(str(original_source) + "/", arguments)


class RuntimeCMakeTests(RuntimeConsumerAssertions, unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="nexus CPU runtime ")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.entry = f'include("{ROOT}/cmake/platform/Runtime.cmake")\n'

    def consumer(self, call=None):
        return prepare_consumer(self.directory, self.entry, call)

    def test_native_c_and_cpp_link_shared_production_targets_and_run(self):
        source, build = self.consumer()
        self.assert_configures(source, build)
        self.assert_builds_and_runs(build)
        self.assert_public_graph(build)

    def test_changed_cpu_facts_reconfigure_and_publish_new_identity(self):
        source, build = self.consumer()
        self.assert_configures(source, build)
        self.assert_builds_and_runs(build)
        bundle = Path((build / "runtime-config-dir.txt").read_text())
        before = json.loads((bundle / "resolved-cpu.json").read_text())
        facts = native_facts()
        facts["clock_hz"] = 2000000
        (source / "cpu.json").write_text(json.dumps(facts) + "\n")
        self.assert_builds_and_runs(build, clock_hz=2000000)
        after = json.loads((bundle / "resolved-cpu.json").read_text())
        self.assertEqual(after["clock_hz"], 2000000)
        self.assertNotEqual(before["input_sha256"], after["input_sha256"])
        self.assertNotEqual(before["configuration_sha256"],
                            after["configuration_sha256"])

    def test_invalid_changed_facts_cannot_build_with_a_stale_bundle(self):
        source, build = self.consumer()
        self.assert_configures(source, build)
        self.assert_builds_and_runs(build)
        bundle = Path((build / "runtime-config-dir.txt").read_text())
        facts = native_facts()
        facts["clock_hz"] = 0
        (source / "cpu.json").write_text(json.dumps(facts) + "\n")
        result = run_process(["cmake", "--build", build, "--parallel", "2"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("positive uint32", result.stdout + result.stderr)
        self.assertFalse((bundle / "resolved-cpu.json").exists())

    def test_missing_assembly_is_rejected(self):
        for index, call in enumerate(("nexus_add_runtime()\n",
                                      "nexus_add_runtime(ASSEMBLY)\n")):
            with self.subTest(call=call):
                source, build = prepare_consumer(
                    self.directory / str(index), self.entry, call)
                result = self.configure(source, build)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("ASSEMBLY", result.stdout + result.stderr)

    def test_unknown_argument_cannot_override_the_cpu_assembly(self):
        source, build = self.consumer(
            'nexus_add_runtime(ASSEMBLY "runtime.toml" CPU "cortex-m4")\n')
        result = self.configure(source, build)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("known arguments", result.stdout + result.stderr)

    def test_duplicate_assembly_keyword_is_rejected(self):
        source, build = self.consumer(
            'nexus_add_runtime(ASSEMBLY "runtime.toml" '
            'ASSEMBLY "runtime.toml")\n')
        result = self.configure(source, build)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("exactly one ASSEMBLY", result.stdout + result.stderr)

    def test_cache_cpu_override_is_rejected(self):
        source, build = self.consumer()
        result = self.configure(source, build, "-DNEXUS_CPU_ARCH=cortex-m4")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not a runtime override", result.stdout + result.stderr)

    def test_second_assembly_cannot_replace_a_resolved_target_graph(self):
        source, build = self.consumer(
            'nexus_add_runtime(ASSEMBLY "runtime.toml")\n'
            'nexus_add_runtime(ASSEMBLY "second.toml")\n')
        (source / "second.toml").write_bytes(
            (source / "runtime.toml").read_bytes())
        result = self.configure(source, build)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("only one resolved Nexus assembly",
                      result.stdout + result.stderr)


class RuntimeInstalledSDKTests(RuntimeConsumerAssertions, unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="runtime source SDK ")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.directory = Path(cls.temporary.name)
        run_ok(["git", "init", "--quiet", cls.directory])
        (cls.directory / "parent.txt").write_text("unrelated consumer owner\n")
        run_ok(["git", "-C", cls.directory, "add", "parent.txt"])
        run_ok(["git", "-C", cls.directory, "-c", "user.name=SDK test",
                "-c", "user.email=sdk-test@local.invalid", "commit",
                "--quiet", "-m", "independent runtime consumer"])
        original = cls.directory / "initial package"
        cls.prefix = cls.directory / "relocated prefix with spaces"
        run_ok([sys.executable, "-B", ROOT / "cmake/package/package_source_sdk.py",
                "--source", ROOT, "--output", original,
                "--development-fixture"])
        original.rename(cls.prefix)
        cls.sdk = cls.prefix / "share/nexus/src"
        cls.entry = "find_package(Nexus COMPONENTS Runtime REQUIRED)\n"

    def setUp(self):
        self.consumer_directory = self.directory / self._testMethodName

    def consumer(self):
        return prepare_consumer(self.consumer_directory, self.entry)

    def sdk_configure(self, source, build, fixture=True):
        arguments = [f"-DCMAKE_PREFIX_PATH={self.prefix}"]
        if fixture:
            arguments.append("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON")
        return self.configure(source, build, *arguments)

    def test_relocated_runtime_component_builds_c_and_cpp_without_checkout(self):
        source, build = self.consumer()
        result = self.sdk_configure(source, build)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_builds_and_runs(build)
        self.assert_public_graph(build, original_source=ROOT)
        bundle = Path((build / "runtime-config-dir.txt").read_text())
        self.assertEqual(bundle.parent.parent, build)
        self.assertFalse((build / "nexus-sdk").exists())

    def test_same_runtime_package_mode_is_idempotent_before_and_after_assembly(self):
        source, build = prepare_consumer(
            self.consumer_directory, self.entry + self.entry,
            'nexus_add_runtime(ASSEMBLY "runtime.toml")\n' + self.entry)
        result = self.sdk_configure(source, build)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_builds_and_runs(build)

    def test_different_sdk_prefixes_cannot_mix_before_runtime_assembly(self):
        second = self.consumer_directory / "second source SDK prefix"
        shutil.copytree(self.prefix, second)
        source = self.consumer_directory / "consumer source"
        build = self.consumer_directory / "consumer build"
        source.mkdir(parents=True)
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.31)\n"
            "project(two_sdk_sources LANGUAGES C)\n"
            'find_package(Nexus COMPONENTS Runtime REQUIRED PATHS '
            f'"{self.prefix}" NO_DEFAULT_PATH)\n'
            f'set(Nexus_DIR "{second}/lib/cmake/Nexus" CACHE PATH '
            '"second SDK source" FORCE)\n'
            "find_package(Nexus COMPONENTS Runtime REQUIRED)\n"
            'file(WRITE "${CMAKE_BINARY_DIR}/second-source.txt" '
            '"${Nexus_SOURCE_DIR}")\n')
        result = self.configure(source, build,
                                "-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON")
        if result.returncode == 0:
            self.assertEqual((build / "second-source.txt").read_text(),
                             str(second / "share/nexus/src"))
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("only one resolved Nexus source SDK",
                      " ".join((result.stdout + result.stderr).split()))

    def test_development_sdk_requires_explicit_fixture_opt_in(self):
        source, build = self.consumer()
        result = self.sdk_configure(source, build, fixture=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("nonpublishable development fixture",
                      result.stdout + result.stderr)

    def test_modified_cpu_resolver_is_rejected_before_runtime_generation(self):
        target = self.sdk / "tools/configure/cpu.py"
        original = target.read_bytes()
        try:
            target.write_bytes(original + b"\n# tampered after packaging\n")
            source, build = self.consumer()
            result = self.sdk_configure(source, build)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("file identity changed: tools/configure/cpu.py",
                          " ".join((result.stdout + result.stderr).split()))
            self.assertFalse((build / "nexus-runtime/generated").exists())
        finally:
            target.write_bytes(original)

    def test_modified_manifest_identity_is_rejected_before_generation(self):
        target = self.sdk / ".nexus-source-sdk.json"
        original = target.read_bytes()
        try:
            manifest = json.loads(original)
            manifest["snapshot_sha256"] = "0" * 64
            target.write_text(json.dumps(manifest) + "\n")
            source, build = self.consumer()
            result = self.sdk_configure(source, build)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("file-map identity changed",
                          result.stdout + result.stderr)
        finally:
            target.write_bytes(original)

    def test_rehashed_missing_runtime_and_cpu_mechanisms_fail_closed(self):
        identity = self.sdk / ".nexus-source-sdk.json"
        original_identity = identity.read_bytes()
        for index, name in enumerate((
                "cmake/platform/Runtime.cmake", "tools/configure/runtime.py",
                "tools/configure/cpu.py", "arch/include/nexus/arch/atomic.h",
                "arch/include/nexus/arch/features.h",
                "arch/cortex_m/private/mechanisms.h",
                "arch/cortex_m/nx_arch_cache.c",
                "arch/cortex_m/nx_arch_mpu.c",
                "arch/cortex_m/nx_arch_security.c")):
            with self.subTest(name=name):
                target = self.sdk / name
                original_source = target.read_bytes()
                try:
                    target.unlink()
                    manifest = json.loads(original_identity)
                    manifest["files_sha256"].pop(name)
                    encoded = json.dumps(manifest["files_sha256"],
                                         sort_keys=True, separators=(",", ":"))
                    manifest["snapshot_sha256"] = hashlib.sha256(
                        encoded.encode()).hexdigest()
                    identity.write_text(json.dumps(manifest) + "\n")
                    source, build = prepare_consumer(
                        self.consumer_directory / str(index), self.entry)
                    result = self.sdk_configure(source, build)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("missing required sources/licenses",
                                  " ".join((result.stdout + result.stderr).split()))
                finally:
                    target.write_bytes(original_source)
                    identity.write_bytes(original_identity)

    def assert_package_modes_rejected(self, first, second):
        source = self.consumer_directory / "mode consumer source"
        build = self.consumer_directory / "mode consumer build"
        source.mkdir(parents=True)
        entries = {
            "Runtime": "find_package(Nexus COMPONENTS Runtime REQUIRED)\n",
            "Platform": "find_package(Nexus REQUIRED)\n"}
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.31)\n"
            "project(mixed_modes LANGUAGES C)\n"
            + entries[first] + entries[second])
        result = self.configure(
            source, build, f"-DCMAKE_PREFIX_PATH={self.prefix}",
            "-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON",
            f"-DNEXUS_ASSEMBLY_FILE={self.sdk}/tools/configure/assemblies/native.toml")
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("mode", result.stdout + result.stderr)

    def test_runtime_then_platform_package_modes_cannot_mix(self):
        self.assert_package_modes_rejected("Runtime", "Platform")

    def test_platform_then_runtime_package_modes_cannot_mix(self):
        self.assert_package_modes_rejected("Platform", "Runtime")


if __name__ == "__main__":
    unittest.main()
