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
    "gd303-io.toml", "gd303-io-freertos.toml",
    "gd303-spi-watchdog.toml", "gd303-spi-watchdog-freertos.toml"}


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
        self.assertEqual(len(rows), 10)
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
static uint16_t calls[4];
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
int main(void) {
    if (nx_fixture_main() != 0 || bad_id) return 1;
    return calls[0] != NX_PWM_ID_COUNT || calls[1] != NX_WATCHDOG_ID_COUNT ||
           calls[2] != NX_SPI_ID_COUNT || calls[3] != NX_SPI_DEVICE_ID_COUNT;
}
'''
        for name in ("pwm", "watchdog", "spi"):
            with self.subTest(assembly=name), tempfile.TemporaryDirectory() as temp:
                result = configure.resolve_ir(fixture / (name + ".toml"))
                generated = Path(temp)
                (generated / "nexus_bindings.h").write_text(bindings.emit(result)[0])
                (generated / "nexus_factory.h").write_text(factory.emit(result)[0])
                source = generated / "harness.c"
                source.write_text(harness)
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
