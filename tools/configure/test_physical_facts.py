"""Conditional pads and engine-specific memory access remain exact facts."""
import unittest


class PhysicalFactBoundaryTests(unittest.TestCase):
    def test_mixed_dma_reachability_requires_an_explicit_engine(self):
        from tools.configure import emission
        result = {"memory": [{"id": "dtcm", "origin": 0x20000000,
                              "size": 131072, "linker": True,
                              "dma_engines": {"DMA1": False, "MDMA": True}}]}
        with self.assertRaisesRegex(ValueError, "engine"):
            emission.dma_regions("port", result)
        lines, count = emission.dma_regions("port", result, engine="MDMA")
        self.assertEqual(count, 1)
        self.assertIn("0x20000000", "\n".join(lines))
        _, count = emission.dma_regions("port", result, engine="DMA1")
        self.assertEqual(count, 0)
        with self.assertRaises(ValueError):
            emission.dma_regions("port", result, engine="nearest-DMA")

    def test_engine_ram_facts_do_not_authorize_flash_read_access(self):
        from tools.configure import emission
        result = {"memory": [{"id": "sram4", "origin": 0x38000000,
                              "size": 65536,
                              "dma_engines": {"BDMA": True, "MDMA": True}}],
                  "flash": {"origin": 0x08000000, "size": 2097152}}
        for engine in ("BDMA", "MDMA"):
            with self.subTest(engine=engine):
                with self.assertRaisesRegex(ValueError, "Flash.*reachability"):
                    emission.dma_regions("port", result, include_flash=True,
                                         engine=engine)
        # Existing shared-reachability families keep their reviewed Flash path.
        result["memory"][0] = {"origin": 0x20000000, "size": 131072,
                               "dma": True}
        lines, count = emission.dma_regions("port", result, include_flash=True)
        self.assertEqual(count, 2)
        self.assertIn("0x08000000", "\n".join(lines))

    def test_each_declared_engine_has_an_explicit_boolean_per_domain(self):
        from tools.configure import physical_facts
        engines = ("DMA1", "MDMA")
        physical_facts.memory_access({"dma_engines": {"DMA1": False,
                                                      "MDMA": True}}, engines)
        for region in ({"dma": False}, {"dma_engines": {"DMA1": False}},
                       {"dma_engines": {"DMA1": False, "MDMA": 1}},
                       {"dma": True, "dma_engines": {"DMA1": False,
                                                       "MDMA": True}},
                       {"dma_engines": {"DMA1": False, "MDMA": True,
                                        "BDMA": False}}):
            with self.subTest(region=region), self.assertRaises(ValueError):
                physical_facts.memory_access(region, engines)

    def test_legacy_shared_reachability_cannot_accept_an_unregistered_engine(self):
        from tools.configure import physical_facts
        physical_facts.memory_access({"dma": False}, ())
        with self.assertRaises(ValueError):
            physical_facts.memory_access({"dma_engines": {"MDMA": True}}, ())

    def test_conditional_pad_never_becomes_an_unconditional_gpio_alias(self):
        from tools.configure import physical_facts
        condition = {"gpio": "PC2", "condition": "SYSCFG.PMCR switch required",
                     "source": "DS12110 Rev11 p57; RM0433 Rev9",
                     "status": "not_exposed"}
        physical_facts.package_pins(["PB0", "PC2_C"], {"PC2_C": condition},
                                    ("PC2_C",))
        for pins, conditions, allowed in (
                (["PB0", "PC2_C"], {}, ("PC2_C",)),
                (["PB0", "PC2_C"], {"PC2_C": condition}, ()),
                (["PB0", "PC2_C"], {"PC2_C": {**condition, "gpio": "PC3"}},
                 ("PC2_C",)),
                (["PB0", "PC2_C"], {"PC2_C": {**condition, "status": "ready"}},
                 ("PC2_C",)),
                (["PB0", "PC2"], {"PC2_C": condition}, ("PC2_C",))):
            with self.subTest(pins=pins, conditions=conditions):
                with self.assertRaises(ValueError):
                    physical_facts.package_pins(pins, conditions, allowed)


if __name__ == "__main__":
    unittest.main()
