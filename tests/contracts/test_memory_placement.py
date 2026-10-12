"""Compile the actual variable-only optional memory placement attributes."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import re


ROOT = Path(__file__).resolve().parents[2]
REGIONS = ("ADDSRAM", "TCM", "CCM")
KINDS = ("DATA", "BSS", "NOINIT")


class MemoryPlacementTests(unittest.TestCase):
    def compile(self, compiler, source, selected=True):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            flag = int(selected)
            (directory / "nexus_config.h").write_text(
                f"#define NEXUS_MEMORY_ADDSRAM {flag}\n"
                f"#define NEXUS_MEMORY_TCM {flag}\n"
                f"#define NEXUS_MEMORY_CCM {flag}\n")
            input_file = directory / "placement.c"
            input_file.write_text('#include "nexus/core/memory.h"\n' + source)
            return subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra",
                "-Werror", "-Wpedantic", "-fcommon", "-I", str(directory),
                "-I", str(ROOT / "core/include"), "-c", str(input_file),
                "-o", str(directory / "placement.o")], capture_output=True,
                text=True, check=False)

    def compilers(self):
        compiler = shutil.which("gcc")
        if not compiler:
            self.fail("The actual maintained GNU C compiler is required")
        optional = shutil.which("clang")
        return [compiler, optional] if optional else [compiler]

    def test_selected_variable_sections_compile(self):
        for compiler in self.compilers():
            for region in REGIONS:
                for kind in KINDS:
                    with self.subTest(compiler=compiler, region=region, kind=kind):
                        initializer = " = 7" if kind == "DATA" else ""
                        result = self.compile(compiler,
                            f"NX_MEMORY_{region}_{kind} unsigned value"
                            f"{initializer};\n")
                        self.assertEqual(result.returncode, 0, result.stderr)

    def test_unselected_use_is_a_compile_error(self):
        for compiler in self.compilers():
            for region in REGIONS:
                for kind in KINDS:
                    with self.subTest(compiler=compiler, region=region, kind=kind):
                        result = self.compile(compiler,
                            f"NX_MEMORY_{region}_{kind} unsigned value;\n", False)
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn("memory region is not selected", result.stderr)

    def test_function_misuse_is_rejected_by_variable_only_attribute(self):
        for compiler in self.compilers():
            for region in REGIONS:
                for kind in KINDS:
                    with self.subTest(compiler=compiler, region=region, kind=kind):
                        result = self.compile(compiler,
                            f"NX_MEMORY_{region}_{kind} void wrong(void) {{}}\n")
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn("nocommon", result.stderr)

    def test_nonzero_bss_and_noinit_initializer_is_rejected(self):
        for compiler in self.compilers():
            for region in REGIONS:
                for kind in ("BSS", "NOINIT"):
                    with self.subTest(compiler=compiler, region=region, kind=kind):
                        result = self.compile(compiler,
                            f"NX_MEMORY_{region}_{kind} unsigned wrong = 17;\n")
                        self.assertNotEqual(result.returncode, 0,
                            "NOLOAD would silently discard the declared value")
                        self.assertIn("zero", result.stderr.lower())

    def test_each_variable_is_independently_garbage_collectable(self):
        nm = shutil.which("nm")
        if not nm:
            self.fail("Actual GNU ELF symbol inspection is required")
        for compiler in self.compilers():
            for region in REGIONS:
                for kind in KINDS:
                    with self.subTest(compiler=compiler, region=region, kind=kind):
                        with tempfile.TemporaryDirectory() as temporary:
                            directory = Path(temporary)
                            (directory / "nexus_config.h").write_text(
                                "#define NEXUS_MEMORY_ADDSRAM 1\n"
                                "#define NEXUS_MEMORY_TCM 1\n"
                                "#define NEXUS_MEMORY_CCM 1\n")
                            initializer = " = 7" if kind == "DATA" else ""
                            source = directory / "gc.c"
                            source.write_text(
                                '#include "nexus/core/memory.h"\n'
                                f"NX_MEMORY_{region}_{kind} volatile unsigned live"
                                f"{initializer};\n"
                                f"NX_MEMORY_{region}_{kind} __attribute__((used)) "
                                f"volatile unsigned dead{initializer};\n"
                                "void entry(void) { live = 1; for (;;) {} }\n")
                            elf = directory / "gc.elf"
                            result = subprocess.run([compiler, "-std=c11",
                                "-ffunction-sections", "-fdata-sections",
                                "-nostdlib", "-Wl,--gc-sections", "-Wl,-e,entry",
                                "-Wl,--build-id=none", "-I", str(directory),
                                "-I", str(ROOT / "core/include"), str(source),
                                "-o", str(elf)], capture_output=True, text=True,
                                check=False)
                            self.assertEqual(result.returncode, 0, result.stderr)
                            symbols = subprocess.run([nm, str(elf)],
                                capture_output=True, text=True, check=True).stdout
                            self.assertIsNotNone(re.search(r"\blive$", symbols,
                                re.MULTILINE))
                            self.assertIsNone(re.search(r"\bdead$", symbols,
                                re.MULTILINE), "Unused placement variable shared "
                                "an input section with live storage")


if __name__ == "__main__":
    unittest.main()
