"""Immutable package metadata and exact hardware binding regressions."""
import dataclasses
import hashlib
import subprocess
import sys
import unittest

from tools.configure import dependencies
from tools.configure.providers import descriptor
from tools.configure.tests.f4_fixture import load_f4_fixture


class DescriptorBoundaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.selected = load_f4_fixture("STM32F407VET6")

    def binding(self, controller):
        return next(item for item in self.selected.provider.hardware_facts.CONTROLLER_BINDINGS
                    if item.controller == controller)

    def test_hardware_tables_retain_the_exact_package_snapshot_identity(self):
        selected = self.selected
        for name, digest in selected.provider.hardware_facts.FACT_SHA256.items():
            path = selected.package.fact(name)
            self.assertEqual(digest, hashlib.sha256(selected.snapshots[path]).hexdigest())
        names = dependencies.source_names("platform", "stm32f4")
        self.assertIn("tools/configure/hardware_bindings.py", names)
        self.assertFalse(any(name.startswith("tools/configure/stm32f4/")
                             for name in names))

    def test_native_configuration_does_not_import_physical_family_adapters(self):
        program = (
            "import sys; from tools.configure.providers import maintained; "
            "maintained('native', test_support=True); "
            "assert not any(name.startswith('tools.configure.stm32f4') "
            "for name in sys.modules)")
        run = subprocess.run([sys.executable, "-B", "-c", program],
                             capture_output=True, text=True, check=False)
        self.assertEqual(run.returncode, 0, run.stderr)

    def test_unknown_family_has_no_cortex_m_fallback(self):
        for family in ("cortex-m4", "stm32", "stm32h743-unreviewed", "../stm32f407"):
            with self.subTest(family=family), self.assertRaises(ValueError):
                descriptor.maintained(family)

    def test_integration_and_binding_are_deeply_immutable(self):
        family = descriptor.maintained("stm32f4")
        port = self.binding("SPI1")
        with self.assertRaises(dataclasses.FrozenInstanceError):
            family.family = "gd32f470"
        with self.assertRaises(dataclasses.FrozenInstanceError):
            port.apb = 1
        with self.assertRaises(TypeError):
            port.dma_modes[0] = ("dma", ())

    def test_st_source_closure_uses_exact_family_device_sdk(self):
        f4 = descriptor.maintained("stm32f4")
        self.assertEqual(f4.git_dependencies, (
            ("cmsis_core", "vendors/arm/CMSIS_6"),
            ("cmsis_device_f4", "vendors/st/cmsis_device_f4"),
            ("hal_driver_f4", "vendors/st/stm32f4_hal")))
        self.assertEqual(f4.package_manifest, "soc/stm32/stm32f4/package.json")
        self.assertEqual(f4.configuration_modules, ())

    def test_full_duplex_directions_keep_distinct_routes(self):
        port = self.binding("SPI1")
        self.assertEqual(port.dma("dma"), ((2, 3, 3), (2, 0, 3)))
        self.assertEqual(port.apb, 2)
        self.assertIsNone(port.dma("unreviewed"))

    def test_i2c_receive_then_transmit_has_exact_distinct_vectors(self):
        port = self.binding("I2C2")
        self.assertEqual(port.irqs, (("I2C2_EV", 33), ("I2C2_ER", 34)))
        self.assertEqual(port.dma("dma-messages"), ((1, 2, 7), (1, 7, 7)))
        irqs = dict(self.selected.provider.hardware_facts.DMA_IRQS)
        self.assertEqual(irqs[1][7], 47)

    def test_exact_part_does_not_invent_a_controller(self):
        f411 = load_f4_fixture("STM32F411RET6")
        controllers = f411.provider.soc["controllers"]
        self.assertNotIn("CAN1", controllers)
        self.assertNotIn("USART0", controllers)


class CMSISCoreDescriptorContracts(unittest.TestCase):
    def test_catalogue_uses_neutral_identity_and_exact_upstream_notice(self):
        from tools.maintenance.source_catalogue import SOURCE_DEPENDENCIES

        core, = [item for item in SOURCE_DEPENDENCIES
                 if item.identifier == "cmsis_core"]
        self.assertEqual(core.path, "vendors/arm/CMSIS_6")
        self.assertEqual(core.upstream_url,
                         "https://github.com/ARM-software/CMSIS_6.git")
        self.assertEqual(core.notice, "LICENSE")
        self.assertFalse(any(item.identifier == "cmsis5"
                             for item in SOURCE_DEPENDENCIES))

    def test_declared_cortex_m_packages_bind_the_neutral_core_identity(self):
        for item in descriptor.families():
            with self.subTest(family=item.family):
                self.assertIn(("cmsis_core", "vendors/arm/CMSIS_6"),
                              item.git_dependencies)


if __name__ == "__main__":
    unittest.main()
