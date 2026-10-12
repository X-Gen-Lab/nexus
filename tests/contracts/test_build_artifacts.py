"""Rebuild actual F407 artifacts and resource checks through production CMake."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class ArtifactBuildTests(unittest.TestCase):
    def test_direct_target_recovers_artifacts_and_rechecks_all_checker_inputs(self):
        cmake = shutil.which("cmake")
        self.assertIsNotNone(cmake, "CMake is required for ARM build contracts")
        self.assertIsNotNone(shutil.which("arm-none-eabi-gcc"),
                             "ARM GCC is required for ARM build contracts")
        with tempfile.TemporaryDirectory(prefix="Nexus ARM artifact contract ") as value:
            root = Path(value)
            source = root / "consumer with spaces"
            source.mkdir()
            build = root / "build with spaces"
            checker = source / "checker"
            assembly = source / "assembly.toml"
            assembly.write_bytes((ROOT / "tools/configure/assemblies/"
                                  "qiming-baremetal-empty.toml").read_bytes())
            modules = ("tools/measurement/resources.py", "tools/measurement/elf.py",
                       "tools/evidence/common.py", "tools/evidence/identity.py",
                       "tools/configure/memory.py")
            for module in modules:
                destination = checker / module
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / module, destination)
            (source / "main.c").write_text(
                '#include "nexus_bindings.h"\n'
                'int main(void) {\n'
                '    nx_platform_start_result_t result = nx_platform_start();\n'
                '    return result.primary == NX_SUCCESS ? 0 : 1;\n}\n')
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.31)\n'
                'project(artifact_consumer LANGUAGES C ASM)\n'
                'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
                'set(NEXUS_BUILD_TESTS OFF CACHE BOOL "")\n'
                'set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "")\n'
                f'set(NEXUS_ASSEMBLY_FILE "{assembly.as_posix()}")\n'
                f'add_subdirectory("{ROOT.as_posix()}" nexus-owned)\n'
                f'set_property(TARGET nexus_platform PROPERTY NEXUS_SOURCE_DIR '
                f'"{checker.as_posix()}")\n'
                'nexus_add_stm32f407_sleep(platform_sleep)\n'
                'nexus_add_firmware(app SOURCES main.c LIBRARIES platform_sleep)\n'
                'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/sleep-policy.txt" '
                'CONTENT "$<TARGET_PROPERTY:platform_sleep,'
                'INTERPROCEDURAL_OPTIMIZATION>\\n")\n')
            commands = []

            def run(*arguments, success=True):
                completed = subprocess.run(arguments, capture_output=True,
                                           text=True, timeout=180, check=False)
                commands.append({"argv": list(arguments),
                                 "returncode": completed.returncode,
                                 "stdout": completed.stdout,
                                 "stderr": completed.stderr})
                if success:
                    self.assertEqual(completed.returncode, 0,
                                     completed.stdout + completed.stderr)
                else:
                    self.assertNotEqual(completed.returncode, 0,
                                        completed.stdout + completed.stderr)
                return completed

            run(cmake, "-S", str(source), "-B", str(build), "-G", "Ninja",
                f"-DCMAKE_TOOLCHAIN_FILE={ROOT}/cmake/toolchains/arm-gcc.cmake")

            def compile_application(*, success=True):
                return run(cmake, "--build", str(build), "--target", "app",
                           "--parallel", "2", success=success)

            compile_application()
            artifacts = [build / "bin" / ("app" + suffix)
                         for suffix in (".elf", ".bin", ".map", ".resources.json")]
            for artifact in artifacts:
                self.assertTrue(artifact.is_file(), artifact)
            elf_digest = hashlib.sha256(artifacts[0].read_bytes()).hexdigest()
            report = json.loads(artifacts[-1].read_text())
            self.assertEqual(report["status"], "passed")
            self.assertEqual(report["physical_status"], "not_executed")
            no_change = compile_application()
            self.assertIn("no work to do", no_change.stdout)
            for artifact in artifacts[1:]:
                artifact.unlink()
                recovered = compile_application()
                self.assertIn("ELF budget: passed", recovered.stdout)
                self.assertTrue(artifact.is_file())
                self.assertEqual(hashlib.sha256(artifacts[0].read_bytes()).hexdigest(),
                                 elf_digest)
            for module in modules:
                with (checker / module).open("a") as file:
                    file.write("\n# Incremental contract input mutation.\n")
                checked = compile_application()
                self.assertIn("ELF budget: passed", checked.stdout)
                self.assertEqual(hashlib.sha256(artifacts[0].read_bytes()).hexdigest(),
                                 elf_digest)
            budget = build / "nexus-owned/generated/resource-budget.json"
            original_budget = budget.read_bytes()
            changed = json.loads(original_budget)
            changed["flash_loaded_max"] = 0
            budget.write_text(json.dumps(changed))
            failed = compile_application(success=False)
            self.assertIn("ELF budget: failed", failed.stdout)
            self.assertEqual(json.loads(artifacts[-1].read_text())["status"], "failed")
            budget.write_bytes(original_budget)
            compile_application()
            final_report = json.loads(artifacts[-1].read_text())
            self.assertEqual(final_report["status"], "passed")
            compile_commands = json.loads((build / "compile_commands.json").read_text())
            app_command = next(entry["command"] for entry in compile_commands
                               if entry["file"] == str(source / "main.c"))
            self.assertNotIn("-Werror", app_command)
            for flag in ("-mcpu=cortex-m4", "-mfloat-abi=hard", "-fshort-enums"):
                self.assertIn(flag, app_command)
            assembly.write_text(assembly.read_text().replace(
                'optimization = "Os"', 'optimization = "Os-lto"'))
            compile_application()
            self.assertEqual((build / "sleep-policy.txt").read_text().strip(), "ON")
            lto_commands = json.loads((build / "compile_commands.json").read_text())
            for entry in lto_commands:
                if entry["file"].endswith(("/main.c", "/system.c", "/sleep.c")):
                    self.assertIn("-flto", entry["command"])
            lto_report = json.loads(artifacts[-1].read_text())
            self.assertEqual(lto_report["status"], "passed")
            destination = os.environ.get("NEXUS_BUILD_CONTRACT_EVIDENCE_DIR")
            if destination:
                output = Path(destination)
                output.mkdir(parents=True, exist_ok=True)
                result = {"schema_version": 1, "kind": "build_artifacts_contract",
                          "physical_status": "not_executed", "commands": commands,
                          "declared_translation_units": len(json.loads(
                              (build / "compile_commands.json").read_text())),
                          "first_build_compiled_translation_units": len(re.findall(
                              r"^\[\d+/\d+\] Building (?:C|ASM) object ",
                              commands[1]["stdout"], re.MULTILINE)),
                          "non_lto_resource_report": final_report,
                          "final_resource_report": lto_report,
                          "artifacts": [{"name": artifact.name,
                                         "bytes": artifact.stat().st_size,
                                         "sha256": hashlib.sha256(
                                             artifact.read_bytes()).hexdigest()}
                                        for artifact in artifacts]}
                (output / "arm-artifacts-contract.json").write_text(
                    json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    unittest.main()
