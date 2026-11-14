"""One selected device view separates physical facts from maintained modes."""
import copy
from dataclasses import FrozenInstanceError
import unittest

from tools.configure import configure as gate, dependencies
from tools.configure.tests.f4_fixture import provider
from tools.configure.tests.device_fixture import (load_soc_facts,
                                                   validate_soc_facts)


class DeviceViewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.provider = provider("STM32F407VET6")

    def inputs(self):
        directory = gate.ROOT / "soc/stm32/stm32f4"
        return (gate.load(directory / "soc.json"),
                gate.load(directory / "devices.json"))

    def resolve(self, soc, catalog, part="STM32F407VET6"):
        return gate.resolve_device_facts(
            soc, part, self.provider, catalog=catalog)

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
                crypto = part.startswith(("STM32F415", "STM32F417"))
                self.assertEqual(len(view.controllers), 67 + 2 * media + 2 * crypto)
                self.assertEqual("ETH" in view.controllers, media)
                self.assertEqual("DCMI" in view.controllers, media)
                self.assertEqual("CRYP" in view.modes, crypto)
                self.assertEqual("HASH" in view.modes, crypto)
                if crypto:
                    self.assertEqual(view.modes["CRYP"].modes,
                                     ("irq-blocks", "dma-blocks"))
                    self.assertEqual(view.modes["HASH"].modes,
                                     ("irq-blocks", "dma-blocks"))
                self.assertIs(selected.soc["controllers"],
                              selected.controller_profiles[view.physical["device"]])

    def test_initial_exact_reference_entries_keep_their_own_review_and_layout(self):
        from tools.configure import selected_contracts
        entries = (("f411re", "STM32F411RET6", "LQFP64", 524288, 86, "ES0287 Rev6"),
                   ("f446ze", "STM32F446ZET6", "LQFP144", 524288, 97, "ES0298 Rev8"),
                   ("f479ni", "STM32F479NIH6", "TFBGA216", 2097152, 93, "ES0321 Rev14"))
        for stem, part, package, flash, irqs, document in entries:
            with self.subTest(part=part):
                result = gate.resolve_ir(gate.ROOT /
                    f"tests/contracts/stm32f4_family/{stem}.toml")
                self.assertEqual((result.part, result.package), (part, package))
                self.assertEqual(result.flash["size"], flash)
                self.assertEqual(result.cpu_profile.irq.external_count, irqs)
                self.assertEqual(result.clock_plan.reviewed_revision_ids, (0x1000,))
                policy = selected_contracts.silicon_policy(result)
                self.assertIn(document, policy.review_scope)
                self.assertNotIn("DEV_ID 0x413", policy.review_scope)
                self.assertEqual(result.clock["core_hz"],
                                 96000000 if stem == "f411re" else 168000000)
                self.assertEqual([(item.kind, item.controller)
                                  for item in result.controllers], [("crc", "CRC")])
                self.assertIsNone(result.physical_pcb_revision)
                self.assertFalse(result.physical_qualified)

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

    def test_f479_sdram_route_has_exact_bonded_pins_and_one_fmc_identity(self):
        soc, catalog = self.inputs()
        selected = self.resolve(soc, catalog, "STM32F479NIH6")
        view = selected.device
        self.assertEqual(view.controllers["FMC"].irqs, ("FMC_IRQn",))
        self.assertEqual(dict(view.controllers["FMC"].irq_numbers), {"FMC_IRQn": 48})
        self.assertEqual(view.modes["FMC"].modes, ("sdram-controller",))
        self.assertEqual(view.modes["FMC"].interfaces["memory_window"],
                         ("cpu-window", "graphics-owned"))
        routes = gate.resolve_routes(gate.load(gate.ROOT /
            "soc/stm32/stm32f4/routes.json"), selected.soc, view.part,
            self.provider, controller_profiles=selected.controller_profiles)
        route = routes["fmc-sdram-bank1-16"]
        self.assertEqual(route["variants"], ["STM32F479NIH6"])
        self.assertEqual(len(route["pins"]), 38)
        self.assertTrue(all(pin["af"] == 12 for pin in route["pins"]))
        wires = {pin["pin"]: pin["function"] for pin in route["pins"]}
        self.assertEqual(wires["PC0"], "FMC_SDNWE")
        self.assertNotIn("PA7", wires)
        self.assertTrue(set(wires).issubset(view.physical["pins"]))
        for part in ("STM32F407ZGT6", "STM32F411RET6", "STM32F446ZET6"):
            self.assertNotIn("FMC", self.resolve(soc, catalog, part).device.controllers)

    def test_f479_sdram_route_cannot_be_assigned_to_the_f446_profile(self):
        soc, catalog = self.inputs()
        selected = self.resolve(soc, catalog, "STM32F479NIH6")
        routes = gate.load(gate.ROOT / "soc/stm32/stm32f4/routes.json")
        route = next(item for item in routes["routes"]
                     if item["id"] == "fmc-sdram-bank1-16")
        route["variants"].append("STM32F446ZET6")
        with self.assertRaisesRegex(ValueError, "maintained controller"):
            gate.resolve_routes(routes, selected.soc, selected.device.part,
                self.provider, controller_profiles=selected.controller_profiles)

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
                    validate_soc_facts(soc, "STM32F407VET6", self.provider.for_soc(soc))


if __name__ == "__main__":
    unittest.main()
