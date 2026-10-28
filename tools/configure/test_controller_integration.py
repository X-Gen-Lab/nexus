"""Selected controller integration reuses the canonical immutable plans."""
from dataclasses import FrozenInstanceError, replace
import unittest

from tools.configure import bindings, configure as gate
from tools.configure.providers import maintained
from tools.configure.providers.common import owned_claims


class ControllerIntegrationTests(unittest.TestCase):
    def test_primary_spi_rejects_foreign_or_reordered_selected_dma(self):
        result = gate.resolve_ir(gate.ROOT /
            "tests/contracts/stm32_assembly/ve-dma.toml")
        item = next(item for item in result.controllers
                    if item.controller == "SPI1")
        lanes = item.integration.dma
        invalid = (tuple(reversed(lanes)),
                   (replace(lanes[0], owner="foreign"), lanes[1]),
                   (replace(lanes[0], controller="SPI2"), lanes[1]))
        provider = maintained("stm32f407")
        for selected in invalid:
            with self.subTest(lanes=selected):
                changed = replace(item, integration=replace(
                    item.integration, dma=selected))
                with self.assertRaisesRegex(ValueError, "integration"):
                    provider.constructor(changed, result,
                        result.board_bindings, result.routes, result.devices)

    def test_cold_resource_projection_reads_the_canonical_plan(self):
        result = gate.resolve_ir(gate.ROOT /
            "tests/contracts/stm32_assembly/ve-exti.toml")
        owners = {item.id for item in result.controllers}
        selected = tuple(owned_claims(result, owners))
        self.assertTrue(selected)
        self.assertTrue(all(any(claim is canonical for canonical in
                                result.resource_plan.claims)
                            for claim in selected))

    def test_selected_pair_keeps_resource_dma_and_irq_owners(self):
        result = gate.resolve_ir(gate.ROOT /
            "tests/contracts/multi_controller_assembly/stm32-zg.toml")
        for item in result.controllers:
            with self.subTest(controller=item.id):
                selected = getattr(item, "integration", None)
                self.assertIsNotNone(selected,
                    "Production controllers need their selected integration")
                expected = tuple(claim for claim in result.resource_plan.claims
                    if item.id in claim.owner.split(";") and
                    claim.mode not in {"reserved", "kernel"})
                self.assertEqual(selected.resources, expected)
                self.assertTrue(all(actual is original for actual, original
                                    in zip(selected.resources, expected)))
                lanes = tuple(lane for lane in result.dma_topology.lanes
                              if lane.owner == item.id)
                self.assertEqual(selected.dma, lanes)
                self.assertTrue(all(actual is original for actual, original
                                    in zip(selected.dma, lanes)))
                self.assertIs(selected.irq, item.options.irq)
                self.assertIs(selected.irq_profile, result.irq)
                self.assertIs(selected.clock_plan, result.clock_plan)
                self.assertIs(selected.clock_inputs, result.clock)
                self.assertIs(selected.pins, item.pins)
                self.assertTrue(any(selected.hardware is binding for binding
                    in maintained(result.soc_family).CONTROLLER_BINDINGS))
                with self.assertRaises(FrozenInstanceError):
                    selected.controller = "foreign"

    def test_shared_exti_vector_remains_owned_by_both_selected_lines(self):
        result = gate.resolve_ir(gate.ROOT /
            "tests/contracts/stm32_assembly/ve-exti.toml")
        shared = next(claim for claim in result.resource_plan.claims
                      if claim.key == "irq:EXTI9_5")
        owners = set(shared.owner.split(";"))
        for item in result.controllers:
            if item.id in owners:
                selected = getattr(item, "integration", None)
                self.assertIsNotNone(selected)
                self.assertTrue(any(claim is shared
                                    for claim in selected.resources))

    def test_physical_part_and_selected_pins_reuse_frozen_values(self):
        result = gate.resolve_ir(gate.ROOT /
            "tests/contracts/stm32_assembly/ve-flash.toml")
        part = getattr(result, "physical_part", None)
        self.assertIsNotNone(part)
        self.assertEqual(part.part, "STM32F407VET6")
        self.assertEqual(part.package, "LQFP100")
        self.assertIs(part.flash, result.flash)
        self.assertIs(part.memory, result.memory)
        with self.assertRaises(TypeError):
            part.flash["size"] = 1
        self.assertIn(".size = 524288u", bindings.emit(result)[1])


if __name__ == "__main__":
    unittest.main()
