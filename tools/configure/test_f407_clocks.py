"""F407 clocks are calculated from one reviewed oscillator/PLL tuple."""
import copy
import tempfile
from pathlib import Path
import unittest

from tools.configure import stm32f4


class F407ClockTests(unittest.TestCase):
    def test_system_review_projects_exact_cuts_without_controller_approval(self):
        from tools.configure import configure, selected_contracts
        expected = (0x1001, 0x1003, 0x1007, 0x100F, 0x101F)
        for board in ("qiming", "sky"):
            with self.subTest(board=board):
                result = configure.resolve_ir(configure.ROOT /
                    f"tools/configure/assemblies/{board}-baremetal-gpio.toml")
                policies = selected_contracts.silicon_policies(result)
                system = policies[None]
                self.assertIsNotNone(system)
                self.assertEqual(expected,
                                 result.clock_plan.reviewed_revision_ids)
                self.assertEqual(expected, system.reviewed_revision_ids)
                self.assertEqual(result.part, system.part)
                self.assertEqual(result.clock_plan.profile, system.profile)
                self.assertEqual("stm32f4", system.provider)
                self.assertEqual("reject", system.unknown_revision)
                self.assertFalse(system.physical_qualified)
                self.assertIn("ES0182 Rev19", system.review_scope)
                self.assertTrue(result.controllers)
                for controller in result.controllers:
                    self.assertIsNone(policies[controller.id])

    def test_managed_usb_roles_consume_the_selected_phy_clock(self):
        from types import SimpleNamespace
        parameters = {**self.parameters(), "source": "hse",
                      "input_hz": 8000000, "pll_m": 8}
        plan = stm32f4.clock_plan("custom", parameters)
        for mode, controller in (("otg-fs", "OTG_FS"),
                ("otg-hs-fs", "OTG_HS"), ("otg-hs-ulpi", "OTG_HS")):
            with self.subTest(mode=mode):
                consumer = SimpleNamespace(kind="usb_device", mode=mode,
                                           controller=controller, options=None)
                stm32f4.validate_clock_consumers(plan, (consumer,))
                hsi = stm32f4.clock_plan("custom", self.parameters())
                if mode != "otg-hs-ulpi":
                    with self.assertRaisesRegex(ValueError, "HSE crystal"):
                        stm32f4.validate_clock_consumers(hsi, (consumer,))
                else:
                    parameters = {**self.parameters(), "pll_n": 288, "pll_q": 8}
                    independent = stm32f4.clock_plan("custom", parameters)
                    self.assertEqual(independent["domains"]["pll48"]["nominal_hz"], 36000000)
                    stm32f4.validate_clock_consumers(independent, (consumer,))

    def test_clock_calculation_is_part_of_generator_freshness_identity(self):
        from tools.configure import dependencies
        self.assertIn("tools/configure/stm32f4/clocks.py",
                      dependencies.source_names("platform", "stm32f4"))

    def test_existing_profile_has_calculated_peripheral_domains(self):
        plan = stm32f4.clock_plan("hse8-pll168", {
            "hse_hz": 8000000, "core_hz": 168000000,
            "apb1_hz": 42000000, "apb2_hz": 84000000})
        self.assertIsNotNone(plan)
        self.assertEqual(plan["domains"]["pll48"]["nominal_hz"], 48000000)
        self.assertEqual(plan["domains"]["apb1_timer"]["nominal_hz"], 84000000)
        self.assertEqual(plan["domains"]["adc"]["nominal_hz"], 21000000)

    def test_hsi_pll84_and_timer_multiplier_are_exact(self):
        plan = stm32f4.clock_plan("custom", self.parameters())
        self.assertEqual(plan["domains"]["core"]["nominal_hz"], 84000000)
        self.assertEqual(plan["domains"]["apb1"]["nominal_hz"], 21000000)
        self.assertEqual(plan["domains"]["apb1_timer"]["nominal_hz"], 42000000)
        self.assertEqual(plan["domains"]["apb2_timer"]["nominal_hz"], 84000000)
        self.assertEqual(plan["parameters"]["flash_wait_states"], 2)
        self.assertEqual(plan["parameters"]["pll_m"], 16)
        self.assertFalse(plan["physical_qualified"])

    def test_direct_input_does_not_claim_or_enable_an_unused_main_pll(self):
        parameters = self.parameters()
        parameters.update(sysclk_source="input", apb1_div=1, apb2_div=1)
        plan = stm32f4.clock_plan("custom", parameters)
        self.assertEqual(plan["domains"]["sysclk"]["nominal_hz"], 16000000)
        self.assertEqual(plan["domains"]["core"]["nominal_hz"], 16000000)
        self.assertEqual(plan["domains"]["pll48"]["nominal_hz"], 0)
        self.assertFalse(plan["parameters"]["pll_enabled"])
        self.assertEqual(plan["parameters"]["sw"], 0)

    def test_direct_input_reserves_pll48_only_by_explicit_static_choice(self):
        from types import SimpleNamespace
        parameters = {**self.parameters(), "sysclk_source": "input"}
        block = SimpleNamespace(kind="block", mode="sdio-native",
                                controller="SDIO", options=None)
        plan = stm32f4.clock_plan("custom", parameters)
        with self.assertRaises(ValueError):
            stm32f4.validate_clock_consumers(plan, (block,))
        parameters["pll48"] = True
        plan = stm32f4.clock_plan("custom", parameters)
        self.assertTrue(plan["parameters"]["pll_enabled"])
        self.assertEqual(plan["domains"]["pll48"]["nominal_hz"], 48000000)
        stm32f4.validate_clock_consumers(plan, (block,))
        parameters.update(source="hse", input_hz=8000000, pll_m=8)
        plan = stm32f4.clock_plan("custom", parameters)
        self.assertEqual(plan["parameters"]["sw"], 1)
        self.assertEqual(plan["domains"]["core"]["nominal_hz"], 8000000)

    def test_internal_usb_rejects_hsi_nominal_48_without_crystal_accuracy(self):
        from types import SimpleNamespace
        for kind, mode, controller in (
                ("usb_device", "device-fs", "OTG_FS"),
                ("usb_host", "host-fs", "OTG_FS"),
                ("usb_device", "device-hs-fs", "OTG_HS"),
                ("usb_host", "host-hs-fs", "OTG_HS")):
            for sysclk_source in ("pll", "input"):
                parameters = {**self.parameters(),
                              "sysclk_source": sysclk_source, "pll48": True}
                if sysclk_source == "input" and controller == "OTG_HS":
                    continue
                item = SimpleNamespace(kind=kind, mode=mode,
                                       controller=controller, options=None)
                plan = stm32f4.clock_plan("custom", parameters)
                self.assertEqual(plan["domains"]["pll48"]["nominal_hz"],
                                 48000000)
                with self.subTest(mode=mode, sysclk=sysclk_source):
                    with self.assertRaisesRegex(ValueError, "external HSE"):
                        stm32f4.validate_clock_consumers(plan, (item,))
                parameters.update(source="hse")
                stm32f4.validate_clock_consumers(
                    stm32f4.clock_plan("custom", parameters), (item,))

    def test_direct_input_rejects_unknown_source_and_untyped_pll_owner(self):
        for choice in ({"sysclk_source": "nearest"}, {"pll48": 1}):
            with self.subTest(choice=choice), self.assertRaises(ValueError):
                stm32f4.clock_plan("custom", {**self.parameters(), **choice})

    def test_usb_consumes_its_exact_phy_clock_and_minimum_ahb_frequency(self):
        from types import SimpleNamespace
        parameters = {**self.parameters(), "pll_n": 192, "pll_p": 8,
                      "pll_q": 4, "ahb_div": 2, "apb1_div": 1,
                      "apb2_div": 1}
        fs = SimpleNamespace(kind="usb_device", mode="device-fs",
                             controller="OTG_FS", options=None)
        with self.assertRaises(ValueError):
            stm32f4.validate_clock_consumers(
                stm32f4.clock_plan("custom", parameters), (fs,))
        parameters.update(pll_n=240, pll_q=5, ahb_div=1)
        ulpi = SimpleNamespace(kind="usb_host", mode="host-hs-ulpi",
                               controller="OTG_HS", options=None)
        with self.assertRaises(ValueError):
            stm32f4.validate_clock_consumers(
                stm32f4.clock_plan("custom", parameters), (ulpi,))
        parameters.update(pll_n=288, pll_q=8)
        plan = stm32f4.clock_plan("custom", parameters)
        self.assertEqual(plan["domains"]["pll48"]["nominal_hz"], 36000000)
        stm32f4.validate_clock_consumers(plan, (ulpi,))
        ulpi.controller = "OTG_FS"
        with self.assertRaises(ValueError):
            stm32f4.validate_clock_consumers(plan, (ulpi,))

    def test_direct_input_pwm_uses_its_actual_timer_divider(self):
        from tools.configure import configure, bindings
        with tempfile.TemporaryDirectory(dir=configure.ROOT / "build") as temp:
            path = Path(temp) / "pwm.toml"
            source = (configure.ROOT /
                      "tests/contracts/stm32_assembly/zg-pwm.toml")
            text = source.read_text().replace(
                'board_package = "zg_board"',
                f'board_package = "{source.parent / "zg_board"}"')
            text = text.replace('clock = "hse8-pll168"', 'clock = "custom"')
            text = text.replace('tick_hz = 1000000', 'tick_hz = 16000000')
            parameters = {**self.parameters(), "sysclk_source": "input",
                          "apb1_div": 1, "apb2_div": 1}
            text += '\n[clock_parameters]\n' + '\n'.join(
                f'{key}="{value}"' if isinstance(value, str) else
                f'{key}={str(value).lower()}'
                for key, value in parameters.items())
            path.write_text(text)
            result = configure.resolve_ir(path)
            self.assertEqual(result.controllers[0].integration.clock_hz,
                             16000000)
            self.assertIn('.prescaler = 0u', bindings.emit(result)[1])

    def test_bus_divide_one_does_not_double_timer(self):
        parameters = self.parameters()
        parameters.update(pll_p=8, apb1_div=1, apb2_div=1)
        plan = stm32f4.clock_plan("custom", parameters)
        self.assertEqual(plan["domains"]["apb1_timer"]["nominal_hz"], 42000000)

    def test_rejects_fractional_unsafe_or_competing_facts(self):
        for change in ({"pll_m": 3}, {"pll_n": 191}, {"pll_q": 1},
                       {"pll_p": 3}, {"apb1_div": 1}, {"supply_mv": 1800},
                       {"css": True}, {"input_hz": 8000000},
                       {"core_hz": 84000000}, {"pll_m": True}):
            parameters = {**self.parameters(), **change}
            with self.subTest(change=change), self.assertRaises(ValueError):
                stm32f4.clock_plan("custom", parameters)

    def test_css_requires_external_oscillator_and_keeps_48_dependency(self):
        parameters = self.parameters()
        parameters.update(source="hse", input_hz=8000000, pll_m=8, css=True)
        before = copy.deepcopy(parameters)
        plan = stm32f4.clock_plan("custom", parameters)
        self.assertEqual(plan["domains"]["pll48"]["nominal_hz"], 48000000)
        self.assertTrue(plan["parameters"]["css"])
        self.assertEqual(parameters, before)

    def test_ethernet_rejects_sub25mhz_before_hardware_admission(self):
        from types import SimpleNamespace
        parameters = {**self.parameters(), "sysclk_source": "input",
                      "apb1_div": 1, "apb2_div": 1}
        selected = stm32f4.clock_plan("custom", parameters)
        for kind in ("eth", "eth_frames"):
            item = SimpleNamespace(kind=kind, mode="raw-mii", controller="ETH",
                                   options=None)
            with self.subTest(kind=kind), self.assertRaisesRegex(
                    ValueError, "Ethernet.*25"):
                stm32f4.validate_clock_consumers(selected, (item,))

    def test_selected_clock_consumers_reject_impossible_kernel_and_baud(self):
        from types import SimpleNamespace
        validate = getattr(stm32f4, "validate_clock_consumers", None)
        self.assertTrue(callable(validate))
        parameters = self.parameters()
        parameters["pll_q"] = 8
        plan = stm32f4.clock_plan("custom", parameters)
        for kind in ("usb_device", "usb_host", "block"):
            mode = "host-fs" if kind == "usb_host" else "device-fs"
            item = SimpleNamespace(kind=kind, mode=mode,
                                   controller="OTG_FS", options=None)
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                validate(plan, (item,))
        parameters.update(pll_n=384, pll_p=8, apb1_div=16, apb2_div=16)
        plan = stm32f4.clock_plan("custom", parameters)
        uart = SimpleNamespace(kind="uart", mode="irq-byte-event",
                               controller="USART1",
                               options=SimpleNamespace(baud=1000000))
        with self.assertRaises(ValueError):
            validate(plan, (uart,))
        parameters.update(pll_n=192, pll_p=8, ahb_div=8, apb1_div=1,
                          apb2_div=4)
        plan = stm32f4.clock_plan("custom", parameters)
        adc = SimpleNamespace(kind="adc", mode="single-shot",
                              controller="ADC1", options=None)
        with self.assertRaises(ValueError):
            validate(plan, (adc,))

    def test_custom_assembly_emits_one_plan_and_actual_uart_clock(self):
        from tools.configure import configure
        with tempfile.TemporaryDirectory(dir=configure.ROOT / "build") as temp:
            path = Path(temp) / "clock.toml"
            board = (configure.ROOT /
                     "tests/contracts/multi_controller_assembly/stm32-ve-board")
            parameters = "\n".join(f"{key}={str(value).lower()}"
                                   if isinstance(value, (int, bool))
                                   else f'{key}="{value}"'
                                   for key, value in self.parameters().items())
            path.write_text(f'schema=2\nboard_package="{board}"\n'
                            'backend="baremetal"\nclock="custom"\n'
                            '[memory]\nmain_stack_bytes=2048\n'
                            '[uart.console]\nbinding="primary-uart"\n'
                            'mode="irq-byte-event"\nbaud=115200\n'
                            '[uart.console.rx]\nformat="events"\ncapacity=16\n'
                            '[uart.console.irq]\npriority=5\n'
                            '[clock_parameters]\n' + parameters)
            result = configure.resolve_ir(path)
            self.assertEqual(result.controllers[0].integration.clock_hz,
                             42000000)
            from tools.configure import emission
            emission.emit_bundle(result, Path(temp))
            header = (Path(temp) / "nexus_config.h").read_text()
            self.assertIn("#define NEXUS_CORE_HZ 84000000u", header)
            self.assertIn("#define NEXUS_APB1_TIMER_HZ 42000000u", header)
            self.assertIn("#define NEXUS_CLOCK_PLLM 16u", header)

    @staticmethod
    def parameters():
        return dict(source="hsi", input_hz=16000000, pll_m=16, pll_n=336,
                    pll_p=4, pll_q=7, ahb_div=1, apb1_div=4, apb2_div=2,
                    adc_div=4, supply_mv=3300, css=False)


if __name__ == "__main__":
    unittest.main()
