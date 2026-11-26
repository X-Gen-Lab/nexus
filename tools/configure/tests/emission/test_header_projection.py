"""Header leaves are a finite projection, with no inferred GPIO naming."""
import json
from pathlib import Path
from types import SimpleNamespace
import unittest

from tools.configure.generation.header_projection import HeaderProjection
from tools.configure.ir import PadBindingIR
from tools.configure.ir import ClockPlanIR
from tools.configure.domains.pll_mnpq import PllMnpqPolicy


def facts():
    return {"schema_version": 1, "timebase_prefix": "SAMPLE",
        "pin_tables": {"capture": "SAMPLE_CAPTURE", "advanced": "SAMPLE_ADVANCED",
                       "adc": "SAMPLE_ADC"},
        "dma_memory": {"region": "bus_ram", "origin_macro": "SAMPLE_RAM_ORIGIN",
            "size_macro": "SAMPLE_RAM_SIZE", "block_mode": "block-dma",
            "lane_macro": "SAMPLE_CARD_LANE"}}


class HeaderProjectionTests(unittest.TestCase):
    def test_optional_record_presence_selects_a_build_feature(self):
        value = facts()
        value["features"] = {"paired_capture": {
            "source": "option_present", "owners": ["pwm_adc"],
            "flag": "pair", "macro": "SAMPLE_PAIR"}}
        owner = SimpleNamespace(kind="pwm_adc", options=SimpleNamespace(
            pair=SimpleNamespace(converters=("ADC1", "ADC2"))))
        result = SimpleNamespace(controllers=(owner,), devices=())
        header = HeaderProjection.from_validated(value)
        self.assertEqual(header.features(result),
                         (("paired_capture", True, "SAMPLE_PAIR"),))
        owner.options.pair = None
        self.assertEqual(header.features(result),
                         (("paired_capture", False, "SAMPLE_PAIR"),))

    def test_package_features_drive_private_modes_and_shared_header_values(self):
        value = facts()
        value["features"] = {"fifo": {"source": "controller", "owners": ["uart"],
            "modes": ["fifo"], "macro": "SAMPLE_FIFO"},
            "shared_bus": {"source": "endpoint", "owners": ["sample-device"],
                           "modes": ["loan", "stream"]},
            "wake": {"source": "option", "owners": ["uart"], "flag": "wake"}}
        result = SimpleNamespace(controllers=(SimpleNamespace(kind="uart",
            mode="fifo", options=SimpleNamespace(wake=False)),), devices=(
            SimpleNamespace(driver="sample-device", options=SimpleNamespace(mode="loan")),))
        flags = HeaderProjection.from_validated(value).features(result)
        self.assertEqual(flags, (("fifo", True, "SAMPLE_FIFO"),
                                 ("shared_bus", True, None), ("wake", False, None)))
        value["features"]["bad;name"] = value["features"]["fifo"]
        with self.assertRaises(ValueError):
            HeaderProjection.from_validated(value)

    def test_selected_capture_requires_the_explicit_exact_part_limit(self):
        value = facts()
        value["physical_capabilities"] = {"capture": {
            "field": "dcmi_bits", "macro": "SELECTED_CAMERA_BITS",
            "values": [8, 10, 12, 14]}}
        selected = HeaderProjection.from_validated(value)
        result = SimpleNamespace(controllers=(SimpleNamespace(kind="capture"),),
            clock_plan=None, physical_part=SimpleNamespace(memory=(),
                capabilities={"dcmi_bits": 12}),
            provider_context=SimpleNamespace(binding_policy=None))
        self.assertIn("#define SELECTED_CAMERA_BITS 12u", selected.render(result, SimpleNamespace()))
        result.physical_part.capabilities = {}
        with self.assertRaisesRegex(ValueError, "capabilit|limit"):
            selected.render(result, SimpleNamespace())

    def test_selected_timer_pad_uses_the_canonical_binding_not_pin_text(self):
        selected = HeaderProjection.from_validated(facts())
        route = SimpleNamespace(channel=2,
            pins=(("logical-pin", "capture", 7),))
        timer = SimpleNamespace(binding=lambda kind, controller:
            SimpleNamespace(route=lambda pins: route))
        pad = PadBindingIR("logical-pin", "PORT_Q", 9, 20, 7, "capture")
        item = SimpleNamespace(kind="timer_capture", controller="COUNTER_A",
            pins=({"pin": "logical-pin", "function": "capture", "af": 7},),
            integration=SimpleNamespace(pads=(pad,)))
        result = SimpleNamespace(controllers=(item,), clock_plan=None,
            physical_part=SimpleNamespace(memory=()),
            provider_context=SimpleNamespace(binding_policy=SimpleNamespace(
                header_lines=lambda prefix:["#define " + prefix + "_TIMEBASE COUNTER_A"])))
        provider = SimpleNamespace(TIMER_ADVANCED=timer)
        lines = selected.render(result, provider)
        self.assertIn("    X(COUNTER_A, 2U, PORT_Q, 9U, 20U)", lines)
        self.assertIn("#define SAMPLE_TIMEBASE COUNTER_A", lines)
        self.assertIn("#define SAMPLE_RAM_SIZE 0u", lines)

    def test_unknown_header_directives_reject_instead_of_becoming_c(self):
        value = facts()
        value["execute"] = "register_write()"
        with self.assertRaises(ValueError):
            HeaderProjection.from_validated(value)

    def test_reviewed_clock_vectors_project_semantics_without_register_encoding(self):
        root = Path(__file__).resolve().parents[4]
        base = root / "soc/stm32/stm32f4/private/bindings"
        header = HeaderProjection.from_validated(json.loads(
            (base / "headers.json").read_text())["domains"]["headers"])
        clock_facts = json.loads((base / "domain_common.json").read_text())["domains"]["clock"]
        clock = PllMnpqPolicy.from_validated(clock_facts)
        profile = clock_facts["profiles"]["hse8-pll168"]["clock"]
        result = SimpleNamespace(controllers=(), clock_plan=ClockPlanIR.from_validated(
            clock.plan("hse8-pll168", profile)), physical_part=SimpleNamespace(memory=()),
            provider_context=SimpleNamespace(binding_policy=None))
        lines = header.render(result, SimpleNamespace())
        self.assertIn("#define NEXUS_CLOCK_PLLM 8u", lines)
        self.assertIn("#define NEXUS_APB1_TIMER_HZ 84000000u", lines)
        self.assertIn("#define NEXUS_FLASH_WAIT_STATES 5u", lines)
        self.assertIn("#define NEXUS_CLOCK_SOURCE_HSE 1u", lines)
        self.assertFalse(any("PLLCFGR" in line or "NEXUS_CLOCK_CFGR" in line for line in lines))


if __name__ == "__main__":
    unittest.main()
