"""Exact support and selected public contracts share one readonly projection.

These configure/emission tests do not replace actual provider and generated
Platform GoogleTests, linked resource measurements or physical qualification.
"""
from dataclasses import FrozenInstanceError, replace
import importlib
import json
from pathlib import Path
import tempfile
import tomllib
import unittest

from tools.configure import matrix, bindings, configure, dependencies, factory
from tools.configure.ir import ControllerIR


class SelectedContractAuthorityTests(unittest.TestCase):
    def compiler(self):
        return importlib.import_module("tools.configure.selected_contracts")

    def selected(self, fixture):
        result = configure.resolve_ir(configure.ROOT / fixture)
        records = self.compiler().for_configuration(result)
        return result, records

    def test_seven_canonical_inputs_keep_selected_mechanisms(self):
        root = "cmake/package/verification/io_evolution/"
        choices = (
            ("native-time-pair", {"rtc", "lptim"},
             "nx_native_lptim_construct("),
            ("native-time-empty", set(), "nx_platform_start_impl("),
            ("h743-high-empty", set(),
             "nx_stm32h743_pll384_system_start("),
            ("h743-high-uart", {"gpio", "uart"},
             "nx_stm32h743_pll384_system_start("),
            ("u575-uart-gpdma", {"uart"},
             "nx_stm32u575_uart_gpdma_initialize("),
            ("g474-can-fd-loopback", {"can_fd"},
             ".mode = NX_STM32G474_FDCAN_INTERNAL_LOOPBACK"),
            ("g474-can-fd-normal", {"can_fd"},
             ".mode = NX_STM32G474_FDCAN_NORMAL"),
            ("g474-pwm-adc", {"pwm_adc"},
             "nx_stm32g474_pwm_adc_initialize("),
            ("g474-pwm-shared", {"pwm"},
             "nx_stm32g474_pwm_shared_initialize("),
            ("h743-uart-rtscts", {"uart"},
             "nx_stm32h743_uart_rtscts_initialize("),
            ("u575-uart-rtscts", {"uart"},
             "nx_stm32u575_uart_rtscts_initialize("))
        self.assertEqual({path.stem for path in
                          (configure.ROOT / root).glob("*.toml")},
                         {choice[0] for choice in choices} | {
                             "g474-adc-group-" + group + "-" + trigger
                             for group in ("regular", "injected")
                             for trigger in ("software", "timer")} |
                         {"h743-uart-fifo", "u575-uart-fifo",
                          "f407zg-eth-frames", "gd470-eth-frames"})
        for name, expected, mechanism in choices:
            with self.subTest(assembly=name):
                result, records = self.selected(root + name + ".toml")
                self.assertEqual({row.public_class for row in records},
                                 expected)
                self.assertIn(mechanism, bindings.emit(result)[1])
                self.assertFalse(result["physical_qualified"])

    def test_canonical_h743_high_flash_keeps_exact_public_interfaces(self):
        result, records = self.selected(
            "tools/configure/assemblies/h743-high-peripherals.toml")
        self.assertEqual({row.public_class for row in records},
                         {"gpio", "uart", "spi", "spi_device", "pwm", "i2c",
                          "i2c_device", "adc", "flash"})
        self.assertIn("nx_stm32h743_pll384_system_start(",
                      bindings.emit(result)[1])
        self.assertFalse(result["physical_qualified"])

    def test_catalogue_has_eight_exact_parts_without_family_inference(self):
        rows = matrix.support_catalogue(configure.ROOT)
        self.assertEqual({row.part for row in rows}, {
            "STM32F103RBT6", "STM32F407VET6", "STM32F407ZGT6",
            "STM32G474RET6", "STM32H743ZIT6", "STM32U575ZIT6Q",
            "GD32F303RCT6", "GD32F470ZGT6"})
        self.assertTrue(all(not row.physical_qualified for row in rows))
        self.assertTrue(all(row.review_sources for row in rows))

    def test_canonical_h743_spi_dma_keeps_exact_faces_lanes_and_mechanism(self):
        root = "tools/configure/assemblies/"
        variants = {"h743-spi-dma": None,
                    "h743-spi-dma-cache-on": None,
                    "h743-spi-dma-runtime-clock": None,
                    "h743-spi-dma-uart-tx": (0, 46),
                    "h743-spi-dma-uart-rx": (1, 45)}
        self.assertEqual({path.stem for path in
                          (configure.ROOT / root).glob("h743-spi-dma*.toml")},
                         set(variants))
        for name, uart in variants.items():
            with self.subTest(assembly=name):
                result, records = self.selected(root + name + ".toml")
                self.assertEqual({row.public_class for row in records},
                                 {"spi", "spi_device"} |
                                 ({"uart"} if uart is not None else set()))
                self.assertEqual({(row.lane, row.request)
                                  for row in result.dma_topology.lanes},
                                 {(2, 83), (3, 84)} |
                                 ({uart} if uart is not None else set()))
                body = bindings.emit(result)[1]
                self.assertIn("nx_stm32h743_spi_dma_initialize(", body)
                self.assertIn("&nx_stm32h743_spi_dma_endpoint_ops", body)
                for row in records:
                    self.assertFalse(row.physical_qualified)
                spi = next(row for row in records if row.public_class == "spi")
                self.assertEqual((spi.unit, spi.word_bits, spi.dma_engines,
                                  spi.cache_line_bytes),
                                 ("bytes", (8,), ("DMA1",),
                                  32 if name.endswith("cache-on") else 0))
                with self.assertRaises(FrozenInstanceError):
                    spi.unit = "words"

    def test_unrouted_f407ve_ethernet_is_not_inherited_from_the_zg_route(self):
        rows = matrix.support_catalogue(configure.ROOT)
        ethernet = {row.part: row for row in rows
                    if row.controller == "ETH" and row.mode == "raw-rmii"}
        self.assertEqual(set(ethernet), {"STM32F407VET6", "STM32F407ZGT6"})
        self.assertFalse(ethernet["STM32F407VET6"].reviewed_route_available)
        self.assertEqual(ethernet["STM32F407VET6"].reviewed_routes, ())
        self.assertTrue(ethernet["STM32F407ZGT6"].reviewed_route_available)
        self.assertTrue(ethernet["STM32F407ZGT6"].reviewed_routes)

    def test_reviewed_gpio_route_cannot_release_the_oscillator_pad(self):
        rows = matrix.support_catalogue(configure.ROOT)
        gpio, = [row for row in rows if row.part == "STM32G474RET6" and
                 row.controller == "GPIOF" and row.mode == "input"]
        self.assertIn("gpio-pf0", gpio.reviewed_routes)
        board_source = configure.ROOT / (
            "tests/contracts/g474_control_assembly/board/board.json")
        board = json.loads(board_source.read_text())
        board["id"] = "g474-oscillator-reservation-negative-software-fixture"
        board["bindings"] = [{"id": "probe", "controller": "GPIOF",
                              "route": "gpio-pf0"}]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "board.json").write_text(json.dumps(board))
            assembly = root / "assembly.toml"
            assembly.write_text(
                'schema = 2\nboard_package = "."\nbackend = "baremetal"\n'
                'clock = "hse24-pll170"\n[memory]\nmain_stack_bytes = 2048\n'
                '[gpio.probe]\nbinding = "probe"\nmode = "input"\n')
            with self.assertRaisesRegex(ValueError,
                                        r"Resource conflict pin:PF0"):
                configure.resolve_ir(assembly)

    def test_nine_bit_words_keep_nominal_units_and_frozen_projection(self):
        _, records = self.selected(
            "tests/contracts/io_evolution_assembly/f407-uart9n1.toml")
        record, = [row for row in records if row.public_class == "uart_word"]
        self.assertEqual((record.unit, record.unit_bytes, record.word_bits),
                         ("words", 2, (9,)))
        self.assertFalse(record.physical_qualified)
        with self.assertRaises(FrozenInstanceError):
            record.unit_bytes = 1

    def test_dma_segment_count_is_words_not_the_parent_byte_limit(self):
        _, records = self.selected(
            "tests/contracts/io_evolution_assembly/"
            "f407-spi-dma-transaction.toml")
        endpoint, = [row for row in records
                      if row.public_class == "spi_dma_transaction_device"]
        self.assertEqual(endpoint.unit, "words")
        self.assertIsNone(endpoint.unit_bytes)
        self.assertEqual(endpoint.word_bits, (8, 16))
        self.assertEqual(endpoint.max_segment_units, "UINT16_MAX")
        self.assertEqual(endpoint.max_segments, "NX_SPI_MAX_SEGMENTS")
        self.assertEqual(endpoint.max_total_units,
                         "NX_SPI_MAX_SEGMENTS * UINT16_MAX")

    def test_spi_segment_count_does_not_become_an_i2c_message_count(self):
        _, records = self.selected(
            "tests/contracts/io_evolution_assembly/"
            "f407-spi-dma-transaction.toml")
        for record in records:
            if record.public_class.startswith("spi_dma_transaction"):
                with self.subTest(public_class=record.public_class):
                    self.assertIsNone(record.max_messages)
                    self.assertEqual(record.max_segments,
                                     "NX_SPI_MAX_SEGMENTS")

    def test_polling_endpoint_width_does_not_change_the_byte_controller(self):
        _, records = self.selected(
            "tests/contracts/io_evolution_assembly/f407-spi-transaction.toml")
        controller, = [row for row in records if row.public_class == "spi"]
        endpoint, = [row for row in records
                      if row.public_class == "spi_transaction_device"]
        self.assertEqual(controller.fields["word_bits"], 8)
        self.assertEqual(controller.word_bits, (8,))
        self.assertEqual(endpoint.word_bits, (8, 16))
        self.assertEqual(endpoint.fields["word_bits"], 0)

    def test_selected_endpoint_ceiling_is_its_authored_device_ceiling(self):
        fixture = configure.ROOT / (
            "tests/contracts/io_evolution_assembly/f407-spi-transaction.toml")
        original = fixture.read_text().replace(
            'board_package = "../stm32_assembly/zg_board"',
            'board_package = "' + str(configure.ROOT /
                                      "tests/contracts/stm32_assembly/zg_board")
            + '"')
        beginning, endpoint = original.split("[spi_device.sensor0]", 1)
        endpoint = endpoint.replace("max_hz = 1000000", "max_hz = 500000", 1)
        for driver, namespace in (("spi-transaction", "spi_transaction_device"),
                                   ("spi-endpoint", "spi_device")):
            with self.subTest(driver=driver), \
                    tempfile.TemporaryDirectory() as temporary:
                path = Path(temporary) / "assembly.toml"
                contents = beginning + "[spi_device.sensor0]" + endpoint
                path.write_text(contents.replace('driver = "spi-transaction"',
                                                  'driver = "' + driver + '"'))
                result = configure.resolve_ir(path)
                records = self.compiler().for_configuration(result)
                controller, = [row for row in records
                                if row.public_class == "spi"]
                selected, = [row for row in records
                              if row.public_class == namespace]
                self.assertEqual(controller.fields["configured_max_hz"],
                                 1000000)
                self.assertEqual(selected.fields["configured_max_hz"], 500000)

    def test_coupled_adc_and_fd_do_not_acquire_ordinary_subface_traits(self):
        for fixture, public_class in (("g474-pwm-adc", "pwm_adc"),
                                     ("g474-can-fd-normal", "can_fd")):
            with self.subTest(fixture=fixture):
                _, records = self.selected(
                    "cmake/package/verification/io_evolution/" + fixture
                    + ".toml")
                self.assertEqual({row.public_class for row in records},
                                 {public_class})
                record, = records
                self.assertFalse(record.physical_qualified)
                self.assertNotEqual(record.timestamp_source, "hardware_sof")

    def test_current_u575_gpdma_remains_bytes_with_exact_named_engine(self):
        _, records = self.selected(
            "cmake/package/verification/io_evolution/u575-uart-gpdma.toml")
        uart, = [row for row in records if row.public_class == "uart"]
        self.assertEqual((uart.unit, uart.unit_bytes, uart.word_bits),
                         ("bytes", 1, (8,)))
        self.assertEqual(uart.dma_engines, ("GPDMA1",))
        self.assertEqual(uart.max_transfer_units, "UINT16_MAX")

    def test_existing_high_clock_policy_is_v_only_and_explicitly_scoped(self):
        result = configure.resolve_ir(configure.ROOT /
            "cmake/package/verification/io_evolution/h743-high-empty.toml")
        policy = self.compiler().silicon_policy(result)
        self.assertEqual(policy.reviewed_revision_ids, (0x2003,))
        self.assertEqual(policy.unknown_revision, "reject")
        self.assertEqual(policy.review_scope, "selected_clock_profile")
        self.assertFalse(policy.physical_qualified)
        with self.assertRaises(FrozenInstanceError):
            policy.reviewed_revision_ids = (0x1003, 0x2003)

    def test_silicon_projection_keeps_the_selected_controller_owner(self):
        result = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/h743-spi-dma-uart-tx.toml")
        compiler = self.compiler()
        policies = compiler.silicon_policies(result)
        spi = next(item for item in result.controllers if item.kind == "spi")
        uart = next(item for item in result.controllers if item.kind == "uart")
        self.assertIsNone(policies[None])
        self.assertIsNone(policies[uart.id])
        self.assertEqual(policies[spi.id].review_scope,
                         "selected_spi_dma_mode")
        with self.assertRaises(TypeError):
            policies[uart.id] = policies[spi.id]
        with self.assertRaisesRegex(ValueError, "owner"):
            compiler.silicon_policy(result, replace(spi, id="not-selected"))
        with self.assertRaisesRegex(ValueError, "owner"):
            compiler.silicon_policy(result, uart)
        foreign = replace(spi, provider="native")
        with self.assertRaisesRegex(ValueError, "owner"):
            compiler.silicon_policy(replace(result, controllers=(foreign,)),
                                    foreign)

    def test_caller_authored_observation_or_workaround_claim_is_rejected(self):
        fixture = configure.ROOT / (
            "cmake/package/verification/io_evolution/h743-high-empty.toml")
        package = tomllib.loads(fixture.read_text())["board_package"]
        original = fixture.read_text().replace(
            'board_package = "' + package + '"',
            'board_package = "' + str((fixture.parent / package).resolve())
            + '"')
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "assembly.toml"
            path.write_text(original)
            baseline = configure.resolve_ir(path)
            self.assertEqual(baseline.controllers, ())
            self.assertEqual(baseline.clock_plan.reviewed_revision_ids,
                             (0x2003,))
        for field in ("observed_revision=8195",
                      "applied_workarounds=4294967295",
                      "reviewed_revision_ids=[4099,8195]"):
            with self.subTest(field=field), \
                    tempfile.TemporaryDirectory() as temporary:
                path = Path(temporary) / "assembly.toml"
                path.write_text(original + "\n[silicon]\n" + field + "\n")
                with self.assertRaisesRegex(ValueError, "silicon"):
                    configure.resolve_ir(path)


