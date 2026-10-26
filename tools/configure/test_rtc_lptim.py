"""Ordinary typed controllers assemble only the explicit Native time models."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import authored
from tools.configure import bindings
from tools.configure import configure as gate
from tools.configure import factory
from tools.configure import selected_contracts
from tools.configure.providers import maintained


class NativeTimeAssemblyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.assembly = self.directory / "assembly.toml"
        self.verification = gate.ROOT / "cmake/package/verification/io_evolution"
        self.pair = (self.verification / "native-time-pair.toml").read_text()
        self.empty = (self.verification / "native-time-empty.toml").read_text()

    def resolve(self, text=None):
        self.assembly.write_text(self.pair if text is None else text)
        return gate.resolve_ir(self.assembly)

    def test_pair_uses_existing_immutable_controller_ir_and_claims(self):
        result = self.resolve()
        self.assertEqual(len(result.controllers), 4)
        records = {item.id: item for item in result.controllers}
        self.assertEqual({item.controller for item in records.values()},
                         {"RTC0", "RTC1", "LPTIM0", "LPTIM1"})
        self.assertEqual(records["counter_a"].options.tick_hz, 32768)
        self.assertEqual(records["counter_b"].options.tick_hz, 1000)
        self.assertEqual(type(records["calendar_a"].options).__name__, "NoOptions")
        with self.assertRaises(FrozenInstanceError):
            records["counter_a"].options.tick_hz = 7
        claims = [entry for entry in result["claims"]
                  if entry["owner"] in records]
        self.assertEqual({entry["key"] for entry in claims},
                         {"controller:RTC0", "controller:RTC1",
                          "controller:LPTIM0", "controller:LPTIM1"})
        for item in result.controllers:
            self.assertEqual(tuple(item["pins"]), ())
            self.assertNotIn("irq_priority", item)
            self.assertNotIn("dma", item)

    def test_existing_lifecycle_emits_construct_or_explicit_reopen(self):
        result = self.resolve()
        _, source = bindings.emit(result)
        self.assertEqual(source.count("static const nx_clock_t s_nx_native_time_clock"), 1)
        self.assertIn("nx_time_now_us()", source)
        for kind, name in [("rtc", "calendar_a"), ("rtc", "calendar_b"),
                           ("lptim", "counter_a"), ("lptim", "counter_b")]:
            self.assertIn("s_nx_port_" + name + ".constructed", source)
            self.assertIn("nx_native_" + kind + "_start(&s_nx_port_" + name, source)
            self.assertIn("nx_native_" + kind + "_construct(&s_nx_port_" + name, source)
            self.assertIn("nx_" + kind + "_port_stop(&s_nx_face_" + name, source)
        self.assertNotIn("memset(", source)
        self.assertNotIn("NativeSystem", source)
        self.assertNotIn("registry", source)

    def test_factory_is_pure_typed_and_keeps_compact_class_local_ids(self):
        header, source = factory.emit(self.resolve())
        self.assertIn("NX_RTC_ID_COUNT = 2", header)
        self.assertIn("NX_LPTIM_ID_COUNT = 2", header)
        self.assertIn("const nx_rtc_port_t* nx_factory_rtc", header)
        self.assertIn("const nx_lptim_port_t* nx_factory_lptim", header)
        self.assertIn("return nx_binding_calendar_a;", source)
        self.assertIn("return nx_binding_counter_b;", source)
        self.assertNotIn("nx_native_rtc_construct(", source)
        self.assertNotIn("nx_native_lptim_start(", source)
        self.assertNotIn("nx_time_now_us(", source)

    def test_selected_units_are_occurrences_and_ticks_without_fake_byte_width(self):
        records = {entry.id: entry for entry in
                   selected_contracts.for_configuration(self.resolve())}
        for name, unit, bound in [("calendar_a", "occurrences", 1),
                                  ("calendar_b", "occurrences", 1),
                                  ("counter_a", "ticks", 65536),
                                  ("counter_b", "ticks", 65536)]:
            self.assertEqual(records[name].unit, unit)
            self.assertIsNone(records[name].unit_bytes)
            self.assertEqual(records[name].max_transfer_units, bound)
            self.assertEqual(records[name].features, ("MODEL",))
            self.assertFalse(records[name].physical_qualified)
        header, source = factory.emit(self.resolve())
        self.assertIn("no fixed byte width", header)
        self.assertIn("NX_FACTORY_CAP_MODEL, 65536U, 0U", source)

    def test_empty_keeps_only_zero_count_stubs_and_no_selected_clock(self):
        result = self.resolve(self.empty)
        header, factory_source = factory.emit(result)
        _, source = bindings.emit(result)
        self.assertIn("NX_RTC_ID_COUNT = 0", header)
        self.assertIn("NX_LPTIM_ID_COUNT = 0", header)
        for forbidden in ["nx_native_rtc_ops", "nx_native_lptim_ops",
                          "nx_native_time_clock", "nx_time_now_us()",
                          "nx_native_rtc_state_t", "nx_native_lptim_state_t"]:
            self.assertNotIn(forbidden, source + factory_source)

    def test_bundle_is_deterministic_and_binds_host_facts_and_cold_sources(self):
        self.assembly.write_text(self.pair)
        output = self.directory / "bundle"
        gate.configure(self.assembly, output)
        first = {p.name: p.read_bytes() for p in output.iterdir()}
        gate.configure(self.assembly, output)
        self.assertEqual(first, {p.name: p.read_bytes() for p in output.iterdir()})
        self.assertIn("RTC0", (output / "resolved.json").read_text())
        paths = json.loads((output / "input_paths.json").read_text())
        self.assertIn(str(gate.ROOT / "soc/native/soc.json"), paths.values())
        self.assertIn(str(gate.ROOT / "boards/native_reference/board.json"), paths.values())

    def test_editor_schema_matches_its_authoritative_generator(self):
        schema = authored.editor_schema()
        self.assertIn("rtc", schema["properties"])
        self.assertIn("lptim", schema["properties"])
        self.assertEqual(set(schema["properties"]["rtc"]["additionalProperties"]["properties"]),
                         {"binding", "mode"})
        self.assertIn("tick_hz", schema["properties"]["lptim"]["additionalProperties"]["required"])
        self.assertEqual(json.loads((gate.ROOT / "tools/configure/assembly.schema.json").read_text()), schema)

    def test_lptim_tick_frequency_is_explicit_positive_uint32(self):
        for value in ["0", "-1", "true", "4294967296", '"32768"']:
            with self.subTest(value=value):
                with self.assertRaisesRegex(gate.ConfigurationError, "tick_hz"):
                    self.resolve(self.pair.replace("tick_hz = 32768", "tick_hz = " + value))
        with self.assertRaisesRegex(gate.ConfigurationError, "tick_hz"):
            self.resolve(self.pair.replace("tick_hz = 32768\n", ""))

    def test_unknown_time_policy_and_irq_fields_are_not_accepted(self):
        for field in ['calendar = "2026-01-01"', 'timezone = "UTC"',
                      'backup_reset = true', 'irq = { priority = 1 }']:
            with self.subTest(field=field):
                with self.assertRaisesRegex(gate.ConfigurationError, field.split()[0]):
                    self.resolve(self.pair.replace('[rtc.calendar_a]', '[rtc.calendar_a]\n' + field))

    def test_duplicate_real_controller_is_a_resource_conflict(self):
        with self.assertRaisesRegex(gate.ConfigurationError, "Resource conflict controller:RTC0"):
            self.resolve(self.pair.replace('binding = "rtc1"', 'binding = "rtc0"'))

    def test_authored_kind_cannot_substitute_another_controller_kind(self):
        with self.assertRaisesRegex(gate.ConfigurationError, "Authored kind differs"):
            self.resolve(self.pair.replace('binding = "rtc0"', 'binding = "lptim0"'))

    def test_physical_providers_do_not_inherit_host_modes(self):
        for family in ["stm32h743", "stm32u575", "stm32f407", "gd32f470"]:
            for kind, mode in [("rtc", "calendar-alarm"),
                               ("lptim", "counter16-oneshot")]:
                with self.subTest(family=family, kind=kind):
                    with self.assertRaises(gate.ConfigurationError):
                        maintained(family).mode_implementation(kind, mode)

    def test_wrong_mode_is_rejected_without_replacing_an_existing_bundle(self):
        self.assembly.write_text(self.pair)
        output = self.directory / "bundle"
        gate.configure(self.assembly, output)
        self.assembly.write_text(self.pair.replace('mode = "calendar-alarm"',
                                                  'mode = "irq-alarm"'))
        with self.assertRaisesRegex(gate.ConfigurationError, "Unsupported mode"):
            gate.configure(self.assembly, output)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
