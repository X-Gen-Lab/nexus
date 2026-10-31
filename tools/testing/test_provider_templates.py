"""Audit directly specialized provider tables from their actual source files."""
from tools.configure.providers.descriptor import source_directory

from pathlib import Path
import os
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

from tools.evidence.common import EvidenceError, file_identity
from tools.measurement.retention import source_contracts


ROOT = Path(__file__).resolve().parents[2]
PUBLIC = """typedef struct { int (*start)(void*); int (*stop)(void*); } nx_probe_ops_t;
typedef struct { const nx_probe_ops_t* ops; void* context; } nx_probe_t;
"""
WRAPPER = '#define NX_PROBE_OPS nx_reviewed_probe_ops\n#include "probe_impl.h"\n'
TEMPLATE = 'const nx_probe_ops_t NX_PROBE_OPS = { .start = real_start, .stop = real_stop };\n'


class ProviderTemplateTests(unittest.TestCase):
    def tree(self, directory, wrapper=WRAPPER, template=TEMPLATE):
        root = Path(directory)
        public = root / "io/include/nexus/io/probe.h"
        source = root / "soc/reviewed/drivers/probe.c"
        header = root / "io/private/probe_impl.h"
        for path, contents in ((public, PUBLIC), (source, wrapper), (header, template)):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents)
        return root, source, header

    def test_actual_timer_tables_are_projected_from_both_families(self):
        _, _, providers, _ = source_contracts(ROOT)
        for family, stem in (("stm32f407", "stm32"), ("gd32f470", "gd32")):
            for name, template in (("timer_capture", "timer_capture_impl.h"),
                                   ("advanced_pwm", "advanced_pwm_impl.h")):
                with self.subTest(family=family, name=name):
                    provider = providers[f"nx_{stem}_{name}_ops"]
                    self.assertEqual(provider["source"], file_identity(
                        ROOT / source_directory(family) / "drivers" / (name + ".c")))
                    self.assertEqual(provider["template_source"], file_identity(
                        ROOT / "io/private" / template))

    def test_direct_projection_keeps_actual_translation_unit_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root, source, header = self.tree(directory)
            _, _, providers, _ = source_contracts(root)
            actual = providers["nx_reviewed_probe_ops"]
            self.assertEqual(actual["source"], file_identity(source))
            self.assertEqual(actual["template_source"], file_identity(header))
            self.assertEqual(actual["fields"], {"start": "real_start", "stop": "real_stop"})

    def test_conditional_wrapper_binding_is_not_guessed(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, '#if CHOICE\n' + WRAPPER + '#endif\n')
            with self.assertRaisesRegex(EvidenceError, "conditional"):
                source_contracts(root)

    def test_duplicate_binding_is_rejected_even_if_identical(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, WRAPPER.splitlines()[0] + '\n' + WRAPPER)
            with self.assertRaisesRegex(EvidenceError, "duplicate"):
                source_contracts(root)

    def test_opaque_macro_expression_is_not_substituted(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, WRAPPER.replace(
                "nx_reviewed_probe_ops", "SELECT(nx_reviewed_probe_ops)"))
            with self.assertRaisesRegex(EvidenceError, "direct"):
                source_contracts(root)

    def test_conditional_template_table_is_not_guessed(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, template='#if CHOICE\n' + TEMPLATE + '#endif\n')
            with self.assertRaisesRegex(EvidenceError, "conditional"):
                source_contracts(root)

    def test_duplicate_template_include_is_not_assumed_guarded(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, WRAPPER + '#include "probe_impl.h"\n')
            with self.assertRaisesRegex(EvidenceError, "ambiguous"):
                source_contracts(root)

    def test_template_table_must_be_the_actual_bound_macro(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, template=TEMPLATE.replace("NX_PROBE_OPS", "NX_OTHER_OPS"))
            with self.assertRaisesRegex(EvidenceError, "missing"):
                source_contracts(root)

    def test_opaque_initializer_is_rejected_by_existing_method_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, template=TEMPLATE.replace("real_start", "(nx_fn_t)real_start"))
            with self.assertRaisesRegex(EvidenceError, "initializer"):
                source_contracts(root)

    def test_template_cannot_escape_the_owned_private_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, WRAPPER.replace('"probe_impl.h"', '"../probe_impl.h"'))
            with self.assertRaisesRegex(EvidenceError, "private"):
                source_contracts(root)

    def test_table_macro_redefinition_inside_template_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, template='#undef NX_PROBE_OPS\n' + TEMPLATE)
            with self.assertRaisesRegex(EvidenceError, "redefined"):
                source_contracts(root)

    def test_quoted_template_name_cannot_shadow_another_owned_header(self):
        with tempfile.TemporaryDirectory() as directory:
            root, source, _ = self.tree(directory)
            (source.parent / "probe_impl.h").write_text(TEMPLATE)
            with self.assertRaisesRegex(EvidenceError, "ambiguous"):
                source_contracts(root)

    def test_binding_after_include_is_not_an_actual_template_binding(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, "\n".join(
                reversed(WRAPPER.splitlines())) + "\n")
            with self.assertRaisesRegex(EvidenceError, "before"):
                source_contracts(root)

    def test_shared_driver_header_cannot_shadow_a_private_template(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory)
            shadow = root / "drivers/probe/other_ip/probe_impl.h"
            shadow.parent.mkdir(parents=True)
            shadow.write_text(TEMPLATE)
            with self.assertRaisesRegex(EvidenceError, "ambiguous"):
                source_contracts(root)


