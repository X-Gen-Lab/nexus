"""Exact selected lifecycle lease, with System time retained during I/O pause."""
from dataclasses import FrozenInstanceError, replace
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import bindings, configure as gate
from tools.configure.calls import DirectCall
from tools.configure.providers import maintained
from soc.stm32.stm32h743.configure.runtime_power import resolve_runtime_power


class RuntimePowerConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.base = (gate.ROOT / 'tools/configure/assemblies/h743-spi-dma.toml').read_text().replace(
            '"../../../boards/stm32h743zi_hsi_reference"',
            '"' + str(gate.ROOT / 'boards/stm32h743zi_hsi_reference') + '"')

    def assembly(self, power='[power]\nmode = "fixed-run"\ncontrollers = ["bus4"]\n', base=None):
        path = self.directory / 'runtime.toml'
        path.write_text((self.base if base is None else base) + '\n' + power)
        return path

    def selected(self):
        try:
            return gate.resolve_ir(self.assembly())
        except ValueError as error:
            self.fail('Exact runtime power assembly must resolve: ' + str(error))

    def test_exact_selection_resolves_one_immutable_lifecycle_domain(self):
        result = self.selected()
        self.assertEqual(result.runtime_power.mode, 'fixed-run')
        self.assertEqual(result.runtime_power.controllers, ('bus4',))
        with self.assertRaises(FrozenInstanceError):
            result.runtime_power.mode = 'stop'
        self.assertEqual(result.to_dict()['runtime_power']['controllers'],
                         ['bus4'])
        self.assertFalse(result.physical_qualified)

    def test_selected_generated_api_uses_existing_clock_domain(self):
        header, source = bindings.emit(self.selected())
        for api in ('nx_platform_runtime_domain', 'nx_platform_runtime_pause',
                    'nx_platform_runtime_resume'):
            self.assertIn(api + '(', header)
            self.assertIn(api + '(', source)
        self.assertIn('clock_domain_impl.h', source)
        self.assertEqual(source.count('static nx_clock_domain_t '), 1)
        self.assertEqual(source.count('static nx_clock_lease_t '), 1)
        self.assertIn('nx_clock_domain_cold_acquire(', source)
        self.assertIn('nx_clock_domain_cold_release(', source)
        self.assertNotRegex(source, r's_nx_runtime_(?:domain|lease)\.')

    def test_resume_admission_precedes_reopen_and_retains_rejection(self):
        result = self.selected()
        provider = maintained(result.soc_family)
        original = provider.constructor
        def with_admission(*args):
            return replace(original(*args),
                           admission=(DirectCall("nx_test_runtime_admit"),))
        with patch.object(provider, "constructor", side_effect=with_admission):
            source = bindings.emit(result)[1]
        resume = source[source.index(
            "nx_platform_start_result_t nx_platform_runtime_resume(void) {"):]
        resumed = resume[:resume.index("\n}")]
        admitted = resumed.index("result.primary = nx_test_runtime_admit();")
        reopen = resumed.index("result.primary = nx_clock_domain_cold_reopen(&s_nx_runtime_domain);")
        self.assertLess(resumed.index("nx_clock_domain_cold_resume_admit(&s_nx_runtime_lease)"), admitted)
        self.assertLess(admitted, reopen)
        self.assertIn("if (result.primary != NX_SUCCESS) { return result; }",
                      resumed[admitted:reopen])
        self.assertNotIn("nx_platform_stop_io", resumed[admitted:reopen])

    def test_logical_controller_name_does_not_select_physical_hardware(self):
        for name in ('sensor_link', 'bus7'):
            with self.subTest(name=name):
                base = self.base.replace('bus4', name)
                power = ('[power]\nmode="fixed-run"\ncontrollers=["' +
                         name + '"]\n')
                result = gate.resolve_ir(self.assembly(power, base))
                self.assertEqual(result.runtime_power.controllers, (name,))
                self.assertEqual(result.controllers[0].id, name)
                self.assertEqual(result.controllers[0].controller, 'SPI4')
                self.assertEqual(result.devices[0].controller, name)
                header, source = bindings.emit(result)
                self.assertIn('nx_platform_runtime_pause(', header)
                self.assertIn('s_nx_port_' + name, source)
                self.assertNotIn('s_nx_port_bus4', source)

    def test_renamed_controller_requires_matching_power_reference(self):
        with self.assertRaises(ValueError):
            gate.resolve_ir(self.assembly(base=
                self.base.replace('bus4', 'sensor_link')))

    def test_logical_id_freedom_keeps_the_exact_hardware_contract(self):
        base = self.base.replace('bus4', 'sensor_link')
        power = {'mode': 'fixed-run', 'controllers': ['sensor_link']}
        result = gate.resolve_ir(self.assembly(
            '[power]\nmode="fixed-run"\ncontrollers=["sensor_link"]\n', base))
        controller = result.controllers[0]
        endpoint = result.devices[0]
        unsupported = (
            replace(result, cpu_profile=replace(result.cpu_profile,
                                                backend='freertos')),
            replace(result, clock_profile='hsi64-fixed-cache-on'),
            replace(result, controllers=(replace(controller,
                                                 controller='SPI5'),)),
            replace(result, controllers=(replace(controller,
                                                 mode='short-poll'),)),
            replace(result, devices=(replace(endpoint, options=replace(
                endpoint.options, mode=1)),)),
        )
        for index, selected in enumerate(unsupported):
            with self.subTest(case=index), self.assertRaisesRegex(
                    ValueError, 'supports only'):
                resolve_runtime_power(power, selected)

    def test_unselected_default_keeps_no_runtime_state_or_public_api(self):
        result = gate.resolve_ir(self.assembly(''))
        header, source = bindings.emit(result)
        self.assertIsNone(result.runtime_power)
        for name in ('nx_platform_runtime_', 'nx_clock_domain_', 'nx_clock_lease_t'):
            self.assertNotIn(name, header + source)

    def test_generated_selection_derives_component_without_authored_vote(self):
        output = self.directory / 'generated'
        try:
            gate.configure(self.assembly(), output)
        except ValueError as error:
            self.fail('Selected runtime power bundle required: ' + str(error))
        self.assertIn('NEXUS_RUNTIME_POWER "ON"', (output / 'selection.cmake').read_text())
        self.assertIn('tools/configure/runtime_power.py',
                      (output / 'configuration_code_paths.json').read_text())

    def test_unknown_or_malformed_power_tables_are_rejected(self):
        for power in ('power = true', '[power]\nmode="stop"\ncontrollers=["bus4"]',
                      '[power]\nmode="fixed-run"\ncontrollers=[]',
                      '[power]\nmode="fixed-run"\ncontrollers=["bus4","bus4"]',
                      '[power]\nmode="fixed-run"\ncontrollers=["device4"]',
                      '[power]\nmode="fixed-run"\ncontrollers=[1]',
                      '[power]\nmode="fixed-run"\ncontrollers=["bus4"]\nfrequency=64000000',
                      '[power]\ncontrollers=["bus4"]'):
            with self.subTest(power=power), self.assertRaises(ValueError):
                gate.resolve_ir(self.assembly(power))

    def test_other_profiles_and_controller_modes_have_no_silent_fallback(self):
        for old, new in (('hsi64-fixed-cache-off', 'hsi64-fixed-cache-on'),
                         ('hsi64-fixed-cache-off', 'hsi64-pll384-cache-off'),
                         ('mode = "dma"', 'mode = "poll"')):
            with self.subTest(choice=new), self.assertRaises(ValueError):
                gate.resolve_ir(self.assembly(base=self.base.replace(old, new)))

    def test_unenrolled_additional_controller_is_rejected(self):
        additional = '\n[gpio.led]\nbinding="led"\nmode="output"\n'
        with self.assertRaises(ValueError):
            gate.resolve_ir(self.assembly(base=self.base + additional))

    def test_explicit_non_table_power_is_not_an_absent_selection(self):
        for value in ("[]", "true", '"fixed-run"'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                gate.resolve_ir(self.assembly("", base=
                    "power = " + value + "\n" + self.base))

    def test_failure_never_publishes_partial_runtime_bundle(self):
        output = self.directory / 'out'
        with self.assertRaises(ValueError):
            gate.configure(self.assembly('[power]\nmode="fixed-run"\ncontrollers=["foreign"]'), output)
        self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
