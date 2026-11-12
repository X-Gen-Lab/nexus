"""Basic F415/F417 crypto uses exact static controllers and pure factory faces."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure as gate, factory


class F407CryptoTests(unittest.TestCase):
    def resolve(self, directory, *, part="STM32F415ZGT6", extra="",
                cipher_mode="irq-blocks", priority=5):
        board = gate.load(gate.ROOT /
            "tests/contracts/stm32f4_family/f415zg-board/board.json")
        board["soc"] = part
        board["bindings"] = [
            {"id": "cipher", "controller": "CRYP", "route": "cryp-internal"},
            {"id": "digest", "controller": "HASH", "route": "hash-internal"},
        ]
        (directory / "board.json").write_text(json.dumps(board))
        template = (gate.ROOT / "tests/contracts/stm32f4_family/f415zg.toml").read_text()
        template = template[:template.index("[crc.signature]")]
        template = template.replace('board_package = "f415zg-board"',
                                    'board_package = "."')
        source = directory / "assembly.toml"
        source.write_text(template +
            f'[cipher.encrypt]\nbinding="cipher"\nmode="{cipher_mode}"\n'
            f'[cipher.encrypt.irq]\npriority={priority}\ncalls_os=false\n'
            '[hash.digest]\nbinding="digest"\nmode="irq-blocks"\n'
            '[hash.digest.irq]\npriority=5\ncalls_os=false\n' + extra)
        return gate.resolve_ir(source)

    def test_two_exact_parts_resolve_distinct_engines_without_dma_or_pads(self):
        for part in ("STM32F415ZGT6", "STM32F417ZGT6"):
            with self.subTest(part=part), tempfile.TemporaryDirectory() as tmp:
                result = self.resolve(Path(tmp), part=part)
                self.assertEqual(result.part, part)
                self.assertEqual([(x.kind, x.controller) for x in result.controllers],
                                 [("cipher", "CRYP"), ("hash", "HASH")])
                for item in result.controllers:
                    self.assertFalse(item.dma)
                    self.assertFalse(item.pins)
                    self.assertEqual(item.options.irq.priority, 5)
                    self.assertFalse(item.options.irq.calls_os)
                self.assertFalse(result.physical_qualified)

    def test_typed_options_and_factory_keep_hardware_behavior_in_c(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = self.resolve(Path(tmp))
            with self.assertRaises(FrozenInstanceError):
                result.controllers[0].options.irq = None
            source = bindings.emit(result)[1]
            for kind in ("cipher", "hash"):
                self.assertIn(f"nx_stm32f4_{kind}_admit", source)
                self.assertIn(f"nx_stm32f4_{kind}_start", source)
                self.assertIn(f"nx_stm32f4_{kind}_irq", source)
                self.assertIn(f"static const nx_stm32f4_{kind}_config_t", source)
                self.assertIn(f"nx_factory_{kind}", factory.emit(result)[0])
                self.assertIn(f"nx_factory_{kind}_ref", factory.emit_strong(result))
            self.assertIn("CRYP_IRQHandler", source)
            self.assertIn("HASH_RNG_IRQHandler", source)
            self.assertNotIn("->AHB2ENR", source)
            self.assertNotIn("->IMR", source)

    def test_non_crypto_device_does_not_inherit_the_same_family_engines(self):
        for part in ("STM32F405ZGT6", "STM32F407ZGT6"):
            with self.subTest(part=part), tempfile.TemporaryDirectory() as tmp:
                with self.assertRaisesRegex(ValueError, "no reviewed exact-part route"):
                    self.resolve(Path(tmp), part=part)

    def test_second_face_cannot_duplicate_the_physical_crypto_owner(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, "conflict|already|duplicate"):
                self.resolve(Path(tmp), extra=(
                    '\n[cipher.second]\nbinding="cipher"\nmode="irq-blocks"\n'
                    '[cipher.second.irq]\npriority=5\ncalls_os=false\n'))

    def test_unimplemented_transport_is_rejected_without_fallback(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, "[Mm]ode"):
                self.resolve(Path(tmp), cipher_mode="dma")

    def test_irq_priority_is_checked_against_the_selected_core_width(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, "priority|IRQ"):
                self.resolve(Path(tmp), priority=16)


if __name__ == "__main__":
    unittest.main()
