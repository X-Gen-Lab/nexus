"""Only the exact reviewed RTC/FreeRTOS context gains backend admission."""
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from tools.configure import configure as gate
from tools.configure.policies.selection import SelectionPolicy
from tools.configure.tests.test_selection_policy import facts


class RtcFreeRtosContextTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="rtc lowpower context ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        source = gate.ROOT / "tests/contracts/f407_freertos_stop"
        self.text = (source / "stop.toml").read_text()
        self.board = json.loads((source / "board/board.json").read_text())

    def resolve(self, text=None, part=None):
        board = dict(self.board)
        if part is not None:
            board["soc"] = part
        package = self.directory / "board"
        package.mkdir(exist_ok=True)
        (package / "board.json").write_text(json.dumps(board))
        path = self.directory / "assembly.toml"
        path.write_text(self.text if text is None else text)
        return gate.resolve_ir(path)

    def test_reviewed_assembly_resolves_exact_part_kernel_and_rtc(self):
        result = self.resolve()
        self.assertEqual(result.physical_part.part, "STM32F407ZGT6")
        self.assertEqual(result.kernel.profile, "lowpower")
        self.assertEqual(result.kernel.lowpower_port, "timebase")
        calendar, = result.controllers
        self.assertEqual((calendar.kind, calendar.mode),
                         ("rtc", "adopt-retained"))
        self.assertFalse(calendar.options.allow_set)
        self.assertTrue(calendar.options.lse_bypass)
        self.assertTrue(calendar.options.periodic_wakeup)

    def test_other_kernel_contexts_do_not_inherit_backend_permission(self):
        for options in ('profile = "standard"',
                        'profile = "lowpower"\nlowpower_port = "custom"'):
            with self.subTest(options=options), self.assertRaisesRegex(
                    gate.ConfigurationError, "RTC.*reviewed RUN"):
                self.resolve(self.text.replace(
                    'profile = "lowpower"\nlowpower_port = "timebase"',
                    options))

    def test_reviewed_context_requires_the_exact_rtc_options_and_mode(self):
        for field in ("allow_set", "periodic_wakeup", "lse_bypass"):
            before = field + " = " + ("false" if field == "allow_set" else "true")
            after = field + " = " + ("true" if field == "allow_set" else "false")
            with self.subTest(field=field), self.assertRaises(
                    gate.ConfigurationError):
                self.resolve(self.text.replace(before, after))
        provision = self.text.replace('mode = "adopt-retained"',
            'mode = "provision-empty"\ninitial = { year = 25, month = 1, '
            'day = 2, weekday = 4, hour = 3, minute = 4, second = 5 }')
        with self.assertRaisesRegex(gate.ConfigurationError, "RTC.*reviewed RUN"):
            self.resolve(provision)

    def test_sibling_parts_keep_their_existing_backend_boundary(self):
        for part in ("STM32F405ZGT6", "STM32F407VET6"):
            with self.subTest(part=part), self.assertRaisesRegex(
                    gate.ConfigurationError, "RTC.*reviewed RUN"):
                self.resolve(part=part)

    def test_context_is_declarative_without_a_vendor_or_part_branch(self):
        value = facts()
        value["rtc"]["part_contexts"]["EXACT_PART"] = {
            "backend": "baremetal", "profile": "run-48",
            "backend_contexts": {"freertos": {
                "modes": ["adopt-retained"], "kernel_profile": "lowpower",
                "lowpower_port": "timebase", "options": {"allow_set": False}}}}
        policy = SelectionPolicy.from_validated(value)
        route = {"kind": "rtc"}
        selected = {"mode": "adopt-retained", "allow_set": False}
        kernel = SimpleNamespace(profile="lowpower", lowpower_port="timebase")
        policy.validate_context(selected, route, backend="freertos",
            clock_profile="run-48", part="EXACT_PART", kernel_profile=kernel)
        with self.assertRaisesRegex(ValueError, "RTC.*reviewed RUN"):
            policy.validate_context(selected, route, backend="freertos",
                clock_profile="run-48", part="OTHER_PART", kernel_profile=kernel)

    def test_malformed_backend_context_facts_reject(self):
        context = {"modes": ["adopt-retained"], "kernel_profile": "lowpower",
                   "lowpower_port": "timebase", "options": {"allow_set": False}}
        for changes in ({"modes": []}, {"kernel_profile": ""},
                        {"lowpower_port": None}, {"options": []},
                        {"options": {"allow_set": []}}):
            value = facts()
            value["rtc"]["default_context"]["backend_contexts"] = {
                "freertos": {**context, **changes}}
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                SelectionPolicy.from_validated(value)


if __name__ == "__main__":
    unittest.main()
