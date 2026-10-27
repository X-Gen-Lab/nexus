"""Polling Flash owns its completion vector without enabling an IRQ mode."""
from tools.configure.providers.descriptor import source_directory
import json
from pathlib import Path
import re
import tempfile
import tomllib
import unittest

from tools.configure import bindings, configure as gate, factory
from tools.configure.providers.common import ConfigurationError, mode_traits

ROOT = Path(__file__).resolve().parents[2]
CASES = (
    ("stm32", "stm32f407", "FLASH", "FLASH_IRQn"),
    ("gd32", "gd32f470", "FMC", "FMC_IRQn"),
)


class FlashPollingIrqTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def assembly(self, label, *, reserved=None, selected=True):
        fixture = ROOT / f"tests/contracts/flash_factory/{label}.toml"
        text = fixture.read_text(encoding="utf-8")
        if reserved is not None:
            authored = tomllib.loads(text)
            board = json.loads((ROOT / "boards" / authored["board"] /
                                "board.json").read_text(encoding="utf-8"))
            board["reserved_resources"].append(reserved)
            package = self.directory / f"{label}-board"
            package.mkdir(exist_ok=True)
            (package / "board.json").write_text(
                json.dumps(board), encoding="utf-8")
            text = text.replace(f'board = "{authored["board"]}"',
                                f'board_package = "{package.as_posix()}"')
        if not selected:
            text = text.split("\n[flash.physical]", 1)[0] + "\n"
        path = self.directory / f"{label}.toml"
        path.write_text(text, encoding="utf-8")
        return path

    def test_selected_physical_controller_owns_named_completion_irq(self):
        for label, _, controller, vector in CASES:
            with self.subTest(family=label):
                result = gate.resolve_ir(self.assembly(label), ROOT)
                self.assertEqual(
                    [("physical", controller, "poll-word")],
                    [(item.id, item.controller, item.mode)
                     for item in result.controllers])
                claims = [item for item in result.claims
                          if item.key == "irq:" + controller]
                self.assertEqual(1, len(claims))
                self.assertEqual("physical", claims[0].owner)
                self.assertEqual("poll-word", claims[0].mode)
                self.assertNotIn("irq:4", {item.key
                                           for item in result.claims})
                self.assertEqual("irq:" + controller,
                                 gate.normalize_resource("irq:" + vector))

    def test_facts_bind_each_exact_cmsis_name_to_vector_four(self):
        headers = {
            "stm32": "vendors/st/cmsis_device_f4/Include/stm32f407xx.h",
            "gd32": ("vendors/gigadevice/gd32f4xx/Firmware/CMSIS/GD/"
                     "GD32F4xx/Include/gd32f4xx.h"),
        }
        for label, family, controller, vector in CASES:
            with self.subTest(family=family):
                facts = json.loads((ROOT / source_directory(family) /
                                    "soc.json").read_text(encoding="utf-8"))
                physical = facts["controllers"][controller]
                self.assertEqual([vector], physical["irq"])
                self.assertEqual({vector: 4}, physical["irq_numbers"])
                self.assertIn("irq:" + vector, physical["resources"])
                self.assertRegex(
                    (ROOT / headers[label]).read_text(encoding="utf-8"),
                    rf"\b{re.escape(vector)}\s*=\s*4\s*,")

    def test_board_irq_aliases_conflict_before_bundle_publication(self):
        for label, _, controller, _ in CASES:
            for suffix in ("", "_IRQn", "_IRQHandler"):
                with self.subTest(family=label, suffix=suffix):
                    assembly = self.assembly(
                        label, reserved="irq:" + controller + suffix)
                    output = self.directory / f"{label}-rejected"
                    with self.assertRaisesRegex(
                            ConfigurationError,
                            "Resource conflict irq:" + controller):
                        gate.configure(assembly, output, ROOT)
                    self.assertFalse(output.exists())

    def test_unselected_flash_does_not_claim_its_completion_irq(self):
        for label, _, controller, _ in CASES:
            with self.subTest(family=label):
                result = gate.resolve_ir(
                    self.assembly(label, selected=False), ROOT)
                self.assertFalse(result.controllers)
                self.assertNotIn("irq:" + controller,
                                 {item.key for item in result.claims})

    def test_unselected_flash_allows_a_board_reserved_completion_irq(self):
        for label, _, controller, _ in CASES:
            for suffix in ("", "_IRQn", "_IRQHandler"):
                with self.subTest(family=label, suffix=suffix):
                    result = gate.resolve_ir(self.assembly(
                        label, selected=False,
                        reserved="irq:" + controller + suffix), ROOT)
                    self.assertFalse(result.controllers)
                    claims = [item for item in result.claims
                              if item.key == "irq:" + controller]
                    self.assertEqual(1, len(claims))
                    self.assertEqual("board-reserved", claims[0].owner)

    def test_polling_lease_does_not_generate_an_isr_or_irq_capability(self):
        self.assertFalse(mode_traits("flash", "poll-word").irq)
        for label, _, controller, _ in CASES:
            with self.subTest(family=label):
                result = gate.resolve_ir(self.assembly(label), ROOT)
                _, source = bindings.emit(result)
                self.assertNotRegex(
                    source, rf"\bvoid\s+{controller}_IRQHandler\s*\(")
                _, capabilities = factory.emit(result)
                match = re.search(
                    r"s_nx_capability_flash_physical\s*=\s*\{\s*([^,]+),",
                    capabilities)
                self.assertIsNotNone(match)
                self.assertIn("NX_FACTORY_CAP_POLLING", match[1])
                self.assertNotIn("NX_FACTORY_CAP_IRQ", match[1])
                for field in ("irq_priority = 5", "calls_os = true"):
                    path = self.directory / f"{label}-irq-option.toml"
                    path.write_text(self.assembly(label).read_text(
                        encoding="utf-8") + field + "\n", encoding="utf-8")
                    with self.assertRaises(ConfigurationError):
                        gate.resolve_ir(path, ROOT)


if __name__ == "__main__":
    unittest.main()
