"""Lifecycle call plans reject executable fragments before C rendering."""
from dataclasses import FrozenInstanceError
import unittest

from tools.configure.calls import Address, DirectCall, Symbol
from tools.configure.providers.common import Construction


class StaticCallTests(unittest.TestCase):
    def test_direct_typed_arguments_preserve_selected_call(self):
        call = DirectCall("nx_prepare", (Address("s_port.tx"),
            Symbol("s_storage"), 64, True, False, None))
        self.assertEqual(call.render(),
            "nx_prepare(&s_port.tx, s_storage, 64u, true, false, NULL)")

    def test_absolute_deadline_uses_explicit_nested_direct_calls(self):
        deadline = DirectCall("nx_deadline_after", (
            DirectCall("nx_time_now_us"), 100000))
        call = DirectCall("nx_initialize", (Address("s_port"), deadline))
        self.assertEqual(call.render(),
            "nx_initialize(&s_port, nx_deadline_after(nx_time_now_us(), 100000u))")

    def test_callee_cannot_be_pointer_expression_or_statement(self):
        for value in ("p->initialize", "(*callback)", "good(); evil", "",
                      "namespace.start", "func\n", 12, None):
            with self.subTest(value=value), self.assertRaises(ValueError):
                DirectCall(value)

    def test_arguments_cannot_hide_arbitrary_c_source(self):
        for value in ("&s_port", "0u", "NULL", "true", "a ? b : c",
                      "(type){0}", {"symbol": "s_port"}, -1, 2**32, 1.5):
            with self.subTest(value=value), self.assertRaises(ValueError):
                DirectCall("nx_prepare", (value,))

    def test_addresses_are_static_object_paths(self):
        for value in ("&s_port", "p->child", "ports[0]", "*port", "port+1",
                      "port..child", "port;", "", None):
            with self.subTest(value=value), self.assertRaises(ValueError):
                Address(value)
        for value in ("p.member", "A | B", "1", "", None):
            with self.subTest(value=value), self.assertRaises(ValueError):
                Symbol(value)

    def test_plan_captures_argument_sequence_and_is_immutable(self):
        arguments = [Address("s_port"), 7]
        call = DirectCall("nx_prepare", arguments)
        arguments.clear()
        self.assertEqual(call.render(), "nx_prepare(&s_port, 7u)")
        with self.assertRaises(FrozenInstanceError):
            call.function = "foreign"

    def test_argument_containers_cannot_discard_or_reorder_values(self):
        for values in ("", b"", {}, set(), {1, 2}, {1: 2}, iter([1, 2])):
            with self.subTest(values=values), self.assertRaises(ValueError):
                DirectCall("nx_prepare", values)

    def test_c_keywords_cannot_be_declared_call_or_reference_names(self):
        for value in ("return", "int", "_Atomic"):
            for constructor in (DirectCall, Address, Symbol):
                with self.subTest(value=value, constructor=constructor):
                    with self.assertRaises(ValueError):
                        constructor(value)
        with self.assertRaises(ValueError):
            Address("state.if")

    def test_call_source_is_in_configuration_and_sdk_input_closure(self):
        from tools.configure.dependencies import source_names
        for mode in ("platform", "runtime"):
            self.assertIn("tools/configure/calls.py",
                          source_names(mode, "stm32f407", sdk=True))

    def test_construction_rejects_legacy_initializer_and_stop_fragments(self):
        for initialize, stop in (
                ("nx_prepare()", ()),
                (DirectCall("nx_prepare"), ("nx_stop()",)),
                (DirectCall("nx_prepare"), ("ready ? nx_stop() : NX_SUCCESS",))):
            with self.subTest(initialize=initialize, stop=stop):
                with self.assertRaisesRegex(ValueError, "direct call"):
                    Construction("nx_state_t", (), initialize, stop, False)

    def test_construction_stop_order_is_an_explicit_sequence(self):
        for values in ({DirectCall("nx_stop")}, {}, iter(()), "", None):
            with self.subTest(values=values):
                with self.assertRaisesRegex(ValueError, "ordered"):
                    Construction("nx_state_t", (), DirectCall("nx_prepare"),
                                 values, False)


if __name__ == "__main__":
    unittest.main()
