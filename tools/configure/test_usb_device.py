"""Exact selected USB directions/FIFOs, cold clock ownership and typed faces."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


class UsbConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def assembly(self, family="stm32-zg", mutate=None):
        source = gate.ROOT / "tests/contracts/usb_assembly" / (family + ".toml")
        content = source.read_text()
        package = source.parent / (family + "-board")
        content = content.replace(f'board_package="{family}-board"',
                                  'board_package=' + json.dumps(str(package)))
        if mutate is not None:
            content = mutate(content)
        path = self.root / "assembly.toml"
        path.write_text(content)
        return path

    def test_three_parts_resolve_exact_software_only_fs_irq_and_no_dma_claim(self):
        for family in ("stm32-zg", "stm32-ve", "gd32"):
            with self.subTest(family=family):
                ir = gate.resolve_ir(self.assembly(family))
                self.assertFalse(ir["physical_qualified"])
                self.assertEqual(len(ir.controllers), 1)
                self.assertEqual(len(ir.devices), 4)
                self.assertEqual(ir.controllers[0].options.rx_fifo_words, 96)
                self.assertFalse(ir.controllers[0].options.vbus_sensing)
                with self.assertRaises(FrozenInstanceError):
                    ir.controllers[0].options.rx_fifo_words = 100
                claims = {item["key"] for item in ir["claims"]}
                self.assertIn("pin:PA11", claims)
                self.assertIn("pin:PA12", claims)
                self.assertIn("irq:USBFS" if family == "gd32" else "irq:OTG_FS", claims)
                self.assertFalse(any(item.startswith("dma:") for item in claims))
                self.assertIn("clock:USBFS48", claims)
                if family == "gd32":
                    self.assertIn("clock:CK48", claims)
                    self.assertIn("clock:PLLSAIP", claims)

    def interrupt_endpoint(self, **changes):
        fields = {"controller": '"fs"', "number": "2", "direction": '"in"',
                  "transfer": '"interrupt"', "max_packet": "10",
                  "fifo_words": "16"}
        fields.update(changes)
        return ("\n[usb_endpoint.notification]\n" +
                "\n".join(key + "=" + value for key, value in fields.items()
                          if value is not None) + "\n")

    def test_interrupt_in_preserves_exact_typed_plan_and_existing_bulk_ports(self):
        for family in ("stm32-zg", "stm32-ve", "gd32"):
            with self.subTest(family=family):
                path = self.assembly(family, mutate=lambda text:
                                     text + self.interrupt_endpoint())
                ir = gate.resolve_ir(path)
                options = next(item.options for item in ir.devices
                               if item.id == "notification")
                self.assertEqual((options.number, options.direction,
                                  options.transfer, options.max_packet,
                                  options.fifo_start,
                                  options.fifo_words),
                                 (2, "in", "interrupt", 10, 160, 16))
                self.assertEqual(len(ir.devices), 5)
                self.assertFalse(ir["physical_qualified"])
                with self.assertRaises(FrozenInstanceError):
                    options.transfer = "bulk"
                claims = {item["key"] for item in ir["claims"]}
                controller = "USBFS" if family == "gd32" else "OTG_FS"
                self.assertIn("usb:" + controller + ":endpoint:2:in", claims)
                self.assertFalse(any(item.startswith("dma:") for item in claims))
                output = self.root / (family + "-interrupt")
                gate.configure(path, output)
                source = (output / "bindings.c").read_text()
                self.assertIn("static nx_usb_endpoint_t s_nx_usb_endpoints_fs[5]",
                              source)
                self.assertIn("NX_USB_PROFILE_INTERRUPT", source)
                self.assertIn("NX_USB_PROFILE_BULK", source)
                self.assertNotIn("malloc", source)

    def test_interrupt_in_accepts_full_speed_non_power_of_two_packet_bounds(self):
        for maximum in (1, 10, 64):
            with self.subTest(maximum=maximum):
                path = self.assembly(mutate=lambda text: text +
                    self.interrupt_endpoint(max_packet=str(maximum)))
                ir = gate.resolve_ir(path)
                options = next(item.options for item in ir.devices
                               if item.id == "notification")
                self.assertEqual(options.max_packet, maximum)

    def test_interrupt_in_rejects_unsupported_and_ambiguous_authored_fields(self):
        changes = ({"max_packet": "0"}, {"max_packet": "65"},
                   {"max_packet": "true"}, {"direction": '"out"',
                    "fifo_words": None}, {"number": "0"}, {"number": "4"},
                   {"transfer": '"isochronous"'}, {"interval_frames": "16"},
                   {"fifo_words": "15"}, {"fifo_words": "161"},
                   {"polling_worker": "true"})
        for change in changes:
            with self.subTest(change=change), self.assertRaises(ValueError):
                gate.resolve_ir(self.assembly(mutate=lambda text:
                    text + self.interrupt_endpoint(**change)))
        for legacy in ('transfer="control"', 'transfer="bulk"'):
            with self.subTest(legacy=legacy), self.assertRaises(ValueError):
                gate.resolve_ir(self.assembly(mutate=lambda text:
                    text.replace(legacy, legacy + "\ninterval_frames=1", 1)))

    def test_interrupt_in_keeps_optional_profile_emission_opt_out(self):
        path = self.assembly(mutate=lambda text:
            text.replace("capabilities=true", "capabilities=false") +
            self.interrupt_endpoint())
        output = self.root / "interrupt-no-capabilities"
        gate.configure(path, output)
        header = (output / "nexus_factory.h").read_text()
        source = (output / "bindings.c").read_text()
        self.assertNotIn("nx_factory_usb_profile", header)
        self.assertNotIn("nx_usb_endpoint_profile_t", source)
        self.assertIn("static nx_usb_endpoint_t s_nx_usb_endpoints_fs[5]", source)

    def test_fifo_starts_are_one_typed_plan_with_exact_selected_storage(self):
        ir = gate.resolve_ir(self.assembly())
        directions = {(item.options.number, item.options.direction): item.options
                      for item in ir.devices}
        self.assertEqual(directions[(0, "in")].fifo_start, 96)
        self.assertEqual(directions[(1, "in")].fifo_start, 128)
        self.assertEqual(directions[(1, "out")].fifo_start, 0)
        with self.assertRaises(FrozenInstanceError):
            ir.devices[0].options.max_packet = 8
        output = self.root / "generated"
        gate.configure(self.assembly(), output)
        source = (output / "bindings.c").read_text()
        header = (output / "nexus_factory.h").read_text()
        self.assertIn("static nx_usb_endpoint_t s_nx_usb_endpoints_fs[4]", source)
        self.assertIn("static nx_usb_event_t s_nx_usb_events_fs[8]", source)
        self.assertIn("const nx_usb_device_port_t* nx_factory_usb_device(", header)
        self.assertIn("const nx_usb_endpoint_port_t* nx_factory_usb_endpoint(", header)
        self.assertIn("&nx_usb_fs_device_ops, &s_nx_port_fs", source)
        self.assertIn("&nx_usb_fs_endpoint_ops, &s_nx_usb_endpoints_fs[", source)
        self.assertNotIn("malloc", source)

    def test_cold_failure_is_counted_before_prepare_and_cleanup_order_is_proved(self):
        output = self.root / "generated"
        gate.configure(self.assembly(), output)
        source = (output / "bindings.c").read_text()
        prepare = source.index("result.primary = prepare_fs();")
        self.assertLess(source.index("s_nx_platform_initialized = 1u;", source.index(
            "nx_platform_start_result_t nx_platform_start")), prepare)
        stop = source.index("static nx_result_t stop_fs(void)")
        device = source.index("nx_usb_device_stop(&s_nx_face_fs)", stop)
        clock = source.index("nx_stm32_usb_clock_stop", device)
        pins = source.index("nx_stm32_pin_restore", clock)
        self.assertLess(device, clock)
        self.assertLess(clock, pins)
        self.assertIn("nx_stm32_pin_capture", source[pins:])

    def test_packet_type_direction_number_and_fifo_bounds_fail_closed(self):
        changes = (("rx_fifo_words=96", "rx_fifo_words=63"),
                   ("fifo_words=32", "fifo_words=15"),
                   ("fifo_words=32", "fifo_words=240"),
                   ("max_packet=64", "max_packet=65"),
                   ('transfer="bulk"', 'transfer="isochronous"'),
                   ('direction="in"', 'direction="sideways"'),
                   ("number=1", "number=4"),
                   ("poll_limit=128", "poll_limit=0"),
                   ("drain_timeout_us=1000", "drain_timeout_us=0"),
                   ("clock_timeout_us=5000", "clock_timeout_us=0"),
                   ("event_capacity=8", "event_capacity=true"))
        for before, after in changes:
            with self.subTest(after=after), self.assertRaisesRegex(ValueError, "USB"):
                gate.resolve_ir(self.assembly(mutate=lambda text: text.replace(before, after)))

    def test_ep0_directions_are_required_and_cannot_be_bulk_or_non64(self):
        changes = (('number=0\ndirection="out"', 'number=2\ndirection="out"'),
                   ('transfer="control"', 'transfer="bulk"'),
                   ('number=0\ndirection="in"\ntransfer="control"\nmax_packet=64',
                    'number=0\ndirection="in"\ntransfer="control"\nmax_packet=32'))
        for before, after in changes:
            with self.subTest(after=after), self.assertRaisesRegex(ValueError, "USB"):
                gate.resolve_ir(self.assembly(mutate=lambda text: text.replace(before, after)))

    def test_duplicate_directions_out_fifo_and_unknown_options_are_rejected(self):
        changes = (('number=1\ndirection="out"', 'number=0\ndirection="out"'),
                   ('direction="out"\ntransfer="bulk"\nmax_packet=64',
                    'direction="out"\ntransfer="bulk"\nmax_packet=64\nfifo_words=16'),
                   ('vbus_sensing=false', 'vbus_sensing=false\ndma=true'))
        for before, after in changes:
            with self.subTest(after=after), self.assertRaises(ValueError):
                gate.resolve_ir(self.assembly(mutate=lambda text: text.replace(before, after)))

    def test_unknown_external_vbus_electrical_path_is_not_promoted_by_pin_facts(self):
        with self.assertRaisesRegex(ValueError, "VBUS.*reviewed"):
            gate.resolve_ir(self.assembly(mutate=lambda text: text.replace(
                "vbus_sensing=false", "vbus_sensing=true")))

    def test_reserved_shared_clock_is_rejected_before_emission(self):
        path = self.assembly("gd32")
        package = self.root / "board"
        package.mkdir()
        source = gate.ROOT / "tests/contracts/usb_assembly/gd32-board/board.json"
        board = gate.load(source)
        board["reserved_resources"].append("clock:CK48")
        (package / "board.json").write_text(json.dumps(board))
        content = path.read_text()
        start = content.index("board_package=")
        end = content.index("\n", start)
        path.write_text(content[:start] + 'board_package="board"' + content[end:])
        with self.assertRaisesRegex(ValueError, "Resource conflict clock:CK48"):
            gate.resolve_ir(path)

    def test_unselected_usb_generates_no_state_clock_or_vectors(self):
        output = self.root / "generated"
        gate.configure(gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml", output)
        source = (output / "bindings.c").read_text()
        self.assertNotIn("static nx_usb_controller_t", source)
        self.assertNotIn("nx_stm32_usb_clock_start", source)
        self.assertNotIn("OTG_FS_IRQHandler", source)


if __name__ == "__main__":
    unittest.main()
