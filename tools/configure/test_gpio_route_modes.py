"""Reviewed GPIO pads select modes without changing package or Board facts."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure as gate


class GpioRouteModeTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def assembly(self, family, route, controller, pin, mode, *, part=None,
                 reserved=()):
        board_name, clock = {
            "stm32f407": ("stm32f407_qiming_v31", "hse8-pll168"),
            "gd32f470": ("gd32f470_liangshan", "hxtal25-pll200"),
        }[family]
        board = gate.load(gate.ROOT / "boards" / board_name / "board.json")
        board["id"] = "gpio-route-software-fixture"
        board["physical_pcb_revision"] = None
        board["source_revision"] = "reviewed-chip-pad-selection-only"
        board["unknowns"] = ["Software fixture; physical wiring unverified"]
        board["provenance"] = []
        if part is not None:
            board["soc"] = part
        mask = 1 << int(pin[2:])
        board["bindings"] = [{"id": "selected", "controller": controller,
                              "route": route, "mask": mask,
                              "initial": mask if mode == "output" else 0}]
        board["reserved_resources"].extend(reserved)
        (self.directory / "board.json").write_text(
            json.dumps(board), encoding="utf-8")
        assembly = self.directory / "assembly.toml"
        assembly.write_text(
            'schema = 2\nboard_package = "."\nbackend = "baremetal"\n'
            f'clock = "{clock}"\noptimization = "Os"\n'
            '[memory]\nmain_stack_bytes = 2048\nlibc_heap_bytes = 0\n'
            'static_ram_limit_bytes = 16384\nflash_load_limit_bytes = 65536\n'
            f'[gpio.selected]\nbinding = "selected"\nmode = "{mode}"\n',
            encoding="utf-8")
        return assembly

    def check_mode(self, family, route, controller, pin, mode, *, part=None):
        assembly = self.assembly(family, route, controller, pin, mode,
                                 part=part)
        try:
            result = gate.resolve_ir(assembly)
        except gate.ConfigurationError as error:
            self.fail(f"Reviewed {controller}/{mode} pad is not selectable: "
                      f"{error}")
        selected, = result.controllers
        self.assertEqual((selected.kind, selected.controller, selected.mode),
                         ("gpio", controller, mode))
        self.assertEqual(selected.options.mask, 1 << int(pin[2:]))
        self.assertEqual(selected.pins[0]["pin"], pin)
        self.assertEqual(selected.pins[0]["function"], "output")
        projected, = result.to_dict()["controllers"]
        self.assertEqual(projected["mode"], mode)
        self.assertEqual(projected["pins"][0]["pin"], pin)
        self.assertFalse(result.physical_qualified)
        source = bindings.emit(result)[1]
        if family == "stm32f407":
            self.assertIn(f".registers = {controller}, .mask = "
                          f"{1 << int(pin[2:])}u,", source)
            self.assertIn(".output = " +
                          ("true" if mode == "output" else "false"), source)
        else:
            index = ord(pin[1]) - ord("A")
            mask = 1 << int(pin[2:])
            initial = mask if mode == "output" else 0
            self.assertIn("nx_gd32_gpio_initialize(&s_nx_port_selected, "
                          f"{index}u, {mask}u, {initial}u, " +
                          ("true" if mode == "output" else "false") + ");",
                          source)

    def test_f407_gpioa_input(self):
        for part in ("STM32F407VET6", "STM32F407ZGT6"):
            with self.subTest(part=part):
                self.check_mode("stm32f407", "gpio-pa0", "GPIOA", "PA0",
                                "input", part=part)

    def test_f407_gpioa_output(self):
        for part in ("STM32F407VET6", "STM32F407ZGT6"):
            with self.subTest(part=part):
                self.check_mode("stm32f407", "gpio-pa0", "GPIOA", "PA0",
                                "output", part=part)

    def test_f407_gpiob_input(self):
        self.check_mode("stm32f407", "gpio-pb2", "GPIOB", "PB2", "input")

    def test_f407_gpioe_input(self):
        for number in (3, 4):
            with self.subTest(pin=number):
                self.check_mode("stm32f407", f"gpio-pe{number}", "GPIOE",
                                f"PE{number}", "input")

    def test_f407_gpiog_input_on_exact_zg_package(self):
        self.check_mode("stm32f407", "gpio-pg9", "GPIOG", "PG9", "input")

    def test_gd470_gpiob_input(self):
        for number in (0, 1):
            with self.subTest(pin=number):
                self.check_mode("gd32f470", f"gpiob{number}", "GPIOB",
                                f"PB{number}", "input")

    def test_existing_gd470_input_uses_mode_not_route_pin_function(self):
        self.check_mode("gd32f470", "gpiod7", "GPIOD", "PD7", "input")

    def test_existing_outputs_remain_selectable(self):
        for family, route, controller, pin in (
                ("stm32f407", "gpio-pb2", "GPIOB", "PB2"),
                ("stm32f407", "gpio-pe3", "GPIOE", "PE3"),
                ("stm32f407", "gpio-pe4", "GPIOE", "PE4"),
                ("stm32f407", "gpio-pg9", "GPIOG", "PG9"),
                ("gd32f470", "gpiob0", "GPIOB", "PB0"),
                ("gd32f470", "gpiob1", "GPIOB", "PB1")):
            with self.subTest(route=route):
                self.check_mode(family, route, controller, pin, "output")

    def test_gpiog_does_not_escape_the_zg_package(self):
        assembly = self.assembly("stm32f407", "gpio-pg9", "GPIOG", "PG9",
                                 "input", part="STM32F407VET6")
        with self.assertRaisesRegex(gate.ConfigurationError, "route"):
            gate.resolve_ir(assembly)

    def test_gpioa_does_not_override_a_board_pin_reservation(self):
        assembly = self.assembly("stm32f407", "gpio-pa0", "GPIOA", "PA0",
                                 "input", reserved=("PA0",))
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "Resource conflict pin:PA0"):
            gate.resolve_ir(assembly)


if __name__ == "__main__":
    unittest.main()
