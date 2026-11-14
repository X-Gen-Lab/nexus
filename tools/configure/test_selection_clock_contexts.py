"""Clock execution restrictions are immutable package facts, not MCU branches."""
import copy
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.configure import bindings, configure
from tools.configure.policies.selection import SelectionPolicy
from tools.configure.test_selection_policy import facts
from tools.configure.tests.f4_fixture import provider as f4_provider


ROOT = Path(__file__).resolve().parents[2]


class ClockContextSelectionTests(unittest.TestCase):
    @staticmethod
    def author(profile="reviewed-eco-plan"):
        value = facts()
        value["clock_contexts"] = {profile: {
            "backends": ["native"], "allow_power": False,
            "forbidden_kinds": ["standby", "timebase"]}}
        return value

    def test_non_vendor_profile_uses_only_its_explicit_reviewed_context(self):
        policy = SelectionPolicy.from_validated(self.author())
        policy.validate_assembly_context(backend="native", clock_profile="reviewed-eco-plan",
            power=None, controllers=(SimpleNamespace(kind="gpio"),))
        for fields in ({"backend": "baremetal"}, {"power": {}},
                       {"controllers": (SimpleNamespace(kind="timebase"),)}):
            context = dict(backend="native", clock_profile="reviewed-eco-plan",
                           power=None, controllers=())
            context.update(fields)
            with self.subTest(fields=fields), self.assertRaisesRegex(
                    ValueError, "clock.*context"):
                policy.validate_assembly_context(**context)

    def test_profiles_without_a_context_rule_keep_existing_admission(self):
        for value in (facts(), self.author()):
            policy = SelectionPolicy.from_validated(value)
            policy.validate_assembly_context(backend="freertos", clock_profile="another-run",
                power={}, controllers=(SimpleNamespace(kind="standby"),))

    def test_explicit_power_permission_remains_a_finite_boolean_choice(self):
        value = self.author()
        value["clock_contexts"]["reviewed-eco-plan"]["allow_power"] = True
        policy = SelectionPolicy.from_validated(value)
        policy.validate_assembly_context(backend="native", clock_profile="reviewed-eco-plan",
                                        power={}, controllers=())
        with self.assertRaises(ValueError):
            policy.validate_assembly_context(backend="native", clock_profile="reviewed-eco-plan",
                power={}, controllers=(SimpleNamespace(kind="standby"),))

    def test_f4_closed_profile_preserves_each_independent_execution_restriction(self):
        policy = f4_provider("STM32F407ZGT6").SELECTION
        profile = "hse8-pll150-sdio375"
        rule = policy.data["clock_contexts"][profile]
        self.assertEqual(("baremetal",), rule.backends)
        self.assertFalse(rule.allow_power)
        self.assertEqual(("lowpower", "standby", "timebase"), rule.forbidden_kinds)
        context = dict(backend="baremetal", clock_profile=profile,
                       power=None, controllers=(SimpleNamespace(kind="gpio"),))
        policy.validate_assembly_context(**context)
        denied = ({"backend": "freertos"}, {"power": {}}, *(
            {"controllers": (SimpleNamespace(kind=kind),)}
            for kind in ("lowpower", "standby", "timebase")))
        for changes in denied:
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                policy.validate_assembly_context(**{**context, **changes})

    def test_author_facts_are_frozen_once_without_a_second_registry(self):
        value = self.author()
        policy = SelectionPolicy.from_validated(value)
        value["clock_contexts"]["reviewed-eco-plan"]["backends"].append("baremetal")
        with self.assertRaises(ValueError):
            policy.validate_assembly_context(backend="baremetal", clock_profile="reviewed-eco-plan",
                                            power=None, controllers=())
        with self.assertRaises(AttributeError):
            policy.data["clock_contexts"]["reviewed-eco-plan"].allow_power = True

    def test_unknown_fields_and_invalid_author_values_are_rejected(self):
        mutations = (
            lambda value: value.update(clock_contexts=[]),
            lambda value: value.update(clock_contexts={"": {}}),
            lambda value: value["clock_contexts"].update({7: {}}),
            lambda value: value["clock_contexts"].update({"other": None}),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(extra=True),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].pop("allow_power"),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(allow_power=0),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(backends="native"),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(backends=[]),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(backends=[True]),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(backends=["native", "native"]),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(backends=["unknown"]),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(forbidden_kinds="standby"),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(forbidden_kinds=[False]),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(forbidden_kinds=[""]),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(forbidden_kinds=["bad.kind"]),
            lambda value: value["clock_contexts"]["reviewed-eco-plan"].update(forbidden_kinds=["standby", "standby"]),
        )
        for mutate in mutations:
            value = copy.deepcopy(self.author())
            mutate(value)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                SelectionPolicy.from_validated(value)

    def test_production_resolver_consults_the_selected_package_context(self):
        selected = SelectionPolicy.validate_assembly_context
        observed = []

        def validate(policy, **context):
            observed.append(context)
            return selected(policy, **context)

        assembly = ROOT / "tests/contracts/stm32f4_sdio_hs/sd37-dma.toml"
        with patch.object(SelectionPolicy, "validate_assembly_context", validate):
            result = configure.resolve_ir(assembly)
        self.assertEqual(1, len(observed))
        self.assertEqual("hse8-pll150-sdio375", observed[0]["clock_profile"])
        self.assertEqual("baremetal", observed[0]["backend"])
        self.assertIsNone(observed[0]["power"])
        self.assertEqual((("sd0", "block", "SDIO", "sdio-dma"),), tuple(
            (item.id, item.kind, item.controller, item.mode)
            for item in observed[0]["controllers"]))
        self.assertIn(".high_speed = true", bindings.emit(result)[1])


if __name__ == "__main__":
    unittest.main()
