"""Only maintained STM32F4 Board identities enter current HIL admission."""

import copy
import json
from pathlib import Path
import unittest

from tools.evidence.common import EvidenceError
from tools.hil.admit import validate_fixture

ROOT = Path(__file__).resolve().parents[2]


class HilF4ScopeTests(unittest.TestCase):
    def fixture(self):
        return json.loads((ROOT / "tools/hil/fixtures/"
                           "stm32f407_qiming_v31.json").read_text())

    def test_each_maintained_f4_board_keeps_its_exact_fixture(self):
        for name in ("stm32f407_qiming_v31", "stm32f407ve_sky_qingchun",
                     "stm32f407zg_qiming_core", "stm32f407zg_qiming_v41"):
            with self.subTest(board=name):
                fixture = json.loads((ROOT / "tools/hil/fixtures" /
                                      (name + ".json")).read_text())
                validate_fixture(fixture)

    def test_retired_platform_cannot_reenter_through_authored_fixture(self):
        for board, part in (("gd32f470-liangshan", "GD32F470ZGT6"),
                            ("stm32g474re-hse24-reference", "STM32G474RET6"),
                            ("stm32h743-nucleo-zi2", "STM32H743ZIT6"),
                            ("stm32u575-nucleo-zi-q", "STM32U575ZIT6Q")):
            with self.subTest(board=board):
                fixture = copy.deepcopy(self.fixture())
                fixture.update(board=board, part=part,
                               scope="software_reference")
                with self.assertRaisesRegex(EvidenceError,
                                            "maintained STM32F4"):
                    validate_fixture(fixture)

    def test_external_f4_board_can_prepare_without_claiming_physical_support(self):
        fixture = self.fixture()
        fixture["board"] = "external-f407-reference"
        fixture["scope"] = "software_reference"
        validate_fixture(fixture)
        self.assertEqual(fixture["physical_status"], "not_executed")


if __name__ == "__main__":
    unittest.main()
