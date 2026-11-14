"""Header leaves are a finite projection, with no inferred GPIO naming."""
import json
from pathlib import Path
from types import SimpleNamespace
import unittest

from tools.configure.header_projection import HeaderProjection
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
        root = Path(__file__).resolve().parents[2]
        base = root / "soc/stm32/stm32f4/bindings"
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
