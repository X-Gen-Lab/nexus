"""Reviewed serial roles and clock domains do not depend on MCU names."""
from copy import deepcopy
import json
from pathlib import Path
from types import SimpleNamespace
import unittest

from tools.configure import rtc, spi_frames, timer_advanced, uart_wires
from tools.configure.policies.selection import SelectionPolicy


ROOT = Path(__file__).resolve().parents[2]


def provider():
    serial = {
        "uart_wire": {
            "controllers": {"SERIAL_A": {"protocols": list(uart_wires.PROTOCOLS),
                                          "flows": list(uart_wires.FLOW)}},
            "irda_max_baud": 115200, "smartcard_prescaler_max": 31},
        "spi_frames": {"controllers": ["BUS_A"], "word_bits": [8, 16],
                       "crc_odd_required": True, "crc_one_wire": False}}
    return SimpleNamespace(PREFIX="unrelated", SELECTION=SimpleNamespace(data={
        "serial_modes": serial, "rtc": {"crystal_resources": ("pin:PB7",)}}),
        soc={"controllers": {"SERIAL_A": {"clock_domain": "apb2"},
                             "BUS_A": {"clock_domain": "apb1"}}})


class SerialModePolicies(unittest.TestCase):
    def test_uart_uses_reviewed_protocols_and_semantic_wire_order(self):
        selected = {"mode": "irq-wire", "baud": 9600,
                    "wire": {"protocol": "synchronous"}}
        route = {"controller": "SERIAL_A", "pins": [
            {"pin": "PB6", "function": "tx", "af": 7},
            {"pin": "PB7", "function": "rx", "af": 7},
            {"pin": "PB5", "function": "ck", "af": 7}]}
        uart_wires.validate(selected, route, provider())
        self.assertEqual(selected["wire"]["protocol"], "synchronous")
        with self.assertRaises(ValueError):
            uart_wires.validate(deepcopy(selected),
                                {**route, "pins": list(reversed(route["pins"]))}, provider())

    def test_spi_frames_use_role_order_and_declared_controller_clock_domain(self):
        selected = {"mode": "irq-frames", "max_hz": 500000, "irq_priority": 5,
                    "frames": {"role": "master", "lines": "two-line",
                               "word_bits": 16, "irq_latency_us": 2}}
        route = {"controller": "BUS_A", "pins": [
            {"pin": pin, "function": role, "af": 5} for pin, role in
            (("PB3", "sck"), ("PB4", "miso"), ("PB5", "mosi"), ("PA15", "nss"))]}
        spi_frames.validate(selected, route, provider())
        spi_frames.validate_clock(selected, route,
                                  {"apb1_hz": 42000000, "apb2_hz": 84000000}, provider())
        with self.assertRaises(ValueError):
            spi_frames.validate(deepcopy(selected), {**route, "controller": "BUS_B"}, provider())

    def test_rtc_crystal_conflicts_are_reviewed_resource_data(self):
        self.assertEqual(rtc.selected_resources({"mode": "adopt-retained"}, provider()),
                         ("pin:PB7",))
        self.assertEqual(rtc.selected_resources({"mode": "adopt-retained", "lse_bypass": True},
                                                provider()), ())

    def test_timer_waveform_kind_uses_declared_policy_without_vendor_prefix(self):
        facts = SimpleNamespace(vector="ADV", irq=8)
        timer = SimpleNamespace(binding=lambda kind, controller:
                                facts if kind == "advanced_pwm" else None)
        selected = SimpleNamespace(PREFIX="unrelated", TIMER_ADVANCED=timer,
                                   WAVEFORM=SimpleNamespace(KINDS={"pwm_group"}))
        self.assertEqual(timer_advanced.selected_hardware(
            {"kind": "pwm_group", "controller": "TIMER_A"},
            {"kind": "counter", "irq": []}, selected)["irq"], ["ADV"])

    def test_policy_rejects_unknown_serial_facts(self):
        path = ROOT / "soc/stm32/stm32f4/bindings/selection_policy.json"
        facts = json.loads(path.read_text())["domains"]["selection"]
        facts["serial_modes"] = provider().SELECTION.data["serial_modes"]
        SelectionPolicy.from_validated(facts)
        facts["serial_modes"]["uart_wire"]["controllers"]["SERIAL_A"]["flows"] = ["made-up"]
        with self.assertRaises(ValueError):
            SelectionPolicy.from_validated(facts)


if __name__ == "__main__":
    unittest.main()
