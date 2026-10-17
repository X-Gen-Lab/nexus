"""Actual normal ARM builds enforce selected H743 RAM audit dependencies.

Only disposable build outputs and fixture assemblies are changed. The normal
production CMake graph, compiler, post-link CLI and checker bodies remain real.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CLI = ROOT / "tools/measurement/h743_flash_ram.py"
REPORT_NAME = "io_evolution_fixture.h743-flash-ram.json"


class H743RamBuildGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.storage = tempfile.TemporaryDirectory(prefix="h743 build gate ")
        cls.root = Path(cls.storage.name)
        cls.projects = {}
        for profile in ("hsi32-fixed-cache-off", "hsi64-fixed-cache-off",
                        "hsi64-pll384-cache-off", "unselected"):
            directory = cls.root / profile
            directory.mkdir()
            assembly = directory / "platform.toml"
            clock = "hsi64-pll384-cache-off" if profile == "unselected" else profile
            text = ("schema = 2\nboard_package = " +
                    json.dumps(str(ROOT / "boards/stm32h743zi_hsi_reference")) +
                    '\nbackend = "baremetal"\nclock = ' + json.dumps(clock) +
                    '\n[memory]\nmain_stack_bytes = 2048\nlibc_heap_bytes = 0\n'
                    'static_ram_limit_bytes = 16384\nflash_load_limit_bytes = 65536\n')
            if profile != "unselected":
                text += '\n[flash.storage]\nbinding = "flash0"\nmode = "poll-word"\n'
            else:
                text += '\n[gpio.led]\nbinding = "led0"\nmode = "output"\n'
            assembly.write_text(text)
            build = directory / "build"
            command = ["cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
                       "-DCMAKE_TOOLCHAIN_FILE=" + str(ROOT / "cmake/toolchains/arm-gcc.cmake"),
                       "-DCMAKE_BUILD_TYPE=Release", "-DNEXUS_ASSEMBLY_FILE=" + str(assembly),
                       "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF",
                       "-DNEXUS_IO_EVOLUTION_FIXTURE=ON", "-DNEXUS_COMPILER_CACHE=OFF",
                       "-DNEXUS_IO_EVOLUTION_FIXTURE_MAIN=" + str(
                           ROOT / "tests/contracts/io_evolution_assembly/controller_main.c")]
            cls.run_command(command)
            cls.run_command(["cmake", "--build", str(build), "--target",
                             "io_evolution_fixture", "--parallel", "1"])
            cls.projects[profile] = build

    @classmethod
    def tearDownClass(cls):
        cls.storage.cleanup()

    @classmethod
    def run_command(cls, arguments, *, success=True):
        result = subprocess.run(arguments, cwd=ROOT, capture_output=True,
                                check=False, timeout=180)
        if success and result.returncode:
            raise AssertionError(repr((arguments, result.returncode,
                                       result.stdout, result.stderr)))
        return result

    def selected(self):
        return self.projects["hsi64-pll384-cache-off"]

    def report(self, build=None):
        path = (build or self.selected()) / "bin" / REPORT_NAME
        self.assertTrue(path.is_file(), "normal selected ARM build omitted its audit report")
        return json.loads(path.read_text())

    def invoke(self, *, report=None, commands=None, extra=()):
        self.assertTrue(CLI.is_file(), "maintained H743 post-link CLI is missing")
        build = self.selected()
        compiler = shutil.which("arm-none-eabi-objdump")
        self.assertTrue(compiler)
        output = report or self.root / (self.id().rsplit(".", 1)[-1] + ".json")
        argv = [sys.executable, "-B", str(CLI),
                "--elf", str(build / "bin/io_evolution_fixture.elf"),
                "--resolved", str(build / "generated/resolved.json"),
                "--compile-commands", str(commands or build / "compile_commands.json"),
                "--objdump", compiler, "--report", str(output), *extra]
        return self.run_command(argv, success=False), output

    def test_normal_selected_builds_audit_all_three_cache_off_profiles(self):
        for profile, build in self.projects.items():
            if profile == "unselected":
                continue
            with self.subTest(profile=profile):
                report = self.report(build)
                self.assertEqual("passed", report["status"])
                self.assertEqual(profile, report["clock_profile"])
                self.assertFalse(report["qualification"])
                self.assertEqual(4, len(report["stack_usage"]))
                self.assertEqual("passed", report["cpu_bridge"]["status"])
                self.assertEqual("passed", report["static_closure"]["status"])
                self.assertEqual("not_proved", report["cpu_bridge"]["callee_body"])

    def test_unselected_build_has_no_pod_audit_or_stack_usage_cost(self):
        build = self.projects["unselected"]
        self.assertFalse((build / "bin" / REPORT_NAME).exists())
        self.assertFalse(list(build.rglob("flash_ram*.su")))
        elf = (build / "bin/io_evolution_fixture.elf").read_bytes()
        from tools.measurement.elf import Elf32
        self.assertNotIn("nx_stm32h743_flash_ram_enter", Elf32(elf).symbols)

    def test_deleting_report_reexecutes_the_normal_link_audit(self):
        build = self.selected()
        previous = self.report()
        (build / "bin" / REPORT_NAME).unlink()
        self.run_command(["cmake", "--build", str(build), "--target",
                          "io_evolution_fixture", "--parallel", "1"])
        current = self.report()
        self.assertGreater(current["objdump_execution"]["started_ns"],
                           previous["objdump_execution"]["started_ns"])

    def test_deleting_a_declared_su_recompiles_and_regenerates_it(self):
        build = self.selected()
        previous = self.report()
        record = previous["stack_usage"][0]
        obj, su = Path(record["object"]["path"]), Path(record["su"]["path"])
        before = obj.stat().st_mtime_ns
        su.unlink()
        self.run_command(["cmake", "--build", str(build), "--target",
                          "io_evolution_fixture", "--parallel", "1"])
        self.assertTrue(su.is_file())
        self.assertGreater(obj.stat().st_mtime_ns, before)
        self.assertEqual("passed", self.report()["status"])

    def test_dynamic_su_is_rejected_by_cli_and_normal_link(self):
        build = self.selected()
        su = Path(self.report()["stack_usage"][0]["su"]["path"])
        original = su.read_bytes()
        try:
            self.assertIn(b"\tstatic", original)
            su.write_bytes(original.replace(b"\tstatic", b"\tdynamic"))
            result, output = self.invoke()
            self.assertNotEqual(0, result.returncode)
            self.assertEqual("failed", json.loads(output.read_text())["status"])
            result = self.run_command(["cmake", "--build", str(build), "--target",
                                       "io_evolution_fixture", "--parallel", "1"],
                                      success=False)
            self.assertNotEqual(0, result.returncode)
        finally:
            su.write_bytes(original)
            self.run_command(["cmake", "--build", str(build), "--target",
                              "io_evolution_fixture", "--parallel", "1"])

    def test_compile_metadata_cannot_drop_required_private_codegen_flags(self):
        rows = json.loads((self.selected() / "compile_commands.json").read_text())
        for row in rows:
            if row["file"].endswith("/drivers/flash_ram.c"):
                row["command"] = row["command"].replace("-mgeneral-regs-only", "")
        path = self.root / "changed-commands.json"
        path.write_text(json.dumps(rows))
        result, output = self.invoke(commands=path)
        self.assertNotEqual(0, result.returncode)
        self.assertEqual("failed", json.loads(output.read_text())["status"])

    def test_argument_database_binds_each_compiled_source_and_output(self):
        rows = json.loads((self.selected() / "compile_commands.json").read_text())
        selected = []
        for row in rows:
            if Path(row["file"]).name in {
                    "flash_ram.c", "flash_ram_owner.c",
                    "flash_ram_transaction.c", "flash_mpu.c"}:
                argv = shlex.split(row.pop("command"))
                row["arguments"] = argv
                row["output"] = argv[argv.index("-o") + 1]
                selected.append(row)
        path = self.root / "argument commands.json"
        path.write_text(json.dumps(rows))
        result, output = self.invoke(commands=path)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual("passed", json.loads(output.read_text())["status"])
        selected[0]["arguments"][-1] = selected[-1]["file"]
        path.write_text(json.dumps(rows))
        result, _ = self.invoke(commands=path)
        self.assertNotEqual(0, result.returncode,
                            "row.file must match the actual -c source argument")

    def test_report_cannot_overwrite_elf_or_alias_an_input(self):
        elf = self.selected() / "bin/io_evolution_fixture.elf"
        original = hashlib.sha256(elf.read_bytes()).hexdigest()
        result, _ = self.invoke(report=elf)
        self.assertNotEqual(0, result.returncode)
        for kind in ("symlink", "hardlink"):
            path = self.root / (kind + "-report.json")
            if kind == "symlink":
                path.symlink_to(elf)
            else:
                os.link(elf, path)
            result, _ = self.invoke(report=path)
            self.assertNotEqual(0, result.returncode)
        self.assertEqual(original, hashlib.sha256(elf.read_bytes()).hexdigest())

    def test_h743_sdk_payload_explicitly_retains_the_complete_audit_closure(self):
        from tools.configure.providers.descriptor import maintained
        required = {"tools/measurement/h743_flash_ram.py",
                    "tools/measurement/flash_execution.py",
                    "tools/measurement/flash_ram_static.py",
                    "tools/measurement/soc_costs.py", "tools/measurement/static_storage.py",
                    "cmake/platform/H743FlashRam.cmake",
                    "cmake/platform/H743StackUsage.cmake"}
        self.assertTrue(required.issubset(set(maintained("stm32h743").owned_payload)))

    def test_discovery_failure_never_writes_over_source_tool_object_or_su(self):
        spec = importlib.util.spec_from_file_location("h743_gate_scratch", CLI)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        scratch = self.root / "isolated SDK prefix"
        scratch.mkdir()
        module.ROOT = scratch
        for name in module.DEPENDENCIES:
            path = scratch / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes((ROOT / name).read_bytes())
        tools = []
        for name in ("compiler", "objdump"):
            path = scratch / name
            path.write_text("#!/bin/sh\nexit 1\n")
            path.chmod(0o755)
            tools.append(path)
        actual = json.loads((self.selected() / "compile_commands.json").read_text())
        rows, sources, objects, usage = [], [], [], []
        for name in module.RAM_SOURCES:
            row = next(row for row in actual if row["file"].endswith("/" + name))
            source = scratch / "soc/stm32h743/drivers" / name
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_bytes(Path(row["file"]).read_bytes())
            obj = scratch / (name + ".obj")
            obj.write_bytes(b"original object input\n")
            su = obj.with_suffix(".su")
            su.write_bytes(b"original stack input\n")
            argv = shlex.split(row["command"])
            argv[0] = str(tools[0])
            argv[argv.index("-o") + 1] = str(obj)
            argv[-1] = str(source)
            rows.append({"directory": str(scratch), "file": str(source),
                         "arguments": argv, "output": str(obj)})
            sources.append(source)
            objects.append(obj)
            usage.append(su)
        usage[-1].unlink()
        commands = scratch / "compile_commands.json"
        commands.write_text(json.dumps(rows))
        protected = [*sources, *tools, objects[0], usage[0]]
        original = {path: path.read_bytes() for path in protected}
        build = self.selected()
        from tools.evidence.common import EvidenceError
        for index, target in enumerate(protected):
            for suffix in module._outputs(scratch / "example.json"):
                with self.subTest(target=target.name, suffix=suffix):
                    report = target if suffix == "report" else (
                        scratch / (str(index) + "-" + suffix + ".json"))
                    alias = module._outputs(report)[suffix]
                    if alias != target:
                        os.link(target, alias)
                    try:
                        with self.assertRaises(EvidenceError):
                            module.audit(build / "bin/io_evolution_fixture.elf",
                                         build / "generated/resolved.json",
                                         commands, tools[1], report)
                    finally:
                        changed = [str(path) for path, content in original.items()
                                   if path.read_bytes() != content]
                        for path, content in original.items():
                            path.write_bytes(content)
                        if alias != target:
                            alias.unlink()
                        self.assertEqual([], changed, "protected inputs changed")


if __name__ == "__main__":
    unittest.main()
