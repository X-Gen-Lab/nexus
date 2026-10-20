"""Whole actual-ARM mutation tests for bounded RAM CFG and stack admission.

The compiler generates every C .su record. The one assembly bridge is the real
production source and needs the separately maintained byte/CPU proof. None of
these tests executes a CPU, qualifies hardware or models the Flash protocol.
"""

from __future__ import annotations

from copy import deepcopy
import hashlib
import importlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.evidence.common import EvidenceError
from tools.measurement.elf import Elf32

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = Path(__file__).with_name("fixtures") / "h743_ram_static"
MODULE = "tools.measurement.flash_ram_static"


class H743FlashRamStaticTests(unittest.TestCase):
    """Each negative first links real bytes, then asserts its precise reason."""

    def setUp(self):
        self.compiler = (os.environ.get("NEXUS_ARM_GCC") or
                         shutil.which("arm-none-eabi-gcc"))
        self.assertTrue(self.compiler, "actual ARM compiler is required")
        self.storage = tempfile.TemporaryDirectory(prefix="h743-static-")
        self.addCleanup(self.storage.cleanup)
        self.directory = Path(self.storage.name)
        self.images = 0

    def command(self, argv, directory):
        result = subprocess.run(argv, cwd=directory, capture_output=True,
                                timeout=30, check=False)
        self.assertEqual(0, result.returncode,
                         repr((argv, result.stdout, result.stderr)))
        return result.stdout

    def link(self, mutation=None, *, duplicate_locals=False):
        directory = self.directory / str(self.images)
        self.images += 1
        directory.mkdir()
        source = directory / "program.c"
        shutil.copyfile(FIXTURE / "program.c", source)
        flags = [self.compiler, "-mcpu=cortex-m7", "-mthumb",
                 "-mfpu=fpv5-d16", "-mfloat-abi=hard", "-Os", "-fno-lto",
                 "-mgeneral-regs-only", "-fno-builtin", "-fstack-usage",
                 "-fno-stack-protector", "-fno-unwind-tables",
                 "-fno-asynchronous-unwind-tables", "-ffreestanding"]
        objects, usage = [], []
        sources = [source]
        if duplicate_locals:
            for label in ("a", "b"):
                local = directory / ("local_" + label + ".c")
                local.write_text(
                    '#define RAM __attribute__((section(".nx_flash_ram"),'
                    'noinline,used))\n'
                    'static RAM unsigned LocalLeaf(unsigned x) {\n'
                    'volatile unsigned lane[2]; lane[0]=x;'
                    'return lane[0]+1; }\n'
                    'RAM unsigned LocalRoot_' + label + '(unsigned x)'
                    ' { return LocalLeaf(x); }\n')
                sources.append(local)
        for item in sources:
            obj = item.with_suffix(".o")
            argv = flags + (["-D" + mutation] if mutation else [])
            self.command(argv + ["-c", str(item), "-o", str(obj)], directory)
            objects.append(obj)
            su = obj.with_suffix(".su")
            self.assertTrue(su.is_file(), "actual compiler .su is required")
            usage.append({"source": str(item), "raw": su.read_bytes()})
        bridge = directory / "bridge.o"
        bridge_source = ROOT / "soc/stm32h743/drivers/flash_ram_enter.S"
        self.command(flags + ["-I", str(ROOT / "soc/stm32h743/private"),
                     "-c", str(bridge_source),
                     "-o", str(bridge)], directory)
        elf_path = directory / "image.elf"
        self.command(flags + ["-nostdlib", "-Wl,--build-id=none", "-T",
                     str(FIXTURE / "image.ld"), *map(str, objects), str(bridge),
                     "-o", str(elf_path)], directory)
        data = elf_path.read_bytes()
        if mutation == "BAD_LITERAL":
            actual = Elf32(data)
            function = next(item for item in actual.defined_symbols
                            if item["name"] == "RamTerminal")
            start = function["value"] & ~1
            section = actual.sections[function["section"]]
            changed = bytearray(data)
            for address in range(start, start + function["size"], 2):
                offset = section["offset"] + address - section["address"]
                half = int.from_bytes(data[offset:offset + 2], "little")
                if half & 0xf800 == 0x4800:
                    delta = (actual.symbol("outside_literal") -
                             ((address + 4) & ~3))
                    self.assertTrue(0 <= delta <= 1020 and delta % 4 == 0)
                    changed[offset:offset + 2] = (
                        (half & 0xff00) | delta // 4).to_bytes(2, "little")
                    break
            else:
                self.fail("actual fixture literal instruction missing")
            data = bytes(changed)
            elf_path.write_bytes(data)
        objdump = self.command(
            [self.compiler, "-print-prog-name=objdump"], directory
        ).decode().strip()
        raw = self.command([objdump, "-drwC", str(elf_path)], directory)
        return (Elf32(data), {"elf_sha256": hashlib.sha256(data).hexdigest(),
                            "objdump": raw}, usage)

    def verify(self, image):
        return importlib.import_module(MODULE).verify_h743_ram_static(*image)

    def accepted(self, **options):
        image = self.link(**options)
        report = self.verify(image)
        self.assertEqual("passed", report["status"])
        self.assertFalse(report["qualification"])
        self.assertEqual(224, report["exception_reserve_bytes"])
        self.assertEqual("not_executed", report["physical_execution"])
        return image, report

    def rejected(self, mutation, reason):
        image = self.link(mutation)
        with self.assertRaisesRegex(EvidenceError, reason, msg=mutation):
            self.verify(image)

    def test_real_call_tail_and_nonreturning_exception_chains(self):
        _, report = self.accepted()
        self.assertGreater(report["thread_chain_bytes"], 0)
        self.assertGreater(report["nmi_chain_bytes"], 0)
        self.assertGreater(report["hardfault_chain_bytes"], 0)
        self.assertEqual(sum(report[key] for key in (
            "thread_chain_bytes", "nmi_chain_bytes", "hardfault_chain_bytes",
            "exception_reserve_bytes")), report["stack_required_bytes"])
        self.assertTrue(any(row["kind"] == "tail" for row in report["edges"]))

    def test_all_ram_functions_include_an_unreferenced_entry(self):
        self.accepted()
        self.rejected("ORPHAN_FP", "FP/coprocessor")

    def test_flash_call_and_tail_branch_cannot_hide_in_veneer(self):
        self.accepted()
        for mutation in ("FLASH_CALL", "FLASH_BRANCH"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation, "target|veneer|PC write")

    def test_literal_fetch_outside_the_exact_pod_is_rejected(self):
        self.accepted()
        self.rejected("BAD_LITERAL", "literal fetch")

    def test_indirect_call_return_target_and_table_branch_are_rejected(self):
        self.accepted()
        for mutation in ("BAD_BLX", "BAD_BX", "BAD_PC_WRITE", "BAD_TABLE"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation, "indirect|PC write|table branch")

    def test_reserved_exchange_cannot_be_plain_high_register_arithmetic(self):
        self.accepted()
        for mutation in ("BAD_BX_RESERVED", "BAD_BLX_RESERVED"):
            with self.subTest(mutation=mutation):
                self.rejected(
                    mutation, "indirect|disassembly has a gap or overlap")

    def test_float_and_unknown_encodings_are_rejected(self):
        self.accepted()
        self.rejected("BAD_FP", "FP/coprocessor")
        self.rejected("BAD_UNKNOWN", "unsupported Thumb")

    def test_lr_memory_base_writeback_invalidates_the_return_identity(self):
        self.accepted()
        for mutation in ("BAD_LR_LOAD_WRITEBACK", "BAD_LR_STORE_WRITEBACK",
                         "BAD_LR_PRE_LOAD_WRITEBACK",
                         "BAD_LR_PRE_STORE_WRITEBACK"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation, "unknown indirect return")

    def test_dual_lr_writeback_is_outside_reviewed_encodings(self):
        self.accepted()
        for mutation in ("BAD_LR_DUAL_LOAD_WRITEBACK",
                         "BAD_LR_DUAL_STORE_WRITEBACK",
                         "BAD_LR_DUAL_PRE_LOAD_WRITEBACK",
                         "BAD_LR_DUAL_PRE_STORE_WRITEBACK"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation, "unsupported Thumb")

    def test_reserved_pop_and_misaligned_c_calls_fail_closed(self):
        self.accepted()
        for mutation, reason in (
                ("BAD_POP_LR_PC", "stack register list|Thumb/data"),
                ("BAD_C_CALL_SP_ALIGNMENT", "C call.*8-byte")):
            with self.subTest(mutation=mutation):
                self.rejected(mutation, reason)

    def test_c_callee_cannot_change_the_bridge_cpu_state(self):
        self.accepted()
        for mutation in ("BAD_CPSIE", "BAD_CONTROL", "BAD_MSP", "BAD_PSP",
                         "BAD_PRIMASK", "BAD_BASEPRI", "BAD_FAULTMASK"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation, "CPU state write|dynamic SP write")

    def test_explicit_sp_stores_cannot_forge_saved_lr_returns(self):
        self.accepted()
        for mutation in ("BAD_SAVED_LR", "BAD_SAVED_LR_WIDE",
                         "BAD_SAVED_LR_DUAL", "BAD_SAVED_LR_BYTE",
                         "BAD_SP_INDEX"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation, "saved LR|SP store")

    def test_explicit_sp_stores_stay_inside_the_owned_frame(self):
        self.accepted()
        self.accepted(mutation="VALID_SP_FRAME_STORES")
        for mutation in ("BAD_CALLER_SP_STORE", "BAD_SP_FRAME_END_WORD",
                         "BAD_SP_FRAME_END_BYTE", "BAD_SP_FRAME_PARTIAL_HALF",
                         "BAD_SP_FRAME_PARTIAL_WORD", "BAD_SP_FRAME_NEGATIVE"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation, "SP store leaves owned frame")

    def test_obvious_stack_address_aliases_cannot_write_through_registers(self):
        self.accepted()
        for mutation in ("BAD_SP_ALIAS", "BAD_SP_ADD_ALIAS", "BAD_MSP_ALIAS",
                         "BAD_SP_COPY_ALIAS", "BAD_SP_ARITH_ALIAS",
                         "BAD_SP_WIDE_ARITH_ALIAS", "BAD_SP_BYTE_ALIAS"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation, "stack-derived store")

    def test_callee_stack_output_cannot_become_an_untracked_store_base(self):
        self.accepted()
        for mutation in ("BAD_CALLEE_STACK_RETURN", "BAD_CALLEE_SPILL_RETURN",
                         "BAD_CALLEE_NONRETURN_STORE"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation, "stack-derived (store|pointer spill)")

    def test_actual_callee_output_replaces_callers_register_provenance(self):
        self.accepted(mutation="VALID_CALLEE_CLEARS_STACK_ARGUMENT")

    def test_register_offset_sp_load_cannot_forget_stack_passed_pointer(self):
        self.accepted()
        self.rejected("BAD_SP_REGISTER_LOAD_ARGUMENT", "indexed SP load")

    def test_recoverable_flash_data_addresses_are_not_ram_literal_permissions(self):
        self.accepted(mutation="VALID_FLASH_CONSTANT_ONLY")
        for mutation in ("BAD_FLASH_DATA_READ", "BAD_FLASH_DATA_LITERAL",
                         "BAD_FLASH_READ_PARTIAL_BOUNDARY",
                         "BAD_FLASH_READ_CALL_RETURN", "BAD_FLASH_READ_JOIN",
                         "BAD_FLASH_READ_UNKNOWN_JOIN"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation, "constant-address Flash data read")

    def test_real_constant_growth_rejects_instead_of_forgetting_values(self):
        self.rejected("BAD_CONSTANT_LOOP", "constant .*bound exceeded")

    def test_constant_detection_does_not_promote_unresolved_c_data_operands(self):
        _, report = self.accepted(mutation="VALID_UNKNOWN_DATA_READ")
        memory = report["constant_memory"]
        self.assertEqual("not_proved", memory["all_data_operands"])
        self.assertEqual("not_proved", memory["device_protocol"])
        self.assertTrue(memory["unresolved_read_sites"])

    def test_a_ram_literal_pc_veneer_is_not_a_reviewed_return(self):
        self.accepted()
        self.rejected("BAD_VENEER", "veneer|PC write")

    def test_a_real_branch_into_mapping_data_is_rejected(self):
        self.accepted()
        self.rejected("BRANCH_DATA", "instruction boundary|target")

    def test_recursion_and_dynamic_compiler_stack_cannot_pass(self):
        self.accepted()
        self.rejected("RECURSION", "recurs")
        self.rejected("DYNAMIC_STACK", "dynamic|SP write")

    def test_nested_exception_c_frames_are_added_to_thread_frame(self):
        self.accepted()
        self.rejected("LARGE_STACK", "stack.*bound|stack.*fit")

    def test_nmi_must_have_no_actual_return_path(self):
        self.accepted()
        self.rejected("RETURNING_NMI", "NMI|nonreturn")

    def test_missing_duplicate_and_misbound_stack_records_are_rejected(self):
        image, _ = self.accepted()
        for mode in ("missing", "duplicate", "source"):
            with self.subTest(mode=mode):
                edited = deepcopy(image[2])
                if mode == "missing":
                    edited = []
                elif mode == "duplicate":
                    edited.append(deepcopy(edited[0]))
                else:
                    edited[0]["source"] = str(FIXTURE / "another.c")
                with self.assertRaisesRegex(EvidenceError, "SU|source"):
                    self.verify((image[0], image[1], edited))

    def test_understated_frame_must_cover_actual_sp_instructions(self):
        image, _ = self.accepted()
        edited = deepcopy(image[2])
        edited[0]["raw"] = b"\n".join(
            row.split(b"\t")[0] + b"\t0\tstatic"
            for row in edited[0]["raw"].splitlines()) + b"\n"
        with self.assertRaisesRegex(EvidenceError, "SU.*frame|frame.*SU"):
            self.verify((image[0], image[1], edited))

    def test_duplicate_local_names_keep_real_source_identity(self):
        _, report = self.accepted(duplicate_locals=True)
        rows = [row for row in report["functions"]
                if row["name"] == "LocalLeaf"]
        self.assertEqual(2, len(rows))
        self.assertEqual(2, len({row["source"] for row in rows}))

    def test_forged_printed_mnemonic_or_byte_never_authorizes_opcode(self):
        image, _ = self.accepted()
        edited = deepcopy(image[1])
        edited["objdump"] = edited["objdump"].replace(b"\tpush", b"\tnop")
        self.assertEqual("passed", self.verify((image[0], edited,
                                                image[2]))["status"])
        edited["objdump"] = edited["objdump"].replace(b"b508", b"bf00", 1)
        with self.assertRaisesRegex(EvidenceError, "bytes differ"):
            self.verify((image[0], edited, image[2]))


class H743RamConstantFlowTests(unittest.TestCase):
    """Decoder/dataflow boundaries only; no CPU or device qualification."""

    def analyze(self, blocks):
        module = importlib.import_module(MODULE)
        functions, starts = {}, {}
        for index, (start, encodings) in enumerate(blocks):
            code, address = {}, start
            for encoded in encodings:
                raw = bytes.fromhex(encoded)
                op = module._decode(address, raw, bridge=False)
                code[address] = {"encoding": encoded}, op, False
                address += len(raw)
            functions[index] = {
                "symbol": {"value": start | 1, "name": "constant_fixture"},
                "code": code, "calls": {}}
            starts[start] = index
        for function in functions.values():
            for address, (row, op, conditional) in function["code"].items():
                if op.kind == "call":
                    function["calls"][address] = starts[op.target]
        return module._constant_memory(
            None, functions, list(reversed(range(len(blocks)))),
            {index: True for index in functions})

    def test_flash_address_alternative_survives_a_control_flow_join(self):
        with self.assertRaisesRegex(EvidenceError,
                                    "constant-address Flash data read"):
            self.analyze([(0x24001000, [
                "0028", "01d0", "0023", "03e0", "40f20003",
                "c0f60003", "1a68", "7047"])])

    def test_unknown_does_not_erase_a_known_flash_alternative(self):
        with self.assertRaisesRegex(EvidenceError,
                                    "constant-address Flash data read"):
            self.analyze([(0x24001000, [
                "0028", "01d0", "00bf", "03e0", "40f20003",
                "c0f60003", "1a68", "7047"])])

    def test_growing_loop_constants_fail_closed(self):
        with self.assertRaisesRegex(EvidenceError, "constant .*bound exceeded"):
            self.analyze([(0x24001000, [
                "0023", "0133", "0028", "fcd1", "7047"])])

    def test_partial_stack_store_invalidates_the_whole_overlapping_word(self):
        report = self.analyze([(0x24001000, [
            "81b0", "40f20003", "c0f60003", "0093", "0022",
            "8df80120", "009b", "1a68", "01b0", "7047"])])
        self.assertTrue(report["unresolved_read_sites"])

    def test_narrow_stack_load_does_not_restore_a_full_word_constant(self):
        report = self.analyze([(0x24001000, [
            "81b0", "40f20003", "c0f60003", "0093",
            "9df80030", "1a68", "01b0", "7047"])])
        self.assertTrue(report["unresolved_read_sites"])

    def test_bl_replaces_the_callers_lr_constant_before_callee_analysis(self):
        start, target = 0x24001000, 0x24002000
        address = start + 10
        displacement = (target - address - 4) & 0x1ffffff
        sign = displacement >> 24
        j1 = 1 ^ ((displacement >> 23) & 1) ^ sign
        j2 = 1 ^ ((displacement >> 22) & 1) ^ sign
        high = 0xf000 | sign << 10 | ((displacement >> 12) & 1023)
        low = 0xd000 | j1 << 13 | j2 << 11 | ((displacement >> 1) & 2047)
        call = (high.to_bytes(2, "little") +
                low.to_bytes(2, "little")).hex()
        report = self.analyze([
            (start, ["10b5", "40f2000e", "c0f6000e", call, "10bd"]),
            (target, ["7346", "1a68", "7047"])])
        self.assertTrue(report["unresolved_read_sites"])

    def test_post_index_writeback_does_not_leave_the_old_base_constant(self):
        report = self.analyze([(0x24001000, [
            "40f20003", "c0f60003", "43f8040b", "1a68", "7047"])])
        self.assertTrue(report["unresolved_read_sites"])


if __name__ == "__main__":
    unittest.main()
