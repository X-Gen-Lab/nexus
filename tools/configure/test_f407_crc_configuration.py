"""Exact fixed CRC mode, static ownership and nominal factory contracts."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure as gate, factory


class F407CrcConfigurationTests(unittest.TestCase):
    def resolve(self, directory, *, density="ve", mode="crc32-words",
                extra="", duplicate=False):
        fixture = gate.ROOT / "tests/contracts/dac_assembly"
        board = gate.load(fixture / f"{density}_board/board.json")
        board["id"] = f"stm32f407{density}-crc-software-fixture"
        board["bindings"] = [{"id": "checksum", "controller": "CRC",
                              "route": "crc-internal"}]
        if duplicate:
            board["bindings"].append({"id": "other", "controller": "CRC",
                                      "route": "crc-internal"})
        (directory / "board.json").write_text(json.dumps(board))
        source = (
            'schema=2\nboard_package="."\nbackend="baremetal"\n'
            'clock="hse8-pll168"\noptimization="Os"\n'
            '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
            '[crc.signature]\nbinding="checksum"\n'
            f'mode="{mode}"\n' + extra)
        if duplicate:
            source += ('\n[crc.other]\nbinding="other"\n'
                       'mode="crc32-words"\n')
        assembly = directory / "assembly.toml"
        assembly.write_text(source)
        return gate.resolve_ir(assembly)

    def test_crc32_word_mode_resolves_for_both_parts_without_pin_claims(self):
        with tempfile.TemporaryDirectory() as directory:
            for density in ("ve", "zg"):
                with self.subTest(density=density):
                    result = self.resolve(Path(directory), density=density)
                    item = result.controllers[0]
                    self.assertEqual("CRC", item.controller)
                    self.assertEqual("crc32-words", item.mode)
                    self.assertEqual((), item.pins)
                    self.assertEqual((), item.dma)
                    claims = {row.key for row in result.claims
                              if row.owner == "signature"}
                    self.assertEqual({"controller:CRC"}, claims)

    def test_real_factory_has_one_nominal_identity_and_concrete_c_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.resolve(Path(directory))
            header, _ = factory.emit(result)
            source = bindings.emit(result)[1]
            self.assertIn("nx_factory_crc(nx_crc_id_t id)", header)
            self.assertIn("nx_factory_crc_ref(nx_crc_ref_t id)",
                          factory.emit_strong(result))
            self.assertIn("nx_stm32_crc_prepare", source)
            self.assertIn("nx_stm32_crc_close", source)
            self.assertIn("nx_stm32_crc_ops", source)
            self.assertNotIn("LL_CRC_", source)
            self.assertNotIn("RCC->", source)

    def test_unimplemented_polynomial_seed_and_byte_policies_are_not_authored(self):
        with tempfile.TemporaryDirectory() as directory:
            for field in ("polynomial=123\n", "seed=0\n", "reflect=true\n"):
                with self.subTest(field=field):
                    with self.assertRaises(ValueError):
                        self.resolve(Path(directory), extra=field)

    def test_two_instances_cannot_reset_the_same_calculator(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "conflict.*controller:CRC"):
                self.resolve(Path(directory), duplicate=True)


if __name__ == "__main__":
    unittest.main()
