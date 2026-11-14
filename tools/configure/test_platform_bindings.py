"""Shared platform assembly is selected data over narrow immutable views."""
import importlib
import copy
import subprocess
import sys
import unittest

from tools.configure import configure
from tools.configure.calls import DirectCall, InitializeSequence
from tools.configure.static_ir import Declaration


class PlatformBindingTests(unittest.TestCase):
    def policy(self):
        module = importlib.import_module("tools.configure.platform_bindings")
        facts = configure.load(configure.ROOT /
            "soc/stm32/stm32f4/bindings/platform.json")
        return module.PlatformBindings.from_validated(facts["domains"]["platform"])

    def test_common_import_selects_no_family_adapter(self):
        result = subprocess.run([sys.executable, "-c", (
            "import sys; import tools.configure.platform_bindings; "
            "assert 'tools.configure.stm32f4' not in sys.modules")],
            cwd=configure.ROOT, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_shared_objects_use_finite_nodes_and_selected_budget_values(self):
        policy = self.policy()
        for relative in ("tools/configure/assemblies/qiming-baremetal-gpio.toml",
                "tests/contracts/stm32_assembly/ve-dma.toml",
                "tests/contracts/can_assembly/stm32-both.toml",
                "tests/contracts/ethernet_assembly/stm32-zg.toml"):
            with self.subTest(assembly=relative):
                result = configure.resolve_ir(configure.ROOT / relative)
                view = policy.selection(result)
                self.assertEqual(view.flash_bytes, result.flash["size"])
                self.assertFalse(hasattr(view, "provider_context"))
                self.assertFalse(hasattr(view, "controllers"))
                nodes = (*policy.state_declarations(result),
                         *policy.platform_declarations(result))
                self.assertTrue(nodes)
                self.assertTrue(all(isinstance(node, (Declaration, InitializeSequence))
                                    for node in nodes))
                self.assertIsInstance(policy.platform_system_start(result), DirectCall)

    def test_absent_shared_owners_allocate_no_receipts(self):
        result = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/qiming-baremetal-empty.toml")
        policy = self.policy()
        self.assertEqual(policy.state_declarations(result), ())
        nodes = policy.platform_declarations(result)
        self.assertEqual([node.name for node in nodes], ["s_nx_platform_lifecycle_config"])

    def test_package_owns_c_symbols_without_exposing_configuration_objects(self):
        module = importlib.import_module("tools.configure.platform_bindings")
        facts = configure.load(configure.ROOT /
            "soc/stm32/stm32f4/bindings/platform.json")["domains"]["platform"]
        changed = copy.deepcopy(facts)
        changed["calls"]["system_start"] = "reviewed_system_start"
        policy = module.PlatformBindings.from_validated(changed)
        result = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/qiming-baremetal-empty.toml")
        self.assertEqual(policy.platform_system_start(result).function,
                         "reviewed_system_start")
        fields = changed["objects"]["platform"]["declarations"][-1]["value"]["record"]
        next(field for field in fields if field[0] == "flash_bytes")[1] = {
            "source": "selected.provider_context", "as": "value"}
        policy = module.PlatformBindings.from_validated(changed)
        with self.assertRaisesRegex(ValueError, "Missing typed static source"):
            policy.platform_declarations(result)


if __name__ == "__main__":
    unittest.main()
