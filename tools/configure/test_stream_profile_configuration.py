"""Selected stream profiles bind real TOML budgets to typed cold start faces."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import re
import json
import tempfile
import unittest

from tools.configure import bindings, configure, factory


class StreamProfileConfiguration(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def resolve(self, kind="uart", family="stm32f407", **changes):
        fixtures = {
            ("uart", "stm32f407"): "stm32_assembly/zg-rx-dma-continuous.toml",
            ("uart", "gd32f470"): "gd32_assembly/rx-dma-continuous.toml",
            ("adc", "stm32f407"): "stm32_assembly/zg-adc-continuous.toml",
            ("adc", "gd32f470"): "gd32_assembly/adc-continuous.toml",
        }
        path = configure.ROOT / "tests/contracts" / fixtures[(kind, family)]
        contents = path.read_text().replace("baud = 115200", "baud = 9600")
        match = re.search(r'(?m)^board_package\s*=\s*"([^"]+)"', contents)
        board = (path.parent / match[1]).resolve()
        contents = contents[:match.start()] + (
            'board_package = "' + str(board) + '"') + contents[match.end():]
        name = re.search(r"(?m)^\[" + kind + r"\.([^].]+)\]$", contents)[1]
        values = dict(max_units_per_second=1000, units_per_block=100,
                      blocks=8, release_delay_us=500000,
                      target_delay_us=99999,
                      on_exhaustion="stop-and-report-gap",
                      source="declared software test envelope; not measured")
        values.update(changes)
        contents += f"\n[{kind}.{name}.stream]\n"
        for key, value in values.items():
            literal = ('"' + value + '"' if isinstance(value, str) else
                       "true" if value is True else "false" if value is False
                       else str(value))
            contents += key + " = " + literal + "\n"
        assembly = self.root / "stream.toml"
        assembly.write_text(contents)
        return configure.resolve_ir(assembly)

    def test_uart_continuous_derives_two_targets_and_one_replacement(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                result = self.resolve(family=family)
                profile = result.controllers[0].options.stream_profile
                self.assertEqual(profile.hardware_targets, 2)
                self.assertEqual(profile.spare_blocks, 1)
                self.assertEqual(profile.unit_bytes, 1)
                self.assertEqual(profile.required_blocks, 8)
                self.assertEqual(profile.required_payload_bytes, 800)
                self.assertEqual(profile.block_fill_us, 100000)
                self.assertEqual(profile.provenance,
                                 "authored_declared_assumptions")
                self.assertFalse(profile.physical_qualified)
                with self.assertRaises(FrozenInstanceError):
                    profile.hardware_targets = 1

    def test_adc_uses_complete_selected_scans_without_resolution_alias(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                result = self.resolve(kind="adc", family=family)
                profile = result.controllers[0].options.stream_profile
                self.assertEqual(profile.unit_bytes, 4)
                self.assertEqual(profile.required_payload_bytes, 3200)
                self.assertEqual(profile.counter_units_per_block, 200)
                self.assertEqual(profile.producer_control,
                                 "trigger-stop-with-gap")

    def test_budget_latency_is_strict_and_capacity_is_not_clamped(self):
        for change in (dict(blocks=7), dict(target_delay_us=100000),
                       dict(units_per_block=1,
                            max_units_per_second=1000001,
                            target_delay_us=0)):
            with self.subTest(change=change), self.assertRaisesRegex(
                    ValueError, "budget|latency|capacity|fill"):
                self.resolve(**change)

    def test_derived_hardware_and_storage_facts_cannot_be_authored(self):
        for change in (dict(hardware_targets=1), dict(spare_blocks=0),
                       dict(unit_bytes=1), dict(physical_qualified=True)):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.resolve(**change)

    def test_unknown_loss_or_backpressure_policy_has_no_silent_fallback(self):
        for policy in ("lossless", "rts-cts", "pause-source", "drop-oldest"):
            with self.subTest(policy=policy), self.assertRaisesRegex(
                    ValueError, "policy|exhaustion|gap"):
                self.resolve(on_exhaustion=policy)

    def test_rate_cannot_understate_selected_uart_nominal_frame_rate(self):
        with self.assertRaisesRegex(ValueError, "rate|nominal"):
            self.resolve(max_units_per_second=959)

    def test_counters_and_target_abi_extents_are_checked(self):
        for change in (dict(max_units_per_second=0), dict(blocks=True),
                       dict(max_units_per_second=(1 << 64) - 1,
                            release_delay_us=2),
                       dict(units_per_block=65536),
                       dict(release_delay_us=(1 << 64) - 1), dict(source="")):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.resolve(**change)
        with self.assertRaisesRegex(ValueError, "counter|DMA|scan"):
            self.resolve(kind="adc", units_per_block=32768)

    def test_readonly_profile_binds_actual_face_and_independent_factory(self):
        for kind in ("uart", "adc"):
            with self.subTest(kind=kind):
                result = self.resolve(kind=kind)
                header, getters = factory.emit(result)
                self.assertIn(f"const nx_{kind}_stream_profile_t*", header)
                self.assertIn(f"nx_factory_{kind}_stream_profile(", header)
                self.assertIn("s_nx_stream_profile_", getters)
                _, generated = bindings.emit(result)
                self.assertIn(f"static const nx_{kind}_stream_profile_t",
                              generated)
                self.assertIn(".port = &s_nx_face_", generated)
                self.assertNotIn("nx_stream_initialize(", generated)

    def test_unselected_profiles_add_no_firmware_object_or_factory(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/stm32_assembly/zg-rx-dma-continuous.toml")
        self.assertIsNone(result.controllers[0].options.stream_profile)
        self.assertNotIn("s_nx_stream_profile_", bindings.emit(result)[1])
        self.assertNotIn("nx_factory_uart_stream_profile(",
                         factory.emit(result)[0])

    def test_persisted_profiles_retain_selected_typed_face_and_cold_helper(self):
        presets = json.loads((configure.ROOT / "CMakePresets.json").read_text())
        configured = {item["name"]: item for item in presets["configurePresets"]}
        built = {item["name"]: item for item in presets["buildPresets"]}
        directory = configure.ROOT / "tests/contracts/io_evolution_assembly"
        main = (directory / "controller_main.c").read_text()
        for family in ("f407", "gd470"):
            for kind in ("uart", "adc"):
                name = f"{family}-{kind}-stream-profile"
                with self.subTest(fixture=name):
                    path = directory / (name + ".toml")
                    self.assertTrue(path.is_file(), str(path))
                    result = configure.resolve_ir(path)
                    self.assertIsNotNone(
                        result.controllers[0].options.stream_profile)
                    header, _ = factory.emit(result)
                    self.assertIn(f"nx_factory_{kind}_stream_profile(", header)
                    preset = "fixture-io-evolution-" + name
                    self.assertIn(preset, configured)
                    self.assertEqual(built[preset]["configurePreset"], preset)
                    self.assertEqual(configured[preset]["cacheVariables"][
                        "NEXUS_IO_EVOLUTION_FIXTURE_DEFINITIONS"],
                        f"NX_FIXTURE_{kind.upper()}_STREAM_PROFILE")
                    self.assertIn(f"nx_factory_{kind}_stream_profile(", main)
                    self.assertIn(f"nx_{kind}_stream_profile_start", main)


if __name__ == "__main__":
    unittest.main()
