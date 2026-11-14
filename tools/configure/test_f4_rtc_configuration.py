"""Exact new-part RTC assemblies use reviewed RUN and authored LSE facts."""
import json
import re
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


PARTS = ("f411re", "f446ze", "f479ni")
INITIAL = ('initial = { year = 25, month = 1, day = 2, weekday = 4, '
           'hour = 3, minute = 4, second = 5 }\n')


class FamilyRtcConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="f4 rtc reviewed run ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def assembly(self, part, mode="adopt-retained", extra=""):
        source = gate.ROOT / "tests/contracts/stm32f4_family"
        board = json.loads((source / (part + "-board/board.json")).read_text())
        board["clocks"] = {"lse_hz": 32768}
        board["bindings"].append({"id": "rtc0", "controller": "RTC",
                                  "route": "rtc-lse-pc14-pc15"})
        package = self.directory / (part + "-board")
        package.mkdir(exist_ok=True)
        (package / "board.json").write_text(json.dumps(board))
        text = (source / (part + ".toml")).read_text().replace(
            'board_package = "' + part + '-board"',
            'board_package = "' + package.as_posix() + '"')
        text += ('\n[rtc.calendar]\nbinding = "rtc0"\n'
                 f'mode = "{mode}"\nallow_set = true\n'
                 'timeout_us = 2000000\npoll_limit = 1000000\n')
        if mode == "provision-empty":
            text += INITIAL
        return text + extra

    def resolve(self, text):
        path = self.directory / "assembly.toml"
        path.write_text(text)
        return gate.resolve_ir(path)

    def test_both_policies_resolve_only_the_selected_reviewed_part_run(self):
        for part in PARTS:
            for mode in ("adopt-retained", "provision-empty"):
                with self.subTest(part=part, mode=mode):
                    result = self.resolve(self.assembly(part, mode))
                    item = next(item for item in result.controllers
                                if item.kind == "rtc")
                    self.assertEqual(item.mode, mode)
                    self.assertEqual(item.options.initial is not None,
                                     mode == "provision-empty")
                    self.assertEqual(item.integration.clock_hz, None)
                    self.assertEqual(result.clock_plan.input.source, "hsi")

    def test_new_part_rtc_does_not_inherit_unreviewed_duration_or_bypass_modes(self):
        for part in PARTS:
            for extra in ("periodic_wakeup = true\n", "lse_bypass = true\n"):
                with self.subTest(part=part, extra=extra):
                    with self.assertRaises(gate.ConfigurationError):
                        self.resolve(self.assembly(part, extra=extra))

    def test_new_part_rtc_rejects_unreviewed_direct_hsi_run(self):
        for part in PARTS:
            with self.subTest(part=part), self.assertRaisesRegex(
                    gate.ConfigurationError, "reviewed.*RTC|RTC.*reviewed"):
                text = self.assembly(part).replace(
                    'sysclk_source = "pll"', 'sysclk_source = "input"')
                text = re.sub(r'apb[12]_div = [0-9]+',
                              lambda match: match.group(0).split('=')[0] + '= 1',
                              text)
                self.resolve(text)


if __name__ == "__main__":
    unittest.main()
