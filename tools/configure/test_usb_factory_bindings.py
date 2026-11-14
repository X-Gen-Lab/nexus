"""USB ROM factories project reviewed callbacks and physical facts verbatim."""
from types import MappingProxyType, SimpleNamespace
import unittest

from tools.configure import usb_otg, usb_profiles
from tools.configure.ir import (EndpointIR, IrqOptions, UsbDeviceOptions,
    UsbEndpointOptions, UsbHostOptions, UsbOtgPowerOptions)
from tools.configure.policies.packet import PacketPolicy


class UsbFactoryBindingTests(unittest.TestCase):
    def fixture(self, managed=False):
        mode = "otg-hs-fs" if managed else "device-hs-fs"
        policy = PacketPolicy.from_validated({"schema_version": 1,
            "usb_modes": {mode: {"high_speed": True, "managed": managed,
                                  "phy": "SAMPLE_PHY"}},
            "host_modes": {"host-hs-fs": "SAMPLE_PHY"}, "dma_alignment": 4,
            "setup_slots": 3, "setup_bytes": 8, "faces": {
                "device": {"setup_ops": "sample_setup", "activation_ops": "sample_activate"},
                "managed": {"setup_ops": "sample_managed_setup",
                    "activation_ops": "sample_managed_activate", "otg_ops": "sample_otg"}}})
        irq = IrqOptions(5, False)
        host = UsbHostOptions(2, 128, 96, 96, "pio", 0, 1000, 5000,
                              "external-owner", irq) if managed else None
        power = UsbOtgPowerOptions("supply", "present", "fault", True, True, False, 100) if managed else None
        item = SimpleNamespace(id="usb", kind="usb_device", mode=mode, controller="USB_CORE",
            options=UsbDeviceOptions(8, 96, False, 100, 1000, 5000, irq, host=host, power=power))
        owners = tuple(SimpleNamespace(id=name, kind="gpio", mask=1 << index)
                       for index, name in enumerate(("supply", "present", "fault"))) if managed else ()
        endpoints = (EndpointIR("control-out", "usb", "usb-fs-endpoint", "usb_device",
                         UsbEndpointOptions(0, 0, "out", "control", 64, 0, 0), ()),
                     EndpointIR("control-in", "usb", "usb-fs-endpoint", "usb_device",
                         UsbEndpointOptions(1, 0, "in", "control", 64, 96, 32), ()))
        result = SimpleNamespace(controllers=(item, *owners), devices=endpoints,
            factory=SimpleNamespace(capabilities=True),
            provider_context=SimpleNamespace(provider=SimpleNamespace(soc={"controllers": {
                "USB_CORE": {"clock_hz": 48000000, "fifo_words": 768, "endpoint_numbers": 5}}})))
        item.integration = SimpleNamespace(constants=MappingProxyType(policy.binding_values(item, configuration=result)))
        return result

    def test_device_profile_and_activation_use_the_selected_fact_owner(self):
        result = self.fixture()
        source = "\n".join(usb_profiles.emit(result)[1] + usb_profiles.emit_activation(result)[1])
        self.assertIn("48000000U, 768U, 96U, 5U", source)
        self.assertIn("&sample_setup", source)
        self.assertIn("&sample_activate", source)
        self.assertNotIn("nx_stm32", source)
        self.assertNotIn("NX_USB_FS_FIFO_WORDS", source)

    def test_managed_role_factory_reuses_the_same_callback_facts(self):
        result = self.fixture(managed=True)
        source = "\n".join(usb_profiles.emit(result)[1] +
            usb_profiles.emit_activation(result)[1] + usb_otg.emit_factory(result)[1])
        for symbol in ("sample_managed_setup", "sample_managed_activate", "sample_otg"):
            self.assertIn("&" + symbol, source)
        self.assertNotIn("nx_stm32", source)


if __name__ == "__main__":
    unittest.main()
