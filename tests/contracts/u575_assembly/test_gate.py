"""U575 software reference never inherits another CPU or memory profile."""
import copy
import importlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch as mock_patch

from tools.evidence.common import EvidenceError
from tools.measurement.elf import Elf32

ROOT = Path(__file__).resolve().parents[3]


class U575ReferenceGateTests(unittest.TestCase):
    def setUp(self):
        self.gate = importlib.import_module("scripts.ci.stm32u575")
        facts = json.loads((ROOT / "soc/stm32/stm32u575/soc.json").read_text())
        self.profile = {"part": "STM32U575ZIT6Q", "soc_family": "stm32u575",
            "board": "stm32u575-nucleo-zi-q", "backend": "baremetal",
            "cpu": facts["cpu"], "clock": facts["clock_profiles"]["msis4"],
            "memory": facts["variants"]["STM32U575ZIT6Q"]["memory"],
            "controllers": []}

    def test_exact_reference_profile_is_accepted_without_physical_claim(self):
        self.gate.validate_profile(self.profile)

    def test_real_resolver_default_memory_includes_unselected_sram4(self):
        from tools.configure import configure
        for backend, leaf in (("baremetal", "io.toml"),
                              ("freertos", "io-freertos.toml")):
            with self.subTest(backend=backend):
                result = configure.resolve_ir(
                    ROOT / "tests/contracts/u575_assembly" / leaf).to_dict()
                self.assertEqual(result["memory"], [
                    {"id": "sram1", "origin": 0x20000000, "size": 196608,
                     "dma_engines": {"GPDMA1": True, "LPDMA1": False},
                     "linker": True},
                    {"id": "sram4", "origin": 0x28000000, "size": 16384,
                     "dma_engines": {"GPDMA1": True, "LPDMA1": True},
                     "linker": False}])
                self.assertEqual(result["cpu"]["mpu_regions"], 8)
                self.assertEqual(result["backend"], backend)
                self.assertFalse(result["physical_qualified"])
                self.gate.validate_profile(result)

    def test_real_selected_sram4_reference_cannot_enter_default_image_gate(self):
        from tools.configure import configure
        result = configure.resolve_ir(ROOT / "tools/configure/assemblies/"
                                      "u575-lpdma-copy-stop2.toml").to_dict()
        self.assertTrue(result["memory"][1]["linker"])
        with self.assertRaisesRegex(EvidenceError, "memory|SRAM"):
            self.gate.validate_profile(result)

    def test_real_default_sram4_geometry_types_engines_and_exact_inventory(self):
        from tools.configure import configure
        baseline = configure.resolve_ir(
            ROOT / "tests/contracts/u575_assembly/io.toml").to_dict()
        self.gate.validate_profile(baseline)
        for field, value in (("id", "other"), ("origin", 0x28000004),
                             ("origin", float(0x28000000)), ("origin", True),
                             ("size", 16380), ("size", 16384.0),
                             ("linker", True), ("linker", 0),
                             ("dma_engines", {"GPDMA1": True, "LPDMA1": False}),
                             ("dma_engines", {"GPDMA1": 1, "LPDMA1": True}),
                             ("dma_engines", {"GPDMA1": True, "LPDMA1": 1}),
                             ("dma_engines", {"LPDMA1": True}),
                             ("dma_engines", {"GPDMA1": True, "LPDMA1": True,
                                              "UNKNOWN": False})):
            with self.subTest(field=field, value=value):
                forged = copy.deepcopy(baseline)
                forged["memory"][1][field] = value
                with self.assertRaises(EvidenceError):
                    self.gate.validate_profile(forged)
        for memory in (baseline["memory"][:1], baseline["memory"][::-1],
                       baseline["memory"] + [baseline["memory"][1]]):
            with self.subTest(memory=memory):
                forged = copy.deepcopy(baseline)
                forged["memory"] = copy.deepcopy(memory)
                with self.assertRaises(EvidenceError):
                    self.gate.validate_profile(forged)
        forged = copy.deepcopy(baseline)
        forged["cpu"]["mpu_regions"] = 16
        with self.assertRaises(EvidenceError):
            self.gate.validate_profile(forged)

    def test_foreign_part_or_cpu_only_fixture_is_rejected(self):
        for field, value in (("part", "STM32U575ZIT6"),
                             ("soc_family", "cpu-runtime"),
                             ("board", "unknown-board")):
            profile = copy.deepcopy(self.profile)
            profile[field] = value
            with self.assertRaises(EvidenceError):
                self.gate.validate_profile(profile)

    def test_same_m33_cpu_cannot_authorize_secure_image_or_soc_cache(self):
        for field, value in (("security", "secure"), ("sau", True),
                             ("icache_line_bytes", 32)):
            profile = copy.deepcopy(self.profile)
            profile["cpu"][field] = value
            with self.assertRaises(EvidenceError):
                self.gate.validate_profile(profile)

    def test_internal_clock_cannot_be_relabelled_as_external_hse(self):
        profile = copy.deepcopy(self.profile)
        profile["clock"]["source"] = "hse"
        with self.assertRaises(EvidenceError):
            self.gate.validate_profile(profile)

    def test_memory_alias_or_capacity_cannot_expand_the_default_profile(self):
        for field, value in (("origin", 0x30000000), ("size", 786432)):
            profile = copy.deepcopy(self.profile)
            profile["memory"][0][field] = value
            with self.assertRaises(EvidenceError):
                self.gate.validate_profile(profile)

    def test_linked_dma_mode_cannot_enter_irq_byte_reference(self):
        profile = copy.deepcopy(self.profile)
        profile["controllers"] = [{"kind": "uart", "controller": "USART1",
                                   "mode": "dma-tx"}]
        with self.assertRaises(EvidenceError):
            self.gate.validate_profile(profile)

    def test_mainstream_gpio_usart2_and_inactive_watchdog_are_exact_modes(self):
        profile = copy.deepcopy(self.profile)
        profile["controllers"] = [
            {"kind": "gpio", "controller": "GPIO" + bank, "mode": "input"}
            for bank in "ABCDEFGH"] + [
            {"kind": "uart", "controller": "USART2", "mode": "irq-bytes"},
            {"kind": "watchdog", "controller": "IWDG", "mode": "independent"}]
        self.gate.validate_profile(profile)

    def test_selected_finite_peripherals_remain_in_single_state_profile(self):
        profile = copy.deepcopy(self.profile)
        profile["controllers"] = [
            {"kind": kind, "controller": controller, "mode": mode}
            for kind, controller, mode in (
                ("spi", "SPI1", "short-poll"),
                ("i2c", "I2C1", "poll-messages"),
                ("adc", "ADC4", "single-shot"),
                ("pwm", "TIM3", "fixed-pwm"),
                ("exti", "EXTI1", "edge-event"),
                ("flash", "FLASH", "poll-word"))]
        self.gate.validate_profile(profile)

    def test_cpu_boolean_and_count_types_cannot_use_numeric_equality(self):
        for field, value in (("dsp", 1), ("dwt_cyccnt", 0),
                             ("sau", 0), ("mpu_regions", 8.0),
                             ("icache_line_bytes", False)):
            profile = copy.deepcopy(self.profile)
            profile["cpu"][field] = value
            with self.subTest(field=field, value=value):
                with self.assertRaises(EvidenceError):
                    self.gate.validate_profile(profile)

    def test_clock_and_memory_integers_cannot_be_floats_or_bools(self):
        for field in ("input_hz", "core_hz", "apb1_hz", "apb2_hz"):
            profile = copy.deepcopy(self.profile)
            profile["clock"][field] = float(profile["clock"][field])
            with self.subTest(clock=field):
                with self.assertRaises(EvidenceError):
                    self.gate.validate_profile(profile)
        for field, value in (("origin", float(0x20000000)),
                             ("size", 196608.0), ("linker", 1)):
            profile = copy.deepcopy(self.profile)
            profile["memory"][0][field] = value
            with self.subTest(memory=field):
                with self.assertRaises(EvidenceError):
                    self.gate.validate_profile(profile)
        for engine, value in (("GPDMA1", 1), ("LPDMA1", 0)):
            profile = copy.deepcopy(self.profile)
            profile["memory"][0]["dma_engines"][engine] = value
            with self.subTest(engine=engine):
                with self.assertRaises(EvidenceError):
                    self.gate.validate_profile(profile)


