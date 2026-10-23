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
import re
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

    def invoke(self, *, report=None, commands=None, elf=None, extra=()):
        self.assertTrue(CLI.is_file(), "maintained H743 post-link CLI is missing")
        build = self.selected()
        compiler = shutil.which("arm-none-eabi-objdump")
        self.assertTrue(compiler)
        output = report or self.root / (self.id().rsplit(".", 1)[-1] + ".json")
        argv = [sys.executable, "-B", str(CLI),
                "--elf", str(elf or build / "bin/io_evolution_fixture.elf"),
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
                self.assertEqual("passed", report["ram_layout"]["status"])
                self.assertEqual("passed", report["cpu_bridge"]["status"])
                self.assertEqual("passed", report["static_closure"]["status"])
                self.assertEqual("separately_checked",
                                 report["cpu_bridge"]["callee_body"])
                closure = report["static_closure"]
                self.assertEqual("proved", closure["return_gate"]["status"])
                self.assertEqual("proved",
                                 closure["return_gate"]["device_protocol"])
                self.assertEqual("proved",
                                 closure["constant_memory"]["all_data_operands"])
                memory = closure["constant_memory"]
                self.assertEqual([], memory["unresolved_read_sites"])
                self.assertEqual([], memory["unresolved_write_sites"])
                pulses = closure["program_invocations"]
                self.assertEqual(1, pulses["maximum_invocations"])
                self.assertEqual(8, pulses["maximum_dynamic_word_stores"])

    def test_unselected_build_has_no_pod_audit_or_stack_usage_cost(self):
        build = self.projects["unselected"]
        self.assertFalse((build / "bin" / REPORT_NAME).exists())
        self.assertFalse(list(build.rglob("flash_ram*.su")))
        elf = (build / "bin/io_evolution_fixture.elf").read_bytes()
        from tools.measurement.elf import Elf32
        self.assertNotIn("nx_stm32h743_flash_ram_enter", Elf32(elf).symbols)

    def test_real_image_guard_record_must_match_its_linked_endpoints(self):
        from tools.measurement.elf import Elf32
        original = (self.selected() / "bin/io_evolution_fixture.elf").read_bytes()
        image = Elf32(original)
        address = image.symbol("g_nx_stm32h743_flash_image")
        load, = [row for row in image.loads
                 if row["virtual"] <= address < address + 16 <=
                 row["virtual"] + row["files"]]
        offset = load["offset"] + address - load["virtual"]
        changed = bytearray(original)
        changed[offset] ^= 4
        path = self.root / "changed-image-guard.elf"
        path.write_bytes(changed)
        result, output = self.invoke(elf=path)
        self.assertNotEqual(0, result.returncode,
                            "actual firmware guard bytes need layout admission")
        report = json.loads(output.read_text())
        self.assertEqual("failed", report["status"])
        self.assertIn("image guard record", report["failure"])
        self.assertIn(report["failure"], result.stderr.decode())

    def test_real_callee_cannot_hide_a_stack_address_from_its_caller(self):
        # The real caller retains its fixed workspace in r6 across this time
        # call, then stores timed_out through r6. A mutated callee must not
        # hide SP provenance when its nonvolatile outputs reach that store.
        result, output = self.mutate_function(
            "nx_stm32h743_flash_ram_time",
            bytes.fromhex("6e46012000217047"),
            "changed-callee-stack-address")
        self.assertNotEqual(0, result.returncode,
                            "actual callee output must reach caller analysis")
        report = json.loads(output.read_text())
        self.assertEqual("failed", report["status"])
        self.assertIn("stack-derived store", report["failure"])

    def mutate_function(self, name, prefix, label):
        from tools.measurement.elf import Elf32
        original = (self.selected() / "bin/io_evolution_fixture.elf").read_bytes()
        image = Elf32(original)
        symbol, = [row for row in image.defined_symbols
                   if row["name"] == name and row["type"] == 2]
        section = image.sections[symbol["section"]]
        start = (symbol["value"] & ~1) - section["address"] + section["offset"]
        self.assertGreaterEqual(symbol["size"], len(prefix))
        self.assertEqual(0, (symbol["size"] - len(prefix)) % 2)
        changed = bytearray(original)
        changed[start:start + symbol["size"]] = (
            prefix + b"\x00\xbf" * ((symbol["size"] - len(prefix)) // 2))
        path = self.root / (label + ".elf")
        path.write_bytes(changed)
        return self.invoke(elf=path)

    def decoded_function(self, name, *, source=None):
        from tools.measurement.elf import Elf32
        from tools.measurement.soc_costs import decode_symbol
        original = (self.selected() / "bin/io_evolution_fixture.elf").read_bytes()
        image = Elf32(original)
        index, symbol = next((index, row) for index, row in
            enumerate(image.defined_symbols) if row["type"] == 2 and
            (row["name"] == name or row["name"].startswith(name + ".")) and
            (source is None or row["file"] == source))
        first = symbol["value"] & ~1
        raw = (self.selected() / "bin" /
               "io_evolution_fixture.h743-flash-ram.objdump.raw").read_bytes()
        lines = []
        for line in raw.decode("ascii").splitlines(keepends=True):
            match = re.match(r"^\s*([0-9a-f]+):\s", line)
            if match and first <= int(match[1], 16) < first + symbol["size"]:
                lines.append(line)
        decoded = decode_symbol(image, index, "".join(lines).encode("ascii"))
        return image, decoded["instructions"]

    @staticmethod
    def branch_link(site, target):
        value = (target - site - 4) & ((1 << 25) - 1)
        s, i1, i2 = (value >> 24) & 1, (value >> 23) & 1, (value >> 22) & 1
        first = 0xf000 | (s << 10) | ((value >> 12) & 1023)
        last = (0xd000 | ((1 ^ i1 ^ s) << 13) | ((1 ^ i2 ^ s) << 11) |
                ((value >> 1) & 2047))
        return first.to_bytes(2, "little") + last.to_bytes(2, "little")

    @staticmethod
    def constant_word(register, value):
        result = b""
        for opcode, word in ((0xf240, value & 65535), (0xf2c0, value >> 16)):
            first = opcode | ((word >> 12) & 15) | (((word >> 11) & 1) << 10)
            last = (register << 8) | (((word >> 8) & 7) << 12) | (word & 255)
            result += first.to_bytes(2, "little") + last.to_bytes(2, "little")
        return result

    def mutate_instruction(self, image, address, encoded, label):
        section, = [row for row in image.sections if row["address"] <= address <
                    address + len(encoded) <= row["address"] + row["size"]]
        offset = section["offset"] + address - section["address"]
        changed = bytearray(image.data)
        changed[offset:offset + len(encoded)] = encoded
        path = self.root / (label + ".elf")
        path.write_bytes(changed)
        return self.invoke(elf=path, report=self.root / (label + ".json"))

    def test_real_busy_window_terminal_cannot_explicitly_read_flash(self):
        # MOVW/MOVT r3,0x08000000; LDR r2,[r3]; RAM self-loop.
        # Same function/layout/SU sizes and nonreturning exception graph.
        result, output = self.mutate_function(
            "nx_stm32h743_flash_ram_terminal",
            bytes.fromhex("40f20003c0f600031a68fee7"), "terminal-flash-read")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("constant-address Flash data read",
                      json.loads(output.read_text())["failure"])

    def test_real_busy_window_join_cannot_forget_a_flash_read(self):
        # The safe owned diagnostic alternative must not erase Flash at a join.
        from tools.measurement.elf import Elf32
        image = Elf32((self.selected() / "bin/io_evolution_fixture.elf").read_bytes())
        safe = image.symbol("g_nx_stm32h743_flash_ram_workspace") + 1744
        prefix = (bytes.fromhex("022904d0") + self.constant_word(3, safe) +
                  bytes.fromhex("03e0") + self.constant_word(3, 0x08000000) +
                  bytes.fromhex("1a68fee7"))
        result, output = self.mutate_function(
            "nx_stm32h743_flash_ram_terminal", prefix,
            "terminal-joined-flash-read")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("constant-address Flash data read",
                      json.loads(output.read_text())["failure"])

    def test_real_bridge_cannot_bypass_its_ram_gate(self):
        from tools.measurement.flash_ram_static import _decode
        image, rows = self.decoded_function("nx_stm32h743_flash_ram_enter")
        gate = image.symbol("nx_stm32h743_flash_ram_return") & ~1
        call, = [row for row in rows if _decode(row["address"],
                 bytes.fromhex(row["encoding"]), bridge=True).target == gate]
        result, output = self.mutate_instruction(
            image, call["address"], bytes.fromhex("00bf00bf"), "bridge-gate-bypass")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("fixed boundary enter protocol differs",
                      json.loads(output.read_text())["failure"])

    def test_real_busy_terminal_cannot_read_through_loaded_destination(self):
        from tools.measurement.elf import Elf32
        image = Elf32((self.selected() / "bin/io_evolution_fixture.elf").read_bytes())
        workspace = image.symbol("g_nx_stm32h743_flash_ram_workspace")
        prefix = self.constant_word(3, workspace) + bytes.fromhex("d3f8a0361a68fee7")
        result, output = self.mutate_function(
            "nx_stm32h743_flash_ram_terminal", prefix, "terminal-loaded-destination")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("unclassified RAM data operand",
                      json.loads(output.read_text())["failure"])

    def test_real_issue_cannot_invoke_the_program_leaf_twice(self):
        from tools.measurement.flash_ram_static import _decode
        image, rows = self.decoded_function("issue", source="flash_ram.c")
        target = image.symbol("nx_stm32h743_flash_ram_program_words") & ~1
        call, = [row for row in rows if _decode(row["address"],
                 bytes.fromhex(row["encoding"]), bridge=False).target == target]
        address = call["address"] + 4
        following, = [row for row in rows if row["address"] == address]
        self.assertEqual("bff34f8f", following["encoding"])
        result, output = self.mutate_instruction(
            image, address, self.branch_link(address, target), "repeat-program-leaf")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("program pulse invocation bound exceeded",
                      json.loads(output.read_text())["failure"])

    def test_real_c_callee_cannot_clobber_the_protected_mpu_capture(self):
        image, rows = self.decoded_function("busy", source="flash_ram.c")
        instruction, = [row for row in rows if row["encoding"] in
                        {"10f00700", "00f00700"}]
        result, output = self.mutate_instruction(
            image, instruction["address"], bytes.fromhex("804600bf"),
            "clobber-protected-capture")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("protected MPU capture",
                      json.loads(output.read_text())["failure"])

    def test_real_c_callee_cannot_write_back_a_protected_capture(self):
        image, rows = self.decoded_function("busy", source="flash_ram.c")
        workspace, = [symbol for symbol in image.defined_symbols if
                      symbol["name"] == "g_nx_stm32h743_flash_ram_workspace"]
        # EQ refines r10 to owned storage; LDR post-index still writes r10.
        prefix = (self.constant_word(3, workspace["value"] + 1728) +
                  bytes.fromhex("9a4502d15af8040b00207047"))
        result, output = self.mutate_instruction(
            image, rows[0]["address"], prefix, "writeback-protected-capture")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("protected MPU capture",
                      json.loads(output.read_text())["failure"])

    def test_real_return_gate_requires_each_observation_and_barrier(self):
        from tools.measurement.flash_execution import _fixed_boundary_instruction
        image, rows = self.decoded_function("nx_stm32h743_flash_ram_return")
        decoded = [(row, _fixed_boundary_instruction(
            row["address"], bytes.fromhex(row["encoding"]))) for row in rows]
        checks = (
            ("both-bank-status", "load", (3, 1, 272, 4, False)),
            ("busy-mask", "tsti", (2, 7)),
            ("effect-ledger", "load", (2, 12, 1788, 4, False)),
            ("bank-bound", "cmpi", (2, 1)),
            ("permitted-target-control", "bic", (3, 2, 0x30)),
            ("target-lock", "orri", (2, 2, 1)),
            ("target-control-readback", "load", (3, 1, 12, 4, False)),
            ("mpu-control-readback", "cmp", (2, 8)),
            ("mpu-region-readback", "load", (3, 1, 8, 4, False)),
            ("mpu-base-readback", "cmp", (2, 10)),
            ("mpu-attributes-readback", "cmp", (2, 11)),
            ("mpu-selector-restore", "cmp", (2, 9)),
            ("vector-readback", "load", (3, 1, 0, 4, False)),
        )
        for label, operation, arguments in checks:
            with self.subTest(obligation=label):
                row, = [row for row, inst in decoded if
                        (inst.op, inst.args) == (operation, arguments)]
                replacement = b"\x00\xbf" * (len(bytes.fromhex(
                    row["encoding"])) // 2)
                result, output = self.mutate_instruction(
                    image, row["address"], replacement, "gate-" + label)
                self.assertNotEqual(0, result.returncode)
                self.assertIn("fixed boundary return protocol differs",
                              json.loads(output.read_text())["failure"])
        for row, inst in decoded:
            if inst.op in ("dsb", "isb"):
                with self.subTest(barrier=inst.op, address=row["address"]):
                    result, output = self.mutate_instruction(
                        image, row["address"], bytes.fromhex("00bf00bf"),
                        "gate-barrier-" + str(row["address"]))
                    self.assertNotEqual(0, result.returncode)
                    self.assertIn("fixed boundary return protocol differs",
                                  json.loads(output.read_text())["failure"])

    def test_real_return_gate_rejects_wrong_vector_and_early_return(self):
        from tools.measurement.flash_execution import _fixed_boundary_instruction
        image, rows = self.decoded_function("nx_stm32h743_flash_ram_return")
        vector, = [inst for row in rows if (inst :=
                   _fixed_boundary_instruction(row["address"],
                   bytes.fromhex(row["encoding"]))).op == "literal" and
                   inst.args[0] == 2]
        wrong = image.symbol("g_pfnVectors") + 1024
        result, output = self.mutate_instruction(
            image, vector.args[1], wrong.to_bytes(4, "little"),
            "gate-wrong-original-vector")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("fixed boundary return protocol differs",
                      json.loads(output.read_text())["failure"])
        result, output = self.mutate_instruction(
            image, rows[0]["address"], bytes.fromhex("7047"),
            "gate-early-return")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("fixed boundary return protocol differs",
                      json.loads(output.read_text())["failure"])

    def test_real_program_leaf_has_exact_staged_reads_and_store_offsets(self):
        from tools.measurement.flash_execution import _fixed_boundary_instruction
        image, rows = self.decoded_function("nx_stm32h743_flash_ram_program_words")
        word, = [row for row in rows if (inst :=
                 _fixed_boundary_instruction(row["address"],
                 bytes.fromhex(row["encoding"]))).op == "load" and
                 inst.args == (2, 0, 1700, 4, False)]
        # An extra read through destination would read the physical busy bank.
        result, output = self.mutate_instruction(
            image, word["address"], bytes.fromhex("0a6800bf"),
            "program-destination-read")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("fixed boundary program_words protocol differs",
                      json.loads(output.read_text())["failure"])
        last, = [row for row in rows if (inst :=
                 _fixed_boundary_instruction(row["address"],
                 bytes.fromhex(row["encoding"]))).op == "store" and
                 inst.args == (2, 1, 28, 4, False)]
        # STR r2,[r1,#32] exceeds the single 32-byte flashword contract.
        result, output = self.mutate_instruction(
            image, last["address"], bytes.fromhex("0a62"),
            "program-store-outside-word")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("fixed boundary program_words protocol differs",
                      json.loads(output.read_text())["failure"])

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
            source = scratch / "soc/stm32/stm32h743/drivers" / name
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
