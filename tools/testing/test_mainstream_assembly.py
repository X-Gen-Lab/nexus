"""Explicit mainstream software fixtures use the ordinary platform build graph."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

import yaml

from scripts.ci import provider_assemblies
from tools.configure import bindings, configure, factory
from tools.evidence.common import EvidenceError


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tests/contracts/mainstream_assembly"
EXPECTED = {
    "f103-io.toml", "f103-io-freertos.toml",
    "f103-spi.toml", "f103-spi-freertos.toml",
    "f103-watchdog.toml", "f103-exti.toml",
    "f103-devices.toml", "f103-devices-freertos.toml",
    "gd303-io.toml", "gd303-io-freertos.toml",
    "gd303-spi-watchdog.toml", "gd303-spi-watchdog-freertos.toml",
    "gd303-devices.toml", "gd303-devices-freertos.toml"}


class MainstreamAssemblyTests(unittest.TestCase):
    def test_workflow_admits_one_mainstream_job_without_duplicate_existing_families(self):
        workflow = yaml.safe_load((ROOT / ".github/workflows/build-matrix.yml").read_text())
        fixtures = workflow["jobs"]["controller-routes"]["strategy"]["matrix"]["fixture"]
        for name in ("mainstream_assembly", "g474_assembly", "h743_assembly", "u575_assembly"):
            with self.subTest(fixture=name):
                self.assertEqual(fixtures.count(name), 1)

    def test_every_explicit_reference_exists_and_resolves_without_mutation(self):
        self.assertEqual({path.name for path in FIXTURE.glob("*.toml")}, EXPECTED)
        paths = [FIXTURE / name for name in sorted(EXPECTED)]
        original = {path: path.read_bytes() for path in paths}
        rows = provider_assemblies.validate_fixtures(ROOT, paths)
        self.assertEqual(len(rows), 14)
        self.assertEqual({row["resolved"]["soc_family"] for row in rows},
                         {"stm32f103", "gd32f303"})
        self.assertEqual({row["resolved"]["backend"] for row in rows},
                         {"baremetal", "freertos"})
        for row in rows:
            resolved = row["resolved"]
            expected = "soft" if resolved["soc_family"] == "stm32f103" else "hard"
            self.assertEqual(resolved["cpu"]["float_abi"], expected)
        self.assertEqual({path: path.read_bytes() for path in paths}, original)

    def test_g474_link_main_compiles_with_each_selected_uart_identity(self):
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "Host C compiler is required")
        fixture = ROOT / "tests/contracts/g474_assembly"
        for name in ("io", "mainstream"):
            with self.subTest(assembly=name), tempfile.TemporaryDirectory() as temp:
                result = configure.resolve_ir(fixture / (name + ".toml"))
                self.assertTrue(any(item.kind == "uart" for item in result.controllers))
                generated = Path(temp)
                (generated / "nexus_bindings.h").write_text(bindings.emit(result)[0])
                (generated / "nexus_factory.h").write_text(factory.emit(result)[0])
                command = [compiler, "-std=c11", "-Werror", "-fsyntax-only",
                           "-DNX_G474_IO_FIXTURE", "-I" + str(generated)]
                command += ["-I" + str(ROOT / layer / "include")
                            for layer in ("io", "core", "arch")]
                execution = subprocess.run(command + [str(fixture / "main.c")],
                                           capture_output=True, text=True)
                self.assertEqual(execution.returncode, 0,
                                 execution.stdout + execution.stderr)

    def test_g474_link_main_passively_retains_each_selected_optional_face(self):
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "Host C compiler is required")
        fixture = ROOT / "tests/contracts/g474_assembly"
        harness = r'''#include "nexus_bindings.h"
#include "nexus_factory.h"
static uint16_t calls[8];
static bool bad_id;
int nx_fixture_main(void);
nx_platform_start_result_t nx_platform_start(void) {
    return (nx_platform_start_result_t){.primary = NX_SUCCESS};
}
nx_result_t nx_platform_stop(void) { return NX_SUCCESS; }
const nx_pwm_port_t* nx_factory_pwm(nx_pwm_id_t id) {
    if (id != calls[0]++) bad_id = true;
    return NULL;
}
const nx_watchdog_port_t* nx_factory_watchdog(nx_watchdog_id_t id) {
    if (id != calls[1]++) bad_id = true;
    return NULL;
}
const nx_spi_port_t* nx_factory_spi(nx_spi_id_t id) {
    if (id != calls[2]++) bad_id = true;
    return NULL;
}
const nx_spi_endpoint_t* nx_factory_spi_device(nx_spi_device_id_t id) {
    if (id != calls[3]++) bad_id = true;
    return NULL;
}
const nx_adc_port_t* nx_factory_adc(nx_adc_id_t id) {
    if (id != calls[4]++) bad_id = true;
    return NULL;
}
const nx_i2c_port_t* nx_factory_i2c(nx_i2c_id_t id) {
    if (id != calls[5]++) bad_id = true;
    return NULL;
}
const nx_i2c_endpoint_t* nx_factory_i2c_device(nx_i2c_device_id_t id) {
    if (id != calls[6]++) bad_id = true;
    return NULL;
}
const nx_flash_port_t* nx_factory_flash(nx_flash_id_t id) {
    if (id != calls[7]++) bad_id = true;
    return NULL;
}
int main(void) {
    const uint16_t selected[] = {
        NX_PWM_ID_COUNT, NX_WATCHDOG_ID_COUNT, NX_SPI_ID_COUNT,
        NX_SPI_DEVICE_ID_COUNT, NX_ADC_ID_COUNT, NX_I2C_ID_COUNT,
        NX_I2C_DEVICE_ID_COUNT, NX_FLASH_ID_COUNT};
    const uint16_t expected[] = {EXPECTED_COUNTS};
    if (nx_fixture_main() != 0 || bad_id) return 1;
    for (size_t index = 0U; index != sizeof(calls) / sizeof(calls[0]); ++index)
        if (selected[index] != expected[index] || calls[index] != selected[index])
            return 1;
    return 0;
}
'''
        expected_counts = {
            "empty": (0, 0, 0, 0, 0, 0, 0, 0),
            "pwm": (2, 0, 0, 0, 0, 0, 0, 0),
            "watchdog": (0, 1, 0, 0, 0, 0, 0, 0),
            "spi": (0, 1, 2, 2, 0, 0, 0, 0),
            "acquisition": (0, 0, 0, 0, 1, 1, 1, 1),
            "acquisition-dma": (0, 0, 0, 0, 1, 1, 1, 1)}
        for name, counts in expected_counts.items():
            with self.subTest(assembly=name), tempfile.TemporaryDirectory() as temp:
                result = configure.resolve_ir(fixture / (name + ".toml"))
                generated = Path(temp)
                (generated / "nexus_bindings.h").write_text(bindings.emit(result)[0])
                (generated / "nexus_factory.h").write_text(factory.emit(result)[0])
                source = generated / "harness.c"
                source.write_text(harness.replace(
                    "EXPECTED_COUNTS", ", ".join(str(count) for count in counts)))
                common = [compiler, "-std=c11", "-Werror", "-I" + str(generated)]
                common += ["-I" + str(ROOT / layer / "include")
                           for layer in ("io", "core", "arch")]
                object_file = generated / "fixture.o"
                executable = generated / "fixture"
                commands = [common + ["-Dmain=nx_fixture_main", "-c",
                                      str(fixture / "main.c"), "-o", str(object_file)],
                            common + [str(source), str(object_file), "-o", str(executable)],
                            [str(executable)]]
                for command in commands:
                    execution = subprocess.run(command, capture_output=True, text=True)
                    self.assertEqual(execution.returncode, 0,
                                     f"{command}: {execution.stdout}{execution.stderr}")

    def test_missing_authored_fixture_fails_closed_in_the_existing_validator(self):
        with self.assertRaises(EvidenceError):
            provider_assemblies.validate_fixtures(ROOT, [FIXTURE / "missing.toml"])


if __name__ == "__main__":
    unittest.main()
