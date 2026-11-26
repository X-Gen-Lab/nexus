"""Static driver construction retains finite declarations and IRQ records."""
from dataclasses import FrozenInstanceError
import unittest

from tools.configure.calls import Address, DirectCall, Symbol
from tools.configure.providers.common import Construction
from tools.configure.static_ir import Declaration, IrqConnection


class TypedConstructionTests(unittest.TestCase):
    def test_face_context_is_a_typed_static_subobject_address(self):
        selected = self.create(face_context=Address("state.controller"))
        self.assertEqual(selected.face_context.render(), "&state.controller")
        with self.assertRaises(ValueError):
            self.create(face_context="&state")

    def create(self, **options):
        return Construction("state_t", (), DirectCall("prepare"),
                            (DirectCall("close"),), False, **options)

    def test_typed_irq_and_storage_remain_immutable_until_emission(self):
        irq = IrqConnection("IRQHandler", (DirectCall("publish"),),
                            exit_barrier=Symbol("nx_arch_dsb"))
        declaration = Declaration("state_t", "state")
        selected = self.create(irq_connections=[irq],
                               static_declarations=[declaration])
        self.assertEqual(selected.irq_connections, (irq,))
        self.assertEqual(selected.static_declarations, (declaration,))
        self.assertFalse(selected.definitions)
        with self.assertRaises(FrozenInstanceError):
            selected.irq_connections = ()

    def test_untyped_statements_cannot_enter_finite_node_fields(self):
        for options in ({"irq_connections": ["IRQ();"]},
                        {"static_declarations": ["static int x;"]}):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.create(**options)


if __name__ == "__main__":
    unittest.main()
