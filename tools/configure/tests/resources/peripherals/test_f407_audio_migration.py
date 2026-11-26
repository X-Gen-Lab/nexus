"""Standard audio replaces the manual SPI2 personality without a legacy role."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import tempfile
import unittest

from tools.configure import authored, bindings, factory
from tools.configure import configure as gate


class AudioMigrationTests(unittest.TestCase):
    def test_old_manual_top_level_role_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="retired i2s role ") as name:
            path = Path(name) / "assembly.toml"
            path.write_text('schema=2\nboard="stm32f407ve_sky_qingchun"\n'
                'backend="baremetal"\nclock="hse8-pll168"\n'
                '[memory]\nmain_stack_bytes=2048\n'
                '[i2s2]\nprofile="slave-tx-stereo16-pb12-pb13-pb15"\n')
            with self.assertRaisesRegex(ValueError, "unknown.*i2s2|retired"):
                gate.resolve_ir(path)

    def test_sky_and_qiming_references_use_the_standard_static_audio_owner(self):
        for name, part in (("sky", "STM32F407VET6"),
                           ("qiming", "STM32F407ZGT6")):
            with self.subTest(name=name):
                path = gate.ROOT / "tools/configure/assemblies" / (
                    name + "-i2s2-slave-tx.toml")
                result = gate.resolve_ir(path)
                self.assertEqual(part, result.part)
                item, = result.controllers
                self.assertEqual(("audio", "I2S2", "slave-tx"),
                                 (item.kind, item.controller, item.mode))
                self.assertEqual(("philips", 16, 16),
                                 (item.options.standard, item.options.sample_bits,
                                  item.options.slot_bits))
                self.assertEqual(("DMA1:stream4:channel0",), item.dma)
                self.assertEqual(("PB12", "PB13", "PB15"),
                                 tuple(pin["pin"] for pin in item.pins))
                self.assertIsNone(result.physical_pcb_revision)
                self.assertFalse(result.physical_qualified)
                self.assertFalse(hasattr(result, "i2s2"))
                with self.assertRaises(FrozenInstanceError):
                    item.options.sample_bits = 32
                source = bindings.emit(result)[1]
                self.assertIn("nx_stm32_audio_prepare(", source)
                self.assertIn("nx_stm32_audio_close(", source)
                self.assertIn("nx_factory_audio", factory.emit(result)[0])

    def test_editor_surface_has_no_retired_configuration_authority(self):
        self.assertNotIn("i2s2", authored.editor_schema()["properties"])

    def test_sdk_consumer_gets_its_existing_owner_from_the_factory(self):
        source = (gate.ROOT / "cmake/package/verification/io_evolution/consumers"
                  / "f407-i2s2-slave-tx.c").read_text()
        self.assertIn("nx_factory_audio_ref(NX_AUDIO_REF_SOUND)", source)
        self.assertIn("nx_audio_request_t", source)
        for retired in ("nx_f407_i2s2_slave_tx_initialize", "s_owner",
                        "NEXUS_F407_I2S2_SLAVE_TX_SELECTED"):
            self.assertNotIn(retired, source)
