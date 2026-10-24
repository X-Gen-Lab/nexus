"""U575 MPU geometry is an exact part fact, never a configurable upper bound."""
import copy
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure, kernel
from tools.configure.providers import stm32u575
from scripts.ci import stm32u575 as image_gate
from tools.evidence.common import EvidenceError


ROOT = Path(__file__).resolve().parents[2]


class U575MpuGeometryTests(unittest.TestCase):
    def fixture(self, backend):
        name = "io-freertos.toml" if backend == "freertos" else "io.toml"
        return ROOT / "tests/contracts/u575_assembly" / name

    def test_machine_fact_and_provider_declare_exact_eight_regions(self):
        facts = configure.load(ROOT / "soc/stm32/stm32u575/soc.json")
        self.assertEqual(facts["cpu"]["mpu_regions"], 8)
        self.assertEqual(stm32u575.CPU_FEATURES["mpu_regions"], 8)

    def test_both_backends_resolve_the_same_exact_geometry(self):
        for backend in ("baremetal", "freertos"):
            with self.subTest(backend=backend):
                result = configure.resolve_ir(self.fixture(backend))
                self.assertEqual(result.cpu_profile.definitions[
                    "NEXUS_CPU_MPU_REGIONS"], 8)
                self.assertEqual(result["cpu"]["mpu_regions"], 8)

    def test_generated_header_and_cmake_share_exact_eight(self):
        for backend in ("baremetal", "freertos"):
            with self.subTest(backend=backend), tempfile.TemporaryDirectory() as value:
                output = Path(value) / "generated"
                configure.configure(self.fixture(backend), output)
                self.assertIn("#define NEXUS_CPU_MPU_REGIONS 8u\n",
                              (output / "nexus_config.h").read_text())
                self.assertIn('set(NEXUS_CPU_MPU_REGIONS "8")',
                              (output / "selection.cmake").read_text())

    def test_unreviewed_sixteen_cannot_override_exact_part_fact(self):
        facts = configure.load(ROOT / "soc/stm32/stm32u575/soc.json")
        facts["cpu"]["mpu_regions"] = 16
        with self.assertRaisesRegex(ValueError, "capabilities differ"):
            configure.validate_soc(facts, "STM32U575ZIT6Q")
        resolved = configure.resolve_ir(self.fixture("baremetal")).to_dict()
        image_gate.validate_profile(resolved)
        forged = copy.deepcopy(resolved)
        forged["cpu"]["mpu_regions"] = 16
        with self.assertRaises(EvidenceError):
            image_gate.validate_profile(forged)

    def test_protected_kernel_keeps_exact_cpu_geometry(self):
        result = configure.resolve_ir(self.fixture("freertos"))
        protected = kernel.resolve({"memory_protection": True},
                                   result.cpu_profile, result["clock"]["core_hz"])
        self.assertTrue(protected.memory_protection)
        self.assertEqual(result.cpu_profile.definitions[
            "NEXUS_CPU_MPU_REGIONS"], 8)


if __name__ == "__main__":
    unittest.main()
