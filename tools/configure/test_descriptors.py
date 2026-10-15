"""Immutable integration and hardware binding boundary regressions."""
import dataclasses
import subprocess
import sys
import unittest

try:
    from .providers import descriptor, hardware
except ImportError:
    from providers import descriptor, hardware


class DescriptorBoundaryTests(unittest.TestCase):
    def test_h743_dma_configuration_binds_private_provider_payload(self):
        payload = descriptor.maintained("stm32h743").owned_payload
        for name in ("soc/stm32h743/drivers/uart_dma.c",
                     "soc/stm32h743/private/stm32h743_dma.h"):
            self.assertIn(name, payload)

    def test_native_configuration_does_not_import_unselected_physical_families(self):
        run = subprocess.run([sys.executable, "-B", "-c",
            "import sys; from tools.configure.providers import maintained; "
            "maintained('native'); "
            "assert not any(name.endswith(('stm32f407', 'gd32f470', 'stm32g474')) "
            "for name in sys.modules)"], capture_output=True, text=True,
            check=False)
        self.assertEqual(run.returncode, 0, run.stderr)

    def test_unknown_family_has_no_cortex_m_fallback(self):
        for family in ("cortex-m4", "stm32", "stm32h743-unreviewed", "../stm32f407"):
            with self.subTest(family=family), self.assertRaises(ValueError):
                descriptor.maintained(family)

    def test_integration_and_binding_are_deeply_immutable(self):
        family = descriptor.maintained("stm32f407")
        port = hardware.maintained("stm32f407", "SPI1")
        with self.assertRaises(dataclasses.FrozenInstanceError):
            family.family = "gd32f470"
        with self.assertRaises(dataclasses.FrozenInstanceError):
            port.apb = 1
        with self.assertRaises(TypeError):
            port.dma_modes[0] = ("dma", ())

    def test_st_source_closure_uses_exact_family_device_sdk(self):
        f4 = descriptor.maintained("stm32f407")
        g4 = descriptor.maintained("stm32g474")
        self.assertEqual(f4.git_dependencies, (
            ("cmsis_core", "vendors/arm/CMSIS_5"),
            ("cmsis_device_f4", "vendors/st/cmsis_device_f4"),
            ("hal_driver_f4", "vendors/st/stm32f4_hal")))
        self.assertEqual(g4.git_dependencies[-1], (
            "cmsis_device_g4", "vendors/st/cmsis_device_g4"))
        self.assertEqual(descriptor.maintained("gd32f470").import_lock,
                         "vendors/gigadevice/gd32f4xx/source.lock.json")

    def test_full_duplex_direction_and_vendor_numbering_stay_distinct(self):
        stm = hardware.maintained("stm32f407", "SPI1")
        gd = hardware.maintained("gd32f470", "SPI4")
        self.assertEqual(stm.dma("dma"), ((2, 3, 3), (2, 0, 3)))
        self.assertEqual(gd.dma("dma-full-duplex"), ((1, 4, 2), (1, 3, 2)))
        self.assertEqual(stm.apb, 2)
        self.assertEqual(gd.apb, 2)
        self.assertIsNone(gd.dma("dma"))

    def test_i2c_receive_then_transmit_has_exact_distinct_vectors(self):
        port = hardware.maintained("stm32f407", "I2C2")
        self.assertEqual(port.irqs, (("I2C2_EV", 33), ("I2C2_ER", 34)))
        self.assertEqual(port.dma("dma-messages"), ((1, 2, 7), (1, 7, 7)))
        self.assertEqual(hardware.dma_irq("stm32f407", 1, 7), 47)
        self.assertEqual(hardware.dma_irq("gd32f470", 0, 7), 47)

    def test_no_cross_family_or_unmaintained_controller_fallback(self):
        for family, controller in (("stm32f407", "USART0"),
                                   ("gd32f470", "SPI1"),
                                   ("stm32g474", "USART1")):
            with self.subTest(family=family, controller=controller):
                with self.assertRaises(ValueError):
                    hardware.maintained(family, controller)


if __name__ == "__main__":
    unittest.main()
