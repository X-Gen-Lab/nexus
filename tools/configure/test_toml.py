"""Authored configuration and typed factory behavioral contracts."""
from contextlib import redirect_stdout
from dataclasses import FrozenInstanceError
import io
from pathlib import Path
import json
import shutil
import tempfile
import unittest
from unittest.mock import patch

import configure as gate


class TomlConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for relative in ("soc/native/soc.json", "soc/native/routes.json",
                         "boards/native_reference/board.json"):
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(gate.ROOT / relative, destination)
        self.assembly = self.root / "assembly.toml"
        self.contents = '''schema = 2
board = "native_reference"
backend = "native"
clock = "model"
[memory]
main_stack_bytes = 2048
libc_heap_bytes = 0
static_ram_limit_bytes = 32768
flash_load_limit_bytes = 65536
'''

    def resolve(self, extra=""):
        self.assembly.write_text(self.contents + extra, encoding="utf-8")
        return gate.resolve_ir(self.assembly, self.root)

    def test_minimal_board_id_resolves_to_typed_ir(self):
        ir = self.resolve()
        self.assertEqual(ir.board, "native-reference")
        self.assertEqual(ir.memory_budget.main_stack_bytes, 2048)
        self.assertEqual(ir.controllers, ())
        self.assertFalse(ir["physical_qualified"])
        with self.assertRaises(FrozenInstanceError):
            ir.board = "other"

    def test_nested_uart_rx_and_irq_contract(self):
        ir = self.resolve('''
[uart.link0]
binding = "uart0"
mode = "irq"
baud = 115200
[uart.link0.rx]
format = "events"
capacity = 64
[uart.link0.irq]
priority = 5
calls_os = false
''')
        uart, = ir.controllers
        self.assertEqual((uart.id, uart.kind, uart.controller),
                         ("link0", "uart", "UART0"))
        self.assertEqual(uart["mode"], "irq-byte-event")
        self.assertEqual(uart["rx_capacity"], 64)
        self.assertEqual(uart.provider, "native")

    def test_authored_kind_must_match_board_binding(self):
        with self.assertRaisesRegex(gate.ConfigurationError, "kind.*binding"):
            self.resolve('[gpio.wrong]\nbinding="uart0"\nmode="output"\n')

    def test_unknown_nested_rx_field_rejected(self):
        with self.assertRaisesRegex(gate.ConfigurationError, "unknown"):
            self.resolve('''[uart.link0]
binding="uart0"
mode="irq"
baud=115200
[ uart.link0.rx ]
format="events"
capacity=32
overwrite=true
[uart.link0.irq]
priority=5
''')

    def test_unknown_memory_field_rejected(self):
        self.contents += 'heap_dynamic = true\n'
        with self.assertRaisesRegex(gate.ConfigurationError, "unknown"):
            self.resolve()

    def test_unknown_board_id_is_not_path_fallback(self):
        self.contents = self.contents.replace("native_reference", "made-up")
        with self.assertRaisesRegex(gate.ConfigurationError, "Board.*package|board"):
            self.resolve()

    def test_external_board_package_is_explicit(self):
        self.contents = self.contents.replace(
            'board = "native_reference"',
            'board_package = "boards/native_reference"')
        self.assertEqual(self.resolve().board, "native-reference")

    def test_duplicate_table_rejected(self):
        with self.assertRaisesRegex(gate.ConfigurationError, "TOML"):
            self.resolve('[memory]\nmain_stack_bytes=1024\n')

    def test_pin_collision_rejected_atomically(self):
        output = self.root / "bundle"
        self.resolve('[gpio.first]\nbinding="led0"\nmode="output"\n')
        gate.configure(self.assembly, output, self.root)
        self.assembly.write_text(self.assembly.read_text() +
                                 '[gpio.second]\nbinding="led0"\nmode="output"\n')
        with self.assertRaisesRegex(gate.ConfigurationError, "Resource conflict"):
            gate.configure(self.assembly, output, self.root)
        self.assertFalse(output.exists())

    def test_generated_factory_is_pure_typed_switch(self):
        self.resolve('[gpio.indicator]\nbinding="led0"\nmode="output"\n')
        output = self.root / "bundle"
        gate.configure(self.assembly, output, self.root)
        header = (output / "nexus_factory.h").read_text()
        source = (output / "bindings.c").read_text()
        self.assertIn("NX_GPIO_ID_INDICATOR", header)
        self.assertIn("nx_factory_gpio(nx_gpio_id_t id)", header)
        factory = source[source.index("nx_factory_gpio(nx_gpio_id_t id)"):]
        self.assertIn("switch (id)", factory)
        self.assertIn("return NULL", factory)
        self.assertNotIn("strcmp", factory)
        self.assertNotIn("initialize", factory)
        self.assertNotIn("malloc", factory)

    def test_generated_factory_empty_class_is_defined(self):
        self.resolve()
        output = self.root / "bundle"
        gate.configure(self.assembly, output, self.root)
        self.assertIn("NX_UART_ID_COUNT = 0", (output / "nexus_factory.h").read_text())
        self.assertIn("nx_factory_uart", (output / "bindings.c").read_text())

    def test_virtual_spi_endpoints_have_explicit_independent_ids(self):
        ir = self.resolve("""
[spi.bus]
binding="spi0"
mode="short-poll"
[spi_device.sensor]
controller="bus"
[spi_device.memory]
controller="bus"
""")
        self.assertEqual([device.id for device in ir.devices],
                         ["sensor", "memory"])
        self.assertEqual([device.kind for device in ir.devices], ["spi", "spi"])

    def test_native_register_model_storage_is_exact_and_per_endpoint(self):
        ir = self.resolve("""
[spi.bus]
binding="spi0"
mode="short-poll"
[spi_device.sensor]
controller="bus"
model_bytes=64
[spi_device.memory]
controller="bus"
model_bytes=128
""")
        self.assertEqual([device["model_bytes"] for device in ir.devices],
                         [64, 128])
        output = self.root / "bundle"
        gate.configure(self.assembly, output, self.root)
        source = (output / "bindings.c").read_text()
        self.assertIn("s_nx_model_sensor[64]", source)
        self.assertIn("s_nx_model_memory[128]", source)
        self.assertNotIn("&g_nx_native_spi", source)

    def test_check_and_explain_use_same_typed_resolution(self):
        self.resolve('[gpio.indicator]\nbinding="led0"\nmode="output"\n')
        for command in ("check", "explain", "list-bindings"):
            with self.subTest(command=command), redirect_stdout(io.StringIO()) as output:
                status = gate.main([command, "--assembly", str(self.assembly),
                                    "--source-root", str(self.root)])
                self.assertEqual(status, 0)
                self.assertIn("native-reference", output.getvalue())
        self.assertFalse((self.root / "generated").exists())

    def test_init_is_minimal_and_refuses_overwrite(self):
        with redirect_stdout(io.StringIO()):
            status = gate.main(["init", "--board", "native_reference",
                                "--backend", "native", "--clock", "model",
                                "--output", str(self.assembly),
                                "--source-root", str(self.root)])
        self.assertEqual(status, 0)
        self.assertEqual(gate.resolve_ir(self.assembly, self.root).controllers, ())
        self.assertEqual(gate.main(["init", "--board", "native_reference",
                                   "--backend", "native", "--clock", "model",
                                   "--output", str(self.assembly),
                                   "--source-root", str(self.root)]), 1)

    def test_editor_schema_uses_resolver_field_model(self):
        import authored
        schema = authored.editor_schema()
        gpio = schema["properties"]["gpio"]["additionalProperties"]
        uart = schema["properties"]["uart"]["additionalProperties"]
        self.assertFalse(gpio["additionalProperties"])
        self.assertEqual(set(uart["properties"]), authored.INSTANCE_FIELDS["uart"])
        self.assertEqual(schema["properties"]["schema"]["const"], 2)
        maintained = gate.ROOT / "tools/configure/assembly.schema.json"
        self.assertEqual(json.loads(maintained.read_text()), schema)

    def test_dma_selector_does_not_create_second_physical_stream(self):
        self.assertEqual(gate.normalize_resource("dma:DMA2:stream3:channel2"),
                         gate.normalize_resource("dma:DMA2:stream3:channel3"))
        self.assertNotEqual(gate.normalize_resource("dma:DMA2:stream3:channel2"),
                            gate.normalize_resource("dma:DMA2:stream4:channel2"))

    def test_reviewed_uart_dma_emits_internal_resource_and_irq_wiring(self):
        source = gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml"
        self.assembly.write_text(source.read_text().replace('mode = "irq"',
                                                           'mode = "dma-tx"'))
        ir = gate.resolve_ir(self.assembly, gate.ROOT)
        uart = next(item for item in ir.controllers if item.kind == "uart")
        self.assertEqual(uart["dma"], ("DMA2:stream7:channel4",))
        self.assertIn("dma:DMA2:stream7", {item["key"] for item in ir["claims"]})
        output = self.root / "bundle"
        gate.configure(self.assembly, output, gate.ROOT)
        generated = (output / "bindings.c").read_text()
        self.assertIn("nx_stm32_uart_dma_state_t", generated)
        self.assertIn("&nx_stm32_uart_dma_ops", generated)
        self.assertIn("nx_stm32_uart_dma_initialize", generated)
        self.assertIn("DMA2_Stream7_IRQHandler", generated)
        self.assertIn("s_nx_port_uart1.uart.registers", generated)
        self.assertNotIn("0x10000000u", generated)

    def test_all_maintained_adc_toml_generate_from_immutable_ir(self):
        filenames = ("stm32_assembly/ve-adc", "stm32_assembly/zg-adc",
                     "stm32_assembly/ve-adc-scan", "stm32_assembly/zg-adc-scan",
                     "gd32_assembly/adc", "gd32_assembly/adc-scan")
        for filename in filenames:
            with self.subTest(fixture=filename):
                source = gate.ROOT / "tests/contracts" / (filename + ".toml")
                output = self.root / filename.replace("/", "-")
                gate.configure(source, output)
                self.assertIn("nx_factory_adc", (output / "bindings.c").read_text())

    def test_factory_count_sentinel_is_reserved_for_all_classes(self):
        for name in ("count", "COUNT", "Count"):
            with self.subTest(name=name):
                with self.assertRaisesRegex(gate.ConfigurationError, "Reserved factory"):
                    self.resolve(f'[gpio.{name}]\nbinding="led0"\nmode="output"\n')
                with self.assertRaisesRegex(gate.ConfigurationError, "Reserved factory"):
                    self.resolve('[spi.bus]\nbinding="spi0"\nmode="short-poll"\n'
                                 f'[spi_device.{name}]\ncontroller="bus"\n')

    def test_gd32_multi_controller_emits_explicit_at_and_vector_bindings(self):
        source = gate.ROOT / "tests/contracts/gd32_assembly/multi.toml"
        output = self.root / "multi-gd32"
        gate.configure(source, output)
        generated = (output / "bindings.c").read_text()
        for descriptor in ("usart0", "usart1", "spi0", "spi4", "i2c0", "i2c1"):
            self.assertIn(f"&nx_gd32_{descriptor}_controller", generated)
        self.assertIn("void USART0_IRQHandler(void)", generated)
        self.assertIn("void USART1_IRQHandler(void)", generated)
        self.assertIn("nx_gd32_uart_irq(&s_nx_port_link_b)", generated)
        self.assertIn(".cs_gpio = GPIOB", generated)
        self.assertIn(".cs_mask = 1u", generated)
        self.assertIn("nx_gd32_pin_restore", generated)

    def test_stm32_spi_dma_uses_exact_streams_and_endpoint_provider(self):
        source = gate.ROOT / "tests/contracts/stm32_assembly/zg-spi.toml"
        contents = source.read_text().replace('board_package = "zg_board"',
            'board_package = "' + str(source.parent / "zg_board") + '"')
        contents = contents.replace('mode = "short-poll"', 'mode = "dma"').replace(
            '\n[spi_device.', '\n[spi.spi0.irq]\npriority = 5\n\n[spi_device.', 1)
        self.assembly.write_text(contents)
        output = self.root / "spi-dma"
        gate.configure(self.assembly, output)
        generated = (output / "bindings.c").read_text()
        for text in ("nx_stm32_spi_dma_state_t", "nx_stm32_spi_dma_endpoint_state_t",
                     "nx_stm32_spi_dma_initialize", "&nx_stm32_spi_dma_endpoint_ops",
                     "void DMA2_Stream3_IRQHandler(void)", "void DMA2_Stream0_IRQHandler(void)",
                     "nx_spi_port_stop(nx_binding_spi0)"):
            self.assertIn(text, generated)
        claims = {claim["key"] for claim in gate.resolve_ir(self.assembly)["claims"]}
        self.assertTrue({"dma:DMA2:stream3", "dma:DMA2:stream0"} <= claims)

    def test_gd32_uart_dma_uses_vendor_channel_vector_and_shared_clock(self):
        source = gate.ROOT / "tests/contracts/gd32_assembly/multi.toml"
        contents = source.read_text().replace('board_package = "software_board"',
            'board_package = "' + str(source.parent / "software_board") + '"')
        self.assembly.write_text(contents.replace('mode = "irq"', 'mode = "dma-tx"', 1))
        output = self.root / "gd-uart-dma"
        gate.configure(self.assembly, output)
        generated = (output / "bindings.c").read_text()
        for text in ("nx_gd32_uart_dma_state_t", "nx_gd32_uart_dma_initialize",
                     "&nx_gd32_uart_dma_ops", "void DMA1_Channel7_IRQHandler(void)",
                     "nx_gd32_uart_dma_uart_irq(&s_nx_port_link_a)",
                     "RCU_AHB1EN_DMA1EN"):
            self.assertIn(text, generated)
        claims = {claim["key"] for claim in gate.resolve_ir(self.assembly)["claims"]}
        self.assertIn("irq:DMA1_Channel7", claims)
        self.assertNotIn("irq:DMA1_Stream7", claims)

    def test_gd32_spi_dma_and_uart_claim_disjoint_physical_engines(self):
        source = gate.ROOT / "tests/contracts/gd32_assembly/multi.toml"
        contents = source.read_text().replace('board_package = "software_board"',
            'board_package = "' + str(source.parent / "software_board") + '"')
        contents = contents.replace('mode = "irq"', 'mode = "dma-tx"', 1)
        contents = contents.replace('mode = "short-poll"',
            'mode = "dma-full-duplex"\n[spi.bus_spi_a.irq]\npriority = 5', 1)
        self.assembly.write_text(contents)
        output = self.root / "gd-spi-dma"
        gate.configure(self.assembly, output)
        generated = (output / "bindings.c").read_text()
        for text in ("nx_gd32_spi_dma_state_t", "nx_gd32_spi_dma_endpoint_state_t",
                     "&nx_gd32_spi_dma_endpoint_ops", "nx_gd32_spi_dma_initialize",
                     "void DMA1_Channel3_IRQHandler(void)", "void DMA1_Channel4_IRQHandler(void)",
                     "nx_spi_port_stop(nx_binding_bus_spi_a)"):
            self.assertIn(text, generated)
        claims = {claim["key"] for claim in gate.resolve_ir(self.assembly)["claims"]}
        self.assertTrue({"dma:DMA1:stream3", "dma:DMA1:stream4", "dma:DMA1:stream7"} <= claims)

    def test_uart_blocks_have_caller_storage_and_mode_specific_ops(self):
        source = gate.ROOT / "tests/contracts/stm32_assembly/zg-dma.toml"
        contents = source.read_text().replace('board_package = "zg_board"',
            'board_package = "' + str(source.parent / "zg_board") + '"')
        self.assembly.write_text(contents.replace('mode = "dma-tx"', 'mode = "irq-blocks"')
            .replace('format = "events"', 'format = "blocks"').replace('capacity = 8\n', ''))
        output = self.root / "uart-blocks"
        gate.configure(self.assembly, output)
        generated = (output / "bindings.c").read_text()
        for text in ("nx_stm32_uart_stream_state_t", "&nx_stm32_uart_stream_ops",
                     "NX_UART_RX_BLOCKS", ".rx_storage = NULL", ".rx_capacity = 0u",
                     "nx_stm32_uart_stream_irq(&s_nx_port_uart1)"):
            self.assertIn(text, generated)
        self.assertNotIn("s_nx_rx_uart1[", generated)
        claims = {claim["key"] for claim in gate.resolve_ir(self.assembly)["claims"]}
        self.assertNotIn("dma:DMA2:stream7", claims)
        with self.assertRaisesRegex(gate.ConfigurationError, "capacity"):
            self.assembly.write_text(self.assembly.read_text().replace('format = "blocks"', 'format = "blocks"\ncapacity = 8'))
            gate.resolve_ir(self.assembly)

    def test_uart_dma_tx_does_not_claim_unimplemented_rx_blocks_combination(self):
        source = gate.ROOT / "tests/contracts/stm32_assembly/zg-dma.toml"
        contents = source.read_text().replace('board_package = "zg_board"',
            'board_package = "' + str(source.parent / "zg_board") + '"')
        self.assembly.write_text(contents.replace('format = "events"', 'format = "blocks"').replace('capacity = 8\n', ''))
        with self.assertRaisesRegex(gate.ConfigurationError, "blocks.*irq-blocks"):
            gate.resolve_ir(self.assembly)

    def test_adc_trigger_dma_reserves_exact_timer_and_dma_without_block_storage(self):
        source = gate.ROOT / "tests/contracts/stm32_assembly/zg-adc.toml"
        contents = source.read_text().replace('board_package = "zg_board"',
            'board_package = "' + str(source.parent / "zg_board") + '"')
        self.assembly.write_text(contents.replace('mode = "single-shot"', 'mode = "trigger-dma"') +
            '\n[adc.adc0.irq]\npriority = 5\n')
        output = self.root / "adc-blocks"
        gate.configure(self.assembly, output)
        generated = (output / "bindings.c").read_text()
        for text in ("nx_stm32_adc_stream_state_t", "&nx_stm32_adc_stream_ops",
                     "nx_stm32_adc_stream_initialize", "DMA2_Stream4_IRQHandler",
                     "nx_adc_port_stream_stop(&s_nx_face_adc0)"):
            self.assertIn(text, generated)
        claims = {claim["key"] for claim in gate.resolve_ir(self.assembly)["claims"]}
        self.assertTrue({"controller:TIM3", "dma:DMA2:stream4"} <= claims)
        self.assembly.write_text(self.assembly.read_text() +
            '\n[pwm.pwm0]\nbinding="pwm0"\nmode="fixed-pwm"\nperiod_ticks=1000\nduty_ticks=100\ntick_hz=1000000\n')
        with self.assertRaisesRegex(gate.ConfigurationError, "Resource conflict controller:TIM3"):
            gate.resolve_ir(self.assembly)

    def test_gd32_adc_trigger_dma_exact_sample_contract_and_timer_claim(self):
        source = gate.ROOT / "tests/contracts/gd32_assembly/dma.toml"
        contents = source.read_text().replace('board_package = "software_board"',
            'board_package = "' + str(source.parent / "software_board") + '"')
        contents += ('\n[adc.adc0]\nbinding="adc0"\nmode="trigger-dma"\nchannels=[0]\n'
                     'sample_times=[1]\nreference_mv=3300\ntimeout_ms=100\n'
                     '[adc.adc0.irq]\npriority=5\n')
        self.assembly.write_text(contents)
        output = self.root / "gd-adc-blocks"
        gate.configure(self.assembly, output)
        generated = (output / "bindings.c").read_text()
        for text in ("nx_gd32_adc_stream_state_t", "&nx_gd32_adc_stream_ops",
                     "nx_gd32_adc_stream_initialize", "void DMA1_Channel0_IRQHandler(void)",
                     "nx_gd32_adc_stream_stop(&s_nx_port_adc0)"):
            self.assertIn(text, generated)
        claims = {claim["key"] for claim in gate.resolve_ir(self.assembly)["claims"]}
        self.assertTrue({"controller:TIMER2", "dma:DMA1:stream0"} <= claims)
        self.assembly.write_text(contents.replace('sample_times=[1]', 'sample_times=[7]'))
        with self.assertRaisesRegex(gate.ConfigurationError, "fifteen cycles"):
            gate.resolve_ir(self.assembly)

    def test_gd32_uart_blocks_use_selected_controller_without_generated_ring(self):
        source = gate.ROOT / "tests/contracts/gd32_assembly/dma.toml"
        contents = source.read_text().replace('board_package = "software_board"',
            'board_package = "' + str(source.parent / "software_board") + '"')
        contents = contents.replace('mode = "irq"', 'mode = "irq-blocks"').replace(
            '[uart.link_b.rx]\nformat = "events"\ncapacity = 4', '[uart.link_b.rx]\nformat = "blocks"')
        self.assembly.write_text(contents)
        output = self.root / "gd-uart-blocks"
        gate.configure(self.assembly, output)
        generated = (output / "bindings.c").read_text()
        for text in ("nx_gd32_uart_stream_state_t", "&nx_gd32_uart_stream_ops",
                     "nx_gd32_uart_stream_initialize_at(&s_nx_port_link_b, &nx_gd32_usart1_controller",
                     "nx_gd32_uart_stream_irq(&s_nx_port_link_b)"):
            self.assertIn(text, generated)
        self.assertNotIn("s_nx_rx_link_b[", generated)

    def test_fixed_dma_providers_reject_other_valid_physical_routes(self):
        cases = (("stm32f407", "stm32_assembly/zg-dma.toml", "USART1"),
                 ("stm32f407", "stm32_assembly/zg-dma.toml", "SPI1"),
                 ("stm32f407", "stm32_assembly/zg-blocks.toml", "ADC1"),
                 ("gd32f470", "gd32_assembly/dma.toml", "USART0"),
                 ("gd32f470", "gd32_assembly/dma.toml", "SPI4"),
                 ("gd32f470", "gd32_assembly/blocks.toml", "ADC0"))
        real_dependency = gate.dependency_identity
        # The temporary fact root reuses independently verified source SDKs.
        with patch.object(gate, "dependency_identity", side_effect=lambda root, relative, expected=None:
                          real_dependency(gate.ROOT, relative, expected)):
            for family, fixture, controller in cases:
                with self.subTest(provider=(family, controller)):
                    package = self.root / "soc" / family
                    package.mkdir(parents=True, exist_ok=True)
                    for filename in ("soc.json", "routes.json"):
                        shutil.copyfile(gate.ROOT / "soc" / family / filename, package / filename)
                    source = gate.ROOT / "tests/contracts" / fixture
                    normalized = gate.load_assembly(source)
                    normalized["board_package"] = str(source.parent / normalized["board_package"])
                    assembly = self.root / "canonical.json"
                    assembly.write_text(json.dumps(normalized))
                    ir = gate.resolve_ir(assembly, self.root)
                    selected_route = next(item["route"] for item in ir.controllers
                                          if item["controller"] == controller)
                    routes = json.loads((package / "routes.json").read_text())
                    route = next(route for route in routes["routes"] if route["id"] == selected_route)
                    route["dma"] = [f"{dma.split(':')[0]}:stream{6-index}:{dma.rsplit(':', 1)[1]}"
                                    for index, dma in enumerate(route["dma"])]
                    (package / "routes.json").write_text(json.dumps(routes))
                    for action in (lambda: gate.resolve_ir(assembly, self.root),
                                   lambda: gate.configure(assembly, self.root / "rejected", self.root)):
                        with self.assertRaisesRegex(gate.ConfigurationError, "DMA route differs from fixed provider"):
                            action()
                    self.assertFalse((self.root / "rejected" / "bindings.c").exists())

    def test_timer_reservation_alias_excludes_adc_and_pwm(self):
        for family, stem, timer in (("stm32_assembly", "zg", "TIM3"),
                                   ("gd32_assembly", "", "TIMER2")):
            for kind in ("blocks", "pwm"):
                with self.subTest(family=family, kind=kind):
                    filename = ((stem + "-") if stem else "") + kind + ".toml"
                    source = gate.ROOT / "tests/contracts" / family / filename
                    normalized = gate.load_assembly(source)
                    original_board = source.parent / normalized["board_package"]
                    board = json.loads((original_board / "board.json").read_text())
                    board["reserved_resources"].append("timer:" + timer)
                    board_package = self.root / "timer_board"
                    board_package.mkdir(exist_ok=True)
                    (board_package / "board.json").write_text(json.dumps(board))
                    normalized["board_package"] = str(board_package)
                    assembly = self.root / "timer.json"
                    assembly.write_text(json.dumps(normalized))
                    with self.assertRaisesRegex(gate.ConfigurationError, "Resource conflict controller:" + timer):
                        gate.resolve_ir(assembly)

    def test_fixed_adc_trigger_cannot_omit_its_physical_timer_claim(self):
        real_dependency = gate.dependency_identity
        with patch.object(gate, "dependency_identity", side_effect=lambda root, relative, expected=None:
                          real_dependency(gate.ROOT, relative, expected)):
            for family, fixture in (("stm32f407", "stm32_assembly/zg-blocks.toml"),
                                    ("gd32f470", "gd32_assembly/blocks.toml")):
                with self.subTest(family=family):
                    package = self.root / "soc" / family
                    package.mkdir(parents=True, exist_ok=True)
                    for filename in ("soc.json", "routes.json"):
                        shutil.copyfile(gate.ROOT / "soc" / family / filename, package / filename)
                    source = gate.ROOT / "tests/contracts" / fixture
                    normalized = gate.load_assembly(source)
                    normalized["board_package"] = str(source.parent / normalized["board_package"])
                    assembly = self.root / "trigger.json"
                    assembly.write_text(json.dumps(normalized))
                    ir = gate.resolve_ir(assembly, self.root)
                    selected = next(item for item in ir.controllers if item["kind"] == "adc")
                    routes = json.loads((package / "routes.json").read_text())
                    route = next(route for route in routes["routes"] if route["id"] == selected["route"])
                    route["mode_resources"] = {}
                    (package / "routes.json").write_text(json.dumps(routes))
                    with self.assertRaisesRegex(gate.ConfigurationError, "Trigger resource differs from fixed provider"):
                        gate.resolve_ir(assembly, self.root)

    def test_gd32_timebase_is_reserved_by_soc_for_every_board(self):
        source = gate.ROOT / "tools/configure/assemblies/liangshan-baremetal-empty.toml"
        normalized = gate.load_assembly(source)
        board = json.loads((Path(normalized["board_package"]) / "board.json").read_text())
        board["reserved_resources"] = []
        package = self.root / "timebase_board"
        package.mkdir()
        (package / "board.json").write_text(json.dumps(board))
        normalized["board_package"] = str(package)
        assembly = self.root / "timebase.json"
        assembly.write_text(json.dumps(normalized))
        claims = {claim["key"]: claim["owner"] for claim in gate.resolve_ir(assembly)["claims"]}
        self.assertEqual(claims.get("controller:TIMER1"), "soc-timebase")
        self.assertEqual(claims.get("irq:TIMER1"), "soc-timebase")
        board["reserved_resources"] = ["TIMER1", "timer:TIMER1", "irq:TIMER1_IRQn"]
        (package / "board.json").write_text(json.dumps(board))
        gate.resolve_ir(assembly)

    def test_spi_child_frequency_below_provider_divider_is_rejected(self):
        source = gate.ROOT / "tests/contracts/gd32_assembly/spi.toml"
        normalized = gate.load_assembly(source)
        normalized["board_package"] = str(source.parent / normalized["board_package"])
        normalized["devices"][0]["max_hz"] = 1000
        assembly = self.root / "canonical.json"
        assembly.write_text(json.dumps(normalized))
        with self.assertRaisesRegex(gate.ConfigurationError, "SPI endpoint speed"):
            gate.resolve_ir(assembly)

    def test_duplicate_builtin_spi_chip_select_rejected(self):
        source = gate.ROOT / "tests/contracts/gd32_assembly/spi.toml"
        self.assembly.write_text(source.read_text().replace(
            'board_package = "software_board"',
            'board_package = "' + str(source.parent / "software_board") + '"') +
            '[spi_device.duplicate]\ncontroller="spi0"\n')
        with self.assertRaisesRegex(gate.ConfigurationError, "Duplicate SPI chip select"):
            gate.resolve_ir(self.assembly)

    def test_typed_ir_and_resolved_json_share_one_identity(self):
        ir = self.resolve('[gpio.indicator]\nbinding="led0"\nmode="output"\n')
        output = self.root / "bundle"
        gate.configure(self.assembly, output, self.root)
        emitted = json.loads((output / "resolved.json").read_text())
        self.assertEqual(emitted, ir.to_dict())
        self.assertEqual(emitted["configuration_sha256"],
                         ir["configuration_sha256"])
