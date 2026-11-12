"""Reviewed classic F4 pads share GPIO and all sixteen EXTI line owners."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure as gate, device_view
from tools.configure.tests.native_fixture import native_fixture


class F407GpioMatrixTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="f407 gpio matrix ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def assembly(self, part, selected, *, extra="", reserved=()):
        board = gate.load(gate.ROOT / "boards/stm32f407zg_qiming_core/board.json")
        board.update(id="f407-pad-matrix-software-only", soc=part,
                     physical_pcb_revision=None, source_revision="silicon-only",
                     provenance=[], unknowns=["No physical wiring qualification"],
                     bindings=selected)
        board["reserved_resources"] += list(reserved)
        (self.directory / "board.json").write_text(json.dumps(board))
        text = ('schema=2\nboard_package="."\nbackend="baremetal"\n'
                'clock="hse8-pll168"\n[memory]\nmain_stack_bytes=2048\n'
                'libc_heap_bytes=0\nstatic_ram_limit_bytes=16384\n'
                'flash_load_limit_bytes=65536\n')
        for item in selected:
            kind = "exti" if item["controller"].startswith("EXTI") else "gpio"
            if item["controller"] == "RTC":
                continue
            text += f'[{kind}.{item["id"]}]\nbinding="{item["id"]}"\n'
            text += ('mode="edge-event"\nedge="both"\nevent_capacity=2\n'
                     'irq.priority=5\n' if kind == "exti" else 'mode="input"\n')
        path = self.directory / "assembly.toml"
        path.write_text(text + extra)
        return path

    def resolve(self, path):
        return gate.resolve_ir(path, test_support=native_fixture(path))

    def test_every_bonded_pad_has_gpio_and_matching_exti_routes(self):
        package = gate.ROOT / "soc/stm32/stm32f4"
        soc, routes = gate.load(package / "soc.json"), gate.load(package / "routes.json")
        parts = {"STM32F405ZGT6", "STM32F407VET6", "STM32F407ZGT6",
                 "STM32F415ZGT6", "STM32F417ZGT6"}
        profiles = device_view.controller_profiles(soc)
        for kind, modes in (("gpio", {"input", "output"}),
                            ("exti", {"edge-event"})):
            declared = {part for part, facts in soc["variants"].items()
                        if any(item["kind"] == kind and
                               modes <= set(item["modes"])
                               for item in profiles[facts["device"]].values())}
            self.assertEqual(parts, declared, kind)
        for part in sorted(parts):
            facts = soc["variants"][part]
            for pin in facts["pins"]:
                with self.subTest(part=part, pin=pin):
                    eligible = [route for route in routes["routes"]
                                if part in route["variants"] and
                                len(route["pins"]) == 1 and
                                route["pins"][0]["pin"] == pin]
                    self.assertTrue(any(route["kind"] == "gpio" and
                                        {"input", "output"} <= set(route["modes"])
                                        for route in eligible))
                    self.assertTrue(any(route["kind"] == "exti" and
                                        route["controller"] == "EXTI" + pin[2:]
                                        for route in eligible))

    def test_all_pad_exti_lines_resolve_to_seven_static_vector_entries(self):
        selected = [{"id": f"edge{line}", "controller": f"EXTI{line}",
                     "route": f"exti{line}-pc{line}"} for line in range(16)]
        for part in ("STM32F407VET6", "STM32F407ZGT6"):
            with self.subTest(part=part):
                result = self.resolve(self.assembly(part, selected))
                self.assertEqual(16, len(result.controllers))
                source = bindings.emit(result)[1]
                for vector in [*(f"EXTI{line}" for line in range(5)),
                               "EXTI9_5", "EXTI15_10"]:
                    self.assertEqual(1, source.count(f"void {vector}_IRQHandler(void) {{"))
                for line in range(16):
                    self.assertIn(f".line = {line}u, .priority = 5u", source)
                self.assertFalse(result.physical_qualified)

    def test_different_ports_still_conflict_on_one_exti_mux_line(self):
        selected = [{"id": "first", "controller": "EXTI1", "route": "exti1-pa1"},
                    {"id": "other", "controller": "EXTI1", "route": "exti1-pb1"}]
        with self.assertRaisesRegex(gate.ConfigurationError, "Resource conflict.*EXTI1"):
            self.resolve(self.assembly("STM32F407VET6", selected))

    def test_package_and_debug_reservations_survive_route_expansion(self):
        for pin, part, diagnostic in (("PF0", "STM32F407VET6", "route"),
                                      ("PA13", "STM32F407VET6", "Resource conflict pin:PA13"),
                                      ("PH0", "STM32F407ZGT6", "Resource conflict pin:PH0")):
            with self.subTest(pin=pin):
                selected = [{"id": "pad", "controller": "GPIO" + pin[1],
                             "route": "gpio-" + pin.lower(), "mask": 1 << int(pin[2:]),
                             "initial": 0}]
                with self.assertRaisesRegex(gate.ConfigurationError, diagnostic):
                    self.resolve(self.assembly(part, selected))

    def test_hse_reservation_can_be_restated_without_duplicate_ownership(self):
        path = self.assembly("STM32F407VET6", [], reserved=("PH0", "PH1"))
        self.assertEqual((), self.resolve(path).controllers)

    def test_hsi_plan_can_use_both_released_hse_pads(self):
        selected = [{"id": f"pad{pin}", "controller": "GPIOH",
                     "route": f"gpio-ph{pin}", "mask": 1 << pin,
                     "initial": 0} for pin in (0, 1)]
        path = self.assembly("STM32F407ZGT6", selected)
        parameters = (gate.ROOT / "tests/contracts/timer_clock/hsi16.toml").read_text()
        parameters = parameters.split("[clock_parameters]\n", 1)[1].split("[memory]", 1)[0]
        path.write_text(path.read_text().replace('clock="hse8-pll168"',
                                                 'clock="custom"') +
                        "\n[clock_parameters]\n" + parameters)
        result = self.resolve(path)
        self.assertEqual(2, len(result.controllers))
        self.assertEqual({"PH0", "PH1"},
                         {pin["pin"] for item in result.controllers
                          for pin in item.pins})

    def test_selected_lse_owner_conflicts_with_generic_gpio(self):
        selected = [{"id": "calendar", "controller": "RTC", "route": "rtc-lse-pc14-pc15"},
                    {"id": "pad", "controller": "GPIOC", "route": "gpio-pc14",
                     "mask": 1 << 14, "initial": 0}]
        text = ('[rtc.calendar]\nbinding="calendar"\nmode="adopt-retained"\n'
                'allow_set=false\ntimeout_us=2000000\npoll_limit=1000000\n')
        with self.assertRaisesRegex(gate.ConfigurationError, "Resource conflict pin:PC14"):
            self.resolve(self.assembly("STM32F407ZGT6", selected, extra=text))
