"""Numeric pinmux selectors are explicit package facts, not MCU defaults."""
from dataclasses import FrozenInstanceError
from types import MappingProxyType
import unittest

from tools.configure.pinmux import NumericPinMuxPolicy


class NumericPinMuxPolicyTests(unittest.TestCase):
    def test_exact_package_selectors_admit_f4_and_sparse_host_model(self):
        for selectors in (list(range(16)), [0, 16, 31]):
            with self.subTest(selectors=selectors):
                policy = NumericPinMuxPolicy.from_validated({
                    "encoding": "numeric", "selectors": selectors})
                self.assertEqual(policy.selectors, tuple(selectors))
                for value in selectors:
                    self.assertEqual(policy.validate(value), value)

    def test_unselected_selector_is_not_admitted_by_a_wider_numeric_range(self):
        policy = NumericPinMuxPolicy.from_validated({
            "encoding": "numeric", "selectors": [0, 16, 31]})
        for value in (1, 15, 17, 30, 32):
            with self.subTest(value=value), self.assertRaisesRegex(
                    ValueError, "console RX.*not declared"):
                policy.validate(value, "console RX")
        f4 = NumericPinMuxPolicy.from_validated({
            "encoding": "numeric", "selectors": list(range(16))})
        with self.assertRaises(ValueError):
            f4.validate(16)

    def test_input_mutation_cannot_change_the_frozen_policy(self):
        values = [0, 7, 15]
        policy = NumericPinMuxPolicy.from_validated({
            "encoding": "numeric", "selectors": values})
        values.append(16)
        self.assertEqual(policy.selectors, (0, 7, 15))
        with self.assertRaises(ValueError):
            policy.validate(16)
        with self.assertRaises(FrozenInstanceError):
            policy.selectors = (0, 16)
        frozen = NumericPinMuxPolicy.from_validated(MappingProxyType({
            "encoding": "numeric", "selectors": (0, 15)}))
        self.assertEqual(frozen.selectors, (0, 15))

    def test_unknown_encoding_missing_or_extra_fields_are_rejected(self):
        for facts in (None, [], {}, {"selectors": [0]},
                {"encoding": "numeric"},
                {"encoding": "remap", "selectors": [0]},
                {"encoding": 0, "selectors": [0]},
                {"encoding": "numeric", "selectors": [0],
                 "registers": "write_register()"}):
            with self.subTest(facts=facts), self.assertRaises(ValueError):
                NumericPinMuxPolicy.from_validated(facts)

    def test_selectors_require_nonempty_distinct_uint32_values(self):
        for selectors in ([], [0, 0], [False], [True], [-1], [1 << 32],
                [0.0], ["0"], [None], [[0]], "0", {0, 1}, 0):
            with self.subTest(selectors=selectors), self.assertRaises(ValueError):
                NumericPinMuxPolicy.from_validated({
                    "encoding": "numeric", "selectors": selectors})
        maximum = (1 << 32) - 1
        policy = NumericPinMuxPolicy.from_validated({
            "encoding": "numeric", "selectors": [maximum]})
        self.assertEqual(policy.validate(maximum), maximum)

    def test_validation_rejects_bool_and_non_integer_selector_inputs(self):
        policy = NumericPinMuxPolicy.from_validated({
            "encoding": "numeric", "selectors": [0, 1, 15]})
        for value in (False, True, 0.0, "1", None, [], {}, -1, 1 << 32):
            with self.subTest(value=value), self.assertRaises(ValueError):
                policy.validate(value)


if __name__ == "__main__":
    unittest.main()
