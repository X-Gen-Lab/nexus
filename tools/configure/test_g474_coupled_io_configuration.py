"""Normal G474 coupled owners keep one typed face and exact hardware facts."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import tempfile
import tomllib
import unittest

from tools.configure import bindings, configure, factory, ir


class G474CoupledIOConfiguration(unittest.TestCase):
    root = configure.ROOT / "tests/contracts/g474_control_assembly"

    def fixture(self, name):
        choices = {"pwm-adc": "g474-pwm-adc",
                   "can-fd": "g474-can-fd-loopback",
                   "can-fd-normal": "g474-can-fd-normal"}
        return (configure.ROOT / "cmake/package/verification/io_evolution" /
                (choices[name] + ".toml"))

    def resolve(self, name, before=None, after=None, extra=""):
        fixture = self.fixture(name)
        contents = fixture.read_text()
        package = tomllib.loads(contents)["board_package"]
        contents = contents.replace(
            'board_package = "' + package + '"',
            'board_package = "' + str(fixture.parent / package) + '"')
        if before is not None:
            self.assertIn(before, contents)
            contents = contents.replace(before, after)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "assembly.toml"
            path.write_text(contents + extra)
            return configure.resolve_ir(path)

    def test_pwm_adc_is_one_immutable_tim1_personality(self):
        result = self.resolve("pwm-adc")
        self.assertEqual(len(result.controllers), 1)
        item = result.controllers[0]
        self.assertEqual((item.kind, item.controller, item.mode),
                         ("pwm_adc", "TIM1", "finite-regular-dma"))
        self.assertIsInstance(item.options, ir.PwmAdcOptions)
        self.assertEqual((item.options.tick_hz, item.options.period_ticks,
                          item.options.trigger_ticks, item.options.deadtime_ticks),
                         (1000000, 100, 50, 170))
        self.assertEqual(item.options.channels, (1,))
        self.assertEqual(item.options.sample_times, (0,))
        with self.assertRaises(FrozenInstanceError):
            item.options.trigger_ticks = 51
        self.assertFalse(result.physical_qualified)

    def test_pwm_adc_factory_does_not_expose_independently_startable_subfaces(self):
        result = self.resolve("pwm-adc")
        header, getters = factory.emit(result)
        self.assertIn("const nx_pwm_adc_t* nx_factory_pwm_adc(", header)
        self.assertIn("NX_PWM_ADC_ID_WINDOW", header)
        self.assertIn("return nx_binding_window;", getters)
        self.assertNotIn("NX_ADC_ID_WINDOW", header)
        self.assertNotIn("NX_PWM_GROUP_ID_WINDOW", header)
        self.assertNotIn("initialize(", getters)
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32g474_pwm_adc_initialize(", source)
        self.assertIn("nx_stm32g474_pwm_adc_close(", source)
        self.assertIn("nx_stm32g474_pwm_adc_dma_irq(", source)
        self.assertIn("nx_stm32g474_pwm_adc_break_irq(", source)
        self.assertNotIn("&nx_stm32g474_adc_ops, &s_nx_port_window", source)

    def test_pwm_adc_owns_every_subresource_and_real_trigger_edge(self):
        result = self.resolve("pwm-adc")
        owners = {claim.key: claim.owner for claim in result.claims}
        expected = {"controller:TIM1", "controller:ADC1", "controller:ADC2",
                    "controller:ADC345", "controller:COMP1", "controller:TIM15",
                    "dma:DMA1:stream1", "controller:DMAMUX1_Channel0",
                    "irq:DMA1_Channel1", "irq:TIM1_BRK_TIM15"}
        self.assertTrue(expected.issubset(owners), expected - set(owners))
        self.assertTrue(all(owners[key] == "window" for key in expected))
        lane, = result.dma_topology.lanes
        self.assertEqual((lane.owner, lane.engine, lane.lane, lane.request),
                         ("window", "DMA1", 1, 5))
        edges = result.trigger_plan.edges
        trigger = next(edge for edge in edges if edge.target == "window:adc")
        self.assertEqual((trigger.source, trigger.event, trigger.action),
                         ("window:trigger", "oc4ref-rising", "sample"))
        self.assertIn("TIM1:TRGO2", trigger.route)
        self.assertNotIn("TIM6", trigger.route)

    def test_pwm_adc_rejects_unimplemented_fixed_window_or_channels(self):
        for before, after in (
                ("tick_hz = 1000000", "tick_hz = 2000000"),
                ("period_ticks = 100", "period_ticks = 101"),
                ("trigger_ticks = 50", "trigger_ticks = 49"),
                ("deadtime_ticks = 170", "deadtime_ticks = 171"),
                ("channels = [1]", "channels = [2]"),
                ("sample_times = [0]", "sample_times = [1]"),
                ("exclusive = true", "exclusive = false"),
                ("priority = 5", "priority = 4")):
            with self.subTest(after=after), self.assertRaises(ValueError):
                self.resolve("pwm-adc", before, after)

    def test_joint_window_rejects_independent_adc_or_tim1_group_owners(self):
        selections = (
            '\n[adc.sensor]\nbinding="sensor"\nmode="single-shot"\n'
            'channels=[1]\nsample_times=[0]\nreference_mv=3300\n',
            '\n[pwm_group.wave]\nbinding="wave"\nmode="coherent-pwm"\n'
            'tick_hz=1000000\nperiod_ticks=1000\ndeadtime_ticks=170\n'
            'exclusive=true\n[pwm_group.wave.irq]\npriority=5\ncalls_os=false\n')
        for extra in selections:
            with self.subTest(extra=extra), self.assertRaisesRegex(
                    ValueError, "conflict|owner|TIM1|ADC1|PA0"):
                self.resolve("pwm-adc", extra=extra)

    def test_joint_constructor_closes_before_comparator_or_gate_release(self):
        result = self.resolve("pwm-adc")
        source = bindings.emit(result)[1]
        start = source.index("static nx_result_t stop_window(void)")
        close = source.index("nx_stm32g474_pwm_adc_close(", start)
        restore = source.index("NX_G474_WRITE(&COMP1->CSR", close)
        self.assertLess(close, restore)
        self.assertNotIn("static bool s_nx_vector_window;", source)
        self.assertIn("DMA1_Channel1_IRQHandler", source)
        self.assertIn("TIM1_BRK_TIM15_IRQHandler", source)

    def test_can_fd_keeps_its_own_typed_value_and_filter_plan(self):
        result = self.resolve("can-fd")
        item, = result.controllers
        self.assertEqual((item.kind, item.controller, item.mode),
                         ("can_fd", "FDCAN1", "iso-fd-loopback"))
        self.assertIsInstance(item.options, ir.CanFdOptions)
        self.assertEqual((item.options.bitrate, item.options.data_bitrate,
                          item.options.event_capacity),
                         (500000, 2000000, 4))
        filter_, = item.options.filters
        self.assertEqual((filter_.id, filter_.mask, filter_.format, filter_.fifo),
                         (256, 2047, "standard", 0))
        with self.assertRaises(FrozenInstanceError):
            filter_.mask = 0
        self.assertFalse(result.physical_qualified)

    def test_can_fd_factory_is_distinct_from_classic_can(self):
        result = self.resolve("can-fd")
        header, getters = factory.emit(result)
        self.assertIn("const nx_can_fd_port_t* nx_factory_can_fd(", header)
        self.assertIn("NX_CAN_FD_ID_FIELDBUS", header)
        self.assertNotIn("NX_CAN_ID_FIELDBUS", header)
        self.assertIn("return nx_binding_fieldbus;", getters)
        self.assertNotIn("initialize(", getters)
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32g474_fdcan_domain_initialize(", source)
        self.assertIn("nx_stm32g474_fdcan_initialize(", source)
        self.assertIn("nx_stm32g474_fdcan_domain_stop(", source)
        self.assertNotIn("HAL_FDCAN_Init(", source)

    def test_can_fd_owns_physical_ram_and_both_vectors_with_one_publisher(self):
        result = self.resolve("can-fd")
        claims = {claim.key for claim in result.claims}
        self.assertTrue({"controller:FDCAN1", "message-ram:FDCAN1",
                         "clock:FDCAN", "irq:FDCAN1_IT0",
                         "irq:FDCAN1_IT1"}.issubset(claims))
        item, = result.controllers
        self.assertEqual(item.options.message_ram_words, 212)
        source = bindings.emit(result)[1]
        self.assertIn("SRAMCAN_BASE", source)
        self.assertIn("FDCAN_CONFIG", source)
        self.assertIn("24000000u", source)
        self.assertIn("FDCAN1_IT0_IRQHandler", source)
        self.assertNotIn("FDCAN1_IT1_IRQHandler", source)
        self.assertNotIn("TIM3_IRQHandler", source)

    def test_can_fd_rejects_unreviewed_bus_or_operation_modes(self):
        for before, after in (
                ('mode = "iso-fd-loopback"', 'mode = "normal"'),
                ("bitrate = 500000", "bitrate = 250000"),
                ("data_bitrate = 2000000", "data_bitrate = 4000000"),
                ("event_capacity = 4", "event_capacity = 0"),
                ("exclusive = true", "exclusive = false"),
                ("priority = 5", "priority = 6")):
            with self.subTest(after=after), self.assertRaises(ValueError):
                self.resolve("can-fd", before, after)

    def test_can_fd_enforces_real_standard_and_extended_filter_bounds(self):
        before = 'filters = [{ id = 256, mask = 2047, format = "standard", fifo = 0 }]'
        invalid = ('[{ id = 2048, mask = 2047, format = "standard", fifo = 0 }]',
                   '[{ id = 1, mask = 2047, format = "standard", fifo = 2 }]',
                   '[{ id = 1, mask = 2047, format = "unknown", fifo = 0 }]',
                   '[' + ', '.join(
                       '{ id = 1, mask = 2047, format = "standard", fifo = 0 }'
                       for _ in range(29)) + ']',
                   '[' + ', '.join(
                       '{ id = 1, mask = 536870911, format = "extended", fifo = 1 }'
                       for _ in range(9)) + ']')
        for values in invalid:
            with self.subTest(values=values), self.assertRaises(ValueError):
                self.resolve("can-fd", before, "filters = " + values)
        result = self.resolve("can-fd", before, "filters = []")
        self.assertEqual(result.controllers[0].options.filters, ())

    def test_can_fd_normal_preserves_the_same_physical_owner_and_filter_plan(self):
        loopback = self.resolve("can-fd")
        normal = self.resolve("can-fd-normal")
        item, = normal.controllers
        self.assertEqual((item.kind, item.controller, item.mode),
                         ("can_fd", "FDCAN1", "iso-fd-normal"))
        self.assertEqual(item.options, loopback.controllers[0].options)
        def physical_claims(result):
            return [(claim.key, claim.owner, claim.source)
                    for claim in result.claims]
        self.assertEqual(physical_claims(normal), physical_claims(loopback))
        for result, mode in ((normal, "iso-fd-normal"),
                             (loopback, "iso-fd-loopback")):
            self.assertTrue(all(claim.mode == mode
                                for claim in result.claims
                                if claim.owner == "fieldbus"))
        self.assertFalse(normal.physical_qualified)
        self.assertEqual(item.options.message_ram_words, 212)
        with self.assertRaises(FrozenInstanceError):
            item.mode = "iso-fd-loopback"

    def test_can_fd_modes_emit_explicit_values_in_the_same_selected_storage(self):
        for fixture, value in (("can-fd", "NX_STM32G474_FDCAN_INTERNAL_LOOPBACK"),
                               ("can-fd-normal", "NX_STM32G474_FDCAN_NORMAL")):
            with self.subTest(fixture=fixture):
                result = self.resolve(fixture)
                source = bindings.emit(result)[1]
                self.assertIn("s_nx_port_fieldbus.mode = " + value + ";", source)
                self.assertIn("static nx_stm32g474_fdcan_state_t s_nx_port_fieldbus;",
                              source)
                self.assertEqual(source.count("nx_stm32g474_fdcan_initialize("), 1)
                header, getters = factory.emit(result)
                self.assertIn("const nx_can_fd_port_t* nx_factory_can_fd(", header)
                self.assertNotIn("initialize(", getters)

    def test_can_fd_uses_one_mode_selector_without_invented_peer_hardware(self):
        for fixture in ("can-fd", "can-fd-normal"):
            for extra in ('operation = "normal"', 'transceiver = "present"',
                          'termination_ohms = 120'):
                with self.subTest(fixture=fixture, extra=extra), \
                        self.assertRaises(ValueError):
                    self.resolve(fixture, "bitrate = 500000",
                                 "bitrate = 500000\n" + extra)

    def test_can_fd_normal_reaches_the_atomic_bundle(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "normal"
            configure.configure(self.fixture("can-fd-normal"), output)
            self.assertTrue((output / ".nexus-config-bundle").is_file())
            self.assertIn("NX_STM32G474_FDCAN_NORMAL",
                          (output / "bindings.c").read_text())
            self.assertIn("NX_CAN_FD_ID_FIELDBUS",
                          (output / "nexus_factory.h").read_text())

    def test_new_faces_reach_the_atomic_bundle_without_weakening_existing_modes(self):
        with tempfile.TemporaryDirectory() as temporary:
            for name in ("pwm-adc", "can-fd"):
                with self.subTest(name=name):
                    output = Path(temporary) / name
                    configure.configure(self.fixture(name), output)
                    self.assertTrue((output / ".nexus-config-bundle").is_file())
                    self.assertIn("nx_factory_" + name.replace("-", "_"),
                                  (output / "nexus_factory.h").read_text())
        for name in ("pwm-group", "timer-capture", "encoder"):
            result = configure.resolve_ir(self.root / (name + ".toml"))
            self.assertFalse(result.physical_qualified)


if __name__ == "__main__":
    unittest.main()
