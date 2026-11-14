"""Execute typed F407 RNG selection, clock admission and its source closure."""

import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from tools.configure import configure

ROOT = Path(__file__).resolve().parents[2]


class EntropyTargetTests(unittest.TestCase):
    def configure(self, *, sysclk_source="input", pll48=False, pll_q=7,
                  build=False, selected=True):
        self.assertIsNotNone(shutil.which("arm-none-eabi-gcc"))
        self.assertIsNotNone(shutil.which("cmake"))
        with tempfile.TemporaryDirectory(prefix="rng typed ", dir=ROOT / "build") as temp:
            source = Path(temp)
            assembly = source / "assembly.toml"
            board = ROOT / "tests/contracts/entropy_assembly/ve-board"
            assembly.write_text(
                f'schema=2\nboard_package="{board}"\nbackend="baremetal"\n'
                'clock="custom"\n[memory]\nmain_stack_bytes=2048\n'
                + ('[entropy.random]\nbinding="rng"\nmode="rng-words"\n'
                   if selected else '')
                + '[clock_parameters]\nsource="hsi"\ninput_hz=16000000\n'
                'pll_m=16\npll_n=336\npll_p=4\n'
                f'pll_q={pll_q}\nahb_div=1\napb1_div=4\napb2_div=2\n'
                'adc_div=4\nsupply_mv=3300\ncss=false\n'
                f'sysclk_source="{sysclk_source}"\n'
                f'pll48={str(pll48).lower()}\n')
            (source / "main.c").write_text(
                '#include "nexus_factory.h"\n'
                '#include "nexus_bindings.h"\n'
                'int main(void) {\n'
                '    if (nx_platform_start().primary != NX_SUCCESS) return 1;\n'
                '    uint32_t word;\n'
                '    (void)nx_entropy_port_try_read(\n'
                '        nx_factory_entropy(NX_ENTROPY_ID_RANDOM), &word);\n'
                '    return (int)nx_platform_stop();\n}\n')
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.31)\n'
                'project(rng_typed_contract LANGUAGES C ASM)\n'
                'set(NEXUS_BUILD_TESTS OFF CACHE BOOL "")\n'
                'set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "")\n'
                'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
                f'set(NEXUS_ASSEMBLY_FILE "{assembly}")\n'
                f'add_subdirectory("{ROOT}" nexus-sdk)\n'
                + ('nexus_add_firmware(app SOURCES main.c)\n' if build else ''))
            result = subprocess.run(
                ["cmake", "-S", str(source), "-B", str(source / "binary"),
                 "-G", "Ninja", f"-DPython3_EXECUTABLE={sys.executable}",
                 f"-DCMAKE_TOOLCHAIN_FILE={ROOT}/cmake/toolchains/arm-gcc.cmake"],
                capture_output=True, text=True, check=False, timeout=180)
            if result.returncode != 0:
                return result
            commands = json.loads((source / "binary/compile_commands.json").read_text())
            rng = [item for item in commands
                   if item["file"].endswith("/drivers/entropy.c")]
            self.assertEqual(len(rng), 1 if selected else 0)
            generated = (source / "binary/nexus-sdk/generated/bindings.c").read_text()
            self.assertEqual("nx_stm32f407_entropy_ops" in generated, selected)
            self.assertNotIn("entropy_instance", generated)
            if not build:
                return result
            environment = dict(os.environ)
            # An outer build_slot owns this complete compiler-using module.
            result = subprocess.run(
                ["cmake", "--build", str(source / "binary"), "--parallel", "2",
                 "--target", "app"], capture_output=True, text=True,
                env=environment, check=False, timeout=180)
            if result.returncode == 0:
                symbols = subprocess.run(
                    ["arm-none-eabi-nm", "--defined-only", "--format=posix",
                     str(source / "binary/bin/app.elf")],
                    capture_output=True, text=True, check=True, timeout=30).stdout
                for name in ("nx_factory_entropy", "nx_stm32f407_entropy_admit",
                             "nx_stm32f407_entropy_start",
                             "nx_stm32f407_entropy_stop"):
                    self.assertRegex(symbols, rf"(?m)^{name} T ")
                self.assertNotIn("entropy_instance", symbols)
            return result

    def test_direct_input_without_reserved_pll48_rejects_the_rng_selection(self):
        result = self.configure()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertRegex(result.stdout + result.stderr,
                         r"Entropy\s+requires\s+its\s+exact\s+selected"
                         r"\s+nominal\s+kernel\s+clock")

    def test_active_main_pll_with_other_q_frequency_cannot_supply_rng(self):
        result = self.configure(sysclk_source="pll", pll_q=8)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertRegex(result.stdout + result.stderr,
                         r"Entropy\s+requires\s+its\s+exact\s+selected"
                         r"\s+nominal\s+kernel\s+clock")

    def test_explicit_direct_hsi_pll48_is_a_valid_rng_clock_owner(self):
        result = self.configure(pll48=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_selected_factory_links_the_public_entropy_word_api(self):
        result = self.configure(pll48=True, build=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_unselected_rng_adds_no_driver_or_hardware_owner(self):
        result = self.configure(selected=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_unknown_entropy_fields_are_rejected(self):
        path = ROOT / "tests/contracts/entropy_assembly/ve.toml"
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temp:
            assembly = Path(temp) / "assembly.toml"
            assembly.write_text(path.read_text().replace('board_package="ve-board"',
                f'board_package="{path.parent / "ve-board"}"') + 'unknown=true\n')
            with self.assertRaisesRegex(ValueError, "unknown"):
                configure.resolve_ir(assembly)

    def test_rng_uses_the_typed_platform_source_closure(self):
        spec = importlib.util.spec_from_file_location(
            "rng_sdk_payload", ROOT / "cmake/package/package_source_sdk.py")
        source_sdk = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(source_sdk)
        required = set(source_sdk.required_files(ROOT))
        current = {"soc/stm32/stm32f4/drivers/entropy.c",
                   "soc/stm32/stm32f4/private/stm32f407_entropy.h",
                   "io/include/nexus/io/entropy.h",
                   "io/src/api/entropy.c"}
        retired = {"soc/stm32/stm32f4/Entropy.cmake",
                   "soc/stm32/stm32f4/entropy_instance.c",
                   "soc/stm32/stm32f4/include/nexus/soc/stm32f407_entropy.h"}
        self.assertTrue(current.issubset(required))
        self.assertFalse(retired & required)

    def test_official_rng_inline_headers_close_the_packaged_source_graph(self):
        spec = importlib.util.spec_from_file_location(
            "entropy_f4_lock", ROOT / "tools/maintenance/stm32f4_hal_lock.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        required = {f"Inc/stm32f4xx_ll_{kind}.h"
                    for kind in ("bus", "rcc", "rng")}
        self.assertLessEqual(required, module.REVIEWED_FILES)
        lock = json.loads((ROOT / "dependencies/stm32f4-hal.lock.json").read_text())
        self.assertLessEqual(required, module.contract(lock))


if __name__ == "__main__":
    unittest.main()
