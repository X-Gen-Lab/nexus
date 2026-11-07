"""Immutable integration and hardware binding boundary regressions."""
import dataclasses
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import subprocess
import sys
import unittest

from tools.configure import dependencies
from tools.configure.providers import descriptor, hardware


class DescriptorBoundaryTests(unittest.TestCase):
    def test_hardware_lookup_loads_only_the_selected_registered_soc(self):
        for family, selected, other, controller in (("stm32f4", "tools.configure.stm32f4",
                 "tests.support.native.configure", "SPI1"),):
            with self.subTest(family=family):
                code = "\n".join((
                    "import sys",
                    "from tools.configure import secondary_controllers",
                    "from tools.configure.providers import hardware",
                    "family, selected, other, controller = sys.argv[1:]",
                    "assert selected not in sys.modules",
                    "assert other not in sys.modules",
                    "binding = hardware.maintained(family, controller)",
                    "assert binding.controller == controller",
                    "assert selected + '.hardware' in sys.modules",
                    "assert other not in sys.modules",
                    "module = sys.modules[selected + '.hardware']",
                    "assert binding in module.CONTROLLER_BINDINGS",
                    "assert hardware.bindings(family) is "
                    "module.CONTROLLER_BINDINGS",
                ))
                run = subprocess.run([
                    sys.executable, "-B", "-c", code,
                    family, selected, other, controller], capture_output=True,
                    text=True, check=False)
                self.assertEqual(run.returncode, 0, run.stderr)

    def test_soc_hardware_tables_are_selected_configuration_inputs(self):
        names = dependencies.source_names("platform", "stm32f4")
        for name in ("hardware", "preflight"):
            helper = "tools/configure/stm32f4/" + name + ".py"
            self.assertIn(helper, names)
            self.assertNotIn(helper, dependencies.source_names(
                "platform", "native", test_support=True))





    def test_native_configuration_does_not_import_unselected_physical_families(self):
        program = (
            "import sys; from tools.configure.providers import maintained; " +
            "maintained('native', test_support=True); " +
            "assert not any(name.startswith('tools.configure.stm32f4') for name in sys.modules)")
        run = subprocess.run([sys.executable, "-B", "-c", program],
                             capture_output=True, text=True, check=False)
        self.assertEqual(run.returncode, 0, run.stderr)

    def test_unknown_family_has_no_cortex_m_fallback(self):
        for family in ("cortex-m4", "stm32", "stm32h743-unreviewed", "../stm32f407"):
            with self.subTest(family=family), self.assertRaises(ValueError):
                descriptor.maintained(family)

    def test_integration_and_binding_are_deeply_immutable(self):
        family = descriptor.maintained("stm32f4")
        port = hardware.maintained("stm32f4", "SPI1")
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

    def test_full_duplex_directions_keep_distinct_f4_routes(self):
        stm = hardware.maintained("stm32f4", "SPI1")
        self.assertEqual(stm.dma("dma"), ((2, 3, 3), (2, 0, 3)))
        self.assertEqual(stm.apb, 2)

    def test_i2c_receive_then_transmit_has_exact_distinct_vectors(self):
        port = hardware.maintained("stm32f4", "I2C2")
        self.assertEqual(port.irqs, (("I2C2_EV", 33), ("I2C2_ER", 34)))
        self.assertEqual(port.dma("dma-messages"), ((1, 2, 7), (1, 7, 7)))
        self.assertEqual(hardware.dma_irq("stm32f4", 1, 7), 47)

    def test_no_cross_family_or_unmaintained_controller_fallback(self):
        for family, controller in (("stm32f4", "USART0"),
                                   ("gd32f470", "SPI1"),
                                   ("stm32g474", "USART1")):
            with self.subTest(family=family, controller=controller):
                with self.assertRaises(ValueError):
                    hardware.maintained(family, controller)


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

    def test_every_mcu_family_binds_the_same_neutral_core_identity(self):
        for item in descriptor.families():
            if item.family == "native":
                continue
            with self.subTest(family=item.family):
                self.assertIn(("cmsis_core", "vendors/arm/CMSIS_6"),
                              item.git_dependencies)



ROOT = Path(descriptor.__file__).resolve().parents[3]
FAMILIES = (
    ("native", "tests.support.native.descriptor"),
    ("stm32f4", "tools.configure.stm32f4_descriptor"),
)

class LocalDescriptorTests(unittest.TestCase):
    def test_selected_local_data_participates_in_existing_input_closure(self):
        for family, module in FAMILIES:
            with self.subTest(family=family):
                names = dependencies.source_names(
                    "platform", family, test_support=(family == "native"))
                self.assertIn(module.replace(".", "/") + ".py", names)
                for other, other_module in FAMILIES:
                    if other != family:
                        self.assertNotIn(other_module.replace(".", "/") + ".py", names)

    def run_isolated(self, code, *arguments):
        return subprocess.run(
            [sys.executable, '-I', '-B', '-c',
             'import sys;sys.path.insert(0,sys.argv.pop(1));' + code,
             str(ROOT), *arguments], cwd=ROOT, capture_output=True, text=True)

    def test_lookup_imports_only_exact_descriptor_not_family_behavior(self):
        code = (
            'from tools.configure.providers import descriptor;'
            'value=descriptor.maintained("stm32f4");'
            'assert "tools.configure.stm32f4_descriptor" in sys.modules;'
            'assert not any(key == "tools.configure.stm32f4" or '
            'key.startswith("tools.configure.stm32f4.") for key in sys.modules);'
            'assert "tests.support.native.descriptor" not in sys.modules')
        result = self.run_isolated(code)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_direct_local_import_has_no_provider_initialization_cycle(self):
        code = (
            'from importlib import import_module;'
            'local=import_module(sys.argv[1]);'
            'from tools.configure.providers import descriptor;'
            'value=descriptor.maintained(sys.argv[2], test_support=(sys.argv[2]=="native"));'
            'assert local.DESCRIPTOR is value;'
            'assert type(value) is descriptor.FamilyDescriptor;'
            'assert value.family==sys.argv[2];'
            'assert not any(key == "tools.configure.stm32f4" or '
            'key.startswith("tools.configure.stm32f4.") for key in sys.modules)')
        for family, module in FAMILIES:
            with self.subTest(family=family):
                result = self.run_isolated(code, module, family)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_returned_type_and_family_cannot_change_the_explicit_selection(self):
        expected = descriptor.maintained('stm32f4')
        for invalid in (object(), dataclasses.replace(expected, family='foreign')):
            with self.subTest(value=type(invalid).__name__), patch.object(
                    descriptor, 'import_module', create=True,
                    return_value=SimpleNamespace(
                        __name__="tools.configure.stm32f4_descriptor",
                        DESCRIPTOR=invalid)):
                with self.assertRaisesRegex(ValueError, 'descriptor.*contract'):
                    descriptor.maintained('stm32f4')

    def test_unknown_family_is_rejected_before_any_module_import(self):
        with patch.object(descriptor, 'import_module', create=True) as loader:
            with self.assertRaisesRegex(ValueError, 'Unmaintained'):
                descriptor.maintained('../unreviewed')
            loader.assert_not_called()

if __name__ == "__main__":
    unittest.main()
