"""Finite endpoint exports preserve immutable parent/child identities."""
from dataclasses import dataclass
from types import MappingProxyType
import unittest


@dataclass(frozen=True)
class Options:
    mode: str


@dataclass(frozen=True)
class Child:
    id: str
    driver: str
    options: Options
    controller: str = "bus"


def recipe():
    return {"limit": 4, "cases": {
        "memory_window-endpoint": {
            "type": "nx_memory_window_port_t", "ops": "nx_window_ops",
            "parents": {"parent_port": "port"},
            "context": {"ref": "parent_port", "as": "address"}},
        "flash_async-endpoint": {
            "type": "nx_flash_async_port_t", "ops": "nx_flash_ops",
            "parents": {"parent_port": "port"},
            "context": {"ref": "parent_port", "as": "address"}},
    }}


class StaticEndpointFacesTests(unittest.TestCase):
    def compile(self, value=None):
        from tools.configure.static_endpoints import EndpointFaces
        return EndpointFaces.from_validated(recipe() if value is None else value)

    def test_single_child_export_uses_endpoint_id_and_existing_parent_context(self):
        child = Child("external-ram", "memory_window-endpoint", Options("cpu-window"))
        declarations = self.compile().project((child,), {"port": "s_nx_port_bus"}, None)
        self.assertEqual(["s_nx_device_external_ram", "nx_device_external_ram"],
                         [item.name for item in declarations])
        source = "\n".join(line for item in declarations for line in item.render())
        self.assertIn(".ops = &nx_window_ops", source)
        self.assertIn(".context = &s_nx_port_bus", source)
        self.assertIn("const nx_memory_window_port_t* const nx_device_external_ram", source)
        self.assertNotIn("malloc", source)

    def test_multiple_driver_cases_share_parent_without_mutable_registry(self):
        children = (Child("ram", "memory_window-endpoint", Options("cpu-window")),
                    Child("rom", "flash_async-endpoint", Options("nor-cfi")))
        declarations = self.compile().project(children, {"port": "owner"}, None)
        self.assertEqual(4, len(declarations))
        self.assertEqual(["s_nx_device_ram", "nx_device_ram", "s_nx_device_rom", "nx_device_rom"],
                         [item.name for item in declarations])
        self.assertEqual(["nx_memory_window_port_t", "nx_flash_async_port_t"],
                         [declarations[index].type_name for index in (0, 2)])

    def test_unknown_duplicate_mutable_or_excess_children_are_rejected(self):
        faces = self.compile()
        child = Child("ram", "memory_window-endpoint", Options("cpu-window"))
        for children in ([child], (child, child), (Child("x", "unknown-endpoint", Options("x")),),
                         tuple(Child("ram" + str(i), child.driver, child.options) for i in range(5))):
            with self.subTest(children=children), self.assertRaises(ValueError):
                faces.project(children, {"port": "owner"}, None)
        with self.assertRaises(ValueError):
            faces.project((child,), {"unrelated": "owner"}, None)

    def test_parent_and_context_grammar_rejects_unbound_or_executable_forms(self):
        for edit in (
            lambda p: p["cases"]["memory_window-endpoint"].update(context={"expression": "call()"}),
            lambda p: p["cases"]["memory_window-endpoint"].update(context={"ref": "unknown", "as": "address"}),
            lambda p: p["cases"]["memory_window-endpoint"].update(type="int; execute()"),
            lambda p: p.update(limit=0),
        ):
            value = recipe()
            edit(value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.compile(value)

    def test_parent_array_reference_uses_explicit_parent_bounds(self):
        value = recipe()
        case = value["cases"]["memory_window-endpoint"]
        case["parents"] = {"parent_array": "objects"}
        case["context"] = {"ref": "parent_array", "as": "address", "index": 1}
        child = Child("ram", "memory_window-endpoint", Options("cpu-window"))
        faces = self.compile(value)
        declarations = faces.project((child,), {"objects": "states"}, None,
                                     parent_counts={"objects": 2})
        source = "\n".join(line for item in declarations for line in item.render())
        self.assertIn(".context = &states[1u]", source)
        for count in (None, {"objects": 1}):
            with self.subTest(count=count), self.assertRaises(ValueError):
                faces.project((child,), {"objects": "states"}, None,
                              parent_counts=count)

    def test_recipe_is_frozen_after_validation(self):
        value = recipe()
        faces = self.compile(value)
        value["cases"]["memory_window-endpoint"]["ops"] = "changed"
        child = Child("ram", "memory_window-endpoint", Options("cpu-window"))
        declarations = faces.project((child,), MappingProxyType({"port": "owner"}), None)
        source = "\n".join(line for item in declarations for line in item.render())
        self.assertIn("nx_window_ops", source)
        self.assertNotIn("changed", source)


class StaticChildGroupTests(unittest.TestCase):
    def recipe(self):
        return {"limit": 4, "cases": {"memory_window-endpoint": {
            "parents": {"parent_port": "port"},
            "declarations": [{"role": "window", "type": "nx_window_state_t", "const": False,
                "value": {"record": [["bus", {"ref": "parent_port", "as": "address"}],
                                     ["index", {"source": "item.index"}]]}}],
            "calls": {"prepare": [{"function": "nx_window_prepare", "args": [
                {"ref": "window", "as": "address"}, {"ref": "parent_port", "as": "address"},
                {"source": "item.index"}]}],
                "close": [{"function": "nx_window_close", "args": [{"ref": "window", "as": "address"}]}]},
            "face": {"type": "nx_memory_window_port_t", "ops": "nx_window_ops",
                     "context": {"ref": "window", "as": "address"}},
        }}}

    def compile(self, value=None):
        from tools.configure.static_endpoints import ChildGroups
        return ChildGroups.from_validated(self.recipe() if value is None else value)

    def test_child_storage_calls_and_export_share_one_selected_identity(self):
        children = (Child("a", "memory_window-endpoint", Options("cpu-window")),
                    Child("b", "memory_window-endpoint", Options("cpu-window")))
        built = self.compile().project(children, {"port": "parent_owner"}, None)
        self.assertEqual(["s_nx_window_a", "s_nx_device_a", "nx_device_a",
                          "s_nx_window_b", "s_nx_device_b", "nx_device_b"],
                         [item.name for item in built.declarations])
        self.assertEqual([0, 1], [call.arguments[-1] for call in built.prepare])
        source = "\n".join(line for item in built.declarations for line in item.render())
        self.assertIn(".context = &s_nx_window_b", source)
        self.assertIn(".index = 1u", source)
        self.assertEqual(["nx_window_close", "nx_window_close"],
                         [call.function for call in built.close])

    def test_child_metadata_is_an_exact_immutable_lookup_without_copying_endpoint(self):
        from tools.configure.records import freeze
        @dataclass(frozen=True)
        class Integration:
            constants: object
        value = self.recipe()
        value["bindings"] = "selected.constants.packet.endpoints"
        value["cases"]["memory_window-endpoint"]["declarations"][0]["value"]["record"].append(
            ["dma_words", {"source": "item.binding.dma_words"}])
        children = (Child("a", "memory_window-endpoint", Options("cpu-window")),)
        selected = Integration(freeze({"packet": {"endpoints": {"a": {"dma_words": 12}}}}))
        built = self.compile(value).project(children, {"port": "owner"}, selected)
        source = "\n".join(line for item in built.declarations for line in item.render())
        self.assertIn(".dma_words = 12u", source)
        for metadata in ({"packet": {"endpoints": {}}},
                         {"packet": {"endpoints": {"other": {"dma_words": 12}}}}):
            with self.subTest(metadata=metadata), self.assertRaises(ValueError):
                self.compile(value).project(children, {"port": "owner"}, Integration(freeze(metadata)))

    def test_disabled_child_storage_has_no_count_read_or_allocated_role(self):
        value = self.recipe()
        value["cases"]["memory_window-endpoint"]["declarations"].append({
            "role": "dma", "type": "uint32_t", "const": False, "enabled": False,
            "count": {"source": "item.binding.unselected_dma_words"}})
        child = Child("a", "memory_window-endpoint", Options("cpu-window"))
        built = self.compile(value).project((child,), {"port": "owner"}, None)
        self.assertNotIn("s_nx_dma_a", [item.name for item in built.declarations])

    def test_declaration_anchor_is_one_explicit_parent_role(self):
        value = self.recipe()
        value["before_role"] = "endpoint_table"
        grouped = self.compile(value)
        self.assertEqual("endpoint_table", grouped.before_role)
        value["before_role"] = "execute();"
        with self.assertRaises(ValueError):
            self.compile(value)

    def test_child_role_cannot_hide_parent_alias_or_reference_other_child(self):
        for mutate in (
            lambda p: p["cases"]["memory_window-endpoint"]["declarations"][0].update(role="parent_port"),
            lambda p: p["cases"]["memory_window-endpoint"]["face"].update(context={"ref": "other_child", "as": "address"}),
        ):
            value = self.recipe()
            mutate(value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.compile(value)



if __name__ == '__main__':
    unittest.main()
