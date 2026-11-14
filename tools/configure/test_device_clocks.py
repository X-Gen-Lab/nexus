"""The selected named policy gates fixed and custom clock paths once."""
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure as gate
from tools.configure.providers import maintained
from tools.configure.stm32f4 import clocks

HSI = dict(source='hsi', input_hz=16000000, sysclk_source='input',
           pll48=False, pll_m=16, pll_n=336, pll_p=4, pll_q=7,
           ahb_div=1, apb1_div=1, apb2_div=1, adc_div=4,
           supply_mv=3300, css=False)


class DeviceClockPolicyTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for name in ('dependencies/platform-compatibility.json',
                     'soc/stm32/stm32f4/soc.json',
                     'soc/stm32/stm32f4/devices.json',
                     'soc/stm32/stm32f4/routes.json',
                     'soc/stm32/stm32f4/references.json',
                     'tests/contracts/stm32f4_family/f411re-board/board.json',
                     'tests/contracts/stm32f4_family/f446ze-board/board.json',
                     'tests/contracts/stm32f4_family/f479ni-board/board.json'):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(gate.ROOT / name, target)
        self.path = self.root / 'assembly.toml'

    def resolve(self, parameters=None, *, board="f411re"):
        text = ('schema=2\nboard_package="tests/contracts/stm32f4_family/' + board + '-board"\n'
                'backend="baremetal"\nclock="' +
                ('hse8-pll168' if parameters is None else 'custom') + '"\n')
        if parameters is not None:
            text += '[clock_parameters]\n'
            for key, value in parameters.items():
                encoded = str(value).lower() if type(value) is bool else json.dumps(value)
                text += key + '=' + encoded + '\n'
        text += '[memory]\nmain_stack_bytes=2048\n'
        self.path.write_text(text)
        with patch.object(gate, 'dependency_identity',
                          side_effect=lambda root, name, expected=None:
                          dict(path=name, commit=expected)):
            return gate.resolve_ir(self.path, self.root)

    def test_exact_hsi16_input_plan_is_admitted(self):
        result = self.resolve(HSI)
        self.assertEqual(result.clock['core_hz'], 16000000)
        self.assertFalse(result.clock_plan.parameters['pll_enabled'])

    def test_family_named_profile_cannot_override_the_selected_policy(self):
        with self.assertRaisesRegex(ValueError, 'clock policy'):
            self.resolve()

    def test_custom_parameters_cannot_bypass_the_selected_policy(self):
        cases = [dict(HSI, sysclk_source='pll', pll_p=4, apb1_div=2),
                 dict(HSI, apb1_div=2), dict(HSI, ahb_div=2),
                 dict(HSI, pll48=True),
                 dict(HSI, source='hse', input_hz=8000000, pll_m=8)]
        for parameters in cases:
            with self.subTest(parameters=parameters), self.assertRaisesRegex(
                    ValueError, 'clock policy'):
                self.resolve(parameters)

    def test_declared_classic_policy_cannot_launder_the_exact_initial_review(self):
        path = self.root / 'soc/stm32/stm32f4/soc.json'
        value = json.loads(path.read_text())
        value['device_profiles']['stm32f411xe']['clock_policy'] = 'rm0090-run'
        path.write_text(json.dumps(value))
        with self.assertRaisesRegex(ValueError, 'reviewed clock policy'):
            self.resolve(dict(HSI, sysclk_source='pll', pll_p=4, apb1_div=2))

    def test_missing_or_unknown_policy_is_not_a_classic_default(self):
        path = self.root / 'soc/stm32/stm32f4/soc.json'
        original = path.read_text()
        for name in (None, 'nearby-f4-clock'):
            data = json.loads(original)
            profile = data['device_profiles']['stm32f411xe']
            if name is None:
                profile.pop('clock_policy')
            else:
                profile['clock_policy'] = name
            path.write_text(json.dumps(data))
            with self.subTest(policy=name), self.assertRaisesRegex(ValueError,
                                                                    'clock policy'):
                self.resolve(HSI)

    def test_auxiliary_policy_is_bound_to_the_exact_independent_review(self):
        path = self.root / 'soc/stm32/stm32f4/soc.json'
        original = path.read_text()
        for board, device, correct, foreign in (
                ("f446ze", "stm32f446", "rm0390-run", "rm0386-run"),
                ("f479ni", "stm32f479", "rm0386-run", "rm0390-run")):
            data = json.loads(original)
            data['device_profiles'][device]['clock_policy'] = correct
            path.write_text(json.dumps(data))
            with self.subTest(device=device, policy=correct):
                self.assertEqual(self.resolve(HSI, board=board).clock['core_hz'], 16000000)
            data['device_profiles'][device]['clock_policy'] = foreign
            path.write_text(json.dumps(data))
            with self.subTest(device=device, policy=foreign), self.assertRaisesRegex(
                    ValueError, 'reviewed clock policy'):
                self.resolve(HSI, board=board)


