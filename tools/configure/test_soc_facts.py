"""Exact package admission and imported SDK inputs fail before publication."""
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure as gate


class PackagePinContractTests(unittest.TestCase):
    def facts(self, family="stm32g474"):
        return gate.load(gate.ROOT / "soc" / family / "soc.json")

    def test_physical_part_cannot_omit_its_complete_package_pin_set(self):
        for family, part in (("stm32f407", "STM32F407VET6"),
                             ("stm32f407", "STM32F407ZGT6"),
                             ("gd32f470", "GD32F470ZGT6"),
                             ("stm32g474", "STM32G474RET6")):
            with self.subTest(part=part):
                soc = self.facts(family)
                soc["variants"][part].pop("pins", None)
                with self.assertRaisesRegex(ValueError, "package pins"):
                    gate.validate_soc(soc, part)

    def test_unselected_physical_variant_cannot_omit_package_pins(self):
        soc = self.facts("stm32f407")
        soc["variants"]["STM32F407ZGT6"]["pins"] = ["PA0"]
        soc["variants"]["STM32F407VET6"].pop("pins", None)
        with self.assertRaisesRegex(ValueError, "package pins"):
            gate.validate_soc(soc, "STM32F407ZGT6")

    def test_pin_set_rejects_empty_duplicate_and_non_gpio_names(self):
        for pins in ([], "PA0", ["PA0", "PA0"], ["PA16"],
                     ["PA0", True], ["BOOT0"], ["pa0"]):
            with self.subTest(pins=pins):
                soc = self.facts()
                soc["variants"]["STM32G474RET6"]["pins"] = pins
                with self.assertRaisesRegex(ValueError, "package pins"):
                    gate.validate_soc(soc, "STM32G474RET6")

    def test_host_model_does_not_invent_a_physical_package(self):
        soc = self.facts("native")
        self.assertEqual(gate.validate_soc(soc, "NATIVE_MODEL")["package"],
                         soc["variants"]["NATIVE_MODEL"]["package"])

    def test_g474_package_inventory_matches_reviewed_controller_modes(self):
        soc = self.facts("stm32g474")
        pins = soc["variants"]["STM32G474RET6"]["pins"]
        # DS12288 Rev6 Table2 lists52 GPIOs for Rx/LQFP64. Table12 and
        # independent Figure7 agree on A/B/C16 each plus D2/F0/F1/G10.
        self.assertEqual(len(pins), 52)
        self.assertEqual({f"P{port}{pin}" for port in "ABC"
                          for pin in range(16)} | {"PD2", "PF0", "PF1", "PG10"},
                         set(pins))
        expected = {"GPIO" + bank: ("gpio", ["input", "output"], {})
                    for bank in "ABCDFG"}
        expected.update({name: ("uart", ["irq-bytes"], {})
                         for name in ("LPUART1", "USART1", "USART2", "USART3",
                                      "UART4", "UART5")})
        expected.update({name: ("spi", ["short-poll"], {})
                         for name in ("SPI1", "SPI2")})
        expected.update({"TIM3": ("pwm", ["fixed-pwm"],
                                  {"encoder": ["quadrature"]}),
                         "TIM4": ("pwm", ["fixed-pwm"], {}),
                         "TIM1": ("pwm_group", ["coherent-pwm"],
                                  {"pwm_adc": ["finite-regular-dma"]}),
                         "TIM5": ("timer_capture", ["edge-capture"], {}),
                         "ADC1": ("adc", ["single-shot", "trigger-dma"], {}),
                         "I2C1": ("i2c", ["poll-messages"], {}),
                         "FLASH": ("flash", ["poll-word"], {}),
                         "IWDG": ("watchdog", ["independent"], {}),
                         "EXTI0": ("exti", ["edge-event"], {}),
                         "EXTI1": ("exti", ["edge-event"], {}),
                         "FDCAN1": ("can_fd",
                                    ["iso-fd-loopback", "iso-fd-normal"], {})})
        self.assertEqual({name: (item["kind"], item["modes"],
                                item.get("interfaces", {}))
                          for name, item in soc["controllers"].items()}, expected)
        for line in (0, 1):
            self.assertEqual(soc["controllers"][f"EXTI{line}"]["modes"],
                             ["edge-event"])
            self.assertEqual(soc["controllers"][f"EXTI{line}"]["pins"],
                             [f"PA{line}"])


class ImportedDependencyContractTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for relative in ("dependencies/platform-compatibility.json",
                         "soc/gd32f470/soc.json", "soc/gd32f470/routes.json",
                         "soc/gd32f470/references.json",
                         "boards/gd32f470_liangshan/board.json"):
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(gate.ROOT / relative, destination)
        self.vendor = self.root / "vendors/gigadevice/gd32f4xx"
        shutil.copytree(gate.ROOT / "vendors/gigadevice/gd32f4xx", self.vendor)
        self.lock_path = self.vendor / "source.lock.json"
        self.soc_path = self.root / "soc/gd32f470/soc.json"
        soc = json.loads(self.soc_path.read_text())
        soc["source_lock"] = "vendors/gigadevice/gd32f4xx/source.lock.json"
        # Fixture facts have the reviewed complete LQFP144 GPIO set, independent
        # of the checked-in package metadata under test.
        soc["variants"]["GD32F470ZGT6"]["pins"] = [
            f"P{port}{pin}" for port in "ABCDEFG" for pin in range(16)] + [
                "PH0", "PH1"]
        self.soc_path.write_text(json.dumps(soc))
        self.assembly = self.root / "assembly.toml"
        self.assembly.write_text('schema=2\nboard="gd32f470_liangshan"\n'
                                 'backend="baremetal"\nclock="hxtal25-pll200"\n'
                                 '[memory]\nmain_stack_bytes=2048\n')

    def resolve(self):
        return gate.resolve_ir(self.assembly, self.root)

    def lock(self):
        return json.loads(self.lock_path.read_text())

    def test_import_has_a_typed_dependency_and_same_locked_input_digest(self):
        result = self.resolve()
        digest = hashlib.sha256(self.lock_path.read_bytes()).hexdigest()
        entry = next((item for item in result["source_dependencies"]
                      if item["path"] == "vendors/gigadevice/gd32f4xx"), None)
        self.assertIsNotNone(entry, "Imported SDK must retain source identity")
        self.assertEqual(entry["kind"], "vendor-source-import")
        self.assertEqual(entry["lock_sha256"], digest)
        self.assertEqual(entry["version"], "3.3.3")
        inputs = {item["path"]: item["sha256"] for item in result["inputs"]}
        self.assertEqual(inputs["vendors/gigadevice/gd32f4xx/source.lock.json"],
                         digest)

    def test_imported_file_change_is_rejected_before_bundle_publication(self):
        source = self.vendor / self.lock()["files"][0]["path"]
        source.write_bytes(source.read_bytes() + b"\n/* unrecorded change */\n")
        output = self.root / "generated"
        with self.assertRaisesRegex(ValueError, "Imported source changed"):
            gate.configure(self.assembly, output, self.root)
        self.assertFalse(output.exists())

    def test_extra_firmware_file_cannot_escape_import_identity(self):
        source = self.vendor / "Firmware/unreviewed.c"
        source.write_text("void extra(void);\n")
        with self.assertRaisesRegex(ValueError, "outside the reviewed import"):
            self.resolve()

    def test_import_lock_cannot_describe_an_unrelated_sdk_directory(self):
        copied = self.root / "vendors/unrelated"
        shutil.copytree(self.vendor, copied)
        soc = json.loads(self.soc_path.read_text())
        soc["source_lock"] = "vendors/unrelated/source.lock.json"
        self.soc_path.write_text(json.dumps(soc))
        with self.assertRaisesRegex(ValueError, "SoC source import lock"):
            self.resolve()

    def test_lock_metadata_change_changes_configuration_identity(self):
        before = self.resolve()
        lock = self.lock()
        lock["version"] = "3.3.3-reviewed-fixture"
        self.lock_path.write_text(json.dumps(lock))
        after = self.resolve()
        self.assertNotEqual(before["configuration_sha256"],
                            after["configuration_sha256"])
        imports = [item for item in after["source_dependencies"]
                   if item["kind"] == "vendor-source-import"]
        self.assertEqual(len(imports), 1, "Changed lock must retain its import")
        self.assertEqual(imports[0]["version"], "3.3.3-reviewed-fixture")

    def test_lock_change_during_emission_invalidates_the_owned_bundle(self):
        output = self.root / "generated"
        gate.configure(self.assembly, output, self.root)
        generate = gate.generate

        def changed(result, destination):
            generate(result, destination)
            self.lock_path.write_bytes(self.lock_path.read_bytes() + b"\n")

        with patch.object(gate, "generate", side_effect=changed):
            with self.assertRaisesRegex(ValueError, "Input changed during"):
                gate.configure(self.assembly, output, self.root)
        self.assertFalse(output.exists())
        self.assertEqual(list(self.root.glob(".nexus-config-*")), [])

    def test_unselected_route_cannot_claim_gpio_outside_exact_package(self):
        routes_path = self.root / "soc/gd32f470/routes.json"
        routes = json.loads(routes_path.read_text())
        route = next(item for item in routes["routes"]
                     if item["kind"] == "gpio")
        soc = json.loads(self.soc_path.read_text())
        soc["controllers"][route["controller"]]["pins"].append("PH2")
        route["pins"][0]["pin"] = "PH2"
        self.soc_path.write_text(json.dumps(soc))
        routes_path.write_text(json.dumps(routes))
        with self.assertRaisesRegex(ValueError, "does not exist on GD32F470"):
            self.resolve()


if __name__ == "__main__":
    unittest.main()
