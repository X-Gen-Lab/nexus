"""USB policy selects a reviewed transport capability, never a vendor label."""
from types import SimpleNamespace
import unittest

from tools.configure import usb_device, usb_host
from tools.configure.policies.packet import PacketPolicy


def facts():
    pins = [["IO_A", "dm", 3], ["IO_B", "dp", 3]]
    return {"schema_version": 1, "usb_modes": {"device-hs-fs": {
        "high_speed": True, "managed": False, "phy": "SAMPLE_INTERNAL"}},
        "host_modes": {"host-fs": "SAMPLE_INTERNAL"},
        "dma_alignment": 4, "setup_slots": 3, "setup_bytes": 8,
        "admission": {"device": {"device-hs-fs": {"controller": "USB_CORE",
            "pins": pins, "engines": ["pio", "dma"], "remote_wakeup": True,
            "rx_fifo_words": [48, 1024], "ulpi_clock_hz": 0,
            "minimum_core_hz": 30000000}},
            "host": {"host-fs": {"controller": "USB_CORE", "pins": pins,
                "engines": ["pio"], "channel_count": 2, "fifo_words": 320,
                "fifo_minima": {"rx": 64, "nonperiodic": 16, "periodic": 16},
                "dma_reserved_words": 32, "ulpi_clock_hz": 0,
                "minimum_core_hz": 30000000}}}}


class UsbPacketCapabilityTests(unittest.TestCase):
    def provider(self):
        return SimpleNamespace(PREFIX="another_vendor", PACKET=PacketPolicy.from_validated(facts()))

    @staticmethod
    def route():
        return {"kind": "usb_host", "controller": "USB_CORE", "pins": [
            {"pin": "IO_A", "function": "dm", "af": 3},
            {"pin": "IO_B", "function": "dp", "af": 3}]}

    def test_explicit_host_capability_accepts_a_different_controller_and_vendor(self):
        selection = dict(mode="host-fs", channel_count=2, rx_fifo_words=128,
            nonperiodic_fifo_words=96, periodic_fifo_words=96, engine="pio",
            ulpi_clock_hz=0, drain_timeout_us=1000, init_timeout_us=5000,
            electrical_handover="external-owner", irq_priority=5)
        provider = self.provider()
        usb_host.validate_selection(selection, self.route(), provider)
        usb_host.validate_clock("USB_CORE", 48000000, provider=provider,
                                kind="usb_host", mode="host-fs")
        with self.assertRaisesRegex(ValueError, "DMA|transport"):
            usb_host.validate_selection({**selection, "engine": "dma", "rx_fifo_words": 96},
                                        self.route(), provider)
        with self.assertRaisesRegex(ValueError, "AHB"):
            usb_host.validate_clock("USB_CORE", 30000000, provider=provider,
                                    kind="usb_host", mode="host-fs")

    def test_remote_and_high_speed_device_require_explicit_selected_capabilities(self):
        provider = self.provider()
        selected = dict(mode="device-hs-fs", event_capacity=8, rx_fifo_words=96,
            vbus_sensing=False, poll_limit=1000, drain_timeout_us=1000,
            clock_timeout_us=5000, irq_priority=5, engine="dma",
            remote_wakeup=True, ulpi_clock_hz=0, electrical_handover="external-owner")
        route = {**self.route(), "kind": "usb_device"}
        usb_device.validate_selection(selected, route, provider, 65536)
        restricted = facts()
        restricted["admission"]["device"]["device-hs-fs"]["remote_wakeup"] = False
        provider.PACKET = PacketPolicy.from_validated(restricted)
        with self.assertRaisesRegex(ValueError, "remote_wakeup"):
            usb_device.validate_selection(selected, route, provider, 65536)
        route["pins"][0]["af"] = 4
        with self.assertRaisesRegex(ValueError, "pads|route"):
            usb_device.validate_selection({**selected, "remote_wakeup": False}, route, provider, 65536)


if __name__ == "__main__":
    unittest.main()
