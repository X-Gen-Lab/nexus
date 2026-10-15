"""Real Ninja consumer proves stable generation and precise incremental work."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import configure


class IncrementalConsumerTests(unittest.TestCase):
    def test_no_change_reconfigure_is_no_work_and_budget_edit_rebuilds(self):
        with tempfile.TemporaryDirectory(prefix="nexus-incremental-") as name:
            root = Path(name)
            assembly = root / "assembly.toml"
            assembly.write_text((configure.ROOT /
                "tools/configure/assemblies/native-empty.toml").read_text())
            (root / "main.c").write_text(
                '#include "nexus_config.h"\n'
                'int main(void) { return NEXUS_MAIN_STACK_BYTES == 0u; }\n')
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.31)\n'
                'project(NexusIncremental LANGUAGES C ASM)\n'
                'set(NEXUS_BUILD_TESTS OFF CACHE BOOL "")\n'
                'set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "")\n'
                'set(NEXUS_ASSEMBLY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assembly.toml")\n'
                'add_subdirectory("' + configure.ROOT.as_posix() + '" nexus)\n'
                'nexus_add_firmware(sample SOURCES main.c)\n')
            build = root / "build"
            commands = []
            def execute(argv):
                result = subprocess.run(argv, cwd=root, capture_output=True,
                                        text=True, timeout=120, check=False)
                commands.append({"argv": argv, "returncode": result.returncode,
                                 "stdout": result.stdout, "stderr": result.stderr})
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result.stdout + result.stderr
            configure_command = ["cmake", "-S", str(root), "-B", str(build),
                                 "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"]
            build_command = ["cmake", "--build", str(build), "--parallel", "2"]
            execute(configure_command)
            execute(build_command)
            self.assertIn("no work to do", execute(build_command))
            generated = build / "nexus/generated"
            before = {path.name: (path.read_bytes(), path.stat().st_mtime_ns)
                      for path in generated.iterdir()}
            execute(configure_command)
            after = {path.name: (path.read_bytes(), path.stat().st_mtime_ns)
                     for path in generated.iterdir()}
            self.assertEqual(before, after)
            self.assertIn("no work to do", execute(build_command))
            assembly.write_text(assembly.read_text().replace(
                "main_stack_bytes = 2048", "main_stack_bytes = 4096"))
            self.assertIn("Building C object", execute(build_command))
            self.assertEqual(before["nexus_factory.h"], (
                (generated / "nexus_factory.h").read_bytes(),
                (generated / "nexus_factory.h").stat().st_mtime_ns))
            self.assertIn("no work to do", execute(build_command))
            # Keep structured execution when a caller explicitly requests an
            # external receipt. Test qualification still uses fresh unittest.
            import os
            destination = os.environ.get("NEXUS_INCREMENTAL_RECEIPT")
            if destination:
                Path(destination).write_text(json.dumps({
                    "schema_version": 1, "status": "passed", "commands": commands,
                    "no_change_file_count": len(before)}, indent=2) + "\n")


if __name__ == "__main__":
    unittest.main()
