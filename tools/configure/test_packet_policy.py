"""Packet storage metadata references canonical endpoints and selected owners."""
from dataclasses import dataclass
from types import SimpleNamespace
import unittest

from tools.configure.policies.packet import PacketPolicy


@dataclass(frozen=True)
class EndpointOptions:
    number: int
    direction: str
    max_packet: int


@dataclass(frozen=True)
class Endpoint:
    id: str
    controller: str
    options: EndpointOptions


class PacketPolicyTests(unittest.TestCase):
    def policy(self):
        return PacketPolicy.from_validated({"schema_version": 1,
            "usb_modes": {"device-fs": {"high_speed": False, "managed": False,
                "phy": "SAMPLE_INTERNAL"}, "otg-hs-ulpi": {
                "high_speed": True, "managed": True, "phy": "SAMPLE_EXTERNAL"}},
            "host_modes": {"host-fs": "SAMPLE_INTERNAL"},
            "dma_alignment": 4, "setup_slots": 3, "setup_bytes": 8,
            "ethernet_memory": "bus_ram"})

    def test_dma_extent_is_private_and_endpoint_identity_is_not_cloned(self):
        children = (Endpoint("control-out", "usb", EndpointOptions(0, "out", 63)),
                    Endpoint("data-in", "usb", EndpointOptions(1, "in", 1)))
        options = SimpleNamespace(engine="dma", remote_wakeup=True, host=None, power=None)
        item = SimpleNamespace(id="usb", kind="usb_device", mode="device-fs", options=options)
        config = SimpleNamespace(devices=children, controllers=(), physical_part=None)
        selected = self.policy().binding_values(item, configuration=config)["packet"]
        self.assertIs(selected.entries[0].child, children[0])
        self.assertEqual(selected.entries[0].dma.extent, 88)
        self.assertEqual(selected.endpoints["data-in"].dma.extent, 24)
        with self.assertRaises(TypeError):
            selected.endpoints["other"] = None
        options.engine = "pio"
        selected = self.policy().binding_values(item, configuration=config)["packet"]
        self.assertIsNone(selected.entries[0].dma)
        self.assertFalse(selected.has_dma)

    def test_non_packet_owners_contribute_nothing(self):
        self.assertEqual(self.policy().binding_values(SimpleNamespace(kind="gpio")), {})


if __name__ == "__main__":
    unittest.main()
