"""Managed OTG has one static silicon owner and explicit power signal links."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure.providers import for_selection
from tools.configure.tests.native_fixture import native_fixture


class UsbOtgConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        package = self.root / "board"
        package.mkdir()
        template = gate.ROOT / "tests/contracts/usb_assembly"
        board = json.loads((template / "stm32-ve-board/board.json").read_text())
        board["bindings"] += [
            {"id": "source", "controller": "GPIOE", "route": "gpio-pe3",
             "mask": 8, "initial": 0},
            {"id": "present", "controller": "GPIOE", "route": "gpio-pe4",
             "mask": 16, "initial": 0},
            {"id": "oc", "controller": "GPIOA", "route": "gpio-pa0",
             "mask": 1, "initial": 0}]
        board["unknowns"].append(
            "Power GPIO links are software signals, not reviewed PCB wiring.")
        (package / "board.json").write_text(json.dumps(board))
        self.source = (template / "stm32-ve.toml").read_text().replace(
            'board_package="stm32-ve-board"', 'board_package="board"')

    def assembly(self, managed=True):
        text = self.source
        if managed:
            text = text.replace('[usb_device.fs]',
                '[gpio.vbus_source]\nbinding="source"\nmode="output"\n'
                '[gpio.vbus_present]\nbinding="present"\nmode="input"\n'
                '[gpio.vbus_overcurrent]\nbinding="oc"\nmode="input"\n'
                '[usb_device.fs]')
            text = text.replace('mode="device-fs"', 'mode="otg-fs"')
            text = text.replace('vbus_sensing=false',
                'vbus_sensing=false\nelectrical_handover="external-owner"\n'
                'host={channel_count=2,rx_fifo_words=128,'
                'nonperiodic_fifo_words=96,periodic_fifo_words=96,'
                'engine="pio",init_timeout_us=200000}\n'
                'power={source="vbus_source",present="vbus_present",'
                'overcurrent="vbus_overcurrent",source_active_high=true,'
                'present_active_high=true,overcurrent_active_high=false,'
                'fault_timeout_us=100000}')
        path = self.root / "assembly.toml"
        path.write_text(text)
        return path

    def test_composite_plan_is_typed_and_emits_one_cold_and_irq_owner(self):
        path = self.assembly()
        ir = gate.resolve_ir(path, test_support=native_fixture(path))
        usb = next(item for item in ir.controllers if item.kind == "usb_device")
        self.assertEqual(usb.mode, "otg-fs")
        self.assertEqual(usb.options.host.channel_count, 2)
        self.assertEqual(usb.options.power.source, "vbus_source")
        self.assertFalse(ir.physical_qualified)
        output = self.root / "generated"
        gate.configure(path, output, test_support=native_fixture(path))
        source = (output / "bindings.c").read_text()
        header = (output / "nexus_factory.h").read_text()
        for symbol in ("nx_stm32_usb_otg_t", "nx_stm32_usb_otg_prepare",
                       "nx_stm32_usb_otg_close", "nx_stm32_usb_otg_irq",
                       "nx_stm32_usb_otg_endpoint_t",
                       "nx_stm32_usb_otg_device_ops"):
            self.assertIn(symbol, source)
        self.assertIn("nx_factory_usb_otg", header)
        self.assertEqual(source.count("nx_stm32_usb_otg_irq("), 1)
        constructed = for_selection(ir).constructor(usb, ir, ir.board_bindings,
                                                    ir.routes, ir.devices)
        self.assertEqual(set(constructed.pin_snapshots),
                         {pin["pin"] for pin in usb.pins})
        snapshots = {value.split("[", 1)[0]
                     for value in constructed.pin_snapshots.values()}
        self.assertEqual(len(snapshots), 1)
        snapshot, = snapshots
        self.assertEqual(source.count("static uint16_t " + snapshot + "["), 1)
        self.assertNotIn("malloc", source)

    def test_unknown_or_output_only_vbus_observation_rejects_before_emission(self):
        for replacement in ('present="missing"', 'present="vbus_source"'):
            path = self.assembly()
            path.write_text(path.read_text().replace('present="vbus_present"',
                                                     replacement))
            with self.subTest(replacement=replacement), self.assertRaises(ValueError):
                gate.resolve_ir(path, test_support=native_fixture(path))

    def test_power_policy_is_explicit_and_not_inferred_from_phy_pads(self):
        path = self.assembly()
        text = path.read_text()
        path.write_text('\n'.join(line for line in text.splitlines()
                                  if not line.startswith('power=')))
        with self.assertRaises(ValueError):
            gate.resolve_ir(path, test_support=native_fixture(path))

    def test_editor_schema_preserves_nested_host_and_power_types(self):
        from tools.configure.authored import editor_schema
        properties = editor_schema()["properties"]["usb_device"]["additionalProperties"]["properties"]
        for key in ("host", "power"):
            self.assertEqual(properties[key]["type"], "object")
            self.assertFalse(properties[key]["additionalProperties"])
        self.assertEqual(properties["host"]["properties"]["engine"]["enum"],
                         ["pio", "dma"])
        self.assertEqual(properties["power"]["properties"]["source_active_high"]["type"],
                         "boolean")

    def test_managed_host_preserves_explicit_logical_pipe_mapping(self):
        path = self.assembly()
        path.write_text(path.read_text().replace('host={channel_count=2',
                                                 'host={pipe_channels=[1,0],channel_count=2'))
        result = gate.resolve_ir(path, test_support=native_fixture(path))
        device = next(item for item in result.controllers if item.kind == "usb_device")
        self.assertEqual(device.options.host.pipe_channels, (1, 0))

    def test_nominal_managed_factory_header_compiles_without_capability_storage(self):
        path = self.assembly()
        path.write_text(path.read_text().replace('capabilities=true', 'capabilities=false'))
        output = self.root / "without_metadata"
        gate.configure(path, output, test_support=native_fixture(path))
        source = '#include "nexus_factory_strong.h"\nconst nx_usb_otg_port_t* owner(void) {\nreturn nx_factory_usb_otg_ref(NX_USB_DEVICE_REF_FS);\n}\n'
        for compiler, standard, suffix in (("cc", "c11", "c"), ("c++", "c++17", "cpp")):
            with self.subTest(standard=standard):
                consumer = self.root / ("consumer." + suffix)
                consumer.write_text(source)
                command = [compiler, "-std=" + standard, "-Wall", "-Wextra",
                           "-Werror", "-pedantic", "-c", str(consumer),
                           "-I" + str(output), "-I" + str(gate.ROOT / "io/include"),
                           "-I" + str(gate.ROOT / "core/include"),
                           "-o", str(self.root / "consumer.o")]
                run = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_ordinary_device_plan_has_no_composite_state_or_host_storage(self):
        path = self.assembly(False)
        output = self.root / "ordinary"
        gate.configure(path, output, test_support=native_fixture(path))
        source = (output / "bindings.c").read_text()
        self.assertNotIn("nx_stm32_usb_otg_t", source)
        self.assertNotIn("nx_stm32_usb_host_t", source)

    def test_remote_wakeup_uses_the_same_managed_owner_for_each_reviewed_phy(self):
        fixtures = gate.ROOT / "tests/contracts/usb_otg_assembly"
        for name in ("ve-fs", "ve-hs-fs-dma", "zg-ulpi-dma"):
            with self.subTest(phy=name):
                board = fixtures / "boards" / name / "board.json"
                (self.root / "board/board.json").write_bytes(board.read_bytes())
                path = self.root / "assembly.toml"
                text = (fixtures / (name + ".toml")).read_text().replace(
                    'board_package="boards/' + name + '"',
                    'board_package="board"')
                path.write_text(text.replace('vbus_sensing=false',
                    'vbus_sensing=false\nremote_wakeup=true'))
                ir = gate.resolve_ir(path, test_support=native_fixture(path))
                device = next(c for c in ir.controllers if c.kind == "usb_device")
                self.assertTrue(device.options.remote_wakeup)
                output = self.root / name
                gate.configure(path, output, test_support=native_fixture(path))
                source = (output / "bindings.c").read_text()
                self.assertEqual(source.count("static nx_usb_remote_runtime_t"), 1)
                self.assertIn(".remote = &s_nx_usb_remote_fs", source)
                self.assertEqual(source.count("static nx_stm32_usb_otg_t"), 1)
                self.assertEqual(source.count("nx_stm32_usb_otg_irq("), 1)


if __name__ == "__main__":
    unittest.main()
