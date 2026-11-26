"""Static instances retain C type checking without executable source fragments."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.configure.calls import Address, DirectCall, Symbol
from tools.configure.static_ir import (Array, ArrayElementAddress, ConstantMacro, Declaration,
    Flags, IrqConnection, PointerMemberAddress, Record, Signed32, Unsigned64)


class StaticInstanceTests(unittest.TestCase):
    def test_constant_macros_keep_only_typed_static_arguments(self):
        args = [Symbol("MODE"), 3]
        value = ConstantMacro(Symbol("NX_CONST_ENCODING"), args)
        args.clear()
        self.assertEqual(value.render(), "NX_CONST_ENCODING(MODE, 3u)")
        for arguments in ((DirectCall("read_register"),), ("MODE|3",),
                          (Record((("field", 1),)),), (Array((1,)),), {3}):
            with self.subTest(arguments=arguments), self.assertRaises(ValueError):
                ConstantMacro(Symbol("NX_CONST_ENCODING"), arguments)
        with self.assertRaises(ValueError):
            ConstantMacro("NX_CONST_ENCODING", (3,))
    def test_signed_literals_and_external_objects_have_explicit_forms(self):
        self.assertEqual(Signed32(-1).render(), "INT32_C(-1)")
        self.assertEqual(Declaration("plan_t", "plan", const=True,
            static=False, extern=True).render(), ["extern const plan_t plan;"])
        for value in (True, -(2**31) - 1, 2**31, "(IRQn_Type)-1"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                Signed32(value)
        for options in ({"extern": True}, {"extern": "extern", "static": False},
                        {"extern": True, "static": False, "alignment": 4},
                        {"extern": True, "static": False, "initializer": 1}):
            with self.subTest(options=options), self.assertRaises(ValueError):
                Declaration("plan_t", "plan", **options)
    def test_wide_constants_are_explicit_without_source_expressions(self):
        self.assertEqual(Unsigned64(2**64 - 2).render(),
                         "UINT64_C(18446744073709551614)")
        for invalid in (True, -1, 2**64, "1 << 63"):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                Unsigned64(invalid)
    def test_exported_pointer_qualifiers_are_explicit_and_frozen(self):
        declaration = Declaration("nx_port_t", "nx_device", Address("face"),
            static=False, pointer=True, pointee_const=True, const=True)
        self.assertEqual(declaration.render(),
            ["const nx_port_t* const nx_device = &face;"])
        for options in ({"pointer": 1}, {"static": "extern"},
                        {"pointee_const": True}):
            with self.subTest(options=options), self.assertRaises(ValueError):
                Declaration("nx_port_t", "nx_device", **options)

    def test_flags_are_only_ordered_unique_enum_symbols(self):
        values = [Symbol("READ"), Symbol("WRITE")]
        flags = Flags(values)
        values.clear()
        self.assertEqual(flags.render(), "READ | WRITE")
        for invalid in ((), {Symbol("READ")}, ("READ|WRITE",),
                        (DirectCall("read"),), (Symbol("READ"), Symbol("READ"))):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                Flags(invalid)
    def test_private_mmio_and_snapshot_refs_have_only_fixed_address_forms(self):
        value = Record((("input", PointerMemberAddress(Symbol("GPIOA"), "IDR")),
                        ("saved", ArrayElementAddress(Symbol("snapshots"), 2))))
        self.assertEqual(value.render(),
            "{ .input = &GPIOA->IDR, .saved = &snapshots[2u] }")
        for base in ("GPIOA", Address("GPIOA"), DirectCall("gpio")):
            with self.subTest(base=base), self.assertRaises(ValueError):
                PointerMemberAddress(base, "IDR")
        for member in ("return", "IDR;start()", "bank.IDR", "IDR[0]"):
            with self.subTest(member=member), self.assertRaises(ValueError):
                PointerMemberAddress(Symbol("GPIOA"), member)
        for index in (True, -1, 0x100000000, "0;start()", Symbol("INDEX")):
            with self.subTest(index=index), self.assertRaises(ValueError):
                ArrayElementAddress(Symbol("snapshots"), index)

    def test_records_capture_ordered_fields_and_nested_values(self):
        fields = [("mask", 3), ("enabled", True),
                  ("owner", Address("state.owner")),
                  ("slots", Array([Symbol("SLOT_A"), None]))]
        value = Record(fields)
        fields.clear()
        self.assertEqual(len(value.fields), 4)
        self.assertIn(".owner = &state.owner", value.render())
        with self.assertRaises(FrozenInstanceError):
            value.fields = ()

    def test_all_nodes_reject_executable_or_unordered_input(self):
        for value in ("RCC->CR = 1", DirectCall("prepare"), -1,
                      0x100000000, 1.5, {}, set(), object()):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    Record((("field", value),))
                with self.assertRaises(ValueError):
                    Array((value,))
                with self.assertRaises(ValueError):
                    Declaration("nx_state_t", "state", value)
        for value in ({"field": 1}, {("field", 1)}, iter((("field", 1),))):
            with self.subTest(value=value), self.assertRaises(ValueError):
                Record(value)
        for value in ({1, 2}, "values", iter((1, 2))):
            with self.subTest(value=value), self.assertRaises(ValueError):
                Array(value)

    def test_fields_and_declarations_are_explicit_identifiers(self):
        for value in ("return", "state->field", "field[0]", "f; bad()", ""):
            for create in (lambda: Record(((value, 1),)),
                           lambda: Declaration(value, "state"),
                           lambda: Declaration("nx_state_t", value)):
                with self.subTest(value=value), self.assertRaises(ValueError):
                    create()
        with self.assertRaises(ValueError):
            Record((("field", 1), ("field", 2)))

    def test_arrays_declarations_and_alignment_are_bounded(self):
        with self.assertRaises(ValueError):
            Array(())
        for count in (0, -1, True, 0x100000000, "4"):
            with self.subTest(count=count), self.assertRaises(ValueError):
                Declaration("uint8_t", "bytes", count=count)
        for alignment in (0, -1, True, 3, "4"):
            with self.subTest(alignment=alignment), self.assertRaises(ValueError):
                Declaration("uint8_t", "bytes", alignment=alignment)
        with self.assertRaises(ValueError):
            Declaration("uint8_t", "bytes", const=1)
        with self.assertRaises(ValueError):
            Declaration("uint8_t", "bytes", Array((1, 2)), count=1)

    def test_irq_order_and_barrier_are_explicit_calls(self):
        calls = [DirectCall("service", (Address("state"),)),
                 DirectCall("shared_service")]
        irq = IrqConnection("DEVICE_IRQHandler", calls,
                            exit_barrier=Symbol("nx_arch_dsb"))
        calls.clear()
        source = "\n".join(irq.render())
        self.assertLess(source.index("service(&state)"),
                        source.index("shared_service()"))
        self.assertLess(source.index("shared_service()"),
                        source.index("nx_arch_dsb()"))
        for invalid in ((), {DirectCall("service")}, ("service();",)):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                IrqConnection("DEVICE_IRQHandler", invalid)
        with self.assertRaises(ValueError):
            IrqConnection("DEVICE_IRQHandler", (DirectCall("service"),),
                          exit_barrier="__DSB();")

    def test_rendered_instances_compile_and_execute_as_c11(self):
        compiler = shutil.which("gcc")
        self.assertIsNotNone(compiler, "The maintained Native compiler is required")
        nodes = (
            Declaration("state_t", "state", Record(())),
            Declaration("uint32_t", "values", Array((7, 9)), const=True,
                        count=2, alignment=8),
            Declaration("config_t", "config", Record((
                ("owner", Address("state")), ("values", Symbol("values")),
                ("enabled", True), ("flags", Flags((Symbol("READ"),
                                                    Symbol("WRITE")))))), const=True),
            Declaration("config_t", "selected", Address("config"),
                        static=False, pointer=True, pointee_const=True, const=True),
            Declaration("uint64_t", "wide", Unsigned64(2**63 + 7), const=True),
            Declaration("int32_t", "signed_value", Signed32(-(2**31)), const=True),
            Declaration("uint32_t", "encoded", ConstantMacro(
                Symbol("NX_CONST_ENCODING"), (3, 2)), const=True),
            Declaration("uint32_t", "external_value", static=False, extern=True),
            IrqConnection("DEVICE_IRQHandler", (
                DirectCall("service", (Address("state"),)),),
                exit_barrier=Symbol("barrier")))
        declarations = "\n".join(line for node in nodes for line in node.render())
        source = """#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { unsigned calls; } state_t;
typedef struct {
    state_t* owner; const uint32_t* values; bool enabled; unsigned flags;
} config_t;
enum { READ = 1, WRITE = 2 };
#define NX_CONST_ENCODING(value, shift) ((value) << (shift))
uint32_t external_value = 19;
static unsigned completed;
static void service(state_t* state) { ++state->calls; }
static void barrier(void) { ++completed; }
""" + declarations + """
int main(void) {
    DEVICE_IRQHandler();
    return !(state.calls == 1 && completed == 1 && config.owner == &state &&
             config.values[1] == 9 && config.enabled && config.flags == 3 &&
             selected == &config && wide == UINT64_C(9223372036854775815) &&
             signed_value == INT32_MIN && external_value == 19 && encoded == 12);
}
"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "instance.c").write_text(source, encoding="utf-8")
            compiled = subprocess.run((compiler, "-std=c11", "-Wall", "-Wextra",
                "-Werror", "-pedantic", str(path / "instance.c"), "-o",
                str(path / "instance")), capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            run = subprocess.run((str(path / "instance"),), check=False)
            self.assertEqual(run.returncode, 0)


if __name__ == "__main__":
    unittest.main()
