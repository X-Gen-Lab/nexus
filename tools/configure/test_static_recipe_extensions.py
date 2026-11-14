"""Finite static connections preserve ownership, types and lifecycle ordering."""
from dataclasses import dataclass, replace
import unittest

from tools.configure.calls import Address, InitializeSequence
from tools.configure.static_ir import Signed32
from tools.configure.static_recipe import StaticRecipe


@dataclass(frozen=True)
class Options:
    index: int = 1
    offset: int = -1
    enabled: bool = False
    owner: str | None = "memory"
    labels: tuple = ("up", "down")


@dataclass(frozen=True)
class Item:
    id: str = "bus"
    options: Options = Options()
    integration: Options = Options()


@dataclass(frozen=True)
class Child:
    id: str
    driver: str = "memory_window-endpoint"
    controller: str = "bus"
    options: Options = Options()


@dataclass(frozen=True)
class Selection:
    controllers: tuple = (Item(), Item("memory"))


@dataclass(frozen=True)
class Vector:
    name: str
    enabled: bool


@dataclass(frozen=True)
class VectorSelection:
    vectors: tuple = (Vector("not a C symbol", False), Vector("IRQSecond", True))


def ref(role, **fields):
    return {"ref": role, "as": "address", **fields}


def call(name, *arguments):
    return {"function": name, "args": list(arguments)}


def recipe():
    return {"state_type": "owner_t", "ops": "owner_ops",
        "declarations": [{"role": "storage", "type": "uint32_t", "const": False,
                          "count": 2}],
        "calls": {"prepare": call("parent_prepare", ref("port")),
                  "close": [call("parent_close", ref("port"))]}}


def children():
    return {"limit": 2, "cases": {"memory_window-endpoint": {
        "parents": {"parent_port": "port"},
        "declarations": [{"role": "window", "type": "window_t", "const": False}],
        "calls": {"prepare": [call("child_prepare", ref("window"), ref("parent_port"),
                                    {"source": "item.index"})],
                  "close": [call("child_close", ref("window"))]},
        "face": {"type": "window_port_t", "ops": "window_ops",
                 "context": ref("window")}}}}