class PrivateValueHeaderCompileTests(unittest.TestCase):
    """Check optional private value headers without inventing bindings."""

    def setUp(self):
        self.storage = tempfile.TemporaryDirectory(prefix="provider values ")
        self.addCleanup(self.storage.cleanup)
        self.area = Path(self.storage.name)

    def compile(self, source, *, language="c", preprocess=False):
        command = shlex.split(os.environ.get(
            "CXX" if language == "c++" else "CC",
            "c++" if language == "c++" else "cc"))
        self.assertTrue(command, "Required host compiler command is empty")
        self.assertIsNotNone(shutil.which(command[0]),
                             "Prepared host compiler is required")
        probe = self.area / ("probe.cpp" if language == "c++" else "probe.c")
        probe.write_text(source)
        output = probe.with_suffix(".o")
        arguments = [*command, "-std=" + ("c++17" if language == "c++"
                                         else "c11"),
                     "-Werror"]
        arguments += (["-E", "-dM", str(probe)] if preprocess else
                      ["-c", str(probe), "-o", str(output)])
        for directory in (ROOT / "io/private", ROOT / "io/include",
                          ROOT / "core/include", ROOT / "arch/include"):
            arguments += ["-I", str(directory)]
        result = subprocess.run(arguments, capture_output=True, text=True,
                                timeout=30)
        return result, output

    def pin_implementation(self, bindings="", checks=""):
        return (bindings + "#define NX_PIN_STATE_IMPLEMENTATION\n"
                '#include "pin_state_impl.h"\n'
                "#ifndef NX_PIN_STATE_IMPLEMENTATION\n"
                "#error caller implementation selector was consumed\n"
                "#endif\n" + checks)

    def test_pin_declarations_need_no_bindings_and_are_idempotent(self):
        source = ('#include "pin_state_impl.h"\n' * 2 +
                  "#if defined(NX_PIN_READ) || defined(NX_PIN_WRITE)\n"
                  "#error declaration-only include introduced register ports\n"
                  "#endif\n"
                  "nx_pin_state_plan_t plan;\n"
                  "nx_pin_state_control_t control;\n")
        for language in ("c", "c++"):
            with self.subTest(language=language):
                result, _ = self.compile(source, language=language)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_pin_default_ports_do_not_escape_the_implementation(self):
        checks = ("#if defined(NX_PIN_READ) || defined(NX_PIN_WRITE)\n"
                  "#error private default register ports escaped\n#endif\n")
        result, _ = self.compile(self.pin_implementation(checks=checks))
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_pin_caller_read_survives_without_leaking_default_write(self):
        bindings = "#define NX_PIN_READ(reg) 17U\n"
        checks = ("#ifdef NX_PIN_WRITE\n"
                  "#error private default write port escaped\n#endif\n"
                  "#ifndef NX_PIN_READ\n#error caller read port was consumed\n"
                  "#endif\n"
                  '_Static_assert(NX_PIN_READ(0) == 17U, '
                  '"caller read changed");\n')
        result, _ = self.compile(self.pin_implementation(bindings, checks))
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_pin_caller_write_survives_without_leaking_default_read(self):
        bindings = "#define NX_PIN_WRITE(reg, value) 29U\n"
        checks = ("#ifdef NX_PIN_READ\n"
                  "#error private default read port escaped\n#endif\n"
                  "#ifndef NX_PIN_WRITE\n"
                  "#error caller write port was consumed\n"
                  "#endif\n"
                  '_Static_assert(NX_PIN_WRITE(0, 0) == 29U, '
                  '"caller write changed");\n')
        result, _ = self.compile(self.pin_implementation(bindings, checks))
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_pin_preserves_both_caller_ports_and_empty_selector(self):
        bindings = ("#define NX_PIN_READ(reg) 17U\n"
                    "#define NX_PIN_WRITE(reg, value) 29U\n")
        checks = ("#if !defined(NX_PIN_READ) || !defined(NX_PIN_WRITE)\n"
                  "#error caller register ports were consumed\n#endif\n"
                  '_Static_assert(NX_PIN_READ(0) == 17U, '
                  '"caller read changed");\n'
                  '_Static_assert(NX_PIN_WRITE(0, 0) == 29U, '
                  '"caller write changed");\n')
        result, _ = self.compile(self.pin_implementation(bindings, checks))
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_pin_private_helper_macros_do_not_escape(self):
        result, _ = self.compile(self.pin_implementation(), preprocess=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        macros = {line.split()[1].split("(")[0]
                  for line in result.stdout.splitlines()
                  if line.startswith("#define NX_PIN_")}
        self.assertEqual(macros, {"NX_PIN_STATE_IMPL_H",
                                  "NX_PIN_STATE_IMPLEMENTATION"})

    def test_can_fd_declarations_are_idempotent_without_specialization(self):
        source = ('#include "can_fd_impl.h"\n' * 2 +
                  "bool (*validate)(const nx_can_fd_frame_t*) = "
                  "nx_can_fd_frame_valid;\n")
        for language in ("c", "c++"):
            with self.subTest(language=language):
                result, _ = self.compile(source, language=language)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_can_fd_private_validator_keeps_c_linkage_in_cpp(self):
        source = ('#include "can_fd_impl.h"\n'
                  'extern "C" bool nx_can_fd_frame_valid('
                  "const nx_can_fd_frame_t* frame) { "
                  "return frame != nullptr; }\n")
        result, output = self.compile(source, language="c++")
        self.assertEqual(result.returncode, 0, result.stderr)
        nm = shutil.which("nm")
        self.assertIsNotNone(nm, "Prepared host symbol tool is required")
        symbols = subprocess.run([nm, "--defined-only", str(output)],
                                 capture_output=True, text=True,
                                 check=True, timeout=10)
        self.assertTrue(any(line.split()[-2:] ==
                            ["T", "nx_can_fd_frame_valid"]
                            for line in symbols.stdout.splitlines()))


class ProviderTemplateCompileTests(unittest.TestCase):
    """Compile real exact specializations; never execute their MCU MMIO."""

    template_directory = None
    selections = (
        ("stm32f407", "timer_capture", "cortex-m4", "STM32F407xx", "f4",
         "NX_CAPTURE", "nx_stm32_timer_capture", "timer_capture_impl.h"),
        ("stm32g474", "timer_capture", "cortex-m4", "STM32G474xx", "g4",
         "NX_CAPTURE", "nx_stm32g474_timer_capture", "timer_capture_impl.h"),
        ("gd32f470", "timer_capture", "cortex-m4", "GD32F470", None,
         "NX_CAPTURE", "nx_gd32_timer_capture", "timer_capture_impl.h"),
        ("stm32f407", "advanced_pwm", "cortex-m4", "STM32F407xx", "f4",
         "NX_ADV", "nx_stm32_advanced_pwm", "advanced_pwm_impl.h"),
        ("gd32f470", "advanced_pwm", "cortex-m4", "GD32F470", None,
         "NX_ADV", "nx_gd32_advanced_pwm", "advanced_pwm_impl.h"),
        ("stm32h743", "uart_fifo", "cortex-m7", "STM32H743xx", "h7",
         "NX_UART_FIFO", "nx_stm32h743_uart_fifo", "uart_fifo_impl.h"),
        ("stm32u575", "uart_fifo", "cortex-m33", "STM32U575xx", "u5",
         "NX_UART_FIFO", "nx_stm32u575_uart_fifo", "uart_fifo_impl.h"),
        ("stm32h743", "uart_rtscts", "cortex-m7", "STM32H743xx", "h7",
         "NX_UART_FIFO", "nx_stm32h743_uart_rtscts", "uart_fifo_impl.h"),
        ("stm32u575", "uart_rtscts", "cortex-m33", "STM32U575xx", "u5",
         "NX_UART_FIFO", "nx_stm32u575_uart_rtscts", "uart_fifo_impl.h"),
    )

    def setUp(self):
        self.storage = tempfile.TemporaryDirectory(prefix="provider templates ")
        self.addCleanup(self.storage.cleanup)
        self.area = Path(self.storage.name)
        (self.area / "nexus_config.h").write_text(
            "/* Exact cache-off compile fixture, no runtime executed. */\n"
            "#define NEXUS_SOC_CACHE_MASK 0U\n")
        command = shlex.split(os.environ.get("NEXUS_ARM_GCC",
                                             "arm-none-eabi-gcc"))
        self.assertTrue(command, "Required ARM compiler command is empty")
        self.assertIsNotNone(shutil.which(command[0]),
                             "Prepared ARM compiler is required")
        self.compiler = command

    def compile(self, selection, *, omit=None, append="", replace=None):
        family, stem, cpu, part, sdk, _, _, template = selection
        original = ROOT / source_directory(family) / "drivers" / (stem + ".c")
        source = original.read_text()
        if omit is not None:
            source, count = re.subn(
                r"^#define " + re.escape(omit) + r"\b[^\n]*\n", "",
                source, flags=re.MULTILINE)
            self.assertEqual(count, 1)
        if replace is not None:
            macro, value = replace
            source, count = re.subn(
                r"^#define " + re.escape(macro) + r"\b[^\n]*\n",
                "#define " + macro + " " + value + "\n", source,
                flags=re.MULTILINE)
            self.assertEqual(count, 1)
        include = '#include "' + template + '"'
        self.assertEqual(source.count(include), 1)
        source = source.replace(include, include + "\n" + append)
        probe = self.area / (family + "_" + stem + ".c")
        probe.write_text(source)
        output = probe.with_suffix(".o")
        includes = [self.area, self.template_directory or ROOT / "io/private",
                    ROOT / source_directory(family) / "private",
                    ROOT / "io/private", ROOT / "soc/common",
                    ROOT / "core/include", ROOT / "io/include",
                    ROOT / "arch/include",
                    ROOT / "vendors/arm/CMSIS_6/CMSIS/Core/Include"]
        if sdk is None:
            vendor = ROOT / "vendors/gigadevice/gd32f4xx/Firmware"
            includes += [vendor / "CMSIS/GD/GD32F4xx/Include",
                         vendor / "GD32F4xx_standard_peripheral/Include"]
        else:
            includes += [
                ROOT / "vendors/st" / ("cmsis_device_" + sdk) / "Include",
                ROOT / "vendors/st" / ("stm32" + sdk + "_hal") / "Inc"]
        arguments = [*self.compiler, "-std=c11", "-Os", "-Werror",
                     "-mcpu=" + cpu, "-mthumb", "-D" + part,
                     "-mfpu=" + {"cortex-m4": "fpv4-sp-d16",
                                  "cortex-m7": "fpv5-d16",
                                  "cortex-m33": "fpv5-sp-d16"}[cpu],
                     "-mfloat-abi=hard", "-fshort-enums",
                     "-DCMSIS_DISABLE_DEPRECATED=1", "-c", str(probe),
                     "-o", str(output)]
        for directory in includes:
            arguments += ["-I", str(directory)]
        result = subprocess.run(arguments, capture_output=True, text=True,
                                timeout=30)
        return result, output

    def test_all_exact_specializations_export_their_named_readonly_table(self):
        nm = Path(shutil.which(self.compiler[0])).with_name("arm-none-eabi-nm")
        self.assertTrue(nm.is_file(), "Prepared ARM symbol tool is required")
        for selection in self.selections:
            with self.subTest(family=selection[0], mode=selection[1]):
                result, output = self.compile(selection)
                self.assertEqual(result.returncode, 0, result.stderr)
                symbols = subprocess.run(
                    [str(nm), "--defined-only", str(output)],
                    capture_output=True, text=True, check=True, timeout=10)
                definitions = {name: kind for _, kind, name in
                               (line.split() for line in
                                symbols.stdout.splitlines()
                                if len(line.split()) == 3)}
                prefix = selection[6]
                for suffix in ("initialize", "irq"):
                    self.assertEqual(
                        definitions.get(prefix + "_" + suffix), "T")
                self.assertEqual(definitions.get(prefix + "_ops"), "R")
                self.assertFalse(any(name.startswith(("NX_CAPTURE_", "NX_ADV_",
                                                       "NX_UART_FIFO_"))
                                     for name in definitions))

    def test_missing_export_binding_is_rejected_by_the_template(self):
        for selection in self.selections:
            with self.subTest(family=selection[0], mode=selection[1]):
                result, _ = self.compile(selection,
                                         omit=selection[5] + "_INITIALIZE")
                self.assertNotEqual(result.returncode, 0,
                                    "Missing binding emitted a macro symbol")
                self.assertIn("requires one complete concrete specialization",
                              result.stderr)

    def test_template_helpers_do_not_escape_the_include(self):
        selections = ((self.selections[1], "NX_CAPTURE_COUNTER_MAX"),
                      (self.selections[3], "NX_TIMER_WRITE"),
                      (self.selections[6], "FIFO_TX_CR1"))
        for selection, helper in selections:
            with self.subTest(family=selection[0], helper=helper):
                check = ("#ifdef " + helper +
                         "\n#error private helper escaped\n"
                         "#endif\n#ifndef " + selection[5] + "_OPS\n"
                         "#error wrapper binding was consumed\n#endif\n")
                result, _ = self.compile(selection, append=check)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_zero_fifo_depth_is_not_a_usable_specialization(self):
        result, _ = self.compile(self.selections[5],
                                 replace=("NX_UART_FIFO_DEPTH", "0U"))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unsupported FIFO depth", result.stderr)

    def test_unknown_flow_selector_is_not_implicitly_enabled(self):
        result, _ = self.compile(self.selections[8],
                                 replace=("NX_UART_FIFO_RTSCTS", "2"))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unsupported FIFO flow selector", result.stderr)


if __name__ == "__main__":
    unittest.main()
