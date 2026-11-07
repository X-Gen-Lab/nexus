"""Exact package admission and imported SDK inputs fail before publication."""
from tools.configure.tests.device_fixture import load_soc_facts
from tools.configure.providers.descriptor import source_directory
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure as gate


class PackagePinContractTests(unittest.TestCase):
    def facts(self, family="stm32f4"):
        return load_soc_facts(gate.ROOT / ("tests/support/native" if family == "native"
                                      else source_directory(family)) / "soc.json")

    def test_physical_part_cannot_omit_its_complete_package_pin_set(self):
        for family, part in (('stm32f4', 'STM32F407VET6'), ('stm32f4', 'STM32F407ZGT6')):
            with self.subTest(part=part):
                soc = self.facts(family)
                soc["variants"][part].pop("pins", None)
                with self.assertRaisesRegex(ValueError, "package pins"):
                    gate.validate_soc(soc, part)

    def test_unselected_physical_variant_cannot_omit_package_pins(self):
        soc = self.facts('stm32f4')
        soc["variants"]["STM32F407ZGT6"]["pins"] = ["PA0"]
        soc["variants"]["STM32F407VET6"].pop("pins", None)
        with self.assertRaisesRegex(ValueError, "package pins"):
            gate.validate_soc(soc, "STM32F407ZGT6")

    def test_pin_set_rejects_empty_duplicate_and_non_gpio_names(self):
        for pins in ([], "PA0", ["PA0", "PA0"], ["PA16"],
                     ["PA0", True], ["BOOT0"], ["pa0"]):
            with self.subTest(pins=pins):
                soc = self.facts()
                soc["variants"]["STM32F407VET6"]["pins"] = pins
                with self.assertRaisesRegex(ValueError, "package pins"):
                    gate.validate_soc(soc, "STM32F407VET6")

    def test_host_model_does_not_invent_a_physical_package(self):
        soc = self.facts("native")
        self.assertEqual(gate.validate_soc(soc, "NATIVE_MODEL",
                                           gate.maintained("native", test_support=True))["package"],
                         soc["variants"]["NATIVE_MODEL"]["package"])





if __name__ == "__main__":
    unittest.main()
