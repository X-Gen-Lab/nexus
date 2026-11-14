"""Hardware metadata has one author and retains the exact bytes it decoded."""

from dataclasses import replace
import hashlib
import json
from pathlib import Path
import tempfile
from types import MappingProxyType
import unittest
from unittest.mock import patch

from tools.configure import configure
from tools.configure.data_provider import DataProvider
from tools.configure.hardware_bindings import load_hardware
from tools.configure.records import thaw
from tools.configure.tests.f4_fixture import load_f4_fixture


class HardwareIdentityTests(unittest.TestCase):
    def test_resolver_cannot_bind_cached_hardware_to_changed_fact_bytes(self):
        construct = DataProvider.from_package

        def mismatched_identity(*arguments, **options):
            provider = construct(*arguments, **options)
            facts = replace(provider.hardware_facts, FACT_SHA256=MappingProxyType({
                **provider.hardware_facts.FACT_SHA256, "bindings": "0" * 64}))
            return replace(provider, software=thaw(provider.software), hardware_facts=facts)

        with patch.object(DataProvider, "from_package", side_effect=mismatched_identity), \
                self.assertRaisesRegex(ValueError, "hardware.*identity|Hardware.*identity|Hardware.*changed"):
            configure.resolve_ir(configure.ROOT / "tools/configure/assemblies/qiming-baremetal-gpio.toml")


class HardwareFactAuthorTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        fixture = load_f4_fixture("STM32F407ZGT6")
        package = fixture.package
        self.package = replace(package, source_directory=self.root)
        self.documents = {}
        for name in ("bindings", "soc", "routes", "device_catalog"):
            self.documents[name] = json.loads(fixture.snapshots[package.fact(name)])
        self.write()

    def write(self):
        for name, document in self.documents.items():
            self.package.fact(name).write_text(json.dumps(document) + "\n")

    def load(self):
        return load_hardware(self.package, read=configure._SnapshotReader({}))

    def test_each_authored_hardware_fact_reaches_the_frozen_view(self):
        baseline = json.dumps(self.documents)
        cases = (
            ("adc_channels", lambda d: d["bindings"]["hardware"]["adc_channels"][0]["pins"].update(PA0=7),
             lambda h: h.ADC_PIN_CHANNELS[("ADC1", "PA0")], 7),
            ("adc_internal", lambda d: d["bindings"]["hardware"]["adc_internal_channels"].update(ADC1=[16]),
             lambda h: h.ADC_INTERNAL_CHANNELS["ADC1"], (16,)),
            ("dac_channels", lambda d: d["bindings"]["hardware"]["dac_channels"][0]["pins"].update(PA4=2),
             lambda h: h.DAC_PIN_CHANNELS[("DAC", "PA4")], 2),
            ("pwm_bus", lambda d: d["bindings"]["hardware"]["pwm_timers"]["TIM3"].update(apb=2),
             lambda h: h.PWM_TIMERS["TIM3"].apb, 2),
            ("adc_trigger", lambda d: d["bindings"]["hardware"]["adc_triggers"].update({"tim3-trgo": "TIM5"}),
             lambda h: h.ADC_TRIGGERS["tim3-trgo"], "TIM5"),
            ("dac_dma", lambda d: d["bindings"]["hardware"]["dac_dma_routes"].update({"1": ["DMA1:stream6:channel7"]}),
             lambda h: h.DAC_DMA_ROUTES[1], ("DMA1:stream6:channel7",)),
            ("pwm_channel", lambda d: next(r for r in d["routes"]["routes"] if r["id"] == "tim3-ch1-pa6")["pins"][0].update(function="ch2"),
             lambda h: h.PWM_PIN_CHANNELS[("TIM3", "PA6")], 2),
            ("uart_dma", lambda d: next(r for r in d["routes"]["routes"] if r["id"] == "usart1-pa9-pa10").update(dma=["DMA2:stream6:channel4"]),
             lambda h: next(b for b in h.CONTROLLER_BINDINGS if b.controller == "USART1").dma("dma-tx"), ((2, 6, 4),)),
            ("secondary", lambda d: d["soc"]["controller_groups"]["rm0090-common"]["controllers"]["USART2"].update(clock_hz=40000000),
             lambda h: h.SECONDARY_FACTS["USART2"]["clock_hz"], 40000000),
        )
        for name, mutate, observe, expected in cases:
            with self.subTest(fact=name):
                self.documents = json.loads(baseline)
                mutate(self.documents)
                self.write()
                self.assertEqual(observe(self.load()), expected)

    def test_irq_vectors_are_derived_from_the_selected_official_catalog(self):
        catalog = next(d for d in self.documents["device_catalog"]["devices"]
                       if d["id"] == "stm32f407")["irq_bindings"]
        catalog["DMA1_Stream0_IRQn"]["number"], catalog["DMA1_Stream1_IRQn"]["number"] = 12, 11
        catalog["USART1_IRQn"]["number"], catalog["USART2_IRQn"]["number"] = 38, 37
        self.write()
        facts = self.load()
        self.assertEqual(dict(facts.DMA_IRQS)[1][0], 12)
        uart = next(b for b in facts.CONTROLLER_BINDINGS if b.controller == "USART1")
        self.assertEqual(uart.irqs, (("USART1", 38),))

    def test_facts_retain_their_loaded_bytes_and_nested_views_are_immutable(self):
        facts = self.load()
        self.assertEqual(dict(facts.FACT_SHA256), {
            name: hashlib.sha256(self.package.fact(name).read_bytes()).hexdigest()
            for name in self.documents})
        with self.assertRaises(TypeError):
            facts.ADC_PIN_CHANNELS["ADC1", "PA0"] = 99
        with self.assertRaises(TypeError):
            facts.SECONDARY_FACTS["USART2"]["clock_hz"] = 1

    def test_malformed_hardware_records_reject_without_python_type_errors(self):
        baseline = json.dumps(self.documents)
        mutations = (
            lambda d: d["bindings"]["hardware"].update(unreviewed=True),
            lambda d: d["bindings"]["hardware"]["pwm_timers"]["TIM3"].update(apb=True),
            lambda d: d["bindings"]["hardware"]["adc_channels"][0]["pins"].update(PZ99=1),
            lambda d: d["bindings"]["hardware"]["dac_channels"][0]["pins"].update(PA4=0),
            lambda d: d["bindings"]["hardware"]["dac_dma_routes"].update({"1": [True]}),
            lambda d: d["bindings"]["hardware"].update(adc_internal_channels=[]),
            lambda d: d["bindings"]["hardware"].update(controller_group=[]),
            lambda d: d["bindings"]["hardware"]["controllers"][0].update(route=[]),
        )
        for index, mutate in enumerate(mutations):
            with self.subTest(case=index):
                self.documents = json.loads(baseline)
                mutate(self.documents)
                self.write()
                with self.assertRaises(ValueError):
                    self.load()


if __name__ == "__main__":
    unittest.main()
