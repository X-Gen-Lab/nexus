"""PMA uses the existing USB faces and exact, separate packet-memory facts."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure.ir import (UsbEndpointOptions, UsbPmaEp1DeviceOptions,
                                UsbPmaDeviceOptions)


FIXTURE = gate.ROOT / "tests/integration/assemblies/stm32f1_pma_device"


class UsbPmaConfigurationTests(unittest.TestCase):
    def setUp(self):
        shared = tempfile.TemporaryDirectory()
        self.addCleanup(shared.cleanup)
        self.directory = Path(shared.name)
        self.assembly = self.directory / "assembly.toml"
        self.content = (FIXTURE / "f103rb.toml").read_text().replace(
            'board_package = "rb-board"',
            'board_package = ' + json.dumps(str(FIXTURE / "rb-board")))

    def test_generated_faces_distinguish_pma_bytes_from_fifo_words(self):
        ir = gate.resolve_ir(FIXTURE / "f103rb.toml")
        options = ir.controllers[0].options
        self.assertIsInstance(options, UsbPmaDeviceOptions)
        self.assertEqual((options.pma_layout.bytes, options.pma_layout.table_bytes,
                          options.pma_layout.tx_offset, options.pma_layout.rx_offset),
                         (512, 64, 64, 128))
        with self.assertRaises(FrozenInstanceError):
            options.pma_layout.tx_offset = 100
        self.assertFalse(ir.physical_qualified)
        self.assertTrue(all(isinstance(item.options, UsbEndpointOptions)
                            and not hasattr(item.options, "fifo_words")
                            for item in ir.devices))
        claims = {claim.key for claim in ir.claims}
        self.assertTrue({"pin:PA0", "pin:PA1", "pin:PA11", "pin:PA12",
                         "clock:USBFS48", "memory:usb-can-pma",
                         "irq:USB_LP_CAN1_RX0"} <= claims)

        out = self.directory / "out"
        gate.configure(FIXTURE / "f103rb.toml", out)
        public = (out / "nexus_factory.h").read_text()
        code = (out / "bindings.c").read_text()
        self.assertIn("nx_factory_usb_endpoint", public)
        self.assertIn("USB_LP_CAN1_RX0_IRQHandler", code)
        self.assertIn("nx_stm32f1_usb_pma_board_prepare", code)
        self.assertNotIn("nx_factory_usb_profile", public)
        self.assertNotIn("nx_factory_usb_activation", public)
        self.assertNotIn("fifo_words", code)

    def test_f102_has_its_own_irq_clock_and_packet_memory(self):
        ir = gate.resolve_ir(FIXTURE / "f102rb.toml")
        self.assertIsInstance(ir.controllers[0].options, UsbPmaDeviceOptions)
        claims = {claim.key for claim in ir.claims}
        self.assertIn("irq:USB_LP", claims)
        self.assertIn("memory:usb-pma", claims)
        self.assertNotIn("irq:USB_LP_CAN1_RX0", claims)
        self.assertNotIn("memory:usb-can-pma", claims)
        out = self.directory / "f102"
        gate.configure(FIXTURE / "f102rb.toml", out)
        code = (out / "bindings.c").read_text()
        self.assertIn("USB_LP_IRQHandler", code)
        self.assertNotIn("USB_LP_CAN1_RX0_IRQHandler", code)
        self.assertIn("48000000", (out / "nexus_config.h").read_text())

    def test_rejects_fifo_handoff_wrong_clock_or_missing_ep0(self):
        bad_assemblies = (
            (self.content + "fifo_words = 16\n", "without FIFO"),
            (self.content.replace('direction = "out"', 'direction = "in"'),
             "one EP0 IN and one EP0 OUT"),
            (self.content.replace('clock = "hse8-mco-pll72"', 'clock = "hsi8"'),
             "clock"),
            (self.content.replace('"self-powered-pa0-pa1-pa11-pa12"', '"absent"'),
             "hardware"),
        )
        for content, message in bad_assemblies:
            with self.subTest(message=message):
                self.assembly.write_text(content)
                with self.assertRaisesRegex(ValueError, message):
                    gate.resolve_ir(self.assembly)

    def test_typed_bulk_pair_has_an_activation_face_and_disjoint_offsets(self):
        ir = gate.resolve_ir(FIXTURE / "f103rb-bulk.toml")
        options = ir.controllers[0].options
        self.assertIsInstance(options, UsbPmaEp1DeviceOptions)
        self.assertEqual((options.ep1_layout.tx_offset,
                          options.ep1_layout.rx_offset,
                          options.ep1_layout.max_packet), (192, 256, 64))
        self.assertEqual({(item.options.number, item.options.direction)
                          for item in ir.devices},
                         {(0, "in"), (0, "out"), (1, "in"), (1, "out")})
        self.assertIn("memory:usb-can-pma", {claim.key for claim in ir.claims})
        out = self.directory / "bulk"
        gate.configure(FIXTURE / "f103rb-bulk.toml", out)
        public = (out / "nexus_factory.h").read_text()
        code = (out / "bindings.c").read_text()
        self.assertIn("nx_factory_usb_activation", public)
        self.assertIn("nx_stm32f1_usb_pma_ep1_activation_ops", code)
        self.assertNotIn("nx_factory_usb_profile", public)
        self.assertNotIn("fifo_words", code)

    def test_rejects_partial_wrong_width_and_wrong_type_bulk_selection(self):
        content = (FIXTURE / "f103rb-bulk.toml").read_text().replace(
            'board_package = "rb-board"',
            'board_package = ' + json.dumps(str(FIXTURE / "rb-board")))
        bad_assemblies = (
            (content.rsplit("[usb_endpoint.data_out]", 1)[0],
             "the selected EP1 pair"),
            (content.replace("max_packet = 64", "max_packet = 32"),
             "64-byte"),
            (content.replace('transfer = "bulk"', 'transfer = "interrupt"'),
             "endpoint number and transfer"),
        )
        for edited, message in bad_assemblies:
            with self.subTest(message=message):
                self.assembly.write_text(edited)
                with self.assertRaisesRegex(ValueError, message):
                    gate.resolve_ir(self.assembly)

    def test_ep1_bulk_is_not_admitted_by_f102_pma(self):
        content = (FIXTURE / "f102rb.toml").read_text().replace(
            'board_package = "f102rb-board"',
            'board_package = ' + json.dumps(str(FIXTURE / "f102rb-board")))
        self.assembly.write_text(
            content.replace('mode = "device-pma"', 'mode = "device-pma-bulk"'))
        with self.assertRaisesRegex(ValueError, "mode"):
            gate.resolve_ir(self.assembly)

    def test_interrupt_pair_projects_the_exact_hardware_type_without_fifo(self):
        fixture = FIXTURE / "f103rb-interrupt.toml"
        ir = gate.resolve_ir(fixture)
        self.assertEqual({(item.options.direction, item.options.transfer)
                          for item in ir.devices if item.options.number == 1},
                         {("in", "interrupt"), ("out", "interrupt")})
        self.assertIn("memory:usb-can-pma", {claim.key for claim in ir.claims})
        out = self.directory / "interrupt"
        gate.configure(fixture, out)
        source = (out / "bindings.c").read_text()
        self.assertIn(".ep1_type = USB_EP_INTERRUPT", source)
        self.assertNotIn("fifo_words", source)

    def test_pma_rejects_mixed_interrupt_and_bulk_on_one_hardware_row(self):
        content = (FIXTURE / "f103rb-interrupt.toml").read_text().replace(
            'board_package = "rb-board"',
            'board_package = ' + json.dumps(str(FIXTURE / "rb-board")))
        self.assembly.write_text(content.replace('transfer = "interrupt"',
                                                 'transfer = "bulk"', 1))
        with self.assertRaisesRegex(ValueError, "endpoint number and transfer"):
            gate.resolve_ir(self.assembly)
