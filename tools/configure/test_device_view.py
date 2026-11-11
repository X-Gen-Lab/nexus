"""One selected device view separates physical facts from maintained modes."""
import copy
from dataclasses import FrozenInstanceError
import unittest

from tools.configure import configure as gate, dependencies
from tools.configure.providers import maintained
from tools.configure.tests.device_fixture import load_soc_facts


class DeviceViewTests(unittest.TestCase):
    def inputs(self):
        directory = gate.ROOT / "soc/stm32/stm32f4"
        return (gate.load(directory / "soc.json"),
                gate.load(directory / "devices.json"))

    def resolve(self, soc, catalog, part="STM32F407VET6"):
        return gate.resolve_device_facts(
            soc, part, maintained("stm32f4"), catalog=catalog)

    def test_exact_part_keeps_its_geometry_and_one_device_profile(self):
        soc, catalog = self.inputs()
        for part, package, flash, pins in (
                ("STM32F405ZGT6", "LQFP144", 1048576, 114),
                ("STM32F407VET6", "LQFP100", 524288, 82),
                ("STM32F407ZGT6", "LQFP144", 1048576, 114),
                ("STM32F415ZGT6", "LQFP144", 1048576, 114),
                ("STM32F417ZGT6", "LQFP144", 1048576, 114)):
            with self.subTest(part=part):
                selected = self.resolve(soc, catalog, part)
                view = selected.device
                self.assertEqual((view.family, view.part), ("stm32f4", part))
                self.assertEqual(view.physical["package"], package)
                self.assertEqual(view.physical["flash"]["size"], flash)
                self.assertEqual(len(view.physical["pins"]), pins)
                self.assertEqual(view.vendor_device.macro, part[:9] + "xx")
                self.assertEqual(view.irq["external_count"], 82)
                self.assertEqual(view.controllers["USART1"].irqs,
                                 ("USART1_IRQn",))
                self.assertFalse(hasattr(view.controllers["USART1"], "modes"))
                self.assertIn("irq-byte-event", view.modes["USART1"].modes)
                media = part.startswith(("STM32F407", "STM32F417"))
                self.assertEqual(len(view.controllers), 69 if media else 67)
                self.assertEqual("ETH" in view.controllers, media)
                self.assertEqual("DCMI" in view.controllers, media)
                self.assertNotIn("CRYP", view.modes)
                self.assertNotIn("HASH", view.modes)
                self.assertIs(selected.soc["controllers"],
                              selected.controller_profiles[view.physical["device"]])

    def test_view_cannot_be_mutated_by_consumer_or_authored_input(self):
        soc, catalog = self.inputs()
        before = copy.deepcopy(soc)
        view = self.resolve(soc, catalog).device
        self.assertEqual(soc, before)
        with self.assertRaises(FrozenInstanceError):
            view.controllers["USART1"].pins = ()
        with self.assertRaises(TypeError):
            view.physical["flash"]["size"] = 1
        with self.assertRaises(TypeError):
            view.modes["I2C1"].interfaces["i2c_target"] = ()
        soc["variants"][view.part]["flash"]["size"] = 1
        soc["controller_groups"]["rm0090-common"]["modes"]["USART1"]["modes"].clear()
        self.assertEqual(view.physical["flash"]["size"], 524288)
        self.assertIn("irq-byte-event", view.modes["USART1"].modes)

    def test_catalog_backed_soc_rejects_a_second_global_controller_authority(self):
        soc, catalog = self.inputs()
        soc["controllers"] = copy.deepcopy(
            soc["controller_groups"]["rm0090-common"]["controllers"])
        with self.assertRaisesRegex(ValueError, "global controller"):
            gate.catalog_soc_facts(soc, "STM32F407VET6", catalog)

    def test_profile_is_explicit_complete_and_bound_to_exact_device(self):
        for change in ("missing", "foreign", "no-controllers",
                       "missing-policy", "foreign-policy"):
            with self.subTest(change=change):
                soc, catalog = self.inputs()
                group = soc["controller_groups"]["rm0090-common"]
                if change == "missing":
                    soc["device_profiles"].clear()
                elif change == "foreign":
                    soc["device_profiles"]["stm32f499"] = copy.deepcopy(
                        soc["device_profiles"]["stm32f407"])
                elif change == "no-controllers":
                    group["controllers"].clear()
                    group["modes"].clear()
                elif change == "missing-policy":
                    group["modes"].pop("USART1")
                else:
                    group["modes"]["INVENTED"] = {"modes": ["input"]}
                with self.assertRaisesRegex(ValueError, "profile|group"):
                    gate.catalog_soc_facts(soc, "STM32F407VET6", catalog)

    def test_profile_cannot_launder_an_unimplemented_mode_or_exact_part(self):
        soc, catalog = self.inputs()
        group = soc["controller_groups"]["rm0090-common"]
        group["modes"]["USART1"]["modes"].append("invented-mode")
        with self.assertRaisesRegex(ValueError, "mode"):
            self.resolve(soc, catalog)
        soc, catalog = self.inputs()
        soc["variants"]["STM32F405VGT6"] = copy.deepcopy(
            soc["variants"]["STM32F407VET6"])
        with self.assertRaisesRegex(ValueError, "part"):
            self.resolve(soc, catalog, "STM32F405VGT6")

    def test_groups_reject_missing_duplicate_and_overlapping_controllers(self):
        for change in ("missing", "duplicate", "overlap"):
            with self.subTest(change=change):
                soc, catalog = self.inputs()
                names = soc["device_profiles"]["stm32f407"]["controller_groups"]
                if change == "missing":
                    names.append("no-such-group")
                elif change == "duplicate":
                    names.append(names[0])
                else:
                    soc["controller_groups"]["duplicate"] = copy.deepcopy(
                        soc["controller_groups"]["ethernet-f4"])
                    names.append("duplicate")
                with self.assertRaisesRegex(ValueError, "group"):
                    self.resolve(soc, catalog)

    def test_absent_media_cannot_be_laundered_into_a_device_profile(self):
        for chip in ("405", "415"):
            for group in ("ethernet-f4", "dcmi-v1"):
                with self.subTest(chip=chip, group=group):
                    soc, catalog = self.inputs()
                    soc["device_profiles"][f"stm32f{chip}"]["controller_groups"].append(group)
                    with self.assertRaisesRegex(ValueError, "IRQ.*selected device"):
                        self.resolve(soc, catalog, f"STM32F{chip}ZGT6")

    def test_same_package_device_selector_cannot_be_swapped(self):
        for first, second in (("405", "407"), ("415", "417")):
            with self.subTest(first=first):
                soc, catalog = self.inputs()
                a, b = f"STM32F{first}ZGT6", f"STM32F{second}ZGT6"
                soc["variants"][a]["device"], soc["variants"][b]["device"] = (
                    soc["variants"][b]["device"], soc["variants"][a]["device"])
                with self.assertRaisesRegex(ValueError, "part device selector"):
                    self.resolve(soc, catalog, a)

    def test_runtime_requires_irq_review_beyond_sdk_compile_inventory(self):
        soc, catalog = self.inputs()
        next(row for row in catalog["devices"]
             if row["id"] == "stm32f405").pop("irq_bindings")
        with self.assertRaisesRegex(ValueError, "Runtime.*IRQ bindings"):
            self.resolve(soc, catalog, "STM32F405ZGT6")

    def test_view_and_review_policy_are_bound_configuration_inputs(self):
        paths = dependencies.source_names("platform", "stm32f4")
        for path in ("tools/configure/device_view.py",
                     "tools/configure/reference_profiles.py"):
            with self.subTest(path=path):
                self.assertEqual(paths.count(path), 1)

    def test_controller_irq_identity_cannot_drift_from_the_reviewed_sdk_bank(self):
        for controller, field, value in (
                ("USART1", "irq", ["USART2_IRQn"]),
                ("USART1", "irq", []),
                ("I2C1", "irq_numbers", {"I2C1_EV": 32, "I2C1_ER": 31}),
                ("USART1", "irq_numbers", [37, 38]),
                ("I2C1", "irq_numbers", True),
                ("I2C1", "irq_numbers", {False: 31, "I2C1_ER": 32})):
            with self.subTest(controller=controller, field=field):
                soc = load_soc_facts(gate.ROOT / "soc/stm32/stm32f4/soc.json")
                soc["controllers"][controller][field] = value
                with self.assertRaisesRegex(ValueError, "IRQ"):
                    gate.validate_soc(soc, "STM32F407VET6")


if __name__ == "__main__":
    unittest.main()
