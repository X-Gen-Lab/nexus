"""Named System clock profiles have reviewed domains and select no PLL."""
from types import SimpleNamespace
import unittest

from tools.configure.domain_policies import DomainPolicies


class FixedClockPolicyTests(unittest.TestCase):
    def test_named_profiles_publish_only_reviewed_domains(self):
        policy = DomainPolicies.from_validated(None, {}, {"domains": {
            "schema_version": 1,
            "clock": {"model": "fixed-profile", "reviews": {
                "lab-clock": ["internal8"], "other-clock": ["internal4"]}},
        }}).clock
        facts = {"source": "hsi", "input_hz": 8000000, "core_hz": 8000000,
                 "apb1_hz": 4000000, "apb1_timer_hz": 8000000}
        plan = policy.plan("internal8", facts)
        policy.validate_clock_policy("lab-clock", plan)
        self.assertEqual(plan["input"]["source"], "hsi")
        self.assertEqual(plan["domains"]["apb1"]["nominal_hz"], 4000000)
        self.assertEqual(plan["domains"]["apb1_timer"]["nominal_hz"], 8000000)
        self.assertNotIn("apb2", plan["domains"])
        self.assertNotIn("parameters", plan)
        self.assertFalse(plan["physical_qualified"])
        with self.assertRaisesRegex(ValueError, "outside"):
            policy.validate_clock_policy("other-clock", plan)
        with self.assertRaisesRegex(ValueError, "Unreviewed"):
            policy.plan("custom", facts)
        with self.assertRaisesRegex(ValueError, "clock domain"):
            policy.validate_consumers(plan, (SimpleNamespace(controller="BUS"),),
                                      controller_domains={"BUS": "apb2"})
