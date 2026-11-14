"""Packet recipes retain static storage, selected faces and fixed lifecycle."""
from dataclasses import dataclass, replace
import json
from pathlib import Path
from types import MappingProxyType, SimpleNamespace
import unittest

from tools.configure.ir import (EndpointIR, IrqOptions, PadBindingIR,
    UsbDeviceOptions, UsbEndpointOptions, UsbHostOptions, UsbOtgPowerOptions,
    EthernetOptions, EthernetFramesOptions)
from tools.configure.policies.packet import PacketPolicy
from tools.configure.static_fragments import StaticFragments
from tools.configure.static_recipe import StaticRecipe

ROOT = Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class Integration:
    pads: tuple
    kernel_policy: str
    irq_profile: object
    memory_regions: tuple
    vector_bindings: tuple
    constants: object


@dataclass(frozen=True)
class Selected:
    id: str
    kind: str
    mode: str
    controller: str
    options: object
    integration: object = None
    mask: int = 1


@dataclass(frozen=True)
class Profile:
    external_count: int = 96
    priority_bits: int = 4
    syscall_priority: object = None


@dataclass(frozen=True)
class Memory:
    begin: int = 0x20000000
    size: int = 65536
    permissions: int = 3


@dataclass(frozen=True)
class Vector:
    handler: str = "OTG_FS_IRQHandler"


@dataclass(frozen=True)
class Result:
    controllers: tuple
    devices: tuple
    physical_part: object
    provider_context: object = None


