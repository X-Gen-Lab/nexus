"""One FSMC owner statically connects exact reviewed external bank faces."""
from tools.configure.tests.native_fixture import native_fixture
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import tomllib
import unittest
from tools.configure.providers import maintained

from tools.configure import bindings, configure as gate, factory, selected_contracts


class F407FSMCConfigurationTests(unittest.TestCase):
    def fixture(self, directory, *, sram=True, lcd=True, extra="",
                timing='address_setup=2,address_hold=1,data_setup=9,bus_turn=1',
                memory_mode="cpu-window"):
        board = gate.load(gate.ROOT /
                          "boards/stm32f407zg_qiming_v41/board.json")
        board["id"] = "f407-fsmc-configuration-software-fixture"
        board["bindings"] = [
            {"id": "fsmc0", "controller": "FSMC", "route": "fsmc-async16"},
            {"id": "sram0", "controller": "FSMC",
             "route": "fsmc-ne3-is62wv51216"},
            {"id": "lcd0", "controller": "FSMC",
             "route": "fsmc-ne4-ili9341"},
            {"id": "busy-pad", "controller": "GPIOD", "route": "gpio-pd0"}]
        board["provenance"] = []
        board["unknowns"] = ["Software fixture; external timing not measured"]
        (directory / "board.json").write_text(json.dumps(board))
        text = ('schema=2\nboard_package="."\nbackend="baremetal"\n'
                'clock="hse8-pll168"\noptimization="Os"\n'
                '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                'flash_load_limit_bytes=65536\nstatic_ram_limit_bytes=32768\n'
                '[external_bus.ext0]\nbinding="fsmc0"\nmode="async16"\n')
        if memory_mode == 'startup-memory':
            text = text.replace('[external_bus.ext0]',
                '[memory.regions.addsram]\nlimit_bytes=4096\n[external_bus.ext0]')
        for kind, name, binding in (("memory_window", "ram0", "sram0"),
                                    ("parallel16", "panel0", "lcd0")):
            if (kind == "memory_window" and not sram or
                    kind == "parallel16" and not lcd):
                continue
            mode = f'mode="{memory_mode}"\n' if kind == 'memory_window' else ''
            text += (f'[{kind}.{name}]\ncontroller="ext0"\nbinding="{binding}"\n' +
                     mode + f'read_timing={{{timing}}}\nwrite_timing={{{timing}}}\n')
        assembly = directory / "assembly.toml"
        assembly.write_text(text + extra)
        return assembly

    def test_one_bus_three_public_classes_and_one_exact_pad_union(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = gate.resolve_ir(self.fixture(Path(temporary)), test_support=native_fixture(self.fixture(Path(temporary))))
            bus, = result.controllers
            self.assertEqual((bus.kind, bus.mode), ("external_bus", "async16"))
            self.assertEqual(len(bus.pins), 41)
            self.assertEqual(len(result.devices), 2)
            endpoints = {item.id: item for item in result.devices}
            self.assertEqual(endpoints["ram0"].options.address, 0x68000000)
            self.assertEqual(endpoints["ram0"].options.bytes, 0x100000)
            self.assertEqual(endpoints["panel0"].options.data_address, 0x6c002000)
            with self.assertRaises(FrozenInstanceError):
                endpoints["ram0"].options.bytes = 16
            keys = [claim.key for claim in result.resource_plan.claims]
            self.assertEqual(keys.count("pin:PG2"), 1)
            self.assertIn("ip:FSMC/control", keys)
            self.assertIn("ip:FSMC/bank3", keys)
            self.assertIn("ip:FSMC/bank4", keys)
            classes = factory.classes(result)
            self.assertTrue({"external_bus", "memory_window", "parallel16"}
                            .issubset({name for name, _, _ in classes}))
            header, _ = factory.emit(result)
            _, source = bindings.emit(result)
            self.assertIn("nx_factory_external_bus", header)
            self.assertIn("nx_factory_memory_window", header)
            self.assertIn("nx_factory_parallel16", header)
            self.assertIn("nx_stm32_fsmc_prepare", source)
            self.assertIn("nx_stm32_fsmc_lcd_prepare", source)
            self.assertIn("nx_stm32_fsmc_lcd_close", source)
            self.assertNotIn("nx_memory_window_port_acquire(", source)
            self.assertNotIn("nx_ili9341_initialize(", source)
            contract = selected_contracts.describe(bus, endpoints["panel0"])
            self.assertEqual((contract.unit, contract.unit_bytes,
                              contract.max_transfer_units), ("words", 2, 76800))
            window = selected_contracts.describe(bus, endpoints["ram0"])
            self.assertEqual((window.kind, window.mode),
                             ("memory_window", "cpu-window"))
            self.assertEqual(window.features, ())
            self.assertEqual(window.fields["window_address"], 0x68000000)

    def test_single_banks_claim_only_their_selected_pads(self):
        for sram, lcd, count in ((True, False, 40), (False, True, 20)):
            with self.subTest(sram=sram, lcd=lcd):
                with tempfile.TemporaryDirectory() as temporary:
                    result = gate.resolve_ir(self.fixture(Path(temporary),
                                                          sram=sram, lcd=lcd), test_support=native_fixture(self.fixture(Path(temporary),
                                                          sram=sram, lcd=lcd)))
                    self.assertEqual(len(result.controllers[0].pins), count)

    def test_empty_external_bus_is_not_a_selected_hardware_transport(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "(?i)(bank|endpoint|child)"):
                gate.resolve_ir(self.fixture(Path(temporary),
                                             sram=False, lcd=False), test_support=native_fixture(self.fixture(Path(temporary),
                                             sram=False, lcd=False)))

    def test_same_physical_bank_cannot_be_selected_twice(self):
        extra = ('[memory_window.alias]\ncontroller="ext0"\nbinding="sram0"\nmode="cpu-window"\n'
                 'read_timing={address_setup=2,address_hold=1,data_setup=9,bus_turn=1}\n'
                 'write_timing={address_setup=2,address_hold=1,data_setup=9,bus_turn=1}\n')
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "(?i)(bank|conflict|owned)"):
                gate.resolve_ir(self.fixture(Path(temporary), extra=extra), test_support=native_fixture(self.fixture(Path(temporary), extra=extra)))

    def test_timing_limits_and_foreign_controller_reject(self):
        for timing in ('address_setup=16,address_hold=1,data_setup=9,bus_turn=1',
                       'address_setup=2,address_hold=0,data_setup=9,bus_turn=1',
                       'address_setup=2,address_hold=1,data_setup=0,bus_turn=1',
                       'address_setup=2,address_hold=1,data_setup=true,bus_turn=1'):
            with self.subTest(timing=timing):
                with tempfile.TemporaryDirectory() as temporary:
                    with self.assertRaises(ValueError):
                        gate.resolve_ir(self.fixture(Path(temporary), timing=timing), test_support=native_fixture(self.fixture(Path(temporary), timing=timing)))

    def test_gpio_conflict_and_controller_parent_reservation_are_rejected(self):
        extra = '[gpio.other]\nbinding="busy-pad"\nmode="output"\n'
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "(?i)(conflict|owned)"):
                gate.resolve_ir(self.fixture(Path(temporary), extra=extra), test_support=native_fixture(self.fixture(Path(temporary), extra=extra)))
        for key in ("controller:FSMC", "ip:FSMC"):
            with self.subTest(key=key):
                with tempfile.TemporaryDirectory() as temporary:
                    directory = Path(temporary)
                    assembly = self.fixture(directory)
                    path = directory / "board.json"
                    board = json.loads(path.read_text())
                    board["reserved_resources"].append(key)
                    path.write_text(json.dumps(board))
                    with self.assertRaisesRegex(ValueError, "(?i)(conflict|owned)"):
                        gate.resolve_ir(assembly, test_support=native_fixture(assembly))

    def test_startup_memory_is_explicit_and_cannot_be_a_whole_bank_loan(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = gate.resolve_ir(self.fixture(Path(temporary), memory_mode="startup-memory"), test_support=native_fixture(self.fixture(Path(temporary), memory_mode="startup-memory")))
            child = next(item for item in result.devices if item.id == "ram0")
            self.assertEqual(child.options.mode, "startup-memory")
            _, source = bindings.emit(result)
            self.assertIn("nx_stm32_fsmc_adopt_boot", source)
            self.assertIn("&g_nx_stm32_sram_boot_plan", source)
            self.assertNotIn("nx_stm32_fsmc_prepare(", source)
            excluded = set(maintained("stm32f4").GPIO_PHASE.excluded_pins(result))
            self.assertEqual(len(excluded), 40)
            self.assertNotIn("PG12", excluded)

    def test_authored_address_or_bank_override_is_rejected(self):
        for extra in ('address=1744830464\n', 'bank=3\n', 'boot_backed=true\n'):
            with self.subTest(extra=extra):
                with tempfile.TemporaryDirectory() as temporary:
                    with self.assertRaisesRegex(ValueError, "(?i)(unknown|missing)"):
                        gate.resolve_ir(self.fixture(Path(temporary), extra=extra), test_support=native_fixture(self.fixture(Path(temporary), extra=extra)))

    def test_both_qiming_profiles_use_real_board_choices_and_exact_pin_union(self):
        for board in ("stm32f407zg_qiming_core", "stm32f407zg_qiming_v41"):
            for profile, count in (("display-touch-sram", 41),
                                    ("sram-window", 40)):
                with self.subTest(board=board, profile=profile):
                    result = gate.resolve_ir(gate.ROOT / "boards" / board /
                                             "assemblies" / (profile + ".toml"), test_support=native_fixture(gate.ROOT / "boards" / board /
                                             "assemblies" / (profile + ".toml")))
                    bus = next(item for item in result.controllers
                               if item.kind == "external_bus")
                    self.assertEqual(len(bus.pins), count)
                    self.assertEqual(result.part, "STM32F407ZGT6")
                    memory = next(item for item in result.devices
                                  if item.driver == "memory_window-endpoint")
                    self.assertEqual(memory.options.mode, "cpu-window")
                    _, source = bindings.emit(result)
                    self.assertIn("nx_stm32_fsmc_prepare", source)
                    self.assertNotIn("nx_stm32_fsmc_adopt_boot", source)

    def test_lcd_only_reference_has_exact_typed_faces_and_no_sram_claim(self):
        expected = {"PD0", "PD1", "PD4", "PD5", "PD8", "PD9", "PD10",
                    "PD14", "PD15", "PE7", "PE8", "PE9", "PE10", "PE11",
                    "PE12", "PE13", "PE14", "PE15", "PG2", "PG12"}
        for board in ("stm32f407zg_qiming_core", "stm32f407zg_qiming_v41"):
            with self.subTest(board=board):
                path = (gate.ROOT / "boards" / board /
                        "assemblies/display-touch.toml")
                result = gate.resolve_ir(path, test_support=native_fixture(path))
                bus = next(item for item in result.controllers
                           if item.kind == "external_bus")
                self.assertEqual({pin["pin"] for pin in bus.pins}, expected)
                self.assertEqual(len(bus.pins), 20)
                self.assertEqual(sum(item.kind == "gpio"
                                     for item in result.controllers), 5)
                self.assertEqual(sum(item.kind == "exti"
                                     for item in result.controllers), 1)
                self.assertEqual(len(result.devices), 1)
                panel, = result.devices
                self.assertEqual(panel.driver, "parallel16-endpoint")
                self.assertEqual(panel.options.command_address, 0x6c000000)
                self.assertEqual(panel.options.data_address, 0x6c002000)
                classes = {name: len(items)
                           for name, _, items in factory.classes(result)}
                self.assertEqual(classes["external_bus"], 1)
                self.assertEqual(classes["parallel16"], 1)
                self.assertNotIn("memory_window", classes)
                keys = {claim.key for claim in result.resource_plan.claims}
                selected_pins = {claim.key for claim in result.resource_plan.claims
                                 if claim.key.startswith("pin:") and
                                 claim.owner != "board-reserved"}
                touch_pins = {"pin:PF10", "pin:PF11", "pin:PA5", "pin:PB0",
                              "pin:PB1", "pin:PB2"}
                self.assertEqual(selected_pins,
                                 {"pin:" + pin for pin in expected} | touch_pins)
                self.assertEqual(len(selected_pins), 26)
                reserved_pins = {claim.key for claim in result.resource_plan.claims
                                 if claim.key.startswith("pin:") and
                                 claim.owner == "board-reserved"}
                self.assertEqual(reserved_pins, {"pin:PA13", "pin:PA14"})
                self.assertEqual(sum(key.startswith("pin:") for key in keys), 28)
                self.assertIn("ip:FSMC/bank4", keys)
                self.assertNotIn("ip:FSMC/bank3", keys)
                self.assertNotIn("pin:PG10", keys)
                self.assertNotIn("pin:PF0", keys)
                self.assertNotIn("sram_population",
                                 tomllib.loads(path.read_text())["hardware"])

    def test_lcd_only_ne3_reservations_are_independent_but_lcd_pads_conflict(self):
        for sram in (False, True):
            with self.subTest(sram=sram):
                with tempfile.TemporaryDirectory() as temporary:
                    directory = Path(temporary)
                    assembly = self.fixture(directory, sram=sram)
                    board_file = directory / "board.json"
                    board = json.loads(board_file.read_text())
                    board["reserved_resources"] += ["pin:PF0", "pin:PG10",
                                                    "ip:FSMC/bank3"]
                    board_file.write_text(json.dumps(board))
                    if sram:
                        with self.assertRaisesRegex(ValueError,
                                                    "(?i)(conflict|owned|reserved)"):
                            gate.resolve_ir(assembly, test_support=native_fixture(assembly))
                    else:
                        result = gate.resolve_ir(assembly, test_support=native_fixture(assembly))
                        self.assertEqual(len(result.controllers[0].pins), 20)
        with tempfile.TemporaryDirectory() as temporary:
            extra = '[gpio.other]\nbinding="busy-pad"\nmode="output"\n'
            with self.assertRaisesRegex(ValueError, "(?i)(conflict|owned)"):
                gate.resolve_ir(self.fixture(Path(temporary), sram=False,
                                             extra=extra), test_support=native_fixture(self.fixture(Path(temporary), sram=False,
                                             extra=extra)))

    def test_lcd_reference_read_phases_cover_general_ili9341_frame_memory(self):
        # ILI9341_DS_V1.11 section 18.3.1 p238: FM low355/high90/cycle450ns.
        # PDF SHA256 a9bbfdf6d078f54a6aca7a56cba91246905358d3a4ed738817bfd3f582b5741c.
        # RM0090 Rev22 Fig438/Table229 p1544/1546: read DATAST/ADDSET HCLK.
        # P1569: fixed one-HCLK turn between same-bank asynchronous reads.
        for board in ("stm32f407zg_qiming_core", "stm32f407zg_qiming_v41"):
            for profile in ("display-touch", "display-touch-sram"):
                with self.subTest(board=board, profile=profile):
                    result = gate.resolve_ir(gate.ROOT / "boards" / board /
                                             "assemblies" / (profile + ".toml"), test_support=native_fixture(gate.ROOT / "boards" / board /
                                             "assemblies" / (profile + ".toml")))
                    lcd = next(item for item in result.devices
                               if item.driver == "parallel16-endpoint")
                    timing = lcd.options.read_timing
                    hz = result.clock["core_hz"]
                    high_clocks = timing.address_setup + 1
                    low_clocks = timing.data_setup
                    self.assertGreaterEqual(low_clocks * 10**9, 355 * hz)
                    self.assertGreaterEqual(high_clocks * 10**9, 90 * hz)
                    self.assertGreaterEqual((high_clocks + low_clocks) * 10**9,
                                            450 * hz)


if __name__ == "__main__":
    unittest.main()
