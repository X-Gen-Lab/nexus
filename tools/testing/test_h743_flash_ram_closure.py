"""Actual ARM ELF mutations for the bounded H743 Flash RAM closure checker.

These tests do not execute a device or replace the provider's GoogleTest
protocol checks. Every negative changes real linked bytes or layout, not a
claimed ``ram_safe`` field. Unsupported instruction or layout rules fail closed;
these fixture executions do not qualify a different production ELF. Compiler
absence fails the test rather than skipping it.
"""

from __future__ import annotations

from copy import deepcopy
import hashlib
import importlib
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.configure import configure as gate
from tools.evidence.common import EvidenceError
from tools.measurement.elf import Elf32


ROOT = Path(__file__).resolve().parents[2]
ASSEMBLIES = ROOT / "cmake/package/verification/io_evolution"
FIXTURES = Path(__file__).with_name("fixtures")
MODULE = "tools.measurement.flash_execution"
RAM_FUNCTIONS = (
    "nx_stm32h743_flash_ram_enter",
    "nx_stm32h743_flash_ram_transaction",
    "nx_stm32h743_flash_ram_pulse",
    "Ram_Helper",
    "Test_Veneer",
    "nx_stm32h743_flash_ram_nmi",
    "nx_stm32h743_flash_ram_hardfault",
)
COLD_FUNCTIONS = ("Reset_Handler", "nx_stm32h743_flash_ram_initialize")


