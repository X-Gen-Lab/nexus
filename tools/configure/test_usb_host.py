"""Selected Host PHY, FIFO, DMA and electrical handover contracts."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure import usb_host
from tools.configure.tests.native_fixture import native_fixture


class UsbHostConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        package = self.root / "board"
        package.mkdir()
        original = gate.ROOT / "tests/contracts/usb_assembly/stm32-zg-board/board.json"
        self.board = json.loads(original.read_text())
        self.package = package

    def assembly(self, mode="host-fs", **changes):
        controller, route = {
            "host-fs": ("OTG_FS", "usbfs-host-pa11-pa12"),
            "host-hs-fs": ("OTG_HS", "usbhs-host-pb14-pb15"),
            "host-hs-ulpi": ("OTG_HS", "usbhs-host-ulpi"),
        }[mode]
        self.board["bindings"] = [
            {"id": "host", "controller": controller, "route": route}]
        (self.package / "board.json").write_text(json.dumps(self.board))
        values = {"channel_count": 2, "rx_fifo_words": 128,
                  "nonperiodic_fifo_words": 96, "periodic_fifo_words": 96,
                  "engine": "pio", "ulpi_clock_hz": 0,
                  "drain_timeout_us": 1000, "init_timeout_us": 5000,
                  "electrical_handover": "external-owner"}
        if mode == "host-hs-ulpi":
            values["ulpi_clock_hz"] = 60000000
        values.update(changes)
        content = ('schema=2\nboard_package="board"\nbackend="baremetal"\n'
                   'clock="hse8-pll168"\n[factory]\ncapabilities=true\n'
                   '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                   'static_ram_limit_bytes=32768\nflash_load_limit_bytes=131072\n'
                   '[usb_host.link]\nbinding="host"\n' +
                   'mode=' + json.dumps(mode) + '\n' +
                   '\n'.join(key + '=' + json.dumps(value)
                             for key, value in values.items()) +
                   '\nirq={priority=5,calls_os=false}\n')
        path = self.root / "assembly.toml"
        path.write_text(content)
        return path

    def resolve(self, path):
        return gate.resolve_ir(path, test_support=native_fixture(path))

    def test_all_phy_choices_keep_physical_speed_and_dma_separate(self):
        for mode in ("host-fs", "host-hs-fs", "host-hs-ulpi"):
            with self.subTest(mode=mode):
                ir = self.resolve(self.assembly(mode))
                item = ir.controllers[0]
                self.assertEqual(item.kind, "usb_host")
                self.assertEqual(item.options.channel_count, 2)
                self.assertFalse(item.options.engine == "dma")
                self.assertFalse(ir.physical_qualified)
                with self.assertRaises(FrozenInstanceError):
                    item.options.channel_count = 8
                claims = {claim.key for claim in ir.claims}
                self.assertIn("irq:" + item.controller, claims)
                self.assertIn("controller:" + item.controller, claims)

    def test_hs_dma_selects_bus_master_domain_without_dma_stream(self):
        ir = self.resolve(self.assembly("host-hs-ulpi", engine="dma"))
        self.assertTrue(ir.controllers[0].options.engine == "dma")
        claims = {claim.key for claim in ir.claims}
        self.assertIn("usb-dma:OTG_HS", claims)
        self.assertFalse(any(key.startswith("dma:") for key in claims))

    def test_logical_pipe_mapping_is_immutable_and_generated_only_when_selected(self):
        path = self.assembly(pipe_channels=[1, 0])
        result = self.resolve(path)
        self.assertEqual(result.controllers[0].options.pipe_channels, (1, 0))
        output = self.root / "mapped"
        gate.configure(path, output, test_support=native_fixture(path))
        source = (output / "bindings.c").read_text()
        self.assertIn("static const uint8_t s_nx_usb_pipe_channels_link[]", source)
        self.assertIn(".pipe_channels = s_nx_usb_pipe_channels_link", source)
        path = self.assembly()
        gate.configure(path, output, test_support=native_fixture(path))
        self.assertNotIn("s_nx_usb_pipe_channels_link", (output / "bindings.c").read_text())

    def test_logical_pipe_mapping_rejects_aliases_bounds_and_wrong_extent(self):
        for mapping in ([0, 0], [0, 2], [0], [0, True], "identity"):
            with self.subTest(mapping=mapping), self.assertRaisesRegex(ValueError, "USB.*pipe"):
                self.resolve(self.assembly(pipe_channels=mapping))

    def test_selected_factory_has_exact_channels_and_explicit_handover(self):
        path = self.assembly("host-hs-fs")
        output = self.root / "generated"
        gate.configure(path, output, test_support=native_fixture(path))
        source = (output / "bindings.c").read_text()
        header = (output / "nexus_factory.h").read_text()
        self.assertIn("nx_factory_usb_host(", header)
        self.assertIn("nx_stm32_usb_host_channel_t s_nx_usb_channels_link[2]", source)
        self.assertIn("NX_STM32_USB_HS_INTERNAL_FS", source)
        self.assertIn("nx_stm32_usb_host_cold_prepare(", source)
        self.assertIn("nx_stm32_usb_host_cold_close(", source)
        self.assertIn("OTG_HS_IRQHandler", source)
        self.assertNotIn("OTG_FS_IRQHandler", source)
        self.assertNotIn("malloc", source)

    def test_invalid_phy_dma_fifo_and_budgets_fail_closed(self):
        invalid = ({"engine": "dma"}, {"channel_count": 9},
                   {"channel_count": True}, {"rx_fifo_words": 63},
                   {"nonperiodic_fifo_words": 250},
                   {"periodic_fifo_words": 15}, {"drain_timeout_us": 0},
                   {"init_timeout_us": 0}, {"ulpi_clock_hz": 60000000},
                   {"electrical_handover": "auto-power"})
        for change in invalid:
            with self.subTest(change=change), self.assertRaisesRegex(ValueError, "USB"):
                self.resolve(self.assembly(**change))
        with self.assertRaisesRegex(ValueError, "USB"):
            self.resolve(self.assembly("host-hs-ulpi", ulpi_clock_hz=48000000))

    def test_host_and_device_cannot_own_the_same_physical_controller(self):
        path = self.assembly()
        self.board["bindings"].append({"id": "device", "controller": "OTG_FS",
                                        "route": "usbfs-pa11-pa12"})
        (self.package / "board.json").write_text(json.dumps(self.board))
        original = (gate.ROOT / "tests/contracts/usb_assembly/stm32-zg.toml").read_text()
        path.write_text(path.read_text() + '\n' + original[original.index('[usb_device.fs]'):]
                        .replace('binding="usbfs"', 'binding="device"'))
        with self.assertRaisesRegex(ValueError, "conflict.*OTG_FS"):
            self.resolve(path)

    def test_selected_ahb_floor_is_checked_independently_of_pll48(self):
        for controller, frequency in (("OTG_FS", 14000000),
                                      ("OTG_HS", 26000000),
                                      ("OTG_HS", 30000000)):
            with self.subTest(controller=controller, frequency=frequency), self.assertRaisesRegex(ValueError, "USB.*AHB"):
                usb_host.validate_clock(controller, frequency)
        usb_host.validate_clock("OTG_FS", 16000000)
        usb_host.validate_clock("OTG_HS", 84000000)


    def test_dma_registers_are_reserved_outside_authored_fifo_payloads(self):
        with self.assertRaisesRegex(ValueError, "FIFO|DMA"):
            self.resolve(self.assembly("host-hs-ulpi", engine="dma",
                rx_fifo_words=512, nonperiodic_fifo_words=256,
                periodic_fifo_words=256))


if __name__ == "__main__":
    unittest.main()
