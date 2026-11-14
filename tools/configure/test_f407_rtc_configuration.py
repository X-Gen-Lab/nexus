"""Exact Qiming LSE calendar assembly has explicit backup ownership policy."""
from tools.configure.tests.native_fixture import native_fixture
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import authored, bindings, factory, gpio_phase
from tools.configure import configure as gate
from tools.configure.providers import for_selection


BOARDS = ("stm32f407zg_qiming_core", "stm32f407zg_qiming_v41")
INITIAL = ('initial = { year = 0, month = 2, day = 29, weekday = 2, '
           'hour = 12, minute = 34, second = 56 }\n')


class F407RtcConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="f407 rtc assembly ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def text(self, board=BOARDS[0], mode="adopt-retained", extra=""):
        return (f'schema = 2\nboard = "{board}"\nbackend = "baremetal"\n'
                'clock = "hse8-pll168"\n[memory]\nmain_stack_bytes = 2048\n'
                'libc_heap_bytes = 0\n[rtc.calendar]\nbinding = "rtc0"\n'
                f'mode = "{mode}"\nallow_set = false\ntimeout_us = 2000000\n'
                'poll_limit = 1000000\n' + extra)

    def resolve(self, text=None):
        path = self.directory / "assembly.toml"
        path.write_text(self.text() if text is None else text)
        return gate.resolve_ir(path, test_support=native_fixture(path))

    def construction(self, result):
        return for_selection(result).constructor(
            result.controllers[0], result, result.board_bindings,
            result.routes, result.devices)

    def private_board(self, mutate):
        board = json.loads((gate.ROOT / "boards" / BOARDS[0] /
                            "board.json").read_text())
        mutate(board)
        directory = self.directory / "board"
        directory.mkdir(exist_ok=True)
        (directory / "board.json").write_text(json.dumps(board))
        return self.text().replace('board = "' + BOARDS[0] + '"',
                                   'board_package = "' + str(directory) + '"')

    def test_both_exact_boards_resolve_immutable_adopt_options(self):
        for board in BOARDS:
            with self.subTest(board=board):
                result = self.resolve(self.text(board))
                item, = result.controllers
                self.assertEqual(("rtc", "RTC", "adopt-retained"),
                                 (item.kind, item.controller, item.mode))
                self.assertEqual("RtcOptions", type(item.options).__name__)
                self.assertFalse(item.options.allow_set)
                self.assertIsNone(item.options.initial)
                self.assertEqual((2000000, 1000000),
                                 (item.options.timeout_us,
                                  item.options.poll_limit))
                with self.assertRaises(FrozenInstanceError):
                    item.options.allow_set = True

    def test_checked_in_profiles_never_select_implicit_provisioning(self):
        for board in BOARDS:
            with self.subTest(board=board):
                result = gate.resolve_ir(gate.ROOT / "boards" / board /
                                         "assemblies/rtc-adopt.toml", test_support=native_fixture(gate.ROOT / "boards" / board /
                                         "assemblies/rtc-adopt.toml"))
                item, = result.controllers
                self.assertEqual("adopt-retained", item.mode)
                self.assertIsNone(item.options.initial)
                self.assertFalse(item.options.allow_set)

    def test_duration_wakeup_is_explicit_and_defaults_to_no_code(self):
        result = self.resolve()
        self.assertFalse(result.controllers[0].options.periodic_wakeup)
        path = self.directory / "assembly.toml"
        path.write_text(self.text(extra="periodic_wakeup = true\n"))
        output = self.directory / "periodic"
        gate.configure(path, output, test_support=native_fixture(path))
        self.assertIn("set(NEXUS_RTC_PERIODIC_WAKEUP ON)",
                      (output / "selection.cmake").read_text())
        self.assertTrue(gate.resolve_ir(
            path, test_support=native_fixture(path)).controllers[0].options.periodic_wakeup)
        for value in ("1", '"true"'):
            with self.subTest(value=value):
                with self.assertRaisesRegex(gate.ConfigurationError,
                                            "periodic_wakeup"):
                    self.resolve(self.text(extra="periodic_wakeup = " +
                                           value + "\n"))

    def test_provision_copies_all_explicit_calendar_fields_including_year_zero(self):
        result = self.resolve(self.text(mode="provision-empty", extra=INITIAL))
        calendar = result.controllers[0].options.initial
        self.assertEqual((0, 2, 29, 2, 12, 34, 56),
                         tuple(getattr(calendar, name) for name in
                               ("year", "month", "day", "weekday", "hour",
                                "minute", "second")))
        with self.assertRaises(FrozenInstanceError):
            calendar.year = 1

    def test_generated_c_owns_no_register_algorithm_or_guessed_initial_date(self):
        result = self.resolve()
        source = bindings.emit(result)[1]
        self.assertIn('#include "stm32f407_rtc.h"', source)
        self.assertIn(".policy = NX_STM32_RTC_ADOPT_RETAINED", source)
        self.assertIn(".allow_set = false, .has_initial = false", source)
        self.assertIn(".timeout_us = 2000000u, .poll_limit = 1000000u", source)
        self.assertIn(".clock = { .read = nx_stm32_rtc_clock_read, "
                      ".context = &g_nx_stm32_system }", source)
        self.assertIn("nx_stm32_rtc_prepare(&s_nx_port_calendar, &s_nx_rtc_config_calendar)", source)
        for forbidden in ("RTC->", "RCC->BDCR", "PWR->CR", "HAL_RTC_Init",
                          ".year =", "RTC_IRQHandler"):
            self.assertNotIn(forbidden, source)

    def test_provision_static_initial_and_cleanup_preserve_retained_effects(self):
        result = self.resolve(self.text(mode="provision-empty", extra=INITIAL))
        construction = self.construction(result)
        self.assertTrue(construction.retain_on_failed_init)
        self.assertEqual({}, dict(construction.pin_snapshots))
        self.assertEqual(["nx_stm32_rtc_close(&s_nx_port_calendar)"],
                         [call.render() for call in construction.stop])
        source = bindings.emit(result)[1]
        self.assertIn(".policy = NX_STM32_RTC_PROVISION_EMPTY", source)
        self.assertIn(".allow_set = false, .has_initial = true", source)
        self.assertIn(".initial = { .year = 0u, .month = 2u, .day = 29u, "
                      ".weekday = 2u, .hour = 12u, .minute = 34u, .second = 56u }", source)
        self.assertIn("nx_stm32_rtc_close(&s_nx_port_calendar)", source)
        self.assertNotIn("nx_rtc_port_stop(", source)

    def test_factory_lookup_never_provisions_or_reads_hardware(self):
        result = self.resolve()
        header, source = factory.emit(result)
        self.assertIn("NX_RTC_ID_COUNT = 1", header)
        self.assertIn("return nx_binding_calendar;", source)
        for forbidden in ("prepare(", "close(", "HAL_", "RTC->", "clock_read("):
            self.assertNotIn(forbidden, source)

    def test_oscillator_claims_conflict_but_never_get_gpio_snapshots(self):
        result = self.resolve()
        claims = {item.key for item in result.claims if item.owner == "calendar"}
        self.assertEqual({"controller:RTC", "pin:PC13", "pin:PC14", "pin:PC15",
                          "clock:LSE", "clock:RTC", "domain:backup",
                          "alarm:RTC_A"}, claims)
        self.assertEqual((), result.controllers[0].pins)
        self.assertEqual((), gpio_phase.selected_pins(result))
        source = bindings.emit(result)[1]
        for forbidden in ("s_nx_platform_pins_config", "GPIOC,", "pin_state_calendar"):
            self.assertNotIn(forbidden, source)

    def test_oscillator_exclusion_does_not_exclude_an_ordinary_led(self):
        result = self.resolve(self.text() +
                              '[gpio.indicator]\nbinding = "led0"\nmode = "output"\n')
        self.assertEqual(("PG13",), gpio_phase.selected_pins(result))
        source = bindings.emit(result)[1]
        self.assertIn("GPIOG,", source)
        self.assertNotIn("GPIOC,", source)

    def test_exclusion_hook_cannot_hide_an_unselected_pad_or_duplicate(self):
        result = self.resolve()
        for excluded in (("PA3",), ("PC14", "PC14")):
            provider = SimpleNamespace(GPIO_PHASE=SimpleNamespace(
                excluded_pins=lambda _result, values=excluded: values))
            with self.subTest(excluded=excluded), patch.object(
                    gpio_phase, "for_selection", return_value=provider):
                with self.assertRaisesRegex(gate.ConfigurationError,
                                            "exclu.*pin|pin.*exclu"):
                    gpio_phase.selected_pins(result)

    def test_oscillator_pad_reservation_is_not_bypassed_by_rtc(self):
        text = self.private_board(lambda board: board["reserved_resources"].append(
            {"key": "pin:PC14", "owner": "external-oscillator-owner"}))
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "Resource conflict pin:PC14"):
            self.resolve(text)

    def test_rtc_duplicate_is_still_exclusive(self):
        text = self.text() + ('[rtc.other]\nbinding = "rtc0"\n'
                               'mode = "adopt-retained"\nallow_set = false\n'
                               'timeout_us = 1\npoll_limit = 1\n')
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "Resource conflict controller:RTC"):
            self.resolve(text)

    def test_board_must_declare_the_exact_32768_hz_lse(self):
        text = self.private_board(lambda board: board["clocks"].update(lse_hz=32000))
        with self.assertRaisesRegex(gate.ConfigurationError, "LSE|lse"):
            self.resolve(text)

    def test_all_cold_policy_fields_are_explicit(self):
        for field, literal in (("allow_set", "false"), ("timeout_us", "2000000"),
                               ("poll_limit", "1000000")):
            with self.subTest(field=field), self.assertRaisesRegex(
                    gate.ConfigurationError, "explicit|requires"):
                self.resolve(self.text().replace(field + " = " + literal + "\n", ""))

    def test_invalid_bounds_and_boolean_types_are_rejected(self):
        for field, old, values in (
                ("allow_set", "false", ("0", '"false"')),
                ("timeout_us", "2000000", ("0", "-1", "true", "4294967296")),
                ("poll_limit", "1000000", ("0", "-1", "true", "4294967296"))):
            for value in values:
                with self.subTest(field=field, value=value), self.assertRaisesRegex(
                        gate.ConfigurationError, field):
                    self.resolve(self.text().replace(field + " = " + old,
                                                      field + " = " + value))

    def test_adopt_refuses_any_initial_and_provision_requires_it(self):
        for mode, extra in (("adopt-retained", INITIAL), ("provision-empty", "")):
            with self.subTest(mode=mode), self.assertRaisesRegex(
                    gate.ConfigurationError, "initial"):
                self.resolve(self.text(mode=mode, extra=extra))

    def test_invalid_calendar_values_and_unknown_fields_are_rejected(self):
        for old, new in (("year = 0", "year = true"),
                         ("year = 0", "year = 100"),
                         ("year = 0", "year = 1"),
                         ("month = 2", "month = 0"),
                         ("day = 29", "day = 30"),
                         ("weekday = 2", "weekday = 0"),
                         ("hour = 12", "hour = 24"),
                         ("minute = 34", "minute = 60"),
                         ("second = 56", "second = 60"),
                         ("second = 56", "second = 56, timezone = 0")):
            with self.subTest(new=new), self.assertRaisesRegex(
                    gate.ConfigurationError, "initial|calendar"):
                self.resolve(self.text(mode="provision-empty",
                                       extra=INITIAL.replace(old, new)))


    def test_editor_schema_expresses_explicit_calendar_and_finite_bounds(self):
        schema = authored.editor_schema()["properties"]["rtc"]["additionalProperties"]
        self.assertEqual("boolean", schema["properties"]["allow_set"]["type"])
        for field in ("timeout_us", "poll_limit"):
            self.assertEqual((1, 0xffffffff),
                             (schema["properties"][field]["minimum"],
                              schema["properties"][field]["maximum"]))
        calendar = schema["properties"]["initial"]
        self.assertEqual({"year", "month", "day", "weekday", "hour", "minute", "second"},
                         set(calendar["required"]))
        self.assertFalse(calendar["additionalProperties"])

    def test_invalid_policy_removes_previously_published_bundle(self):
        path = self.directory / "assembly.toml"
        path.write_text(self.text())
        output = self.directory / "generated"
        gate.configure(path, output, test_support=native_fixture(path))
        self.assertTrue((output / "nexus_factory_strong.h").is_file())
        path.write_text(self.text(mode="provision-empty"))
        with self.assertRaisesRegex(gate.ConfigurationError, "initial"):
            gate.configure(path, output, test_support=native_fixture(path))
        self.assertFalse(output.exists())


    def test_default_crystal_keeps_explicit_immutable_source_choice(self):
        result = self.resolve()
        self.assertFalse(result.controllers[0].options.lse_bypass)

    def test_bypass_requires_matching_external_clock_fact(self):
        def bypass(board):
            board["clocks"]["lse_bypass"] = True
            rtc = next(item for item in board["bindings"]
                       if item["controller"] == "RTC")
            rtc["route"] = "rtc-lse-bypass-pc14"
        text = self.private_board(bypass)
        result = self.resolve(text + 'lse_bypass = true\n')
        self.assertTrue(result.controllers[0].options.lse_bypass)
        self.assertIn(".lse_bypass = true", bindings.emit(result)[1])
        self.assertNotIn("pin:PC13", [item.key for item in result.resource_plan.claims
                                    if item.owner == "calendar"])

    def test_bypass_cannot_reuse_a_crystal_pad_route(self):
        text = self.private_board(
            lambda board: board["clocks"].update(lse_bypass=True))
        with self.assertRaisesRegex(gate.ConfigurationError, "RTC.*route"):
            self.resolve(text + 'lse_bypass = true\n')

    def test_bypass_cannot_relabel_a_crystal_board(self):
        with self.assertRaisesRegex(gate.ConfigurationError, "[Bb]ypass|source"):
            self.resolve(self.text() + 'lse_bypass = true\n')

    def test_crystal_reserves_pc13_against_gpio_transitions(self):
        def pin(board):
            board["bindings"].append({
                "id": "pc13", "controller": "GPIOC", "route": "gpio-pc13",
                "mask": 1 << 13, "initial": 0,
                "electrical": {"mode": "input", "pull": "none"}})
        text = self.private_board(pin)
        text += '[gpio.pad]\nbinding="pc13"\nmode="input"\n'
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "Resource conflict pin:PC13"):
            self.resolve(text)


if __name__ == "__main__":
    unittest.main()
