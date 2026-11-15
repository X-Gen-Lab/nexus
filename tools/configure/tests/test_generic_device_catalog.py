"""Catalog authors select device identifiers, paths, IRQs and SDK lock aliases."""
import copy
import unittest

from tools.configure import configure
from tools.configure.providers.common import (
    validate_vendor_device, vendor_device_catalog)


def catalog():
    return {"schema_version": 1, "scope": "sdk_device_compile", "sdk": {
        "directory": "vendors/sample/chip-sdk", "dependency": "chip_sdk",
        "commit": "1" * 40, "core_directory": "vendors/sample/cpu-core",
        "core_dependency": "cpu_sdk", "core_commit": "2" * 40},
        "devices": [{"id": "chip-alpha", "define": "ACME_CHIP_A",
            "header": "include/part_alpha.h", "startup": "boot/reset_entry.S",
            "external_irq_count": 2, "last_external_irq": {"name": "IRQ_LAST", "number": 1},
            "nvic_priority_bits": 3, "timebase_presence": {"COUNTER_A": True},
            "irq_bindings": {"IRQ_FIRST": {"number": 0, "handler": "enter_serial"},
                             "IRQ_LAST": {"number": 1, "handler": "enter_counter"}}}],
        "inputs": {"include/part_alpha.h": {"bytes": 123, "sha256": "a" * 64},
                   "boot/reset_entry.S": {"bytes": 456, "sha256": "b" * 64}}}


def soc():
    return {"variants": {"SAMPLE_A100": {"device": "chip-alpha"}},
        "source_lock": {"chip_sdk": "1" * 40, "cpu_sdk": "2" * 40},
        "device_profiles": {"chip-alpha": {"controller_groups": ["base"],
            "clock_policy": "sample-fixed", "part_prefix": "SAMPLE_A"}},
        "controller_groups": {"base": {"controllers": {"SERIAL_A": {
            "kind": "uart", "irq": ["IRQ_FIRST"], "pins": [], "irq_numbers": [0]}},
            "modes": {"SERIAL_A": {"modes": ["irq-byte-event"]}}}}}


class GenericDeviceCatalogTests(unittest.TestCase):
    def test_non_st_identifiers_and_explicit_paths_do_not_inherit_f4_naming(self):
        selected = vendor_device_catalog(catalog())["chip-alpha"]
        self.assertEqual(selected.macro, "ACME_CHIP_A")
        self.assertEqual(selected.startup, "reset_entry.S")
        self.assertEqual(selected.irq_bindings["IRQ_FIRST"].handler, "enter_serial")
        value = configure.catalog_soc_facts(soc(), "SAMPLE_A100", catalog())
        self.assertEqual(value.device.identifier, "chip-alpha")
        self.assertEqual(value.soc["irq"], {"priority_bits": 3, "external_count": 2})

    def test_empty_timebase_map_and_safe_startup_basename_are_valid(self):
        value = catalog()
        value["devices"][0]["timebase_presence"] = {}
        vendor_device_catalog(value)
        validate_vendor_device({"macro": "CHIP_CORE0", "startup": "vendor-reset.c"})

    def test_exact_named_sdk_locks_cannot_be_laundered_through_another_alias(self):
        for mutate in (
            lambda value: value["source_lock"].update(chip_sdk="3" * 40, elsewhere="1" * 40),
            lambda value: value["source_lock"].update(cpu_sdk="3" * 40, elsewhere="2" * 40),
            lambda value: value.pop("source_lock"),
        ):
            value = soc()
            mutate(value)
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "source lock"):
                configure.catalog_soc_facts(value, "SAMPLE_A100", catalog())

    def test_paths_identifiers_and_boolean_presence_reject_injection(self):
        for mutate in (
            lambda value: value["sdk"].update(directory="../outside"),
            lambda value: value["sdk"].update(core_directory="C:/outside"),
            lambda value: value["sdk"].update(directory="vendors/sdk;-DOTHER"),
            lambda value: value["devices"][0].update(header="include/../wrong.h"),
            lambda value: value["devices"][0].update(startup="/tmp/entry.S"),
            lambda value: value["devices"][0].update(define="CHIP -DOTHER"),
            lambda value: value["devices"][0]["irq_bindings"]["IRQ_FIRST"].update(handler="entry()"),
            lambda value: value["devices"][0].update(timebase_presence={"TICK": 1}),
        ):
            value = copy.deepcopy(catalog())
            mutate(value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                vendor_device_catalog(value)
        for startup in ("../entry.S", "subdir/entry.S", "entry.S;-DOTHER", "-entry.S"):
            with self.subTest(startup=startup), self.assertRaises(ValueError):
                validate_vendor_device({"macro": "CHIP", "startup": startup})

    def test_canonical_irq_alias_collisions_cannot_choose_a_different_publisher(self):
        value = catalog()
        value["devices"][0]["irq_bindings"] = {
            "COUNTER": {"number": 0, "handler": "first_entry"},
            "COUNTER_IRQn": {"number": 1, "handler": "second_entry"}}
        with self.assertRaisesRegex(ValueError, "collision"):
            vendor_device_catalog(value)

    def test_metadata_ids_and_dependency_aliases_are_not_c_symbol_names(self):
        value, facts = catalog(), soc()
        value["sdk"].update(dependency="chip-sdk", core_dependency="cpu-core-sdk")
        value["devices"][0]["timebase_presence"] = {"slow-tick": True}
        facts["source_lock"] = {"chip-sdk": "1" * 40, "cpu-core-sdk": "2" * 40}
        configure.catalog_soc_facts(facts, "SAMPLE_A100", value)


if __name__ == "__main__":
    unittest.main()
