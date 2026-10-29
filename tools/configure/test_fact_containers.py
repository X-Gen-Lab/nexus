"""Raw JSON and validated immutable facts share narrow container contracts."""

from types import MappingProxyType
import unittest

from tools.configure import configure, cpu
from tools.configure.ir import freeze
from tools.configure.providers import common
from tools.configure.test_cpu_profiles import facts, irq


class FactContainerTests(unittest.TestCase):
    def test_fact_object_admission_preserves_raw_or_readonly_mapping_identity(self):
        for value in ({"required": 1}, MappingProxyType({"required": 1})):
            with self.subTest(container=type(value).__name__):
                self.assertIs(configure.obj(value, {"required"}, (), "facts"),
                              value)

    def test_fact_object_admission_retains_required_unknown_and_shape_checks(self):
        for wrap in (dict, MappingProxyType):
            with self.subTest(container=wrap.__name__):
                with self.assertRaisesRegex(ValueError, "missing"):
                    configure.obj(wrap({}), {"required"}, (), "facts")
                with self.assertRaisesRegex(ValueError, "unknown"):
                    configure.obj(wrap({"required": 1, "extra": 2}),
                                  {"required"}, (), "facts")
        for value in (None, [], (), "object", {1}, iter([1])):
            with self.subTest(container=type(value).__name__):
                with self.assertRaisesRegex(ValueError, "expected object"):
                    configure.obj(value, {"required"}, (), "facts")

    def test_object_fields_admit_raw_and_immutable_mapping_without_repair(self):
        for value in ({"required": 1}, MappingProxyType({"required": 1})):
            with self.subTest(container=type(value).__name__):
                cpu.fields(value, {"required"}, "facts")
                self.assertEqual(value, {"required": 1})

    def test_cpu_profiles_preserve_all_nine_raw_and_frozen_fact_decisions(self):
        for arch in ("cortex-m0", "cortex-m0plus", "cortex-m3", "cortex-m4",
                     "cortex-m7", "cortex-m23", "cortex-m33", "cortex-m55",
                     "cortex-m85"):
            with self.subTest(arch=arch):
                raw_cpu, raw_irq = facts(arch), irq(arch)
                raw = cpu.resolve(raw_cpu, raw_irq, "baremetal")
                immutable = cpu.resolve(freeze(raw_cpu), freeze(raw_irq),
                                        "baremetal")
                self.assertEqual(raw.to_dict(), immutable.to_dict())

    def test_object_fields_keep_missing_unknown_and_nonobject_rejection(self):
        for wrap in (dict, MappingProxyType):
            with self.subTest(container=wrap.__name__):
                with self.assertRaisesRegex(ValueError, "missing"):
                    cpu.fields(wrap({}), {"required"}, "facts")
                with self.assertRaisesRegex(ValueError, "unknown"):
                    cpu.fields(wrap({"required": 1, "unexpected": 2}),
                               {"required"}, "facts")
        for value in (None, True, [], (), "object", b"object", {1}, iter([1])):
            with self.subTest(value=type(value).__name__):
                with self.assertRaisesRegex(ValueError, "expected object"):
                    cpu.fields(value, {"required"}, "facts")

    def test_frozen_facts_do_not_relax_boolean_feature_rejection(self):
        for wrap in (dict, freeze):
            for invalid in (1, "true", None):
                with self.subTest(container=wrap.__name__, invalid=invalid):
                    with self.assertRaises(ValueError):
                        cpu.resolve(wrap(facts("cortex-m33", dsp=invalid)),
                                    wrap(irq("cortex-m33")), "baremetal")

    def test_array_contract_preserves_raw_list_or_frozen_tuple_identity(self):
        for value in ([], (), [1, 2], (1, 2),
                      (MappingProxyType({"resource": "clock"}),)):
            with self.subTest(container=type(value).__name__, size=len(value)):
                self.assertIs(common.sequence(value, "fact array"), value)

    def test_arrays_reject_unordered_and_general_iterable_inputs(self):
        class CannotIterate:
            def __iter__(self):
                raise AssertionError("Rejected input must not be consumed")

        for value in (None, True, "array", b"array", {},
                      MappingProxyType({}), {1, 2}, frozenset({1}),
                      range(2), iter([1, 2]), CannotIterate()):
            with self.subTest(container=type(value).__name__):
                with self.assertRaisesRegex(ValueError, "expected array"):
                    common.sequence(value, "fact array")


if __name__ == "__main__":
    unittest.main()
