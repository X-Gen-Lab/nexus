"""Default bindings connect typed owners to the handwritten platform engine."""
from dataclasses import replace
import unittest
from unittest.mock import patch

from tools.configure import bindings, configure, runtime_power
from tools.configure.calls import DirectCall
from tools.configure.providers import maintained
from tools.configure.providers.common import ConfigurationError
from tools.configure.tests.native_fixture import native_fixture


class PlatformLifecycleEmissionTests(unittest.TestCase):
    def resolve(self, relative):
        assembly = configure.ROOT / relative
        return configure.resolve_ir(
            assembly, test_support=native_fixture(assembly))

    def test_default_native_and_f4_use_one_handwritten_coordinator(self):
        for relative in (
                "tests/support/native/assemblies/native-gpio.toml",
                "tools/configure/assemblies/qiming-baremetal.toml",
                "tests/contracts/multi_controller_assembly/stm32-zg.toml"):
            with self.subTest(assembly=relative):
                header, source = bindings.emit(self.resolve(relative))
                self.assertIn('"nexus/core/platform_lifecycle.h"', header)
                self.assertIn('"platform_lifecycle_impl.h"', source)
                self.assertIn("nx_platform_lifecycle_start_local(", source)
                self.assertIn("nx_platform_lifecycle_stop_local(", source)
                self.assertIn("static nx_platform_lifecycle_state_t "
                              "s_nx_platform_lifecycle;", source)
                self.assertNotIn("nx_platform_start_impl", source)
                self.assertNotIn("nx_platform_stop_impl", source)
                self.assertNotIn("s_nx_platform_initialized", source)
                self.assertNotIn("s_nx_platform_started", source)
                self.assertNotIn("diagnostic->", source)
                self.assertNotIn("for (", source)
                self.assertNotIn("while (", source)

    def test_retained_failure_policy_is_static_dispatch_metadata(self):
        result = self.resolve(
            "tests/support/native/assemblies/native-gpio.toml")
        provider = maintained("native", test_support=True)
        original = provider.constructor(
            result.controllers[0], result, result.board_bindings,
            result.routes, result.devices)
        for retained in (False, True):
            with self.subTest(retained=retained):
                selected = replace(original, retain_on_failed_init=retained)
                with patch.object(provider, "constructor", return_value=selected):
                    source = bindings.emit(result)[1]
                self.assertIn("(nx_lifecycle_acquisition_t){ "
                              + original.initialize.render() + ", "
                              + str(retained).lower() + " }", source)

    def test_admission_is_a_fixed_direct_call_in_its_physical_case(self):
        result = self.resolve(
            "tests/support/native/assemblies/native-gpio.toml")
        provider = maintained("native", test_support=True)
        original = provider.constructor(
            result.controllers[0], result, result.board_bindings,
            result.routes, result.devices)
        selected = replace(original, admission=(DirectCall("nx_probe_admit"),))
        with patch.object(provider, "constructor", return_value=selected):
            source = bindings.emit(result)[1]
        admission = source[source.index("nx_platform_admit(size_t index)"):]
        self.assertIn("case 0u:", admission)
        self.assertIn("return nx_probe_admit();", admission)
        self.assertNotIn("result.primary", admission)

    def test_permanent_effects_are_a_plan_constant(self):
        result = self.resolve(
            "tests/support/native/assemblies/native-gpio.toml")
        provider = maintained("native", test_support=True)
        original = provider.constructor(
            result.controllers[0], result, result.board_bindings,
            result.routes, result.devices)
        with patch.object(provider, "constructor", return_value=replace(
                original, pre_c_effects=True)):
            source = bindings.emit(result)[1]
        self.assertIn(".permanent_effects = true", source)
        self.assertNotIn("remaining_effects |=", source)

    def test_empty_native_plan_has_no_dummy_controller_storage(self):
        source = bindings.emit(self.resolve(
            "tests/support/native/assemblies/native-empty.toml"))[1]
        self.assertIn(".controller_count = 0u", source)
        self.assertIn(".ordinals = NULL", source)
        self.assertNotIn("s_nx_platform_ordinals[]", source)

    def test_runtime_power_still_rejects_unmaintained_provider(self):
        result = self.resolve(
            "tools/configure/assemblies/qiming-baremetal.toml")
        with self.assertRaisesRegex(ConfigurationError,
                                    "no maintained lifecycle"):
            runtime_power.resolve({"mode": "fixed-run",
                                   "controllers": [result.controllers[0].id]},
                                  result)


if __name__ == "__main__":
    unittest.main()