class SelectedAuxiliaryPolicyTests(unittest.TestCase):
    def admit(self, name, *, shared=False, **changes):
        parameters = {**HSI, **changes}
        if shared:
            parameters.update(pll_m=10, pll_n=192, pll_q=5)
        parameters.setdefault("pll_sai", dict(
            m=10, n=192, sai1=dict(q=5, post_divider=5)))
        plan = clocks.plan("custom", parameters)
        maintained("stm32f4").validate_clock_policy(name, plan)
        return plan

    def test_exact_policies_admit_their_legal_independent_or_shared_input(self):
        independent = self.admit("rm0390-run")
        self.assertEqual(independent["parameters"]["pll_m"], 16)
        self.assertEqual(independent["pll_sai"]["m"], 10)
        shared = self.admit("rm0386-run", shared=True)
        self.assertEqual(shared["parameters"]["pll_m"], shared["pll_sai"]["m"])

    def test_f479_requires_shared_m_and_the_exact_datasheet_vco_minimum(self):
        with self.assertRaisesRegex(ValueError, "shared.*PLLM"):
            self.admit("rm0386-run")
        low = dict(m=16, n=112, sai1=dict(q=7, post_divider=1))
        with self.assertRaisesRegex(ValueError, "192 MHz"):
            self.admit("rm0386-run", pll_sai=low)
        # This actual 112 MHz VCO belongs to F446's separate electrical range.
        self.assertEqual(self.admit("rm0390-run", pll_sai=low)
                         ["pll_sai"]["n"], 112)

    def test_f446_does_not_inherit_the_f479_ltdc_r_output(self):
        with self.assertRaisesRegex(ValueError, "PLLSAI R"):
            self.admit("rm0390-run", pll_sai=dict(
                m=10, n=192, ltdc=dict(r=4, post_divider=16)))

    def test_old_and_hsi_only_devices_cannot_silently_take_an_auxiliary_pll(self):
        for name in ("rm0090-run", "rm0383-run"):
            with self.subTest(policy=name), self.assertRaisesRegex(
                    ValueError, "clock policy.*PLLSAI"):
                self.admit(name)


class SelectedHighSpeedClockTests(unittest.TestCase):
    def selected(self, policy, **changes):
        parameters = dict(HSI, sysclk_source="pll", **changes)
        result = clocks.plan("custom", parameters)
        maintained("stm32f4").validate_clock_policy(policy, result)
        return result

    def test_f411_hsi96_has_independent_legal_bus_and_48mhz_domains(self):
        result = self.selected("rm0383-run", pll_n=384, pll_p=4, pll_q=8,
                               apb1_div=2, apb2_div=1)
        hz = {key: value["nominal_hz"]
              for key, value in result["domains"].items()}
        self.assertEqual((hz["core"], hz["apb1"], hz["apb2"], hz["pll48"]),
                         (96000000, 48000000, 96000000, 48000000))
        self.assertEqual(result["parameters"]["flash_wait_states"], 3)

    def test_f446_and_f479_hsi168_do_not_require_overdrive(self):
        for policy in ("rm0390-run", "rm0386-run"):
            with self.subTest(policy=policy):
                result = self.selected(policy, pll_p=2, apb1_div=4,
                                       apb2_div=2)
                self.assertEqual(result["domains"]["core"]["nominal_hz"],
                                 168000000)
                self.assertEqual(result["domains"]["pll48"]["nominal_hz"],
                                 48000000)
                self.assertEqual(result["parameters"]["flash_wait_states"], 5)

    def test_high_profile_does_not_relabel_hsi_as_a_usb_crystal(self):
        result = self.selected("rm0386-run", pll_p=2, apb1_div=4, apb2_div=2)
        from types import SimpleNamespace
        item = SimpleNamespace(kind="usb_device", mode="device-fs",
                               controller="OTG_FS")
        with self.assertRaisesRegex(ValueError, "external HSE crystal"):
            clocks.validate_consumers(result, [item])

    def test_f479_hsi168_shares_nonunit_input_with_audio_and_ltdc(self):
        auxiliary = dict(m=10, n=192, sai1=dict(q=5, post_divider=5),
                         ltdc=dict(r=4, post_divider=16))
        result = self.selected("rm0386-run", pll_m=10, pll_n=210,
                               pll_p=2, apb1_div=4, apb2_div=2,
                               pll_sai=auxiliary)
        self.assertEqual(result["domains"]["core"]["nominal_hz"], 168000000)
        self.assertEqual(result["domains"]["pll48"]["nominal_hz"], 48000000)
        self.assertEqual(result["pll_sai"]["sai1"]["kernel_hz"], 12288000)
        self.assertEqual(result["pll_sai"]["ltdc"]["pixel_hz"], 4800000)
        from types import SimpleNamespace
        clocks.validate_consumers(result, (
            SimpleNamespace(kind="audio", controller="SAI1", mode="master-tx"),
            SimpleNamespace(kind="display", controller="LTDC", mode="rgb-scanout")))

    def test_unreviewed_tuple_is_not_admitted_by_a_nearby_device_policy(self):
        for policy in ("rm0383-run", "rm0390-run", "rm0386-run"):
            with self.subTest(policy=policy), self.assertRaisesRegex(
                    ValueError, "clock policy"):
                self.selected(policy, pll_p=4, apb1_div=2, apb2_div=1)

    def test_classic_policy_does_not_inherit_f411_apb_limits(self):
        with self.assertRaisesRegex(ValueError, "clock policy"):
            self.selected("rm0090-run", pll_n=384, pll_p=4, pll_q=8,
                          apb1_div=2, apb2_div=1)

    def test_maintained_crc_consumers_exercise_the_reviewed_high_profiles(self):
        for stem, hz in (("f411re", 96000000), ("f446ze", 168000000),
                         ("f479ni", 168000000)):
            with self.subTest(assembly=stem):
                result = gate.resolve_ir(gate.ROOT /
                    f"tests/contracts/stm32f4_family/{stem}.toml")
                self.assertEqual(result.clock_plan.domains["core"].nominal_hz,
                                 hz)
                self.assertEqual(result.clock_plan.domains["pll48"].nominal_hz,
                                 48000000)
                self.assertFalse(result.physical_qualified)


if __name__ == '__main__':
    unittest.main(verbosity=2)
