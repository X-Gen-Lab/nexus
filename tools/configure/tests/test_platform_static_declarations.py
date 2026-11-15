"""Platform storage and fixed owner connections use finite typed nodes."""
import unittest

from tools.configure import configure
from tools.configure.calls import InitializeSequence
from tools.configure.static_ir import Declaration
from tools.configure.providers import for_selection


class PlatformStaticDeclarationTests(unittest.TestCase):
    def test_selected_storage_and_platform_connections_have_no_c_fragments(self):
        fixtures = (
            "tools/configure/assemblies/qiming-baremetal-gpio.toml",
            "tests/contracts/stm32_assembly/zg-spi.toml",
            "tests/contracts/can_assembly/stm32-both.toml",
            "tests/contracts/ethernet_assembly/stm32-zg.toml",
            "tests/contracts/f407_capture_audio/zg-i2s3-duplex-blocks.toml",
            "boards/stm32f407zg_qiming_v41/assemblies/sram-startup.toml")
        for fixture in fixtures:
            with self.subTest(fixture=fixture):
                result = configure.resolve_ir(configure.ROOT / fixture)
                provider = for_selection(result)
                nodes = [*provider.state_declarations(result),
                         *provider.platform_declarations(result)]
                for item in result.controllers:
                    nodes.extend(provider.instance_declarations(item, result))
                self.assertTrue(nodes)
                self.assertTrue(all(isinstance(node, (Declaration,
                    InitializeSequence)) for node in nodes),
                    "Static storage and fixed call order must remain typed")

    def test_retained_rtc_controller_uses_fixed_typed_power_connections(self):
        from tools.configure.static_ir import ControllerPowerConnection
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/rtc_bypass_assembly/adopt-periodic.toml")
        indices = list(range(len(result.controllers)))
        nodes = for_selection(result).platform_power_controllers(result, indices)
        self.assertTrue(nodes)
        self.assertTrue(all(isinstance(node, ControllerPowerConnection)
                            for node in nodes))
        for node in nodes:
            self.assertEqual(node.suspend.function, "nx_stm32_rtc_suspend")
            self.assertEqual(node.resume.function, "nx_stm32_rtc_resume")
            self.assertTrue(node.retain_on_failed_resume)


if __name__ == "__main__":
    unittest.main()
