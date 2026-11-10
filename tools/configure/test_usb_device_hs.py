"""Static HS device selection distinguishes internal FS PHY from ULPI HS."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure.tests.native_fixture import native_fixture


class UsbHsDeviceConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.package = self.root / "board"
        self.package.mkdir()

    def assembly(self, ulpi=False):
        source = gate.ROOT / "tests/contracts/usb_assembly/stm32-zg.toml"
        board = json.loads((source.parent / "stm32-zg-board/board.json").read_text())
        board["bindings"] = [{"id": "usbhs", "controller": "OTG_HS",
                              "route": "usbhs-device-ulpi" if ulpi else
                                       "usbhs-device-pb14-pb15"}]
        (self.package / "board.json").write_text(json.dumps(board))
        text = source.read_text().replace('board_package="stm32-zg-board"',
                                           'board_package="board"')
        text = text.replace('binding="usbfs"', 'binding="usbhs"')
        text = text.replace('mode="device-fs"',
                            'mode="device-hs-ulpi"' if ulpi else
                            'mode="device-hs-fs"')
        text = text.replace('vbus_sensing=false',
                            'vbus_sensing=false\nelectrical_handover="external-owner"\n'
                            'ulpi_clock_hz=' + ('60000000' if ulpi else '0'))
        if ulpi:
            text = text.replace('rx_fifo_words=96', 'rx_fifo_words=512')
            text = text.replace('transfer="bulk"\nmax_packet=64',
                                'transfer="bulk"\nmax_packet=512')
            text = text.replace('fifo_words=32', 'fifo_words=128')
        else:
            text = text.replace('number=1', 'number=5')
        path = self.root / "assembly.toml"
        path.write_text(text)
        return path

    def test_hs_internal_phy_selects_fs_packet_geometry_and_six_numbers(self):
        path = self.assembly()
        ir = gate.resolve_ir(path, test_support=native_fixture(path))
        self.assertEqual(ir.controllers[0].controller, "OTG_HS")
        self.assertEqual(ir.devices[2].options.number, 5)
        self.assertEqual(ir.devices[2].options.max_packet, 64)
        self.assertFalse(ir.physical_qualified)

    def test_ulpi_512_byte_plan_emits_hs_owner_and_exact_profile(self):
        path = self.assembly(True)
        output = self.root / "generated"
        gate.configure(path, output, test_support=native_fixture(path))
        source = (output / "bindings.c").read_text()
        header = (output / "nexus_factory.h").read_text()
        self.assertIn("nx_factory_usb_profile_for_speed", header)
        self.assertIn("s_nx_usb_fs_fallback_profile_fs", source)
        self.assertIn("nx_stm32_usb_hs_cold_prepare", source)
        self.assertIn("nx_stm32_usb_hs_clock_state_t", source)
        self.assertIn("NX_USB_PROFILE_HIGH_SPEED", source)
        self.assertIn("NX_USB_PROFILE_EXTERNAL_ULPI", source)
        self.assertIn("OTG_HS_IRQHandler", source)
        self.assertNotIn("OTG_FS_IRQHandler", source)
        self.assertNotIn("malloc", source)

    def test_hs_dma_is_typed_and_allocates_exact_private_staging(self):
        path = self.assembly(True)
        path.write_text(path.read_text().replace('clock_timeout_us=5000',
            'clock_timeout_us=5000\nengine="dma"'))
        ir = gate.resolve_ir(path, test_support=native_fixture(path))
        self.assertEqual(ir.controllers[0].options.engine, "dma")
        output = self.root / "generated"
        gate.configure(path, output, test_support=native_fixture(path))
        source = (output / "bindings.c").read_text()
        self.assertIn("nx_usb_dma_storage_t", source)
        self.assertIn("s_nx_usb_dma_regions_fs", source)
        self.assertIn(".dma = true", source)
        self.assertIn("NX_USB_PROFILE_AHB_DMA", source)

    def test_isochronous_hs_dma_has_one_transaction_and_fs_fallback_mps(self):
        path = self.assembly(True)
        content = path.read_text().replace('transfer="bulk"\nmax_packet=512',
            'transfer="isochronous"\nmax_packet=1024')
        content = content.replace('rx_fifo_words=512', 'rx_fifo_words=288')
        content = content.replace('fifo_words=128', 'fifo_words=32', 1)
        content = content.replace('fifo_words=128', 'fifo_words=256')
        content = content.replace('clock_timeout_us=5000',
            'clock_timeout_us=5000\nengine="dma"')
        path.write_text(content)
        output = self.root / "iso-generated"
        gate.configure(path, output, test_support=native_fixture(path))
        source = (output / "bindings.c").read_text()
        self.assertIn("NX_USB_PROFILE_ISOCHRONOUS", source)
        self.assertIn("1024U", source)
        self.assertIn("1023U", source)
        path.write_text(content.replace('max_packet=1024', 'max_packet=2048'))
        with self.assertRaisesRegex(ValueError, "packet|MPS"):
            gate.resolve_ir(path, test_support=native_fixture(path))


    def test_largest_out_packet_needs_rx_space_in_addition_to_setup_status(self):
        path = self.assembly(True)
        path.write_text(path.read_text().replace('rx_fifo_words=512',
                                                 'rx_fifo_words=64'))
        with self.assertRaisesRegex(ValueError, "RX|FIFO"):
            gate.resolve_ir(path, test_support=native_fixture(path))


if __name__ == "__main__":
    unittest.main()
