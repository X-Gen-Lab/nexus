"""A binding declares the admitted Board supply window for programming."""
from types import SimpleNamespace
import unittest

from tools.configure.ir import ControllerIR
from tools.configure.providers.common import mode_traits
from tools.configure.selection import normalize_controller


class FlashSupplyTests(unittest.TestCase):
    def test_halfword_programming_projects_required_board_supply(self):
        provider = SimpleNamespace(binding=lambda *args: SimpleNamespace(
            limits={"route_supply_mv": (2700, 3600)}))
        route = {"kind": "flash", "id": "internal-flash",
                 "controller": "FLASH", "pins": []}
        selection = {"id": "flash", "binding": "flash0", "mode": "poll-halfword"}
        traits = mode_traits("flash", "poll-halfword")

        def normalize(board_fields):
            return normalize_controller(selection, route, board_fields, {}, provider,
                clock={}, clock_plan=None, clock_profile="internal8",
                pin_states=None, traits=traits)

        instance = ControllerIR.create(normalize({"supply_mv": 3300}), "sample")
        self.assertEqual(instance.route_supply_mv, 3300)
        for board_fields in ({}, {"supply_mv": 2699}, {"supply_mv": 3601}):
            with self.subTest(board_fields=board_fields), self.assertRaisesRegex(
                    ValueError, "Board route supply"):
                normalize(board_fields)