class StaticRecipeExtensionTests(unittest.TestCase):
    def build(self, author=None, item=Item(), devices=()):
        return StaticRecipe.from_validated(author or recipe()).construct(
            item, Selection(), None, None, devices)

    def test_children_prepare_after_parent_and_close_before_parent(self):
        for sequence in (False, True):
            with self.subTest(sequence=sequence):
                author = recipe()
                if sequence:
                    author["calls"]["prepare"] = {"role": "prepare_parent",
                        "sequence": [call("parent_begin", ref("port")),
                                     call("parent_prepare", ref("port"))]}
                author.update(child_groups=children(), pre_c_effects=True,
                              face_context=ref("port", member="base"))
                result = self.build(author, devices=(Child("ram-a"), Child("ram-b"),
                                                     Child("elsewhere", controller="other")))
                self.assertTrue(result.retain_on_failed_init)
                self.assertTrue(result.pre_c_effects)
                self.assertEqual(Address("s_nx_port_bus.base"), result.face_context)
                sequence_node, = result.initialize_sequences
                self.assertIsInstance(sequence_node, InitializeSequence)
                expected = (["parent_begin"] if sequence else []) + [
                    "parent_prepare", "child_prepare", "child_prepare"]
                self.assertEqual(expected, [node.function for node in sequence_node.calls])
                self.assertEqual(sequence_node.function, result.initialize.function)
                self.assertEqual(["child_close", "child_close", "parent_close"],
                                 [node.function for node in result.stop])
                self.assertEqual(7, len(result.static_declarations))

    def test_empty_child_selection_preserves_the_parent_initializer(self):
        author = recipe()
        author["child_groups"] = children()
        result = self.build(author)
        self.assertEqual("parent_prepare", result.initialize.function)
        self.assertEqual((), result.initialize_sequences)
        self.assertEqual(1, len(result.static_declarations))

    def test_child_array_alias_uses_the_actual_parent_storage_bound(self):
        author = recipe()
        group = children()
        selected = group["cases"]["memory_window-endpoint"]
        selected["parents"]["parent_slots"] = "storage"
        selected["calls"]["prepare"][0]["args"].append(ref("parent_slots", index={
            "source": "item.child.options.index", "as": "u32"}))
        author["child_groups"] = group
        result = self.build(author, devices=(Child("ram"),))
        self.assertIn("&s_nx_storage_bus[1u]",
                      result.initialize_sequences[0].calls[1].render())
        with self.assertRaisesRegex(ValueError, "exceeds"):
            self.build(author, devices=(Child("ram", options=Options(index=2)),))

    def test_face_context_and_pre_c_values_are_typed(self):
        author = recipe()
        author.update(face_context=ref("port", member="controller"), pre_c_effects=True)
        self.assertEqual(Address("s_nx_port_bus.controller"), self.build(author).face_context)
        for field, value in (("pre_c_effects", 1), ("face_context", {"symbol": "state"}),
                             ("face_context", "&state")):
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                self.build({**author, field: value})

    def test_global_and_explicit_symbols_match_every_reference(self):
        author = recipe()
        author["declarations"][0]["static"] = False
        author["calls"]["prepare"]["args"].append(ref("storage", index=1))
        result = self.build(author)
        self.assertEqual("g_nx_storage_bus", result.static_declarations[0].name)
        self.assertIn("&g_nx_storage_bus[1u]", result.initialize.render())
        author["declarations"][0]["symbol"] = "g_nx_boot_receipt"
        result = self.build(author)
        self.assertEqual("g_nx_boot_receipt", result.static_declarations[0].name)
        self.assertIn("&g_nx_boot_receipt[1u]", result.initialize.render())
        author["declarations"][0]["symbol"] = "g;execute()"
        with self.assertRaises(ValueError):
            self.build(author)

    def test_explicit_symbol_collisions_are_rejected(self):
        author = recipe()
        author["declarations"][0]["symbol"] = "shared"
        author["declarations"].append({"role": "second", "type": "uint32_t",
                                       "const": False, "symbol": "shared"})
        with self.assertRaisesRegex(ValueError, "symbol|distinct"):
            self.build(author)

    def test_owner_role_is_finite_and_owner_must_be_selected(self):
        author = recipe()
        owner = {"owner": {"source": "item.options.owner", "role": "sdram_config"}}
        author["calls"]["prepare"]["args"].append(owner)
        self.assertIn("&s_nx_sdram_config_memory", self.build(author).initialize.render())
        with self.assertRaisesRegex(ValueError, "selected"):
            self.build(author, replace(Item(), options=replace(Options(), owner="ghost")))
        owner["owner"]["role"] = "config;execute()"
        with self.assertRaises(ValueError):
            self.build(author)

    def test_child_array_owner_can_be_a_symbol_only_with_selected_identity(self):
        author = recipe()
        author["child_groups"] = children()
        reference = {"owner": {"source": "item.options.owner", "role": "window",
                               "as": "symbol"}}
        author["calls"]["prepare"]["args"].append(reference)
        item = replace(Item(), options=replace(Options(), owner="ram"))
        self.assertIn("parent_prepare(&s_nx_port_bus, s_nx_window_ram)",
                      self.build(author, item, (Child("ram"),)).initialize_sequences[0].calls[0].render())
        with self.assertRaisesRegex(ValueError, "selected"):
            self.build(author, item, (Child("ram", controller="other"),))
        reference["owner"]["as"] = "raw"
        with self.assertRaises(ValueError):
            self.build(author, item, (Child("ram"),))

    def test_irq_guard_skips_unselected_names_and_requires_bool(self):
        author = recipe()
        author["irq"] = [{"name": {"source": "selected.missing", "as": "symbol"},
                          "enabled": {"source": "item.options.enabled", "as": "bool"},
                          "calls": [call("irq_handler", ref("port"))], "barrier": None}]
        self.assertEqual((), self.build(author).irq_connections)
        author["irq"][0]["name"] = "IRQHandler"
        self.assertEqual(1, len(self.build(author, replace(Item(), options=replace(
            Options(), enabled=True))).irq_connections))
        author["irq"][0]["enabled"] = 1
        with self.assertRaisesRegex(ValueError, "boolean"):
            self.build(author)

    def test_irq_map_filters_disabled_entries_before_symbol_resolution(self):
        author = recipe()
        author["irq"] = [{"map": {"source": "selected.vectors", "limit": 2,
            "value": {"name": {"source": "entry.name", "as": "symbol"},
                      "enabled": {"source": "entry.enabled", "as": "bool"},
                      "calls": [call("irq_handler", ref("port"))], "barrier": None}}}]
        result = self.build(author, replace(Item(), integration=VectorSelection()))
        self.assertEqual(["IRQSecond"], [entry.name for entry in result.irq_connections])

    def test_signed_constants_and_sources_keep_int32_bounds(self):
        author = recipe()
        author["calls"]["prepare"]["args"] += [{"s32": -1},
            {"source": "item.options.offset", "as": "s32"}]
        self.assertEqual((Signed32(-1), Signed32(-1)), self.build(author).initialize.arguments[-2:])
        for offset in (-2147483649, 2147483648, True):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                self.build(author, replace(Item(), options=replace(Options(), offset=offset)))

    def test_array_index_uses_direct_typed_source_and_checks_the_count(self):
        author = recipe()
        reference = ref("storage", index={"source": "item.options.index", "as": "u32"})
        author["calls"]["prepare"]["args"].append(reference)
        self.assertIn("&s_nx_storage_bus[1u]", self.build(author).initialize.render())
        for index in (2, -1, True):
            with self.subTest(index=index), self.assertRaises(ValueError):
                self.build(author, replace(Item(), options=replace(Options(), index=index)))
        for bad in ({"source": "item.options.index", "as": "bool"},
                    {"source": "item.options.index + 1", "as": "u32"}):
            reference["index"] = bad
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                self.build(author)

    def test_scalar_string_map_only_emits_explicit_enum_or_symbol_values(self):
        author = recipe()
        author["declarations"] = [{"role": "encodings", "type": "uint32_t", "const": True,
            "count": {"length": "item.options.labels", "limit": 2},
            "value": {"map": {"source": "item.options.labels", "limit": 2,
                "value": {"source": "entry", "values": {"up": 1, "down": 2}}}}}]
        self.assertEqual("{ 1u, 2u }", self.build(author).static_declarations[0].initializer.render())
        with self.assertRaises(ValueError):
            self.build(author, replace(Item(), options=replace(Options(), labels=("execute()",))))
        author["declarations"][0]["value"]["map"]["value"] = {"source": "entry"}
        with self.assertRaises(ValueError):
            self.build(author)


if __name__ == "__main__":
    unittest.main()
