"""The selected TIM1 group is a real typed, exclusive cold assembly."""
from pathlib import Path
import copy
import json
import subprocess
import sys
import tempfile
import unittest

from tools.configure import bindings, configure, factory


class G474ControlConfiguration(unittest.TestCase):
    fixture = configure.ROOT / "tests/contracts/g474_control_assembly/pwm-group.toml"

    def resolve(self, before=None, after=None):
        if before is None:
            return configure.resolve_ir(self.fixture)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "assembly.toml"
            contents = self.fixture.read_text().replace(
                'board_package = "board"',
                'board_package = "' + str(self.fixture.parent / "board") + '"')
            path.write_text(contents.replace(before, after))
            return configure.resolve_ir(path)

    def test_real_kind_has_typed_factory_and_exact_resource_claims(self):
        result = self.resolve()
        item = result.controllers[0]
        self.assertEqual(item.kind, "pwm_group")
        self.assertEqual(item.options.deadtime_ticks, 170)
        header, getters = factory.emit(result)
        self.assertIn("const nx_pwm_group_t* nx_factory_pwm_group(", header)
        self.assertIn("NX_PWM_GROUP_ID_WAVE", header)
        self.assertIn("return nx_binding_wave;", getters)
        claims = {claim["key"] for claim in result["claims"]}
        self.assertTrue({"controller:TIM1", "controller:TIM15",
                         "controller:COMP1", "pin:PA1", "pin:PA4",
                         "pin:PA8", "pin:PA9", "pin:PA10"} <= claims)

    def test_cold_provider_owns_comparator_settle_and_failure_cleanup(self):
        _, source = bindings.emit(self.resolve())
        self.assertIn("nx_stm32g474_pwm_group_initialize(", source)
        self.assertIn("nx_stm32g474_pwm_group_close(", source)
        self.assertIn("nx_stm32g474_pwm_group_irq(", source)
        self.assertIn("TIM1_BRK_TIM15_IRQHandler", source)
        self.assertIn("COMP1->CSR", source)
        self.assertIn("NX_G474_SYSTEM_POLL", source)
        self.assertIn("6u", source)
        self.assertNotIn("nx_pwm_group_start(", source)

    def test_bad_geometry_and_implicit_ownership_have_no_fallback(self):
        for before, after in (("deadtime_ticks = 170", "deadtime_ticks = 171"),
                              ("tick_hz = 1000000", "tick_hz = 999999"),
                              ("period_ticks = 1000", "period_ticks = 1"),
                              ("exclusive = true", "exclusive = false"),
                              ("priority = 5", "priority = 4")):
            with self.subTest(after=after), self.assertRaises(ValueError):
                self.resolve(before, after)

    def test_reviewed_personalities_keep_one_physical_tim3_identity(self):
        soc = configure.load(configure.ROOT / "soc/stm32g474/soc.json")
        self.assertEqual(soc["controllers"]["TIM3"]["kind"], "pwm")
        self.assertEqual(soc["controllers"]["TIM3"]["interfaces"],
                         {"encoder": ["quadrature"]})
        result = configure.resolve_ir(self.fixture.parent / "encoder.toml")
        self.assertEqual(result.controllers[0].kind, "encoder")
        self.assertEqual(result.controllers[0].controller, "TIM3")
        self.assertEqual(result.controllers[0].options.counting, "x4-ti12")
        header, _ = factory.emit(result)
        self.assertIn("const nx_encoder_t* nx_factory_encoder(", header)
        self.assertIn("NX_ENCODER_ID_POSITION", header)
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32g474_encoder_initialize(", source)
        self.assertIn("nx_stm32g474_encoder_close(", source)
        self.assertNotIn("TIM3_IRQHandler", source)

    def test_full_width_capture_uses_actual_tim5_face_and_private_release(self):
        result = configure.resolve_ir(self.fixture.parent / "timer-capture.toml")
        self.assertEqual(result.controllers[0].controller, "TIM5")
        self.assertEqual(result.controllers[0].options.tick_hz, 1000000)
        header, _ = factory.emit(result)
        self.assertIn("NX_TIMER_CAPTURE_ID_CAPTURE", header)
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32g474_timer_capture_initialize(", source)
        self.assertIn("nx_stm32g474_timer_capture_release(", source)
        self.assertIn("TIM5_IRQHandler", source)
        self.assertNotIn("nx_timer_capture_start(", source)

    def test_personality_facts_reject_unreviewed_or_duplicate_primary_kind(self):
        original = configure.load(configure.ROOT / "soc/stm32g474/soc.json")
        for interfaces in ({"pwm": ["fixed-pwm"]},
                           {"encoder": ["unsupported"]},
                           {"unknown": ["quadrature"]},
                           {"encoder": []}, ["encoder"]):
            facts = copy.deepcopy(original)
            facts["controllers"]["TIM3"]["interfaces"] = interfaces
            with self.subTest(interfaces=interfaces), self.assertRaises(ValueError):
                configure.validate_soc(facts, "STM32G474RET6")

    def test_actual_cli_emits_each_timer_bundle_for_cmake(self):
        with tempfile.TemporaryDirectory() as temporary:
            for fixture in ("timer-capture", "encoder"):
                with self.subTest(fixture=fixture):
                    output = Path(temporary) / fixture
                    result = subprocess.run([
                        sys.executable, str(configure.ROOT /
                            "tools/configure/configure.py"),
                        "--assembly", str(self.fixture.parent /
                                          (fixture + ".toml")),
                        "--output", str(output)],
                        capture_output=True, text=True, check=False)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertTrue((output / ".nexus-config-bundle").is_file())

    def test_two_public_personalities_cannot_own_the_same_physical_tim3(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "conflict.toml"
            original = (self.fixture.parent / "encoder.toml").read_text()
            original = original.replace('board_package = "encoder_board"',
                'board_package = "' + str(self.fixture.parent / "encoder_board") + '"')
            path.write_text(original + '\n[pwm.load]\nbinding="pwm3"\n'
                'mode="fixed-pwm"\ntick_hz=1000000\nperiod_ticks=1000\nduty_ticks=0\n')
            with self.assertRaisesRegex(ValueError, "Resource conflict controller:TIM3"):
                configure.resolve_ir(path)

    def test_encoder_rejects_unreviewed_input_filter_polarity_or_counting(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "bad-encoder.toml"
            original = (self.fixture.parent / "encoder.toml").read_text().replace(
                'board_package = "encoder_board"',
                'board_package = "' + str(self.fixture.parent / "encoder_board") + '"')
            for before, after in (('counting = "x4-ti12"', 'counting = "x8"'),
                                  ('filter_a = 0', 'filter_a = 16'),
                                  ('invert_b = false', 'invert_b = 0'),
                                  ('exclusive = true', 'exclusive = false')):
                path.write_text(original.replace(before, after))
                with self.subTest(after=after), self.assertRaises(ValueError):
                    configure.resolve_ir(path)

    def test_control_fixtures_have_normal_firmware_presets_and_face_retention(self):
        document = json.loads((configure.ROOT / "CMakePresets.json").read_text())
        configured = {item["name"]: item for item in document["configurePresets"]}
        built = {item["name"]: item for item in document["buildPresets"]}
        main = (configure.ROOT /
            "tests/contracts/io_evolution_assembly/controller_main.c").read_text()
        for fixture, kind in (("pwm-group", "pwm_group"),
                              ("timer-capture", "timer_capture"),
                              ("encoder", "encoder")):
            with self.subTest(fixture=fixture):
                name = "fixture-io-evolution-g474-" + fixture
                self.assertTrue(name in configured, name)
                self.assertEqual(built[name]["configurePreset"], name)
                self.assertTrue(configured[name]["cacheVariables"][
                    "NEXUS_ASSEMBLY_FILE"].endswith("/" + fixture + ".toml"))
                self.assertIn("NX_FIXTURE_RETAIN_FACES(" + kind + ",", main)


if __name__ == "__main__":
    unittest.main()