class ExistingConsumerAuthorityTests(unittest.TestCase):
    def test_selected_soc_policy_helpers_belong_to_the_actual_input_closure(self):
        helpers = {
            family: f"soc/stm32/{family}/configure/selected_support.py"
            for family in ("stm32h743", "stm32u575")}
        for family, expected in helpers.items():
            with self.subTest(family=family):
                paths, identity = dependencies.snapshot(mode="platform",
                                                        family=family)
                self.assertIn(expected, paths)
                self.assertEqual(sum(row["path"] == expected for row in
                                     identity["files"]), 1)
                self.assertTrue(all(other == expected or other not in paths
                                    for other in helpers.values()))
        native, _ = dependencies.snapshot(mode="platform", family="native")
        self.assertTrue(set(helpers.values()).isdisjoint(native))

    def test_selected_compiler_is_a_real_configuration_dependency(self):
        paths, identity = dependencies.snapshot(mode="platform",
                                                family="stm32g474")
        helper = "tools/configure/selected_contracts.py"
        self.assertIn(helper, paths)
        self.assertEqual(sum(row["path"] == helper
                             for row in identity["files"]), 1)

    def test_factory_metadata_requires_an_implemented_provider_mode(self):
        # A known public shape alone cannot authorize this actual provider.
        item = ControllerIR.create({
            "id": "capture", "binding": "capture", "kind": "timer_capture",
            "controller": "TIM5", "mode": "edge-capture",
            "tick_hz": 1000000, "edge": "rising", "filter": 0,
            "exclusive": True, "irq_priority": 5}, "stm32f103")
        with self.assertRaisesRegex(ValueError, "Unmaintained provider mode"):
            factory._capability(item, False)

    def test_factory_model_flag_cannot_disagree_with_its_actual_provider(self):
        item = ControllerIR.create({
            "id": "pin", "binding": "pin", "kind": "gpio",
            "controller": "GPIO0", "mode": "input", "mask": 1,
            "initial": 0}, "native")
        with self.assertRaisesRegex(ValueError, "model.*provider"):
            factory._capability(item, False)

    def test_capability_class_cannot_invent_a_face_for_another_kind(self):
        item = ControllerIR.create({
            "id": "pin", "binding": "pin", "kind": "gpio",
            "controller": "GPIO0", "mode": "input", "mask": 1,
            "initial": 0}, "native")
        with self.assertRaisesRegex(ValueError, "class.*selected"):
            factory._capability(item, True, public_class="mdio")


if __name__ == "__main__":
    unittest.main()
