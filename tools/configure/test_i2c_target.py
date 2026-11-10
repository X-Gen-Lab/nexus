"""Finite target/SMBus selection has one physical I2C and static C owner."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import authored, bindings, factory
from tools.configure import configure as gate
from tools.configure.providers import maintained
from tools.configure.tests.native_fixture import native_fixture


class I2cTargetConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="i2c target ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.board = self.directory / "board"
        self.board.mkdir()
        board = json.loads((gate.ROOT /
            "tests/contracts/stm32_assembly/ve_board/board.json").read_text())
        board["bindings"] += [
            {"id": "target0", "controller": "I2C1",
             "route": "target-i2c1-pb6-pb7"},
            {"id": "target-alert", "controller": "I2C1",
             "route": "target-i2c1-pb6-pb7-pb5"},
            {"id": "target2", "controller": "I2C2",
             "route": "target-i2c2-pb10-pb11"},
            {"id": "target3", "controller": "I2C3",
             "route": "target-i2c3-pa8-pc9"}]
        (self.board / "board.json").write_text(json.dumps(board))

    def text(self, mode="target-irq", binding="target0", options=""):
        return (f'schema = 2\nboard_package = "{self.board}"\n'
                'backend = "baremetal"\nclock = "hse8-pll168"\n'
                '[memory]\nmain_stack_bytes = 2048\nlibc_heap_bytes = 0\n'
                '[i2c_target.target]\n'
                f'binding = "{binding}"\nmode = "{mode}"\n'
                'own_address = 82\naddress_bits = 7\nsecond_address = 0\n'
                'max_hz = 100000\nirq_latency_us = 4\n'
                'pec = false\nalert = false\n' + options +
                '[i2c_target.target.irq]\npriority = 5\ncalls_os = false\n')

    def resolve(self, text=None):
        path = self.directory / "assembly.toml"
        path.write_text(self.text() if text is None else text)
        return gate.resolve_ir(path, test_support=native_fixture(path))

    def test_target_and_smbus_have_immutable_named_options_and_real_clock(self):
        for mode in ("target-irq", "smbus-device"):
            with self.subTest(mode=mode):
                result = self.resolve(self.text(mode))
                item, = result.controllers
                self.assertEqual(("i2c_target", "I2C1", mode),
                                 (item.kind, item.controller, item.mode))
                self.assertEqual("I2cTargetOptions", type(item.options).__name__)
                self.assertEqual(82, item.options.own_address)
                self.assertEqual(42000000, item.integration.clock_hz)
                self.assertEqual("i2c", item.integration.hardware.kind)
                with self.assertRaises(FrozenInstanceError):
                    item.options.own_address = 83

    def test_nondefault_profile_projects_the_same_apb_into_c_configuration(self):
        from tools.configure.stm32f4.clocks import DEFAULT
        parameters = {**DEFAULT, "pll_p": 4}
        choices = "\n[clock_parameters]\n" + "\n".join(
            name + " = " + (str(value).lower() if type(value) is bool else
                            '"' + value + '"' if isinstance(value, str) else
                            str(value)) for name, value in parameters.items()) + "\n"
        result = self.resolve(self.text().replace('clock = "hse8-pll168"',
                                                  'clock = "custom"') + choices)
        self.assertEqual(21000000, result.controllers[0].integration.clock_hz)
        self.assertIn(".clock_hz = 21000000u", bindings.emit(result)[1])

    def test_constructor_has_direct_c_admit_prepare_close_and_no_algorithm(self):
        result = self.resolve()
        item, = result.controllers
        construction = maintained("stm32f4").constructor(
            item, result, result.board_bindings, result.routes, result.devices)
        self.assertTrue(construction.retain_on_failed_init)
        self.assertEqual("nx_stm32_i2c_target_admit",
                         construction.admission[0].function)
        self.assertEqual("nx_stm32_i2c_target_prepare",
                         construction.initialize.function)
        self.assertEqual("nx_stm32_i2c_target_close",
                         construction.stop[-1].function)
        self.assertEqual({"PB6", "PB7"}, set(construction.pin_snapshots))
        source = bindings.emit(result)[1]
        self.assertIn(".clock_hz = 42000000u", source)
        self.assertIn(".own_address = 82u", source)
        self.assertIn("nx_stm32_i2c_target_irq(&s_nx_port_target)", source)
        for forbidden in ("I2C1->", "NVIC_", "HAL_I2C_", "while ("):
            self.assertNotIn(forbidden, source)

    def test_factory_lookup_is_typed_and_has_no_start_effect(self):
        header, source = factory.emit(self.resolve())
        self.assertIn('"nexus/io/i2c_target.h"', header)
        self.assertIn("NX_I2C_TARGET_ID_TARGET", header)
        self.assertIn("const nx_i2c_target_port_t*", header)
        self.assertIn("return nx_binding_target;", source)
        for forbidden in ("prepare(", "RCC->", "HAL_", "calloc"):
            self.assertNotIn(forbidden, source)

    def test_selected_contract_preserves_request_and_direction_byte_limits(self):
        from tools.configure.selected_contracts import describe
        for mode in ("target-irq", "smbus-device"):
            with self.subTest(mode=mode):
                item, = self.resolve(self.text(mode)).controllers
                record = describe(item)
                self.assertEqual(("bytes", 1, (8,)),
                                 (record.unit, record.unit_bytes, record.word_bits))
                self.assertEqual((512, 256, 2, 512),
                                 (record.max_transfer_units,
                                  record.max_segment_units,
                                  record.max_segments, record.max_total_units))
                self.assertIsNone(record.max_messages)
                self.assertIn("IRQ", record.features)
                self.assertFalse(any("DMA" in name for name in record.features))

    def test_checked_in_software_fixtures_keep_exact_parts_and_explicit_modes(self):
        directory = gate.ROOT / "tests/contracts/i2c_target"
        for name in ("ve-target", "ve-target-84", "ve-smbus",
                     "zg-target", "zg-smbus"):
            path = directory / (name + ".toml")
            with self.subTest(fixture=name):
                result = gate.resolve_ir(path, test_support=native_fixture(path))
                item, = result.controllers
                self.assertEqual("i2c_target", item.kind)
                self.assertEqual("smbus-device" if "smbus" in name else
                                  "target-irq", item.mode)
                self.assertEqual("STM32F407VET6" if name.startswith("ve") else
                                  "STM32F407ZGT6", result.part)

    def test_all_three_exact_controllers_and_ten_bit_own_address(self):
        for binding, instance in (("target0", "I2C1"),
                                  ("target2", "I2C2"),
                                  ("target3", "I2C3")):
            text = self.text(binding=binding).replace("address_bits = 7",
                "address_bits = 10").replace("own_address = 82",
                                             "own_address = 683")
            with self.subTest(instance=instance):
                item, = self.resolve(text).controllers
                self.assertEqual(instance, item.controller)
                self.assertEqual(683, item.options.own_address)

    def test_alert_requires_exact_board_smba_route_and_smbus(self):
        text = self.text("smbus-device", "target-alert").replace(
            "alert = false", "alert = true")
        result = self.resolve(text)
        self.assertEqual(("PB6", "PB7", "PB5"),
                         tuple(pin["pin"] for pin in result.controllers[0].pins))
        for changed in (text.replace('"target-alert"', '"target0"'),
                        text.replace('"smbus-device"', '"target-irq"')):
            with self.subTest(text=changed), self.assertRaisesRegex(
                    gate.ConfigurationError, "SMBA|alert|SMBus|SMBUS"):
                self.resolve(changed)

    def test_foreign_master_binding_does_not_silently_select_target_personality(self):
        with self.assertRaisesRegex(gate.ConfigurationError, "kind|route|binding"):
            self.resolve(self.text(binding="i2c0"))

    def test_same_physical_master_and_target_cannot_have_two_owners(self):
        text = self.text() + ('[i2c.master]\nbinding = "i2c0"\n'
            'mode = "poll-messages"\nmax_hz = 100000\n'
            '[i2c_device.sensor]\ncontroller = "master"\naddress = 80\n')
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "Resource conflict controller:I2C1"):
            self.resolve(text)

    def test_smbus_pec_is_explicit_and_conservative_latency_is_checked(self):
        text = self.text("smbus-device").replace("pec = false", "pec = true")
        self.assertTrue(self.resolve(text).controllers[0].options.pec)
        for changed in (text.replace("irq_latency_us = 4", "irq_latency_us = 60"),
                        text.replace("irq_latency_us = 4", "irq_latency_us = 0"),
                        text.replace("max_hz = 100000", "max_hz = 400000"),
                        text.replace('"smbus-device"', '"target-irq"')):
            with self.subTest(text=changed), self.assertRaisesRegex(
                    gate.ConfigurationError, "PEC|pec|latency|SMBus|SMBUS"):
                self.resolve(changed)

    def test_unsupported_address_combinations_and_os_irq_are_rejected(self):
        for old, new in (("own_address = 82", "own_address = 0"),
                         ("address_bits = 7", "address_bits = 8"),
                         ("second_address = 0", "second_address = 82"),
                         ("calls_os = false", "calls_os = true")):
            with self.subTest(new=new), self.assertRaises(gate.ConfigurationError):
                self.resolve(self.text().replace(old, new))
        ten = self.text().replace("address_bits = 7", "address_bits = 10")
        for text in (ten.replace("second_address = 0", "second_address = 89"),
                     ten.replace('"target-irq"', '"smbus-device"')):
            with self.assertRaisesRegex(gate.ConfigurationError, "10|ten|SMBus"):
                self.resolve(text)

    def test_explicit_fields_and_boolean_types_do_not_get_fallbacks(self):
        for line in ("own_address = 82\n", "address_bits = 7\n",
                     "second_address = 0\n", "max_hz = 100000\n",
                     "irq_latency_us = 4\n", "pec = false\n", "alert = false\n"):
            with self.subTest(line=line), self.assertRaisesRegex(
                    gate.ConfigurationError, "explicit|requires"):
                self.resolve(self.text().replace(line, ""))
        for field in ("pec", "alert"):
            with self.assertRaisesRegex(gate.ConfigurationError, field):
                self.resolve(self.text().replace(field + " = false",
                                                 field + " = 0"))

    def test_editor_schema_matches_finite_typed_options(self):
        schema = authored.editor_schema()["properties"]["i2c_target"]["additionalProperties"]
        self.assertEqual("boolean", schema["properties"]["pec"]["type"])
        self.assertEqual([7, 10], schema["properties"]["address_bits"]["enum"])
        self.assertEqual(1023, schema["properties"]["own_address"]["maximum"])
        self.assertFalse(schema["additionalProperties"])

    def test_provider_rejects_route_bytes_that_disagree_with_c_af4_facts(self):
        import copy
        route = next(row for row in json.loads((gate.ROOT /
            "soc/stm32/stm32f4/routes.json").read_text())["routes"]
            if row["id"] == "target-i2c1-pb6-pb7")
        for field, value in (("af", 7), ("function", "sda"), ("pin", "PA6")):
            changed = copy.deepcopy(route)
            changed["pins"][0][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(
                    gate.ConfigurationError, "route|AF4|pad"):
                maintained("stm32f4").validate_selection(
                    {"mode": "target-irq"}, changed)


if __name__ == "__main__":
    unittest.main()