class PacketStaticRecipesTests(unittest.TestCase):
    def document(self):
        return json.loads((ROOT / "soc/stm32/stm32f4/bindings/packet.json").read_text())

    def build(self, item, devices=(), owners=()):
        document = self.document()
        policy = PacketPolicy.from_validated(document["domains"]["packet"])
        memory = MappingProxyType({"id": "bus_ram", "origin": 0x20000000,
                                   "size": 65536, "dma": True})
        physical = SimpleNamespace(memory=(memory,))
        result = Result((item, *owners), devices, physical,
            SimpleNamespace(provider=SimpleNamespace(ETHERNET=SimpleNamespace(memory_region="bus_ram"),
                soc={"controllers": {
                    "OTG_FS": {"clock_hz": 48000000, "fifo_words": 320, "endpoint_numbers": 4},
                    "OTG_HS": {"clock_hz": 48000000, "fifo_words": 1024, "endpoint_numbers": 6}}})))
        values = policy.binding_values(item, configuration=result)
        selected = Integration((PadBindingIR("wire", "GPIOA", 0, 1, 10, "dm"),),
            "NX_IRQ_KERNEL_NONE", Profile(), (Memory(),), (Vector(),),
            MappingProxyType(values))
        item = replace(item, integration=selected)
        result = replace(result, controllers=(item, *owners))
        entry, = (row for row in document["bindings"] if row["kind"] == item.kind and
                  item.mode in row.get("modes", (row.get("mode"),)))
        library = StaticFragments.from_validated(document.get("static_fragments", {}))
        recipe = StaticRecipe.from_validated(library.expand(entry["static_recipe"]))
        built = recipe.construct(item, result, MappingProxyType({}), MappingProxyType({}), devices)
        return built, "\n".join(line for declaration in built.static_declarations for line in declaration.render())

    def test_device_modes_preserve_selected_dma_remote_and_managed_storage(self):
        irq = IrqOptions(5, False)
        child = EndpointIR("control-out", "usb", "usb-fs-endpoint", "usb_device",
            UsbEndpointOptions(0, 0, "out", "control", 64, 0, 0), ())
        for mode in ("device-fs", "device-hs-fs", "device-hs-ulpi",
                     "otg-fs", "otg-hs-fs", "otg-hs-ulpi"):
            managed = mode.startswith("otg-")
            for dma in (False, True) if "hs" in mode else (False,):
                with self.subTest(mode=mode, dma=dma):
                    host = UsbHostOptions(2, 128, 96, 96, "dma" if dma else "pio",
                        60000000 if mode.endswith("ulpi") else 0, 1000, 5000,
                        "managed" if managed else "external-owner", irq, (1, 0)) if managed else None
                    power = UsbOtgPowerOptions("source", "present", "fault",
                        True, True, False, 100) if managed else None
                    options = UsbDeviceOptions(8, 96, False, 100, 1000, 5000, irq,
                        engine="dma" if dma else "pio", remote_wakeup=True,
                        host=host, power=power)
                    item = Selected("usb", "usb_device", mode, "OTG_HS" if "hs" in mode else "OTG_FS", options)
                    owners = tuple(Selected(name, "gpio", "output", "GPIOA", None)
                                   for name in ("source", "present", "fault")) if managed else ()
                    built, source = self.build(item, (child,), owners)
                    self.assertFalse(built.definitions)
                    self.assertIn("nx_usb_remote_runtime_t", source)
                    self.assertEqual("nx_stm32_usb_otg_prepare" if managed else
                        "nx_stm32_usb_hs_cold_prepare" if "hs" in mode else "nx_stm32_usb_cold_prepare",
                        built.initialize.function)
                    self.assertIn("s_nx_device_control_out", source)
                    self.assertEqual("s_nx_usb_dma_control_out[88]" in source, dma)
                    if dma:
                        self.assertLess(source.index("s_nx_usb_dma_control_out[88]"),
                                        source.index("nx_usb_dma_storage_t"))

    def test_host_pio_and_dma_do_not_allocate_unselected_pipe_or_region_arrays(self):
        irq = IrqOptions(5, False)
        for engine, pipes in (("pio", ()), ("dma", (1, 0))):
            with self.subTest(engine=engine):
                options = UsbHostOptions(2, 128, 96, 96, engine, 60000000,
                    1000, 5000, "external-owner", irq, pipes)
                built, source = self.build(Selected("usb", "usb_host", "host-hs-ulpi", "OTG_HS", options))
                self.assertEqual(built.initialize.function, "nx_stm32_usb_host_cold_prepare")
                self.assertEqual("s_nx_usb_host_regions_usb[" in source, engine == "dma")
                self.assertEqual("s_nx_usb_pipe_channels_usb[" in source, bool(pipes))

    def test_ethernet_variants_share_one_c_owner_and_export_the_same_mdio_context(self):
        irq = IrqOptions(5, False)
        for kind, mode in (("eth", "raw-rmii"), ("eth_frames", "mii-vlan-sg-ptp")):
            with self.subTest(kind=kind):
                fields = (2, 2, 1536, (2, 0, 0, 0, 0, 1), 5000, 1000, irq)
                options = (EthernetOptions(*fields) if kind == "eth" else
                           EthernetFramesOptions(*fields, 1, True))
                built, source = self.build(Selected("mac", kind, mode, "ETH", options))
                self.assertIn("s_nx_eth_storage_mac[3072]", source)
                self.assertIn("g_nx_mdio_binding_mac", source)
                self.assertIn(".context = &s_nx_port_mac", source)
                self.assertIn("extern const nx_mdio_port_t* const g_nx_mdio_binding_mac;",
                              "\n".join(built.exports))

    def test_default_pio_device_has_no_remote_or_dma_runtime_storage(self):
        child = EndpointIR("control-out", "usb", "usb-fs-endpoint", "usb_device",
            UsbEndpointOptions(0, 0, "out", "control", 64, 0, 0), ())
        options = UsbDeviceOptions(8, 96, False, 100, 1000, 5000, IrqOptions(5, False))
        _, source = self.build(Selected("usb", "usb_device", "device-fs", "OTG_FS", options),
                               (child,))
        self.assertNotIn("nx_usb_remote_runtime_t", source)
        self.assertNotIn("s_nx_usb_dma_control_out", source)
        self.assertNotIn("nx_usb_dma_storage_t", source)
        self.assertIn(".remote = NULL", source)


if __name__ == "__main__":
    unittest.main()
