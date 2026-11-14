"""RNG is one typed selected controller and a Platform-owned PLL48 consumer."""
from pathlib import Path
import json
import tempfile
import unittest

from tools.configure import configure, factory, bindings, selected_contracts


class EntropyConfiguration(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def resolve(self, *, part="STM32F407VET6", parameters=None, duplicate=False):
        board = configure.load(configure.ROOT /
            "tests/contracts/multi_controller_assembly/stm32-ve-board/board.json")
        board.update(id="rng-software-fixture", soc=part,
                     bindings=[dict(id="rng", controller="RNG", route="rng-internal")])
        if duplicate:
            board["bindings"].append(dict(id="rng-other", controller="RNG",
                                           route="rng-internal"))
        (self.root / "board.json").write_text(json.dumps(board))
        clock = "custom" if parameters else "hse8-pll168"
        contents = ('schema=2\nboard_package="."\nbackend="baremetal"\n'
                    f'clock="{clock}"\noptimization="Os"\n'
                    '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                    '[entropy.random]\nbinding="rng"\nmode="rng-words"\n')
        if duplicate:
            contents += '[entropy.other]\nbinding="rng-other"\nmode="rng-words"\n'
        if parameters:
            contents += '[clock_parameters]\n' + parameters
        path = self.root / "rng.toml"
        path.write_text(contents)
        return configure.resolve_ir(path)

    @staticmethod
    def direct_hsi(*, pll48=True, sysclk="input", q=7):
        return ('source="hsi"\ninput_hz=16000000\npll_m=16\npll_n=336\n'
                f'pll_p=4\npll_q={q}\nahb_div=1\napb1_div=4\napb2_div=2\n'
                'adc_div=4\nsupply_mv=3300\ncss=false\n'
                f'sysclk_source="{sysclk}"\npll48={str(pll48).lower()}\n')

    def test_exact_parts_generate_one_typed_face_and_the_existing_private_owner(self):
        for part in ("STM32F407VET6", "STM32F407ZGT6"):
            with self.subTest(part=part):
                result = self.resolve(part=part)
                item, = result.controllers
                self.assertEqual(item.kind, "entropy")
                self.assertEqual(item.controller, "RNG")
                self.assertEqual(item.pins, ())
                self.assertEqual(item.integration.clock_hz, 48000000)
                header, source = factory.emit(result)
                self.assertIn("nx_factory_entropy", header)
                self.assertIn("NX_ENTROPY_ID_RANDOM", header)
                self.assertIn("nx_binding_random", source)
                generated = bindings.emit(result)[1]
                self.assertIn("nx_stm32f407_entropy_state_t", generated)
                self.assertIn("nx_stm32f407_entropy_ops", generated)
                self.assertIn("nx_stm32f407_entropy_admit", generated)
                self.assertIn("nx_stm32f407_entropy_start", generated)
                self.assertIn("nx_stm32f407_entropy_stop", generated)
                self.assertNotIn("entropy_instance", generated)
                self.assertEqual(selected_contracts.describe(item).unit_bytes, 4)

    def test_direct_input_without_reserved_pll48_rejects_before_generation(self):
        with self.assertRaisesRegex(ValueError, "Entropy.*nominal kernel clock"):
            self.resolve(parameters=self.direct_hsi(pll48=False))

    def test_active_main_pll_with_other_q_frequency_cannot_supply_rng(self):
        with self.assertRaisesRegex(ValueError, "Entropy.*nominal kernel clock"):
            self.resolve(parameters=self.direct_hsi(pll48=False, sysclk="pll", q=8))

    def test_explicit_direct_hsi_pll48_is_a_valid_rng_clock_owner(self):
        result = self.resolve(parameters=self.direct_hsi())
        self.assertEqual(result.controllers[0].integration.clock_hz, 48000000)
        self.assertIn("nx_stm32f407_entropy_start", bindings.emit(result)[1])

    def test_other_family_cannot_silently_select_the_f407_rng(self):
        source = (configure.ROOT /
                  "tests/support/native/assemblies/native-empty.toml").read_text()
        path = self.root / "native.toml"
        path.write_text(source +
                        '\n[entropy.random]\nbinding="uart0"\nmode="rng-words"\n')
        with self.assertRaises(ValueError):
            configure.resolve_ir(path, test_support=True)

    def test_the_unique_hardware_rng_cannot_be_selected_twice(self):
        with self.assertRaises(ValueError):
            self.resolve(duplicate=True)


if __name__ == "__main__":
    unittest.main()