class H743FlashRamClosureTests(unittest.TestCase):
    """Closure categories, each using real positive and negative ELFs."""

    def setUp(self):
        # A linked fixture alone does not prove that its checker executed.
        self.assertIsNotNone(importlib.util.find_spec(MODULE),
                             "maintained RAM closure verifier is missing")
        self.verify = importlib.import_module(MODULE).verify_h743_ram_pod
        self.compiler = (os.environ.get("NEXUS_ARM_GCC") or
                         shutil.which("arm-none-eabi-gcc"))
        self.assertTrue(self.compiler, "actual pinned ARM compiler required")
        self.storage = tempfile.TemporaryDirectory(prefix="h743-ram-closure-")
        self.addCleanup(self.storage.cleanup)
        self.directory = Path(self.storage.name)
        self.images = 0
        self.selected = gate.resolve(
            ASSEMBLIES / "h743-high-peripherals.toml", root=ROOT)
        self.unselected = gate.resolve(
            ASSEMBLIES / "h743-high-empty.toml", root=ROOT)

    def command(self, argv, directory):
        result = subprocess.run(argv, cwd=directory, capture_output=True,
                                check=False, timeout=30)
        if result.returncode:
            self.fail(f"command={argv!r}\nstdout={result.stdout!r}\n"
                      f"stderr={result.stderr!r}")
        return result.stdout

    def link(self, *, selected=True, mutation=None):
        """Build ARM code and retain the raw decoder inputs in memory."""
        directory = self.directory / str(self.images)
        self.images += 1
        directory.mkdir()
        defines = ["-DSELECTED=" + str(int(selected))]
        if mutation and mutation != "BAD_LOAD_TAIL":
            defines.append("-D" + mutation + "=1")
        linker = (FIXTURES / "h743_flash_ram_checker.ld.in").read_text()
        record = (".nx_flash_image (READONLY) : ALIGN(4) {\n"
                  "g_nx_stm32h743_flash_image = .;\n"
                  "LONG(__nx_flash_live_start); LONG(__nx_flash_live_end);\n"
                  "LONG(__nx_flash_image_load_start);\n"
                  "LONG(__nx_flash_image_load_end);\n"
                  "} >FLASH :flash" if selected else "")
        linker = linker.replace("@IMAGE_RECORD@", record).replace(
            "@BAD_LOAD_TAIL@", "+ 4" if mutation == "BAD_LOAD_TAIL" else "")
        script = directory / "image.ld"
        script.write_text(linker)
        elf_path = directory / "image.elf"
        argv = [self.compiler, "-mcpu=cortex-m7", "-mthumb",
                "-mfpu=fpv5-d16", "-mfloat-abi=hard", "-nostdlib", "-fno-lto",
                "-Wl,--build-id=none", "-Wl,--emit-relocs", "-T", str(script),
                *defines, "-x", "assembler-with-cpp",
                str(FIXTURES / "h743_flash_ram_checker.S"), "-o", str(elf_path)]
        if mutation == "BAD_ELF_ENTRY":
            argv.insert(1, "-Wl,-e,Flash_Helper")
        if mutation and mutation.startswith("CPU_"):
            argv.append("-DEXTERNAL_CPU_BRIDGE=1")
            original = ROOT / "soc/stm32h743/drivers/flash_ram_enter.S"
            changes = {
                "CPU_BAD_MSP": ("msr msp, r4", "msr msp, r5"),
                "CPU_BAD_PSP": ("msr psp, r5", "msr psp, r4"),
                "CPU_BAD_CONTROL": ("msr control, r6", "msr control, r1"),
                "CPU_CLEAR_FPCA": ("bic r1, r6, #2", "bic r1, r6, #6"),
                "CPU_BAD_ALIGNMENT": ("#NX_H743_FLASH_RAM_STACK_TOP_BYTES",
                                      "#(NX_H743_FLASH_RAM_STACK_TOP_BYTES - 4)"),
                "CPU_FOREIGN_CALL": ("bl nx_stm32h743_flash_ram_transaction",
                                     "bl Flash_Helper"),
                "CPU_PRIVATE_PUSH": ("bl nx_stm32h743_flash_ram_transaction",
                                     "push {r0, r1}\n    bl "
                                     "nx_stm32h743_flash_ram_transaction"),
                "CPU_BAD_MASK_CAPTURE": ("mrs r12, primask\n    cpsid i",
                                         "cpsid i\n    mrs r12, primask"),
                "CPU_EARLY_MASK": ("pop.w {r4-r11, lr}",
                                   "msr primask, r7\n    pop.w {r4-r11, lr}"),
                "CPU_BAD_CALLEE": ("msr primask, r12",
                                   "mov r8, r0\n    msr primask, r12"),
                "CPU_BAD_LR": ("msr primask, r12",
                               "mov lr, r0\n    msr primask, r12"),
            }
            if mutation == "CPU_VALID":
                source = original
            else:
                old, new = changes[mutation]
                text = original.read_text()
                self.assertEqual(1, text.count(old))
                source = directory / "bridge.S"
                source.write_text(text.replace(old, new))
            argv += ["-I", str(ROOT / "soc/stm32h743/private"), str(source)]
        if mutation in ("DUPLICATE_FLASH_LOCALS", "DUPLICATE_RAM_LOCALS"):
            for index in range(2):
                source = directory / f"local{index}.S"
                section = (".nx_flash_ram.local" if
                           mutation == "DUPLICATE_RAM_LOCALS" else ".text.local")
                body = (f"movs r0,#{index}\nbx lr\n" if
                        mutation == "DUPLICATE_RAM_LOCALS" else
                        ".word 0x47702000\n")
                source.write_text(
                    '.syntax unified\n.thumb\n.eabi_attribute 28,1\n'
                    '.file "local' + str(index) +
                    '.c"\n.section ' + section + ',"ax",%progbits\n'
                    '.local repeated_local\n.type repeated_local,%function\n'
                    '.thumb_func\nrepeated_local:\n' + body +
                    '.size repeated_local,.-repeated_local\n')
                argv.append(str(source))
        self.command(argv, directory)
        data = elf_path.read_bytes()
        elf = Elf32(data)
        objdump = self.command(
            [self.compiler, "-print-prog-name=objdump"], directory
        ).decode().strip()
        raw = self.command([objdump, "-drwC", str(elf_path)], directory)
        # Observations are tool output, not a trusted declaration of safety.
        # The checker must validate each instruction/relocation against data.
        observations = {
            "elf_sha256": hashlib.sha256(data).hexdigest(),
            "objdump": raw,
            "command": argv,
            "cold_roots": list(COLD_FUNCTIONS),
            "ram_roots": list(RAM_FUNCTIONS),
            "physical_execution": "not_executed",
        }
        return elf, self.selected if selected else self.unselected, observations

    def accepted(self, **options):
        image = self.link(**options)
        report = self.verify(*image)
        self.assertEqual("passed", report["status"])
        self.assertEqual("not_executed", report["physical_execution"])
        return image, report

    def rejected(self, mutation, **options):
        image = self.link(mutation=mutation, **options)
        # An actual compiler/link error is a fixture error, not checker RED.
        # Rejection is meaningful only after these bytes exist.
        with self.assertRaises(EvidenceError, msg=mutation):
            self.verify(*image)

    def test_flash_callee_literal_or_veneer_is_rejected(self):
        """ELF01: reachable targets/data must remain inside owned RAM."""
        self.accepted()
        for mutation in ("FLASH_CALL", "FLASH_LITERAL", "FLASH_VENEER"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation)

    def test_missing_copy_or_wrong_lma_cannot_pass_section_name_check(self):
        """ELF02: cold reachability and exact copy length have byte evidence."""
        image, report = self.accepted()
        elf = image[0]
        self.assertEqual(elf.symbol("__nx_flash_ram_end") -
                         elf.symbol("__nx_flash_ram_start"),
                         elf.symbol("__nx_flash_ram_load_end") -
                         elf.symbol("__nx_flash_ram_load_start"))
        self.assertGreater(report["copied_bytes"], 0)
        for mutation in ("MISSING_COPY_CALL", "WRONG_COPY_SOURCE",
                         "BAD_LOAD_TAIL"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation)

    def test_private_stack_fits_code_frames_and_preserves_actual_cpu_context(self):
        """ELF03: emitted register flow, not a host bridge stub, is checked."""
        _, report = self.accepted()
        # Two nested extended exception frames (104 + alignment each), the
        # actual reachable call frames and the bridge transfer must fit.
        self.assertGreaterEqual(report["stack_required_bytes"], 304)
        # 320 bytes really fits the observed 304-byte path; 256 does not.
        # A checker hardcoding a 1024-byte array or checking hardware frames
        # alone cannot satisfy both real linked inputs. The provider's actual
        # 1024-byte workspace is still checked independently in its own ELF.
        _, fitted = self.accepted(mutation="FITTED_STACK")
        self.assertEqual(report["stack_required_bytes"],
                         fitted["stack_required_bytes"])
        self.assertLessEqual(fitted["stack_required_bytes"], 320)
        self.assertEqual(["CONTROL", "MSP", "PRIMASK", "PSP"],
                         sorted(report["preserved_cpu_registers"]))
        for mutation in ("SHORT_STACK", "MARGINAL_STACK", "BAD_STACK_TOP",
                         "WRONG_MSP_RESTORE",
                         "WRONG_PSP_RESTORE", "WRONG_CONTROL_RESTORE",
                         "CLEAR_FPCA", "WRONG_MASK_RESTORE"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation)

    def test_foreign_vector_or_early_vtor_restore_is_rejected(self):
        """ELF04: original aliases and RAM transition code are both examined."""
        self.accepted()
        for mutation in ("BAD_ORIGINAL_NMI", "FLASH_NMI_VECTOR",
                         "EARLY_VTOR_RESTORE"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation)

    def test_indirect_control_flow_or_fp_instruction_is_rejected(self):
        """ELF05: no unknown indirect target or live FP use is guessed safe."""
        self.accepted()
        for mutation in ("INDIRECT_CALL", "FP_INSTRUCTION",
                         "MISSING_REQUESTED_ROOT"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation)

    def test_unselected_image_has_no_ram_load_or_workspace(self):
        """ELF06: default cost is observed in real bytes; policy is explicit."""
        selected, selected_report = self.accepted()
        self.assertGreater(selected_report["static_ram_bytes"], 0)
        empty = self.link(selected=False)
        elf = empty[0]
        self.assertNotIn("g_nx_stm32h743_flash_ram_workspace", elf.symbols)
        self.assertFalse(any(section["size"] for section in elf.sections
                             if section["name"] == ".nx_flash_ram"))
        report = self.verify(*empty)
        self.assertEqual("not_selected", report["status"])
        self.assertEqual(0, report["static_ram_bytes"])
        self.assertEqual(0, report["copied_bytes"])
        # Even real safe-looking code cannot authorize an unsupported cache
        # profile. Actual cold/runtime cache+MPU rejection belongs to the GTs.
        mismatched = deepcopy(selected[1])
        mismatched["clock_profile"] = "hsi64-fixed-cache-on"
        with self.assertRaises(EvidenceError):
            self.verify(selected[0], mismatched, selected[2])

    def test_real_nzcv_flags_cannot_hide_flash_data_access(self):
        """ELF07: concrete execution uses architectural carry/overflow flags."""
        self.accepted()
        for mutation in ("WRONG_ADD_CARRY_PATH", "WRONG_SUB_CARRY_PATH",
                         "WRONG_MOV_CARRY_PATH", "WRONG_ADD_OVERFLOW_PATH"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation)

    def test_unknown_entry_argument_cannot_prune_a_dangerous_path(self):
        """ELF08: arbitrary caller data is not a proven concrete input."""
        self.accepted()
        self.rejected("UNKNOWN_ARGUMENT_PATH")

    def test_all_callee_saved_registers_preserve_arbitrary_incoming_bits(self):
        """ELF09: one convenient register sample does not prove preservation."""
        self.accepted()
        self.rejected("CLOBBER_UNOBSERVED_R8")

    def test_special_pc_and_sp_writes_are_not_normal_register_assignments(self):
        """ELF10: architectural PC/SP effects cannot disappear in a decoder."""
        self.accepted()
        for mutation in ("LOAD_PC_INDIRECT", "WIDE_SP_WRITE"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation)

    def test_actual_reset_path_must_execute_the_initializer_call(self):
        """ELF11: graph reachability alone does not prove cold execution."""
        self.accepted()
        self.rejected("COPY_SKIPPED_BY_REAL_BRANCH")

    def test_every_readback_mismatch_path_must_reject_initialization(self):
        """ELF12: checking only the first byte cannot prove all readbacks."""
        self.accepted()
        self.rejected("ONLY_FIRST_READBACK_REJECTS")

    def test_any_unequal_byte_must_fail_without_value_specific_assumptions(self):
        """ELF13: a low-bit sample does not represent all unequal bytes."""
        self.accepted()
        self.rejected("ONLY_LSB_MISMATCH_REJECTS")

    def test_masked_values_are_not_evidence_of_full_byte_readback(self):
        """ELF14: dropping a loaded bit invalidates full-byte provenance."""
        self.accepted()
        self.rejected("MASKED_SUCCESS_READBACK")

    def test_reset_symbol_matches_actual_vector_and_elf_entry(self):
        """ELF15: the proven reset path is the one the image selects."""
        self.accepted()
        for mutation in ("BAD_VECTOR_RESET", "BAD_ELF_ENTRY"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation)

    def test_stack_register_restore_requires_identity_for_any_legal_stack(self):
        """ELF16: preserved MSP/PSP bits cannot be inferred from samples."""
        self.accepted()
        for mutation in ("CORRUPT_SAVED_MSP_IDENTITY",
                         "CORRUPT_SAVED_PSP_IDENTITY"):
            with self.subTest(mutation=mutation):
                self.rejected(mutation)

    def test_incoming_return_identity_cannot_be_replaced_by_a_sample_value(self):
        """ELF17: a known numeric sentinel is not the caller's actual LR."""
        self.accepted()
        self.rejected("FORGED_INCOMING_LR_RETURN")

    def test_incoming_return_value_is_unknown_for_data_and_branch_proofs(self):
        """ELF18: return authorization never supplies a known LR value."""
        self.accepted()
        self.rejected("UNKNOWN_INCOMING_LR_PATH")

    def test_duplicate_local_functions_use_actual_symbol_identity(self):
        """ELF19: unrelated local names cannot block or alias owned code."""
        for mutation in ("DUPLICATE_FLASH_LOCALS", "DUPLICATE_RAM_LOCALS"):
            with self.subTest(mutation=mutation):
                image, report = self.accepted(mutation=mutation)
                symbols = [symbol for symbol in image[0].defined_symbols
                           if symbol["name"] == "repeated_local"]
                self.assertEqual(2, len(symbols))
                self.assertNotEqual(symbols[0]["value"], symbols[1]["value"])
                observations = dict(image[2])
                observations["ram_roots"] = [*RAM_FUNCTIONS, "repeated_local"]
                with self.assertRaises(EvidenceError):
                    self.verify(image[0], image[1], observations)

    def cpu_bridge_verifier(self):
        module = importlib.import_module(MODULE)
        verify = getattr(module, "verify_h743_cpu_bridge", None)
        self.assertTrue(callable(verify), "actual CPU-only bridge checker missing")
        return verify

    def test_actual_cpu_bridge_has_a_separate_conditional_aapcs_proof(self):
        verify = self.cpu_bridge_verifier()
        elf, _, observations = self.link(mutation="CPU_VALID")
        report = verify(elf, observations)
        self.assertEqual("passed", report["status"])
        self.assertEqual("fixed_pod_boundary_with_AAPCS_contract",
                         report["scope"])
        self.assertEqual(0, report["private_frame_bytes"])
        self.assertEqual(36, report["caller_frame_bytes"])
        self.assertEqual(168, report["bridge_bytes"])
        self.assertEqual("proved", report["return_gate"]["device_protocol"])
        self.assertEqual(8, report["program_words"]["staged_word_reads"])
        self.assertEqual(8, report["program_words"]["dynamic_word_stores"])
        self.assertEqual(elf.symbol("__nx_flash_ram_stack_start"),
                         report["stack_start"])
        self.assertEqual(elf.symbol("__nx_flash_ram_stack_end"),
                         report["stack_end"])
        self.assertEqual("separately_checked", report["callee_body"])
        self.assertFalse(report["qualification"])

    def test_actual_cpu_bridge_rejects_lost_cpu_callee_and_return_identity(self):
        verify = self.cpu_bridge_verifier()
        for mutation in ("CPU_BAD_MSP", "CPU_BAD_PSP", "CPU_BAD_CONTROL",
                         "CPU_BAD_CALLEE", "CPU_BAD_LR"):
            with self.subTest(mutation=mutation):
                elf, _, observations = self.link(mutation=mutation)
                with self.assertRaises(EvidenceError):
                    verify(elf, observations)

    def test_actual_cpu_bridge_saves_mask_before_and_restores_after_frames(self):
        verify = self.cpu_bridge_verifier()
        for mutation in ("CPU_BAD_MASK_CAPTURE", "CPU_EARLY_MASK"):
            with self.subTest(mutation=mutation):
                elf, _, observations = self.link(mutation=mutation)
                with self.assertRaises(EvidenceError):
                    verify(elf, observations)

    def test_actual_cpu_bridge_checks_fpca_stack_and_its_only_c_call(self):
        verify = self.cpu_bridge_verifier()
        for mutation in ("CPU_CLEAR_FPCA", "CPU_BAD_ALIGNMENT",
                         "CPU_PRIVATE_PUSH", "CPU_FOREIGN_CALL"):
            with self.subTest(mutation=mutation):
                elf, _, observations = self.link(mutation=mutation)
                with self.assertRaises(EvidenceError):
                    verify(elf, observations)


if __name__ == "__main__":
    unittest.main()