class U575ActualVectorGateTests(unittest.TestCase):
    """Link only an official vector/tool fixture, not a provider substitute."""

    def setUp(self):
        U575ReferenceGateTests.setUp(self)

    @classmethod
    def setUpClass(cls):
        cls.storage = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.storage.name)
        compiler = (os.environ.get("NEXUS_ARM_GCC") or
                    shutil.which("arm-none-eabi-gcc"))
        if not compiler:
            raise AssertionError("actual maintained ARM compiler is required")
        source = cls.directory / "vector-fixture.c"
        source.write_text("""
void SystemInit(void) {}
void TIM2_IRQHandler(void) {}
void USART1_IRQHandler(void) { __asm__ volatile("nop"); }
void USART2_IRQHandler(void) { __asm__ volatile("nop; nop"); }
void EXTI0_IRQHandler(void) { __asm__ volatile("nop; nop; nop"); }
void EXTI1_IRQHandler(void) { __asm__ volatile("nop; nop; nop; nop"); }
void __libc_init_array(void) {}
int main(void) { for (;;) {} }
""")
        linker = cls.directory / "vector-fixture.ld"
        linker.write_text("""
ENTRY(Reset_Handler)
MEMORY { FLASH(rx): ORIGIN = 0x08000000, LENGTH = 2M
RAM(rw): ORIGIN = 0x20000000, LENGTH = 192K }
_estack = 0x20030000; __nx_msp_end = _estack;
SECTIONS {
.isr_vector : { KEEP(*(.isr_vector)) } > FLASH
.text : { *(.text*) *(.rodata*) } > FLASH
.data : { _sdata = .; *(.data*) _edata = .; } > RAM AT> FLASH
_sidata = LOADADDR(.data);
.bss (NOLOAD) : { _sbss = .; *(.bss*) _ebss = .; } > RAM
}
""")
        cls.image = cls.directory / "vector-fixture.elf"
        argv = [compiler, "-mcpu=cortex-m33", "-mthumb", "-mfpu=fpv5-sp-d16",
            "-mfloat-abi=hard", "-fshort-enums", "-Os", "-nostdlib",
            "-Wl,--gc-sections", "-T", str(linker), str(source),
            str(ROOT / "vendors/st/cmsis_device_u5/Source/Templates/gcc/"
                "startup_stm32u575xx.s"), "-o", str(cls.image)]
        result = subprocess.run(argv, capture_output=True, timeout=30)
        if result.returncode:
            raise AssertionError(result.stderr.decode())

    @classmethod
    def tearDownClass(cls):
        cls.storage.cleanup()

    def test_real_official_u575_vectors_match_selected_elf_handlers(self):
        self.profile["controllers"] = [{"kind": "uart", "controller": "USART1",
                                        "mode": "irq-bytes"}]
        result = self.gate.validate_image(Elf32(self.image.read_bytes()),
                                          self.profile)
        self.assertEqual(result["vector_count"], 142)
        self.assertEqual(result["physical_status"], "not_executed")

    def test_real_vector_bytes_cannot_substitute_stack_or_irq_owner(self):
        self.profile["controllers"] = [{"kind": "uart", "controller": "USART1",
                                        "mode": "irq-bytes"}]
        image = self.image.read_bytes()
        elf = Elf32(image)
        vector = next(s for s in elf.sections if s["name"] == ".isr_vector")
        for slot, value in ((0, 0x30030000), (61, elf.symbol("USART1_IRQHandler")),
                             (77, elf.symbol("TIM2_IRQHandler"))):
            data = bytearray(image)
            struct.pack_into("<I", data, vector["offset"] + slot * 4, value)
            with self.subTest(slot=slot):
                with self.assertRaises(EvidenceError):
                    self.gate.validate_image(Elf32(bytes(data)), self.profile)

    def test_selected_usart2_vector_is_independent_of_usart1(self):
        self.profile["controllers"] = [{"kind": "uart", "controller": "USART2",
                                        "mode": "irq-bytes"}]
        image = self.image.read_bytes()
        elf = Elf32(image)
        result = self.gate.validate_image(elf, self.profile)
        self.assertEqual(["USART2"], result["selected_uarts"])
        vector = next(s for s in elf.sections if s["name"] == ".isr_vector")
        corrupted = bytearray(image)
        struct.pack_into("<I", corrupted, vector["offset"] + 78 * 4,
                         elf.symbol("USART1_IRQHandler"))
        with self.assertRaises(EvidenceError):
            self.gate.validate_image(Elf32(bytes(corrupted)), self.profile)

    def test_selected_exti_vector_cannot_substitute_the_adjacent_line(self):
        self.profile["controllers"] = [{"kind": "exti", "controller": "EXTI1",
                                        "mode": "edge-event"}]
        image = self.image.read_bytes()
        elf = Elf32(image)
        self.gate.validate_image(elf, self.profile)
        vector = next(s for s in elf.sections if s["name"] == ".isr_vector")
        corrupted = bytearray(image)
        struct.pack_into("<I", corrupted, vector["offset"] + 28 * 4,
                         elf.symbol("EXTI0_IRQHandler"))
        with self.assertRaises(EvidenceError):
            self.gate.validate_image(Elf32(bytes(corrupted)), self.profile)

    def test_matrix_bytes_identity_verifies_the_actual_linked_artifact(self):
        identity = self.gate.arch_compile.digest(self.image)
        # The old runner sent this producer identity directly to the generic
        # size-schema verifier. Exercise that actual boundary before and after
        # the new strict adapter, not a mocked file or ELF parser.
        adapter = getattr(self.gate, "matrix_artifact",
                          self.gate.verify_file_identity)
        self.assertEqual(self.image.resolve(), adapter(identity))
        for field, value in (("bytes", identity["bytes"] + 1),
                             ("bytes", True), ("sha256", "0" * 64)):
            with self.subTest(field=field, value=value):
                with self.assertRaises(EvidenceError):
                    adapter({**identity, field: value})

    def inspection_boundary(self, mutation=None):
        """Inject external build/checkout changes around real ELF inspection.

        The tiny official-vector ELF is a tool fixture. The shared builder and
        resolver are external boundaries; the production outer runner, real
        objdump, image/profile parser and file/snapshot verifiers execute.
        """
        with tempfile.TemporaryDirectory(prefix="u575-inspection-") as temp:
            directory = Path(temp)
            root = directory / "source"
            root.mkdir()
            for name in ("CMakeLists.txt", "soc/input.c", "scripts/tool.py"):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("/* observed input */\n")
            fixture = root / "tests/contracts/u575_assembly/empty.toml"
            fixture.parent.mkdir(parents=True)
            fixture.write_text("# external resolver fixture\n")
            fixtures = [{"assembly": str(fixture), "inputs": [],
                         "resolved": {"source_dependencies": []}}]
            source = {"root": str(root), "commit": "1" * 40,
                      "tree": "2" * 40, "dirty": True, "submodules": []}
            built = {}
            producer = self.gate.provider_assemblies
            original_execute = producer.Recorder.execute

            def build_matrix(_root, _assemblies, _compiler, output, **_kwargs):
                output.mkdir(parents=True)
                image = output / "image.elf"
                image.write_bytes(self.image.read_bytes())
                resolved = output / "resolved.json"
                resolved.write_text(json.dumps(self.profile))
                dependency = output / "external-header.h"
                dependency.write_text("/* compiler dependency */\n")
                tool = output / "external-tool"
                shutil.copyfile(shutil.which("true"), tool)
                built.update(elf=image, resolved=resolved,
                             dependency=dependency, tool=tool,
                             report=output / "report.json")
                digest = self.gate.arch_compile.digest
                matrix = {"source_inputs": producer.source_inputs(root, fixtures),
                    "tools": {"external": digest(tool)}, "fixtures": [{
                        "compiler_dependencies": [digest(dependency)],
                        "artifacts": {"elf": digest(image),
                                      "resolved": digest(resolved)}}]}
                built["report"].write_text(json.dumps(matrix))
                return 0

            def inspect(recorder, argv, label):
                raw = original_execute(recorder, argv, label)
                if mutation in built:
                    with built[mutation].open("ab") as stream:
                        stream.write(b" ")
                elif mutation == "source":
                    (root / "soc/input.c").write_text("/* changed */\n")
                elif mutation == "added_source":
                    (root / "soc/new.c").write_text("/* new source */\n")
                elif mutation == "automation":
                    (root / "scripts/tool.py").write_text("# changed\n")
                elif mutation == "added_automation":
                    (root / "scripts/new.py").write_text("# new automation\n")
                elif mutation == "git":
                    source["commit"] = "3" * 40
                return raw

            with (mock_patch.object(producer, "run", side_effect=build_matrix),
                  mock_patch.object(producer, "validate_fixtures",
                                    return_value=fixtures),
                  mock_patch.object(producer.Recorder, "execute", new=inspect),
                  mock_patch.object(self.gate, "git_source", create=True,
                                    side_effect=lambda _: copy.deepcopy(source))):
                output = directory / "output"
                compiler = (os.environ.get("NEXUS_ARM_GCC") or
                            shutil.which("arm-none-eabi-gcc"))
                result = self.gate.run(root, compiler, output)
                return result, json.loads((output / "report.json").read_text())

    def test_unchanged_inspection_inputs_are_accepted(self):
        result, report = self.inspection_boundary()
        self.assertEqual(0, result, report)
        self.assertEqual("passed", report["status"])
        self.assertEqual("not_executed", report["physical_status"])

    def test_inspection_rejects_changed_inputs_and_checkout(self):
        for mutation in ("elf", "resolved", "dependency", "tool", "report",
                         "source", "added_source", "automation",
                         "added_automation", "git"):
            with self.subTest(mutation=mutation):
                result, report = self.inspection_boundary(mutation)
                self.assertEqual(1, result, report)
                self.assertEqual("failed", report["status"])

    def test_matrix_identity_rejects_generic_or_mixed_schema_authorities(self):
        identity = self.gate.arch_compile.digest(self.image)
        adapter = getattr(self.gate, "matrix_artifact",
                          self.gate.verify_file_identity)
        generic = {"path": identity["path"], "size": identity["bytes"],
                   "sha256": identity["sha256"]}
        for value in (generic, {**identity, "size": identity["bytes"]},
                      {key: item for key, item in identity.items()
                       if key != "bytes"}):
            with self.subTest(value=value):
                with self.assertRaises(EvidenceError):
                    adapter(value)


if __name__ == "__main__":
    unittest.main()
