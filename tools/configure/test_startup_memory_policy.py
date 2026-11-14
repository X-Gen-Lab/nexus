"""Boot placement references the selected external endpoint geometry once."""
import copy
import importlib
import subprocess
import sys
import unittest

from tools.configure import configure
from tools.configure.policies.external_bus import ExternalBusPolicy


class StartupMemoryPolicyTests(unittest.TestCase):
    def facts(self):
        return configure.load(configure.ROOT /
            "soc/stm32/stm32f4/bindings/startup_memory.json")["domains"]["startup_memory"]

    def external(self):
        return configure.load(configure.ROOT /
            "soc/stm32/stm32f4/bindings/external_bus_policy.json")["domains"]["external_bus"]

    def test_common_import_does_not_select_a_family(self):
        result = subprocess.run([sys.executable, "-c", (
            "import sys; import tools.configure.startup_memory; "
            "assert 'tools.configure.stm32f4' not in sys.modules")],
            cwd=configure.ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_budget_capacity_uses_the_existing_endpoint_author(self):
        module = importlib.import_module("tools.configure.startup_memory")
        facts = self.facts()
        external = self.external()
        external["endpoints"][facts["endpoint_profile"]]["bytes"] = 0x200000
        selected = ExternalBusPolicy.from_validated(external)
        policy = module.StartupMemoryPolicy.from_validated(facts, external_bus=selected)
        self.assertIs(policy.endpoint, selected.endpoints[facts["endpoint_profile"]])
        assembly = configure.load_assembly(configure.ROOT /
            "tools/configure/assemblies/qiming-baremetal-empty.toml")
        budgets = {"regions": {"addsram": {"limit_bytes": 0x180000}},
                   "main_stack_bytes": 512}
        self.assertEqual(policy.defer_budget(assembly, budgets, "STM32F407ZGT6"),
                         ("addsram",))

    def test_unknown_policy_and_insufficient_stack_are_rejected(self):
        module = importlib.import_module("tools.configure.startup_memory")
        external = ExternalBusPolicy.from_validated(self.external())
        facts = self.facts()
        changed = copy.deepcopy(facts)
        changed["register_program"] = "write()"
        with self.assertRaises(ValueError):
            module.StartupMemoryPolicy.from_validated(changed, external_bus=external)
        policy = module.StartupMemoryPolicy.from_validated(facts, external_bus=external)
        assembly = configure.load_assembly(configure.ROOT /
            "tools/configure/assemblies/qiming-baremetal-empty.toml")
        with self.assertRaisesRegex(ValueError, "512|stack|MSP"):
            policy.defer_budget(assembly,
                {"regions": {"addsram": {"limit_bytes": 4096}},
                 "main_stack_bytes": 256}, "STM32F407ZGT6")


if __name__ == "__main__":
    unittest.main()
