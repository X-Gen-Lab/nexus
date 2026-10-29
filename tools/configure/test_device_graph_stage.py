"""Device decisions leave resolution as one frozen, complete typed graph.

USB FIFO placement, enhanced SPI faces and ADC trigger identity are decisions
across controller/endpoint boundaries. No consumer may observe an intermediate
controller before those decisions have been admitted.
"""

from collections.abc import Mapping
from dataclasses import FrozenInstanceError
from pathlib import Path
from unittest.mock import patch
import unittest

from tools.configure import configure
from tools.configure.ir import ControllerIR, EndpointIR, thaw


class DeviceGraphStageTests(unittest.TestCase):
    def resolve_graph(self, relative):
        observed = []
        stage = configure._resolve_device_graph

        def capture(assembly, **arguments):
            authored_before = thaw(assembly)
            graph = stage(assembly, **arguments)
            self.assertEqual(thaw(assembly), authored_before)
            observed.append(graph)
            return graph

        with patch.object(configure, "_resolve_device_graph", side_effect=capture):
            result = configure.resolve_ir(configure.ROOT / relative)
        self.assertEqual(len(observed), 1)
        return observed[0], result

    def test_empty_graph_has_typed_immutable_plans_and_no_builder_escape(self):
        graph, result = self.resolve_graph("tools/configure/assemblies/native.toml")
        self.assertNotIsInstance(graph, Mapping)
        self.assertEqual(graph.controllers, ())
        self.assertEqual(graph.devices, ())
        self.assertEqual(graph.inputs, ())
        self.assertIs(graph.resources, result.resource_plan)
        self.assertIs(graph.triggers, result.trigger_plan)
        self.assertIs(graph.dma, result.dma_topology)
        with self.assertRaises(FrozenInstanceError):
            graph.controllers = ()
        with self.assertRaises(TypeError):
            graph["controllers"]

    def test_usb_graph_contains_final_endpoint_order_and_nonoverlapping_fifos(self):
        graph, result = self.resolve_graph(
            "tests/contracts/usb_assembly/stm32-zg.toml")
        controller, = graph.controllers
        self.assertIsInstance(controller, ControllerIR)
        self.assertTrue(all(isinstance(item, EndpointIR) for item in graph.devices))
        self.assertIs(graph.devices, result.devices)
        self.assertEqual(tuple(item.options.index for item in graph.devices),
                         (0, 1, 2, 3))
        ranges = tuple((item.options.fifo_start, item.options.fifo_words)
                       for item in graph.devices if item.options.direction == "in")
        self.assertEqual(ranges, ((96, 32), (128, 32)))
        claims = {claim.key for claim in graph.resources.claims}
        self.assertIn("usb:OTG_FS:endpoint:0:in", claims)
        self.assertIn("usb:OTG_FS:endpoint:1:out", claims)
        with self.assertRaises(FrozenInstanceError):
            graph.devices[0].options.fifo_start = 0

    def test_spi_graph_selects_the_complete_sequence_face_before_publication(self):
        graph, result = self.resolve_graph(
            "tests/contracts/io_evolution_assembly/f407-spi-dma-transaction.toml")
        spi = next(item for item in graph.controllers if item.kind == "spi")
        self.assertEqual(spi.interface_kind, "spi_dma_transaction")
        self.assertEqual(tuple(item.controller for item in graph.devices), (spi.id,))
        self.assertEqual(graph.devices[0].driver, "spi-dma-transaction")
        self.assertEqual(next(item for item in result.controllers
                              if item.id == spi.id).interface_kind,
                         spi.interface_kind)
        owners = {item.id for item in graph.controllers}
        self.assertTrue(all(lane.owner in owners for lane in graph.dma.lanes))
        self.assertTrue(any(lane.owner == spi.id for lane in graph.dma.lanes))

    def test_adc_trigger_identity_and_review_input_belong_to_the_same_graph(self):
        graph, result = self.resolve_graph(
            "cmake/package/verification/io_evolution/"
            "g474-adc-group-regular-timer.toml")
        adc, = graph.controllers
        edge_index = next(index + 1 for index, edge in
                          enumerate(graph.triggers.edges)
                          if edge.target == adc.id + ":adc")
        self.assertEqual(adc.options.trigger_identity, edge_index)
        self.assertEqual(result.controllers[0].options.trigger_identity, edge_index)
        self.assertEqual(len(graph.inputs), 1)
        self.assertIsInstance(graph.inputs[0], Path)
        self.assertTrue(graph.inputs[0].is_file())
        self.assertIn(graph.inputs[0].relative_to(configure.ROOT).as_posix(),
                      {item["path"] for item in result.inputs})


if __name__ == "__main__":
    unittest.main()
