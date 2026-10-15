"""Cold GPIO ownership covers selected pads with one retained snapshot each."""
import json
from dataclasses import replace
from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import bindings, configure as gate, gpio_phase
from tools.configure.providers import maintained


class ColdPinStorageTests(unittest.TestCase):
    def resolve(self, relative):
        return gate.resolve_ir(gate.ROOT / relative)

    def test_five_controllers_reuse_their_existing_exact_pin_storage(self):
        for label in ("stm32-ve", "stm32-zg", "gd32"):
            with self.subTest(label=label):
                result = self.resolve(
                    "tests/contracts/multi_controller_assembly/" + label
                    + ".toml")
                source = bindings.emit(result)[1]
                retained = re.findall(
                    r"static uint16_t s_nx_\w*pin_(?:state|saved|snapshots)"
                    r"\w*\[(\d+)u?\];", source)
                self.assertEqual(sum(map(int, retained)),
                                 len(gpio_phase.selected_pins(result)))
                self.assertNotIn("s_nx_platform_pin_state", source)
                self.assertNotIn("uint16_t*", source)

    def test_gpio_and_legacy_uart_keep_only_selected_pin_snapshots(self):
        for relative in (
                "tools/configure/assemblies/qiming-baremetal.toml",
                "tools/configure/assemblies/liangshan-baremetal.toml",
                "tests/contracts/g474_assembly/io.toml"):
            with self.subTest(relative=relative):
                result = self.resolve(relative)
                source = bindings.emit(result)[1]
                retained = re.findall(
                    r"static uint16_t s_nx_\w*pin_(?:state|saved|snapshots)"
                    r"\w*\[(\d+)u?\];", source)
                self.assertEqual(sum(map(int, retained)),
                                 len(gpio_phase.selected_pins(result)))

    def test_can_usb_ethernet_and_timer_reuse_their_persistent_leases(self):
        for relative in (
                "can_assembly/stm32-both.toml", "can_assembly/gd32-both.toml",
                "usb_assembly/stm32-ve.toml", "usb_assembly/stm32-zg.toml",
                "usb_assembly/gd32.toml",
                "ethernet_assembly/stm32-zg.toml", "ethernet_assembly/gd32.toml",
                "timer_assembly/stm32-ve.toml", "timer_assembly/stm32-zg.toml",
                "timer_assembly/gd32.toml"):
            with self.subTest(relative=relative):
                result = self.resolve("tests/contracts/" + relative)
                source = bindings.emit(result)[1]
                retained = re.findall(
                    r"static uint16_t s_nx_\w*pin_(?:state|saved|snapshots)"
                    r"\w*\[(\d+)u?\];", source)
                self.assertEqual(sum(map(int, retained)),
                                 len(gpio_phase.selected_pins(result)))
                self.assertNotIn("s_nx_platform_pin_state", source)

    def test_endpoint_cs_is_in_the_cold_lease_without_a_second_claim_source(self):
        for label, hidden in (("stm32-ve", "PE3"), ("gd32", "PB0")):
            with self.subTest(label=label):
                result = self.resolve(
                    "tests/contracts/multi_controller_assembly/" + label
                    + ".toml")
                controller_pins = {pin["pin"] for item in result.controllers
                                   for pin in item["pins"]}
                self.assertNotIn(hidden, controller_pins)
                self.assertIn(hidden, gpio_phase.selected_pins(result))

    def test_empty_graph_emits_no_pin_state_or_mmio_helpers(self):
        for relative in (
                "tools/configure/assemblies/qiming-baremetal-empty.toml",
                "tools/configure/assemblies/liangshan-baremetal-empty.toml",
                "tests/contracts/g474_assembly/empty.toml"):
            with self.subTest(relative=relative):
                result = self.resolve(relative)
                self.assertEqual(gpio_phase.selected_pins(result), ())
                self.assertEqual(gpio_phase.declarations(result), [])
                self.assertNotIn("nx_platform_pins_", bindings.emit(result)[1])

    def test_constructor_cannot_lend_storage_for_an_unselected_physical_pin(self):
        result = self.resolve(
            "tests/contracts/multi_controller_assembly/stm32-ve.toml")
        provider = maintained(result.soc_family)
        constructor = provider.constructor

        def invalid_slots(*args):
            construction = constructor(*args)
            return replace(construction,
                           pin_snapshots={"PH0": "unowned_storage[0]"})

        with patch.object(provider, "constructor", side_effect=invalid_slots):
            with self.assertRaisesRegex(ValueError, "unselected.*pin|pin.*unselected"):
                bindings.emit(result)

    def test_reserved_owner_names_do_not_transfer_unselected_physical_pins(self):
        for identity, custom_reservation in (("board-reserved", False),
                                              ("reserved-led", True)):
            with self.subTest(identity=identity), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                data = gate.load_assembly(
                    gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml")
                board_path = Path(data["board_package"]) / "board.json"
                board = json.loads(board_path.read_text())
                if custom_reservation:
                    board["reserved_resources"] = [
                        {"key": "pin:PA13", "owner": identity},
                        {"key": "pin:PA14", "owner": identity}]
                (root / "board.json").write_text(json.dumps(board))
                data["board_package"] = str(root)
                data["controllers"][0]["id"] = identity
                assembly = root / "assembly.json"
                assembly.write_text(json.dumps(data))
                result = gate.resolve_ir(assembly)
                self.assertNotIn("PA13", gpio_phase.selected_pins(result))
                self.assertNotIn("PA14", gpio_phase.selected_pins(result))
                self.assertEqual(len(gpio_phase.selected_pins(result)), 3)


if __name__ == "__main__":
    unittest.main()
