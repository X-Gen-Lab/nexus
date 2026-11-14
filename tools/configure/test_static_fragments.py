"""Single-level static fragments share declarations without a template VM."""
from dataclasses import dataclass
import unittest


class StaticFragmentTests(unittest.TestCase):
    def library(self):
        return {"declarations": {"config": [{"role": "config", "type": "config_t",
            "const": True, "value": {"record": [["capacity", 4]]}}]},
            "values": {"owner": {"ref": "port", "as": "address"}}}

    def recipe(self):
        return {"state_type": "state_t", "declarations": [{"use": "config"}],
            "calls": {"prepare": {"function": "prepare", "args": [
                {"use": "owner"}, {"ref": "config", "as": "address"}]}, "close": []}}

    def test_shared_fragments_expand_into_the_existing_finite_recipe(self):
        from tools.configure.static_fragments import StaticFragments
        from tools.configure.static_recipe import StaticRecipe

        @dataclass(frozen=True)
        class Device:
            id: str = "console"

        library = self.library()
        fragments = StaticFragments.from_validated(library)
        library["declarations"]["config"][0]["type"] = "mutated_t"
        expanded = fragments.expand(self.recipe())
        built = StaticRecipe.from_validated(expanded).construct(Device(), None, {}, {}, ())
        self.assertEqual(built.static_declarations[0].type_name, "config_t")
        self.assertEqual(built.initialize.render(),
                         "prepare(&s_nx_port_console, &s_nx_config_console)")

    def test_missing_recursive_overridden_or_executable_fragments_reject(self):
        from tools.configure.static_fragments import StaticFragments
        for library in (
                {"values": {"loop": {"use": "loop"}}},
                {"values": {"code": "RCC->CR = 1"}},
                {"values": {"bad": {"expression": "1+2"}}},
                {"declarations": {"loop": [{"use": "config"}]}},
                {"unknown": {}}):
            with self.subTest(library=library), self.assertRaises(ValueError):
                StaticFragments.from_validated(library)
        fragments = StaticFragments.from_validated(self.library())
        for recipe in ({"value": {"use": "absent"}},
                       {"value": {"use": "owner", "override": 1}}):
            with self.subTest(recipe=recipe), self.assertRaises(ValueError):
                fragments.expand(recipe)


if __name__ == "__main__":
    unittest.main()
