"""Finite I2S is a physical SPI2 personality, not a software dependency list."""

import importlib.util
import os
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(os.environ.get("NEXUS_PRIMARY_ROOT", ROOT))))
from tools.configure import resource_graph
from tools.configure.configure import fail, integer, normalize_resource, obj, text

SOURCE = Path(os.environ.get("NEXUS_I2S2_SOURCE", Path(__file__).with_name("i2s2.py")))
SPEC = importlib.util.spec_from_file_location("nexus_i2s2_under_test", SOURCE)
role = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = role
SPEC.loader.exec_module(role)
AUTHORED = Path(os.environ.get("NEXUS_I2S2_AUTHORED",
                              Path(__file__).with_name("authored.py")))
AUTHORED_SPEC = importlib.util.spec_from_file_location("nexus_i2s2_authored", AUTHORED)
authored = importlib.util.module_from_spec(AUTHORED_SPEC)
AUTHORED_SPEC.loader.exec_module(authored)


class F407I2s2RoleTests(unittest.TestCase):
    def normalize(self, selection):
        assembly = {"schema": 2, "board_package": "external-board",
                    "backend": "baremetal", "clock": "hse8-pll168",
                    "memory": {"main_stack_bytes": 2048}, "i2s2": selection}
        return authored.normalize(assembly, Path("reference.toml"), ROOT,
                                  fail, obj, integer, text)

    def test_authored_role_does_not_select_a_software_library_or_endpoint(self):
        selected = {"profile": "slave-tx-stereo16-pb12-pb13-pb15"}
        normalized = self.normalize(selected)
        self.assertEqual(normalized.i2s2, selected)
        self.assertEqual(normalized.controllers, ())
        self.assertEqual(normalized.devices, ())
        self.assertEqual(normalized.components, ())

    def test_authored_role_rejects_unreviewed_hardware_properties(self):
        with self.assertRaises(ValueError):
            self.normalize({"profile": "slave-tx-stereo16-pb12-pb13-pb15",
                            "pins": ["PA0"]})

    def resolve(self, choice=None, **changes):
        options = dict(family="stm32f407", part="STM32F407VET6",
                       backend="baremetal", clock_profile="hse8-pll168")
        options.update(changes)
        return role.resolve(choice, **options)

    def selected(self, **changes):
        return self.resolve({"profile": "slave-tx-stereo16-pb12-pb13-pb15"},
                            **changes)

    def test_default_has_no_claim_or_profile(self):
        self.assertIsNone(self.resolve())

    def test_exact_parts_freeze_same_hardware_claims(self):
        for part in ("STM32F407VET6", "STM32F407ZGT6"):
            with self.subTest(part=part):
                selected = self.selected(part=part)
                self.assertEqual(selected.profile,
                                 "slave-tx-stereo16-pb12-pb13-pb15")
                self.assertEqual(selected.part, part)
                self.assertEqual(selected.claims, (
                    "controller:SPI2", "pin:PB12", "pin:PB13", "pin:PB15",
                    "dma:DMA1:stream4:channel0", "irq:DMA1_Stream4", "irq:SPI2"))
                with self.assertRaises((AttributeError, TypeError)):
                    selected.profile = "changed"

    def test_unknown_fields_and_mutable_hardware_selectors_reject(self):
        for extra in ("pin_route", "dma", "irq", "components", "binding"):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                self.resolve({"profile": "slave-tx-stereo16-pb12-pb13-pb15",
                              extra: "arbitrary"})
        for bad in ({}, [], True, "slave", {"profile": "continuous"}):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                self.resolve(bad)

    def test_other_part_family_backend_or_clock_rejects(self):
        for change in ({"part": "STM32F405VGT6"}, {"family": "stm32h743"},
                       {"backend": "freertos"}, {"clock_profile": "hsi16"}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.selected(**change)

    def test_claims_exclude_spi2_and_pin_and_vector_owners(self):
        selected = self.selected()
        own = [{"key": key, "owner": "i2s2", "mode": selected.profile,
                "source": "RM0090/DS8626 chip reference"}
               for key in selected.claims]
        for key in ("controller:SPI2", "pin:PB12", "pin:PB13", "pin:PB15",
                    "irq:SPI2", "irq:DMA1_Stream4"):
            with self.subTest(key=key), self.assertRaises(ValueError):
                resource_graph.resolve(own + [{"key": key, "owner": "other",
                                              "mode": "spi", "source": "model"}])

    def test_dma_claim_normalizes_to_physical_engine_before_graph(self):
        selected = self.selected()
        own = [{"key": normalize_resource(key), "owner": "i2s2",
                "mode": selected.profile, "source": "chip reference"}
               for key in selected.claims]
        other = {"key": normalize_resource("dma:DMA1:stream4:channel7"),
                 "owner": "other", "mode": "other", "source": "model"}
        with self.assertRaises(ValueError):
            resource_graph.resolve(own + [other])
        resource_graph.resolve(own + [{**other, "key": "dma:DMA2:stream4"}])


if __name__ == "__main__":
    unittest.main()
