"""Serial recipes preserve selected layout and C ownership without family code."""
import json
import unittest

from tools.configure import configure
from tools.configure.data_provider import Binding
from tools.configure.static_ir import Array, Record, Symbol
from tools.configure.static_fragments import StaticFragments


class SerialRecipeTests(unittest.TestCase):
    def test_uart_profiles_preserve_complete_static_construction(self):
        document = json.loads((configure.ROOT /
            "soc/stm32/stm32f4/bindings/uart.json").read_bytes())
        fragments = StaticFragments.from_validated(document.get("static_fragments", {}))
        definitions = tuple(Binding.create(value, fragments=fragments)
                            for value in document["bindings"])
        cases = ["uart_rx_compact_assembly/f407-" + mode + ".toml" for mode in (
            "irq", "dma-tx", "rx-dma-idle", "rx-dma-continuous",
            "dma-duplex", "dma-duplex-continuous", "mixed-rx-tx", "mixed-duplex-tx")]
        cases += ["io_evolution_assembly/f407-uart7e2.toml",
                  "io_evolution_assembly/f407-uart9n1.toml",
                  "io_evolution_assembly/f407-uart9n1-probe.toml",
                  "stm32_assembly/zg-blocks.toml", "uart_wire_assembly/six-wire-zg.toml",
                  "multi_controller_assembly/stm32-zg-freertos.toml",
                  "uart_rx_compact_assembly/f407-alternate-dma.toml"]
        for relative in cases:
            result = configure.resolve_ir(configure.ROOT / "tests/contracts" / relative)
            for item in result.controllers:
                if item.kind != "uart":
                    continue
                with self.subTest(assembly=relative, controller=item.controller, mode=item.mode):
                    binding, = (value for value in definitions if item.mode in value.modes and
                        (not value.controllers or item.controller in value.controllers))
                    actual = binding.recipe.construct(
                        item, result, result.board_bindings, result.routes, result.devices)
                    self.assert_selected_contract(actual, item)

    def assert_selected_contract(self, construction, item):
        # Compare typed wiring with the resolved resource contract, rather than
        # another emitter. GoogleTest exercises these declarations with real C.
        self.assertEqual(construction.definitions, ())
        self.assertTrue(construction.retain_on_failed_init)
        self.assertTrue(construction.initialize.function.endswith("_cold_prepare"))
        self.assertTrue(construction.stop[0].function.endswith("_cold_close"))
        self.assertEqual(construction.initialize.arguments,
                         construction.stop[0].arguments)
        declarations = construction.static_declarations
        pins, = (node for node in declarations
                 if node.type_name == "nx_stm32_platform_pin_config_t")
        self.assertTrue(pins.const)
        self.assertEqual(pins.count, len(item.integration.pads))
        self.assertIsInstance(pins.initializer, Array)
        self.assertEqual(len(pins.initializer.values), len(item.integration.pads))
        for node, pad in zip(pins.initializer.values, item.integration.pads):
            values = dict(node.fields)
            self.assertEqual((values["registers"], values["port_index"],
                              values["pin"]),
                             (Symbol(pad.registers), pad.port_index, pad.number))

        config, = (node for node in declarations if node.type_name in (
            "nx_stm32_uart_cold_config_t", "nx_stm32_uart_dma_cold_config_t"))
        self.assertTrue(config.const)
        values = dict(config.initializer.fields)
        uart = dict(values["uart"].fields) if "uart" in values else values
        self.assertEqual(uart["registers"], Symbol(item.controller))
        self.assertEqual(uart["clock_hz"], item.integration.clock_hz)
        self.assertEqual(uart["baud"], item.options.baud)
        self.assertEqual(uart["priority"], item.options.irq.priority)
        if item.mode == "irq-words":
            self.assertIsNone(uart["format"])
            self.assertEqual((item.options.frame.data_bits, item.options.frame.parity,
                              item.options.frame.stop_bits), (9, "none", 1))
            receive, = (node for node in declarations
                        if node.name == "s_nx_rx_" + item.id.replace("-", "_"))
            self.assertEqual(receive.type_name, "uint16_t")
        elif item.options.frame is not None:
            frame, = (node for node in declarations
                      if node.type_name == "nx_uart_format_t")
            self.assertTrue(frame.const)
            self.assertEqual(dict(frame.initializer.fields), {
                "data_bits": item.options.frame.data_bits,
                "parity": Symbol("NX_UART_PARITY_" + item.options.frame.parity.upper()),
                "stop": item.options.frame.stop_bits,
                "flow": Symbol("NX_UART_FLOW_" + item.options.frame.flow.upper())})
        vectors, = (node for node in declarations if node.type_name == "IRQn_Type")
        self.assertTrue(vectors.const)
        selected_irqs = {entry.irq for entry in item.integration.vector_bindings}
        selected_irqs.update(entry.irq for entry in item.integration.dma_bindings)
        self.assertEqual({entry.name for entry in vectors.initializer.values},
                         selected_irqs)
        self.assertEqual(uart["vector_count"], len(selected_irqs))

        if not item.integration.dma_bindings:
            self.assertNotIn("resources", values)
            return
        resources = dict(values["resources"].fields)
        directions = ("transmit", "receive") if item.mode.startswith("dma-duplex") \
            else ("transmit",) if item.mode == "dma-tx" else ("receive",)
        self.assertEqual(len(directions), len(item.integration.dma_bindings))
        for direction, route in zip(directions, item.integration.dma_bindings):
            self.assertEqual(dict(resources[direction].fields), {
                "dma": Symbol(route.registers), "stream": Symbol(route.stream),
                "irq": Symbol(route.irq), "lane": route.lane,
                "channel": route.request})
        for direction in {"transmit", "receive"} - set(directions):
            self.assertEqual(resources[direction], Record(()))
        self.assertEqual(values["continuous"], item.mode.endswith("continuous"))
        regions, = (node for node in declarations
                    if node.type_name == "nx_dma_memory_region_t")
        self.assertTrue(regions.const)
        self.assertEqual(resources["region_count"], regions.count)


if __name__ == "__main__":
    unittest.main()
