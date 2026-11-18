"""Exercise change selection against two real Git trees and portable paths."""

import hashlib
import json
import shutil
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.testing import build_impact


class HardwareImpactTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.write(".gitignore", "/build/\n")
        for family, board in (("f4", "alpha"), ("g0", "beta")):
            self.write(f"soc/st/{family}/package.json", json.dumps(
                {"family": family, "scope": "production"}))
            self.write(f"boards/{board}/board.json", json.dumps(
                {"id": board, "soc": family + "-part", "soc_family": family}))
            self.assembly(board, board)
        self.git("add", ".")
        self.git("commit", "-qm", "Fixture with two unrelated families")

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root,
                                       text=True).strip()

    def write(self, name, data):
        target = self.root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(data)

    def assembly(self, name, board):
        self.write("tools/configure/assemblies/" + name + ".toml",
                   f'schema=2\nbackend="baremetal"\nboard="{board}"\n')

    def test_unborn_repository_uses_empty_base(self):
        self.git("symbolic-ref", "HEAD", "refs/heads/unborn")
        selected = build_impact.plan_hardware(self.root, ["boards/alpha/board.json"])
        self.assertEqual(selected["assemblies"], ["tools/configure/assemblies/alpha.toml"])

    def test_board_move_selects_both_trees_and_no_unrelated_family(self):
        self.assembly("alpha", "beta")
        self.assembly("new", "alpha")
        self.git("add", ".")
        result = build_impact.plan_hardware(
            self.root, ["boards/alpha/board.json", "tools/configure/assemblies/new.toml"])
        self.assertEqual(result["assemblies"], [
            "tools/configure/assemblies/alpha.toml", "tools/configure/assemblies/new.toml"])
        self.assertFalse(result["fallbacks"])
        self.assertEqual(result["contexts"]["tools/configure/assemblies/new.toml"]["part"],
                         "f4-part")

    def test_renamed_board_keeps_unmigrated_consumers_visible(self):
        self.git("mv", "boards/alpha", "boards/renamed")
        paths = ["boards/alpha/board.json", "boards/renamed/board.json"]
        result = build_impact.plan_hardware(self.root, paths)
        self.assertEqual(result["assemblies"], ["tools/configure/assemblies/alpha.toml"])

    def test_old_compiler_graph_survives_unrelated_edits_but_shared_inputs_expire(self):
        uart = "soc/st/f4/drivers/uart/uart.c"
        self.write(uart, "int counter;\n")
        self.write("core/private.h", "/* header */\n")
        self.git("add", ".")
        self.git("commit", "-qm", "Add independent driver and selected private header")
        tree = self.git("rev-parse", "HEAD^{tree}")
        assembly = "tools/configure/assemblies/alpha.toml"
        graph = build_impact._receipt_path(self.root, assembly)
        graph.parent.mkdir(parents=True)
        inputs = ["core/private.h"]
        graph.write_text(json.dumps({"tree": tree, "schema": 1, "assembly": assembly,
                                    "sha256": hashlib.sha256(inputs[0].encode()).hexdigest(),
                                    "inputs": inputs}))
        self.write("docs/note.md", "# A new document\n")
        self.git("add", ".")
        self.git("commit", "-qm", "Independent documentation")
        self.write(uart, "int counter = 1;\n")
        self.assertFalse(build_impact.plan_hardware(self.root, [uart])["assemblies"])
        self.write("core/private.h", '#include "uart.c"\n')
        self.git("add", "core")
        self.git("commit", "-qm", "Header now consumes the driver")
        selected = build_impact.plan_hardware(self.root, [uart])
        self.assertEqual(selected["assemblies"], [assembly])
        self.assertTrue(selected["fallbacks"])

    def test_new_family_keeps_f4_negative_receipts_and_unknown_additions_expire_them(self):
        uart = "soc/st/f4/drivers/uart.c"
        self.write(uart, "int uart;\n")
        self.assembly("alpha-uart", "alpha")
        self.git("add", ".")
        self.git("commit", "-qm", "F4 UART and empty assemblies")
        tree = self.git("rev-parse", "HEAD^{tree}")
        empty = "tools/configure/assemblies/alpha.toml"
        selected = "tools/configure/assemblies/alpha-uart.toml"
        for assembly, inputs in ((empty, [empty]), (selected, [selected, uart])):
            graph = build_impact._receipt_path(self.root, assembly)
            graph.parent.mkdir(parents=True)
            graph.write_text(json.dumps({
                "tree": tree, "schema": 1, "assembly": assembly, "inputs": inputs,
                "sha256": hashlib.sha256("\0".join(inputs).encode()).hexdigest(),
            }))
        self.write("soc/other/f3/package.json", '{"family":"f3"}')
        self.write("soc/other/f3/CMakeLists.txt", "# independently selected\n")
        self.write("soc/other/f3/driver.c", "int other;\n")
        self.write("boards/gamma/board.json", '{"soc":"f3-part","soc_family":"f3"}')
        self.write("boards/gamma/init.c", "int board;\n")
        self.assembly("gamma", "gamma")
        self.git("add", ".")
        self.git("commit", "-qm", "Register independent family, board and assembly")
        self.write(uart, "int uart = 1;\n")
        self.assertEqual(build_impact.plan_hardware(self.root, [uart])["assemblies"], [selected])
        self.write("core/pin_binding.h", "#define PIN 1\n")
        self.git("add", "core")
        self.git("commit", "-qm", "A new shared header may change include lookup")
        self.assertEqual(build_impact.plan_hardware(self.root, [uart])["assemblies"],
                         [selected, empty])

    def test_new_manifest_with_duplicate_family_fails_catalogue_validation(self):
        self.write("soc/other/conflict/package.json",
                   '{"family":"f4","scope":"production"}')
        self.git("add", "soc")
        with self.assertRaisesRegex(ValueError, "Duplicate production family"):
            build_impact.plan_hardware(self.root, ["soc/other/conflict/package.json"])

    def test_startup_or_common_build_rule_cannot_bypass_arm_link(self):
        for name in ("soc/st/f4/system/CMakeLists.txt", "soc/st/f4/system/startup.cmake",
                     "soc/st/f4/system/flash.ld"):
            with self.subTest(name=name):
                self.assertEqual(build_impact.plan_hardware(self.root, [name])["assemblies"],
                                 ["tools/configure/assemblies/alpha.toml"])
        for name in ("cmake/platform/Firmware.cmake", "CMakeLists.txt",
                     "tools/configure/emission.py"):
            self.assertEqual(len(build_impact.plan_hardware(self.root, [name])["assemblies"]), 2)

    def test_real_target_dependencies_exclude_unselected_target_and_expire(self):
        assembly = "tools/configure/assemblies/alpha.toml"
        directory = build_impact._receipt_path(self.root, assembly).parent
        directory.mkdir(parents=True)
        ninja = shutil.which("ninja") or str(Path(__file__).resolve().parents[2] / ".venv/bin/ninja")
        source = "soc/st/f4/drivers/uart.c"
        header = "soc/st/f4/drivers/uart.h"
        self.write(source, '#include "uart.h"\nint main(void) {return CODE;}\n')
        self.write(header, "#define CODE 0\n")
        self.write("unused.c", "int unused;\n")
        (directory / "CMakeCache.txt").write_text(
            f"CMAKE_MAKE_PROGRAM:FILEPATH={ninja}\nNEXUS_ASSEMBLY_FILE:FILEPATH={self.root / assembly}\n")
        (directory / "build.ninja").write_text(
            "rule cc\n  command = cc -MD -MF $out.d -o $out -c $in\n"
            "  depfile = $out.d\n  deps = gcc\n"
            f"build driver.o: cc {self.root / source}\n"
            f"build unused.o: cc {self.root / 'unused.c'}\n"
            "build nexus_firmware: phony driver.o\n")
        (directory / "generated").mkdir()
        for name in ("input_paths", "configuration_code_paths"):
            (directory / "generated" / (name + ".json")).write_text("{}")
        alias = directory / "header-link"
        alias.symlink_to(self.root / header)
        (directory / "generated/input_paths.json").write_text(json.dumps({
            "header": str(self.root / header), "alias": alias.name,
            "unowned": str(Path("/usr/include")), "generated": "CMakeCache.txt"}))
        self.git("add", ".")
        tree = self.git("write-tree")
        subprocess.run([ninja, "nexus_firmware"], cwd=directory,
                       capture_output=True, check=True)
        resolutions = []
        resolve = Path.resolve

        def measured(path, *args, **kwargs):
            resolutions.append(path)
            return resolve(path, *args, **kwargs)

        with patch.object(Path, "resolve", measured):
            self.assertTrue(build_impact.record_hardware_inputs(self.root, assembly, directory, tree))
        self.assertEqual(resolutions.count(self.root / header), 1)
        actual = json.loads((directory / "nexus-target-inputs.json").read_bytes())["inputs"]
        self.assertEqual(actual, sorted((source, header)))
        (directory / ".ninja_deps").unlink()
        self.assertFalse(build_impact.record_hardware_inputs(self.root, assembly, directory, tree))
        self.assertFalse((directory / "nexus-target-inputs.json").exists())

    def test_changed_broken_assembly_cannot_disappear_from_arm_gate(self):
        name = "tools/configure/assemblies/alpha.toml"
        self.write(name, 'schema=3\nbackend="baremetal"\nboard="alpha"\n')
        with self.assertRaisesRegex(ValueError, "schema"):
            build_impact.plan_hardware(self.root, [name])

    def test_layout_dependency_uses_its_owned_assembly(self):
        name = "tests/contracts/flash_mode/f4.toml"
        layout = "tests/contracts/flash_mode/image-layout.json"
        self.write(name, 'schema=2\nbackend="baremetal"\nboard="alpha"\nlayout="image-layout.json"\n')
        self.write(layout, '{}')
        self.git("add", ".")
        self.git("commit", "-qm", "Declared layout input")
        self.assertEqual(build_impact.plan_hardware(self.root, [layout])["assemblies"], [name])

    def test_supplier_and_overlay_use_actual_family_and_backend_owners(self):
        self.write("soc/st/f4/package.json", json.dumps({
            "family": "f4", "dependencies": {"hal": "stm32f4_hal"}}))
        self.write("tools/configure/assemblies/rtos.toml",
                   'schema=2\nbackend="freertos"\nboard="alpha"\n')
        self.git("add", ".")
        self.git("commit", "-qm", "One family owns a supplier and kernel")
        for name in ("vendors/st/stm32f4_hal", "dependencies/stm32f4-hal.lock.json"):
            self.assertEqual(build_impact.plan_hardware(self.root, [name])["assemblies"],
                             ["tools/configure/assemblies/alpha.toml",
                              "tools/configure/assemblies/rtos.toml"])
        for name in ("ext/freertos", "os/freertos/mpu/patches/arm_cm4_mpu-port.patch"):
            self.assertEqual(build_impact.plan_hardware(self.root, [name])["assemblies"],
                             ["tools/configure/assemblies/rtos.toml"])
        self.assertFalse(build_impact.plan_hardware(self.root,
                         ["vendors/st/stm32u5_hal"])["assemblies"])

    def test_family_fallback_is_local_and_public_header_expands(self):
        private = build_impact.plan_hardware(
            self.root, ["soc/st/f4/drivers/uart/uart.c"])
        self.assertEqual(private["assemblies"], ["tools/configure/assemblies/alpha.toml"])
        self.assertTrue(private["fallbacks"])
        public = build_impact.plan_hardware(self.root, ["io/include/nexus/io/uart.h"])
        self.assertEqual(len(public["assemblies"]), 2)
        self.assertTrue(public["fallbacks"])
        self.assertFalse(build_impact.plan_hardware(
            self.root, ["tools/documentation/test_cli.py"])["assemblies"])
