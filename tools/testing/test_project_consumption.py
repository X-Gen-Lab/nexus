"""Create and execute an independent public Native project from pinned bytes."""

import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from tools.configure import metadata
from tools.dev import projects


ROOT = Path(__file__).resolve().parents[2]


class CreatedProjectConsumptionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="public project ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.templates = self.root / "template checkout"
        self.templates.mkdir()
        self.run_checked(["git", "init", "--quiet", str(self.templates)])
        self.git("config", "user.name", "Public fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        payload = {
            "CMakeLists.txt": (
                'cmake_minimum_required(VERSION 3.31)\n'
                'project(public_created_project LANGUAGES C ASM)\n'
                'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
                'set(NEXUS_ASSEMBLY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assembly.toml" '
                'CACHE FILEPATH "Product assembly")\n'
                'set(NEXUS_BUILD_TESTS OFF CACHE BOOL "Host tests" FORCE)\n'
                'set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Workload" FORCE)\n'
                'add_subdirectory("${NEXUS_SOURCE}" platform EXCLUDE_FROM_ALL)\n'
                'nexus_add_firmware(public_application SOURCES main.c)\n'
                'enable_testing()\n'
                'add_test(NAME public_created_project COMMAND public_application)\n'
                'set_tests_properties(public_created_project PROPERTIES TIMEOUT 10)\n'
            ).encode(),
            "assembly.toml": (
                'schema=2\nboard="native_reference"\nbackend="native"\n'
                'clock="model"\n[memory]\nmain_stack_bytes=2048\n'
                'libc_heap_bytes=0\n[gpio.visible]\nbinding="led0"\n'
                'mode="output"\n'
            ).encode(),
            "main.c": (
                '#include "nexus_bindings.h"\n'
                '#include "nexus_factory_strong.h"\n'
                '#if !defined(__STDC_VERSION__) || __STDC_VERSION__ != 201112L\n'
                '#error "The public application requires ISO C11"\n'
                '#endif\n'
                'int main(void) {\n'
                '    const nx_gpio_port_t* visible = '
                'nx_factory_gpio_ref(NX_GPIO_REF_VISIBLE);\n'
                '    if (visible == NULL) return 1;\n'
                '    nx_platform_start_result_t start = nx_platform_start();\n'
                '    if (start.primary != NX_SUCCESS) return 2;\n'
                '    nx_result_t write = nx_gpio_port_write(visible, 1U, 0U);\n'
                '    nx_result_t stop = nx_platform_stop();\n'
                '    return write == NX_SUCCESS && stop == NX_SUCCESS ? 0 : 3;\n'
                '}\n'
            ).encode(),
        }
        template = self.templates / "templates/public-native"
        template.mkdir(parents=True)
        for name, contents in payload.items():
            (template / name).write_bytes(contents)
        policy = metadata.load_policy(ROOT)
        manifest = {
            "schema": 1, "version": 1, "templates": {"public-native": {
                "platform": {"interface_epoch": policy["factory_abi_epoch"],
                             "assembly_schema": 2},
                "files": {name: hashlib.sha256(contents).hexdigest()
                          for name, contents in payload.items()}}},
        }
        (self.templates / "templates/manifest.json").write_text(
            json.dumps(manifest) + "\n")
        (self.templates / "dependencies").mkdir()
        lock = {"schema_version": 1,
                "commit": self.run_checked(
                    ["git", "-C", str(ROOT), "rev-parse", "HEAD"]).stdout.strip(),
                "tree": self.run_checked(
                    ["git", "-C", str(ROOT), "rev-parse", "HEAD^{tree}"]
                ).stdout.strip()}
        (self.templates / "dependencies/nexus.lock.json").write_text(
            json.dumps(lock) + "\n")
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "Create public integration fixture")
        self.revision = self.git("rev-parse", "HEAD").stdout.strip()
        self.output = self.root / "independent application"

    def run_checked(self, arguments):
        environment = dict(os.environ)
        environment["NEXUS_BUILD_JOBS"] = "1"
        environment["CMAKE_BUILD_PARALLEL_LEVEL"] = "1"
        for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH",
                     "OBJC_INCLUDE_PATH", "CMAKE_INCLUDE_PATH",
                     "CMAKE_LIBRARY_PATH", "LIBRARY_PATH"):
            environment.pop(name, None)
        result = subprocess.run(arguments, cwd=self.root, env=environment,
                                capture_output=True, text=True, timeout=120,
                                check=False)
        self.assertEqual(result.returncode, 0,
                         repr(arguments) + "\n" + result.stdout + result.stderr)
        return result

    def git(self, *arguments):
        return self.run_checked(["git", "-C", str(self.templates), *arguments])

    def test_create_commands_configure_build_and_execute_the_c11_application(self):
        result = projects.create(self.templates, self.revision, "public-native",
                                 self.output, source_root=ROOT, test_support=True)
        commands = result["commands"]
        self.assertEqual(set(commands), {"configure", "build", "test"})
        self.assertIn("-DNEXUS_SOURCE=" + str(ROOT), commands["configure"])
        self.assertFalse(result["qualification"])
        for phase in ("configure", "build", "test"):
            execution = self.run_checked(commands[phase])
            if phase == "test":
                self.assertIn("public_created_project", execution.stdout)
                self.assertIn("100% tests passed", execution.stdout)
        entries = json.loads(
            (self.output / "build/compile_commands.json").read_text())
        application, = [entry for entry in entries
                        if Path(entry["file"]) == self.output / "main.c"]
        arguments = application.get("arguments") or shlex.split(
            application["command"])
        self.assertIn("-std=c11", arguments)
        self.assertFalse((self.output / ".git").exists())


if __name__ == "__main__":
    unittest.main()
