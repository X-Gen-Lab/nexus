"""Shared static objects reuse the same finite recipe value language."""
from dataclasses import dataclass
import unittest

from tools.configure.calls import InitializeSequence
from tools.configure.static_ir import Declaration


@dataclass(frozen=True)
class SelectedOwners:
    mask: int
    values: tuple[int, ...] = (3, 7)
    extension: int | None = None
    enabled: bool = False
    register_member: str = "LISR"


class StaticObjectTests(unittest.TestCase):
    def test_selected_register_member_is_a_checked_symbol(self):
        from tools.configure.static_recipe import StaticObjects
        group = StaticObjects.from_validated({"symbols": {"status": "status"},
            "declarations": [{"role": "status", "type": "uint32_t", "const": True,
                "pointer": True, "value": {"pointer_member": {
                    "base": {"symbol": "DMA1"}, "member": {
                        "source": "selected.register_member", "as": "symbol"}}}}]})
        nodes = group.project(SelectedOwners(0, register_member="HISR"))
        self.assertIn("&DMA1->HISR", nodes[0].render()[0])
        for member in ("return", "HISR;write()", "HISR[0]"):
            with self.subTest(member=member), self.assertRaises(ValueError):
                group.project(SelectedOwners(0, register_member=member))

    def test_device_recipe_omits_disabled_storage_and_exports_selected_endpoint(self):
        from tools.configure.static_recipe import StaticRecipe

        @dataclass(frozen=True)
        class Device:
            id: str = "bus"
            integration: SelectedOwners = SelectedOwners(0)

        @dataclass(frozen=True)
        class Child:
            id: str = "part"
            controller: str = "bus"
            driver: str = "memory_window-endpoint"

        group = StaticRecipe.from_validated({"state_type": "state_t",
            "declarations": [{"role": "extra", "type": "uint32_t", "const": False,
                              "enabled": {"source": "selected.enabled"}}],
            "calls": {"prepare": {"function": "prepare", "args": []}, "close": []},
            "endpoint_faces": {"limit": 2, "cases": {"memory_window-endpoint": {
                "type": "window_t", "ops": {"symbol": "window_ops"},
                "parents": {"parent_port": "port"},
                "context": {"ref": "parent_port", "as": "address"}}}}})
        result = group.construct(Device(), None, {}, {}, (Child(),))
        self.assertEqual(len(result.static_declarations), 2)
        self.assertEqual(result.static_declarations[-1].name, "nx_device_part")
        self.assertIn(".context = &s_nx_port_bus",
                      result.static_declarations[0].render()[0])

    def test_scalar_maps_optional_records_and_unselected_storage_are_finite(self):
        from tools.configure.static_recipe import StaticObjects
        document = {"symbols": {"values": "samples", "wide": "wide",
                                "optional": "optional_record", "spare": "spare"},
            "declarations": [
                {"role": "values", "type": "uint32_t", "const": True,
                 "count": {"length": "selected.values", "limit": 4},
                 "value": {"map": {"source": "selected.values", "limit": 4,
                                     "value": {"source": "entry"}}}},
                {"role": "wide", "type": "uint64_t", "const": True,
                 "value": {"source": "selected.mask", "as": "u64"}},
                {"role": "optional", "type": "optional_t", "const": True,
                 "value": {"optional": "selected.extension", "value": {"record": [
                    ["value", {"source": "selected.extension"}]]}, "default": None}},
                {"role": "spare", "type": "uint32_t", "const": False,
                 "enabled": {"source": "selected.enabled", "as": "bool"}}]}
        group = StaticObjects.from_validated(document)
        nodes = group.project(SelectedOwners(2**40))
        self.assertEqual(len(nodes), 3)
        self.assertIn("{ 3u, 7u }", nodes[0].render()[0])
        self.assertIn("UINT64_C(1099511627776)", nodes[1].render()[0])
        self.assertIsNone(nodes[2].initializer)
        nodes = group.project(SelectedOwners(1, extension=5, enabled=True))
        self.assertEqual(len(nodes), 4)
        self.assertIn(".value = 5u", nodes[2].render()[0])

    def definition(self):
        return {"symbols": {"receipt": "s_receipt", "config": "s_config",
                            "close": "close_owner"},
            "declarations": [
                {"role": "receipt", "type": "uint32_t", "const": False},
                {"role": "config", "type": "config_t", "const": True,
                 "value": {"record": [["receipt", {"ref": "receipt", "as": "address"}],
                                       ["mask", {"source": "selected.mask"}]]}}],
            "sequences": [{"role": "close", "sequence": [
                {"function": "close_receipt", "args": [
                    {"ref": "receipt", "as": "address"}]}]}]}

    def test_fixed_objects_and_call_sequence_share_one_typed_projection(self):
        from tools.configure.static_recipe import StaticObjects
        group = StaticObjects.from_validated(self.definition())
        nodes = group.project(SelectedOwners(7))
        self.assertEqual(len(nodes), 3)
        self.assertTrue(all(isinstance(node, Declaration) for node in nodes[:2]))
        self.assertIsInstance(nodes[2], InitializeSequence)
        self.assertIn(".receipt = &s_receipt, .mask = 7u", nodes[1].render()[0])
        self.assertEqual(nodes[2].function, "close_owner")
        self.assertEqual(nodes[2].calls[0].render(), "close_receipt(&s_receipt)")
        self.assertEqual(group.project(None), ())

    def test_unknown_symbols_source_text_and_mutable_inputs_reject(self):
        from tools.configure.static_recipe import StaticObjects
        for field, value in (("unexpected", True), ("declarations", {"C": "int x;"})):
            document = self.definition()
            document[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                StaticObjects.from_validated(document)
        document = self.definition()
        document["symbols"]["receipt"] = "s_receipt;operate()"
        with self.assertRaises(ValueError):
            StaticObjects.from_validated(document)
        group = StaticObjects.from_validated(self.definition())
        with self.assertRaises(ValueError):
            group.project({"mask": 7})


if __name__ == "__main__":
    unittest.main()
