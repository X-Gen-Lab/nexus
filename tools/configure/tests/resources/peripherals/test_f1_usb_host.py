"""Exact fixed-A Host power admission and same-resource Device exclusion."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


FIXTURE = gate.ROOT / "tests/integration/assemblies/stm32f1_connectivity_host"


class ConnectivityHostConfigTests(unittest.TestCase):
    def test_both_exact_devices_claim_one_core_and_guarded_power(self):
        for name in ("f105vc", "f107vc"):
            with self.subTest(name=name):
                ir = gate.resolve_ir(FIXTURE / (name + ".toml"))
                self.assertEqual(len(ir.controllers), 1)
                host = ir.controllers[0]
                self.assertEqual((host.kind, host.mode, host.controller),
                                 ("usb_host", "host-fs", "OTG_FS"))
                self.assertEqual(host.options.channel_count, 1)
                self.assertFalse(ir.physical_qualified)
                claims = {claim.key for claim in ir.claims}
                self.assertTrue({"clock:USBFS48", "irq:OTG_FS", "pin:PA9",
                                 "pin:PB0", "pin:PB1", "pin:PB10"} <= claims)

    def test_rejects_additional_channels_or_absent_power(self):
        source = (FIXTURE / "f105vc.toml").read_text().replace(
            'board_package = "f105vc-board"',
            'board_package = ' + json.dumps(str(FIXTURE / "f105vc-board")))
        cases = (('channel_count = 1', 'channel_count = 2', 'channel'),
                 ('"external-guarded-pb0-pb1-pb10"', '"absent"', 'hardware'))
        with tempfile.TemporaryDirectory() as scratch:
            path = Path(scratch) / "assembly.toml"
            for previous, replacement, message in cases:
                with self.subTest(message=message):
                    path.write_text(source.replace(previous, replacement))
                    with self.assertRaisesRegex(ValueError, message):
                        gate.resolve_ir(path)

    def test_host_and_device_cannot_select_two_owners_of_same_core(self):
        with tempfile.TemporaryDirectory() as scratch:
            workspace = Path(scratch)
            board = workspace / "board"
            board.mkdir()
            data = json.loads((FIXTURE / "f105vc-board/board.json").read_text())
            data["bindings"].append({
                "id": "usb-b", "controller": "OTG_FS",
                "route": "otg-fs-sensed-pa9-pa11-pa12",
            })
            (board / "board.json").write_text(json.dumps(data))
            source = (FIXTURE / "f105vc.toml").read_text().replace(
                'board_package = "f105vc-board"', 'board_package = "board"')
            device = FIXTURE.parent / "stm32f1_connectivity_dwc/f105vc.toml"
            source += "\n[usb_device.fs]" + device.read_text().split(
                "[usb_device.fs]", 1)[1]
            path = workspace / "assembly.toml"
            path.write_text(source)
            with self.assertRaisesRegex(ValueError, "conflict|owner"):
                gate.resolve_ir(path)
