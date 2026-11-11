"""Hardware EXTI events own a pad/line without an IRQ or software queue."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure, factory


class ExtiHardwareConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def assembly(self, *, part="STM32F407VET6", fields="", peer=False,
                 hardware_pin="PA5", peer_pin="PB6"):
        facts = configure.load(configure.ROOT /
                               "boards/stm32f407zg_qiming_core/board.json")
        facts.update(id="exti-hardware-software-reference", soc=part,
                     physical_pcb_revision=None,
                     source_revision="software-chip-route-reference",
                     provenance=[], unknowns=["Physical wiring unverified"],
                     bindings=[{"id": "source", "controller": "EXTI5",
                                "route": "exti5-" + hardware_pin.lower()}])
        if peer:
            line = peer_pin[2:]
            facts["bindings"].append({"id": "observer",
                "controller": "EXTI" + line,
                "route": "exti" + line + "-" + peer_pin.lower()})
        (self.root / "board.json").write_text(json.dumps(facts))
        source = ('schema=2\nboard_package="."\nbackend="baremetal"\n'
                  'clock="hse8-pll168"\n[memory]\nmain_stack_bytes=2048\n'
                  'libc_heap_bytes=0\nstatic_ram_limit_bytes=16384\n'
                  'flash_load_limit_bytes=65536\n'
                  '[exti.source]\nbinding="source"\n'
                  'mode="hardware-event"\nedge="both"\npull="up"\n' + fields)
        if peer:
            source += ('\n[exti.observer]\nbinding="observer"\n'
                       'mode="edge-event"\nedge="falling"\n'
                       'event_capacity=2\nirq.priority=6\n')
        path = self.root / "assembly.toml"
        path.write_text(source)
        return path

    def test_both_parts_generate_a_static_face_without_irq_or_ring(self):
        for part in ("STM32F407VET6", "STM32F407ZGT6"):
            with self.subTest(part=part):
                ir = configure.resolve_ir(self.assembly(part=part))
                item, = ir.controllers
                self.assertEqual("both", item.options.edge)
                self.assertEqual("up", item.options.pull)
                self.assertFalse(hasattr(item.options, "irq"))
                self.assertFalse(hasattr(item.options, "event_capacity"))
                with self.assertRaises(FrozenInstanceError):
                    item.options.edge = "rising"
                keys = {claim.key for claim in ir.resource_plan.claims
                        if claim.owner == "source"}
                self.assertIn("controller:EXTI5", keys)
                self.assertIn("exti:line:5", keys)
                self.assertIn("pin:PA5", keys)
                self.assertFalse(any(key.startswith("irq:") for key in keys))
                header, source = bindings.emit(ir)
                self.assertIn("nx_binding_source", header)
                self.assertIn("nx_factory_exti", factory.emit(ir)[0])
                self.assertIn("nx_factory_exti_ref", factory.emit_strong(ir))
                self.assertIn("nx_stm32_exti_hardware_state_t", source)
                self.assertIn("nx_stm32_exti_hardware_ops", source)
                self.assertIn("nx_stm32_exti_hardware_prepare", source)
                self.assertIn("nx_stm32_exti_hardware_close", source)
                self.assertIn("s_nx_platform_syscfg_clock_added", source)
                self.assertNotIn("s_nx_events_", source)
                self.assertNotIn("EXTI9_5_IRQHandler", source)
                self.assertFalse(ir.physical_qualified)

    def test_hardware_source_and_shared_vector_irq_observer_remain_distinct(self):
        ir = configure.resolve_ir(self.assembly(peer=True))
        source = bindings.emit(ir)[1]
        self.assertEqual(1, source.count("void EXTI9_5_IRQHandler(void) {"))
        self.assertIn("s_nx_events_observer", source)
        self.assertNotIn("s_nx_events_source", source)
        self.assertIn("{ &s_nx_port_observer }", source)
        irq = [claim for claim in ir.resource_plan.claims
               if claim.key == "irq:EXTI9_5"]
        self.assertEqual(1, len(irq))
        self.assertEqual("observer", irq[0].owner)

    def test_event_and_irq_modes_cannot_own_different_pads_of_one_mux_line(self):
        with self.assertRaisesRegex(ValueError, "Resource conflict.*EXTI5"):
            configure.resolve_ir(self.assembly(peer=True, peer_pin="PB5"))

    def test_hardware_mode_rejects_irq_queue_and_internal_source_fields(self):
        for field in ("event_capacity=2\n", "irq.priority=5\n",
                      "irq.priority=5\nirq.calls_os=false\n",
                      "threshold_code=3\n"):
            with self.subTest(field=field), self.assertRaisesRegex(
                    ValueError, "hardware.*(IRQ|queue|storage)|threshold_code"):
                configure.resolve_ir(self.assembly(fields=field))

    def test_edge_and_pad_pull_remain_explicit_and_validated(self):
        for edge in ("rising", "falling", "both"):
            path = self.assembly()
            path.write_text(path.read_text().replace('edge="both"',
                                                     f'edge="{edge}"'))
            self.assertEqual(edge, configure.resolve_ir(
                path).controllers[0].options.edge)
        for edge in ('edge="wrong"', 'edge=1', ''):
            path = self.assembly()
            path.write_text(path.read_text().replace('edge="both"', edge))
            with self.subTest(edge=edge), self.assertRaises(ValueError):
                configure.resolve_ir(path)

    def test_all_gpio_exti_routes_offer_both_explicit_personalities(self):
        from tools.configure.tests.device_fixture import load_soc_facts
        package = configure.ROOT / "soc/stm32/stm32f4"
        soc = load_soc_facts(package / "soc.json")
        routes = configure.load(package / "routes.json")["routes"]
        for line in range(16):
            self.assertIn("hardware-event",
                          soc["controllers"][f"EXTI{line}"]["modes"])
        for route in routes:
            if route["kind"] == "exti" and route["controller"] != "EXTI16":
                with self.subTest(route=route["id"]):
                    self.assertIn("hardware-event", route["modes"])

    def test_checked_in_arm_fixtures_keep_all_sixteen_sources_queue_free(self):
        for name in ('ve-hardware-baremetal.toml',
                     'zg-hardware-freertos.toml'):
            path = (configure.ROOT / 'tests/contracts/gpio_exti_assembly' /
                    name)
            with self.subTest(fixture=name):
                ir = configure.resolve_ir(path)
                sources = [item for item in ir.controllers
                           if item.kind == 'exti']
                self.assertEqual(16, len(sources))
                self.assertTrue(all(item.mode == 'hardware-event'
                                    for item in sources))
                self.assertFalse(any(claim.key.startswith('irq:EXTI')
                                     for claim in ir.resource_plan.claims))
                source = bindings.emit(ir)[1]
                self.assertNotIn('s_nx_events_', source)
                self.assertNotIn('EXTI9_5_IRQHandler', source)


if __name__ == "__main__":
    unittest.main()
