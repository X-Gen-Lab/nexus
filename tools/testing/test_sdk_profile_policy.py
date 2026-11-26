"""Installed loaned-RX and MCUboot profiles preserve narrow API boundaries."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.maintenance import consumer_profiles
from tools.configure import configure
from tools.evidence.common import file_identity

ROOT = Path(__file__).resolve().parents[2]
NEW_NAMES = ('f407 loaned rx', 'native mcuboot control',
             'f407 mcuboot control', 'f407 mcuboot boot')


class SDKProfilePolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for name, filename in [('consumers', 'verify_consumers.py'),
                               ('packager', 'package_source_sdk.py')]:
            spec = importlib.util.spec_from_file_location(
                'profile_' + name, ROOT / 'cmake/package' / filename)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            setattr(cls, name, module)

    def test_optional_ui_and_drive_sources_are_exported_only_when_selected(self):
        plain = self.packager.source_profile()
        default_dependencies = self.packager.runtime_dependency_closure(plain)
        for component, path, source, notice in (
                ('cia402', 'ext/canopennode', '301/CO_NMT_Heartbeat.c', 'LICENSE'),
                ('lvgl', 'ext/lvgl', 'src/core/lv_obj.c', 'LICENCE.txt')):
            with self.subTest(component=component):
                self.assertNotIn(path, default_dependencies)
                profile = self.packager.source_profile(components=(component,))
                self.assertIn(path,
                              self.packager.runtime_dependency_closure(profile))
                required = set(self.packager.required_files(ROOT, profile))
                self.assertIn(path + '/' + source, required)
                self.assertIn(path + '/' + notice, required)

    def test_native_controls_require_explicit_host_support_export(self):
        controls = ('model.h', 'rtc_lptim_model.h', 'spi_dma_model.h')
        for host_support in (False, True):
            profile = self.packager.source_profile(host_support=host_support)
            required = set(self.packager.required_files(ROOT, profile))
            for header in controls:
                name = 'tests/support/native/include/nexus/io/native/' + header
                with self.subTest(host_support=host_support, header=header):
                    self.assertEqual(self.packager.selected(
                        name, root=ROOT, profile=profile), host_support)
                    self.assertEqual(name in required, host_support)

    def test_selected_payload_contains_only_maintained_smoke_consumers(self):
        expected = {
            'cmake/package/verification/io_evolution/consumers/' + item[2] + '.c'
            for item in consumer_profiles.SELECTED_CONSUMERS}
        actual = {name for name in self.packager.SDK_VERIFICATION_FILES
                  if '/io_evolution/consumers/' in name}
        self.assertEqual(actual, expected)

    def profile(self, name):
        return next(item for item in consumer_profiles.SERVICE_CONSUMERS
                    if item.name == name)

    def prepared(self, directory, name):
        profile = self.profile(name)
        prefix = directory / 'prefix'
        sdk = prefix / 'share/nexus/src'
        shutil.copytree(ROOT / 'cmake/package/verification' / profile.scenario,
                        sdk / 'cmake/package/verification' / profile.scenario)
        if profile.scenario == 'mcuboot_swap':
            key = Path('cmake/package/verification/mcuboot_boot/public_key.c')
            (sdk / key).parent.mkdir(parents=True)
            shutil.copyfile(ROOT / key, sdk / key)
        (sdk / 'dependencies').mkdir()
        shutil.copyfile(ROOT / 'dependencies/platform-compatibility.json',
                        sdk / 'dependencies/platform-compatibility.json')
        if name == 'native mcuboot control':
            controls = Path('tests/support/native/include')
            shutil.copytree(ROOT / controls, sdk / controls)
            files = {path.relative_to(sdk).as_posix(): file_identity(path)['sha256']
                     for path in sdk.rglob('*') if path.is_file()}
            (sdk / '.nexus-source-sdk.json').write_text(json.dumps({
                'profile': {'host_support': True}, 'files_sha256': files}))
        def execute(argv):
            return subprocess.run(argv, check=True, capture_output=True, text=True)
        source, build, command = self.consumers.prepare_service_consumer(
            prefix, directory, profile, execute, fixture=True,
            expected_revision='a' * 40)
        return sdk, source, build, command, profile

    def image_fixture(self, directory, name):
        sdk, source, build, _, profile = self.prepared(directory, name)
        if not (sdk / '.nexus-source-sdk.json').exists():
            (sdk / '.nexus-source-sdk.json').write_text('{}\n')
        config = build / 'nexus/generated'
        config.mkdir(parents=True)
        (build / 'resolved-sdk.txt').write_text(str(sdk))
        (build / 'resolved-config.txt').write_text(str(config))
        (config / 'bindings.c').write_text('/* explicit process fixture */\n')
        family = ('gd32f470' if name == 'gd470 loaned rx' else
                  'stm32f4' if profile.arm else 'native')
        resolved = {'enum_abi': 'short-enums' if profile.arm else 'native-int',
                    'soc_family': family,
                    'backend': 'baremetal' if profile.arm else 'native',
                    'cpu': {'arch': 'cortex-m4' if profile.arm else 'native',
                            'float_abi': 'hard', 'fpu': 'fpv4-sp-d16'}}
        (config / 'resolved.json').write_text(json.dumps(resolved))
        flags = ['-std=c11'] + (['-mcpu=cortex-m4', '-mthumb',
                 '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard', '-fshort-enums']
                if profile.arm else [])
        files = [path for path in source.glob('*.c')] + [config / 'bindings.c']
        rows = [{'file': str(path), 'directory': str(build),
                 'arguments': ['cc', *flags, '-c', str(path)]} for path in files]
        (build / 'compile_commands.json').write_text(json.dumps(rows))
        image = build / 'bin' / ('installed_services.elf' if profile.arm
                                 else 'installed_services')
        image.parent.mkdir()
        header = bytearray(64)
        header[:6] = b'\x7fELF\x01\x01' if profile.arm else b'\x7fELF\x02\x01'
        header[18:20] = (40 if profile.arm else 62).to_bytes(2, 'little')
        image.write_bytes(header)
        if profile.arm:
            (image.parent / 'installed_services.resources.json').write_text(
                json.dumps({'status': 'passed', 'violations': [],
                            'elf': file_identity(image),
                            'resolved': file_identity(config / 'resolved.json')}))
        calls = []
        def execute(argv):
            calls.append(argv)
            if 'retention.py' in ' '.join(argv):
                Path(argv[argv.index('--output') + 1]).write_text(json.dumps({
                    'status': 'passed', 'elf': file_identity(image)}))
            stdout = ''.join(symbol + ' T 1000 10\n' for symbol in sorted(
                self.consumers.service_symbols(profile)))
            return subprocess.CompletedProcess(argv, 0, stdout, '')
        return sdk, source, build, profile, calls, execute

    def test_new_image_checks_join_all_owned_translation_units(self):
        for name in NEW_NAMES:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                sdk, source, build, profile, calls, execute = self.image_fixture(
                    Path(tmp), name)
                report = self.consumers.check_service_image(
                    sdk, source, build, profile, execute)
                self.assertEqual(report['runtime_executed'], not profile.arm)
                self.assertEqual(report['physical_status'], 'not_executed')
                self.assertTrue(calls)

    def test_loaned_checker_requires_selected_factory_and_public_gpio(self):
        required = {'nx_factory_gpio', 'nx_gpio_port_read'}
        for name in NEW_NAMES[:1]:
            for missing in sorted(required):
                with self.subTest(name=name, missing=missing):
                    with tempfile.TemporaryDirectory() as tmp:
                        fixture = self.image_fixture(Path(tmp), name)
                        sdk, source, build, profile, _, execute = fixture
                        self.assertLessEqual(
                            required, self.consumers.service_symbols(profile))

                        def without_selected_gpio(argv):
                            result = execute(argv)
                            if argv[0] == 'arm-none-eabi-nm':
                                result.stdout = ''.join(
                                    line + '\n'
                                    for line in result.stdout.splitlines()
                                    if not line.startswith(missing + ' '))
                            return result

                        with self.assertRaisesRegex(
                                ValueError, 'retention is incomplete'):
                            self.consumers.check_service_image(
                                sdk, source, build, profile,
                                without_selected_gpio)

    def test_full_boot_cannot_claim_a_key_source_without_compiling_it(self):
        with tempfile.TemporaryDirectory() as tmp:
            sdk, source, build, profile, calls, execute = self.image_fixture(
                Path(tmp), 'f407 mcuboot boot')
            path = build / 'compile_commands.json'
            rows = json.loads(path.read_text())
            rows = [row for row in rows
                    if Path(row['file']).name != 'public_key.c']
            path.write_text(json.dumps(rows))
            with self.assertRaisesRegex(ValueError, 'unique compile row'):
                self.consumers.check_service_image(
                    sdk, source, build, profile, execute)
            self.assertEqual(calls, [])

    def test_uart_hardware_facts_are_required_for_both_sdk_backends(self):
        expected = {'soc/stm32/stm32f4/drivers/uart/uart_hardware.c',
                    'soc/stm32/stm32f4/drivers/uart/stm32f407_uart_hardware.h'}
        for backend in ('baremetal', 'freertos'):
            with self.subTest(backend=backend):
                profile = self.packager.source_profile(backend=backend)
                self.assertLessEqual(expected, set(
                    self.packager.required_files(ROOT, profile)))

    def test_mcuboot_all_eighty_locked_sources_are_required(self):
        lock = json.loads((ROOT / 'dependencies/mcuboot.lock.json').read_text())
        self.assertEqual(len(lock['files']), 80)
        expected = {'ext/mcuboot/' + name for name in lock['files']}
        expected.update({'dependencies/mcuboot.lock.json',
                         'tools/maintenance/mcuboot_lock.py',
                         'components/protocols/mcuboot/CMakeLists.txt',
                         'components/protocols/mcuboot/include/nexus/components/mcuboot_control.h',
                         'components/protocols/mcuboot/port/flash_map_backend/flash_map_backend.h',
                         'components/protocols/mcuboot/private/upstream_profile.cmake'})
        self.assertLessEqual(expected, set(self.packager.required_files(ROOT, self.packager.source_profile(
            components=('network', 'mcuboot')))))

    def test_all_311_selected_lwip_sources_remain_required(self):
        lock = json.loads((ROOT / 'dependencies/lwip.lock.json').read_text())
        self.assertEqual(len(lock['files']), 311)
        expected = {'ext/lwip/' + name for name in lock['files']}
        self.assertFalse(expected - set(self.packager.required_files(ROOT, self.packager.source_profile(
            components=('network', 'mcuboot')))),
                         'Every locked lwIP source/notice must be mandatory')

    def test_dependency_export_selects_exact_locked_mcuboot_closure(self):
        lock = json.loads((ROOT / 'dependencies/mcuboot.lock.json').read_text())
        entries = {name: ('100644', 'blob', 'a' * 40) for name in lock['files']}
        entries['unreviewed.c'] = ('100644', 'blob', 'b' * 40)
        with patch.object(self.packager, 'tree_entries', return_value=entries):
            exported = self.packager.dependency_entries(ROOT, 'ext/mcuboot')
        self.assertEqual(set(exported), set(lock['files']))

    def test_each_new_authored_fixture_is_in_required_payload(self):
        for name in NEW_NAMES:
            profile = self.profile(name)
            sources = self.consumers.service_sources(ROOT, profile)
            paths = [ROOT / 'cmake/package/verification' / profile.scenario /
                     profile.assembly, *sources.values()]
            for path in paths:
                self.assertIn(path.relative_to(ROOT).as_posix(),
                              self.packager.required_files(ROOT,
                                  self.packager.source_profile(
                                      components=('network', 'mcuboot'),
                                      host_support=True)))
                self.assertTrue(path.is_file())

    def test_loaned_preparation_keeps_storage_outside_generic_application(self):
        for name in NEW_NAMES[:1]:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                _, source, _, command, profile = self.prepared(Path(tmp), name)
                cmake = (source / 'CMakeLists.txt').read_text()
                self.assertIn('Nexus::LwIPLoanedRXStorage', cmake)
                self.assertIn('PRIVATE Nexus::LwIPLoanedRX)', cmake)
                self.assertIn('SOURCES composition.c', cmake)
                self.assertIn('set(CMAKE_C_STANDARD 11)', cmake)
                self.assertIn('NEXUS_LWIP_LOANED_RX',
                              (source / 'private_headers.c').read_text())
                self.assertTrue(profile.arm)
                self.assertTrue(any('CMAKE_TOOLCHAIN_FILE' in x for x in command))

    def test_native_control_explicitly_links_its_owned_model_target(self):
        with tempfile.TemporaryDirectory() as temporary:
            _, source, _, _, _ = self.prepared(
                Path(temporary), 'native mcuboot control')
            self.assertIn('Nexus::NativeModel',
                          (source / 'CMakeLists.txt').read_text())

    def test_native_control_rejects_missing_host_support_or_changed_model(self):
        for fault in ('host_support', 'model_header'):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as tmp:
                directory = Path(tmp)
                sdk, _, _, _, profile = self.prepared(
                    directory, 'native mcuboot control')
                if fault == 'host_support':
                    manifest = sdk / '.nexus-source-sdk.json'
                    identity = json.loads(manifest.read_text())
                    identity['profile']['host_support'] = False
                    manifest.write_text(json.dumps(identity))
                else:
                    model = sdk / 'tests/support/native/include/nexus/io/native/model.h'
                    model.write_bytes(model.read_bytes() + b'\n/* changed */\n')
                output = directory / 'second consumer'
                output.mkdir()
                with self.assertRaisesRegex(ValueError, 'Native model'):
                    self.consumers.prepare_service_consumer(
                        directory / 'prefix', output, profile,
                        lambda argv: subprocess.run(
                            argv, check=True, capture_output=True, text=True),
                        fixture=True, expected_revision='a' * 40)

    def test_native_control_resolves_the_public_nor_model(self):
        assembly = (ROOT / 'cmake/package/verification/mcuboot_control' /
                    'native.toml')
        resolved = configure.resolve_ir(assembly, root=ROOT, test_support=True)
        self.assertEqual(resolved.soc_family, 'native')
        self.assertEqual(len(resolved.controllers), 1)
        flash = resolved.controllers[0]
        self.assertEqual((flash.id, flash.kind, flash.binding, flash.controller,
                          flash.mode),
                         ('physical', 'flash', 'flash0', 'FLASH0', 'poll-byte'))

    def test_control_preparation_selects_generic_and_explicit_port(self):
        for name in NEW_NAMES[1:3]:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                _, source, _, _, _ = self.prepared(Path(tmp), name)
                cmake = (source / 'CMakeLists.txt').read_text()
                self.assertIn('nexus_add_mcuboot_control()', cmake)
                self.assertIn('target_link_libraries(installed_public PUBLIC '
                              'Nexus::MCUBootControl)', cmake)
                self.assertIn('Nexus::MCUBootPort', cmake)
                self.assertNotIn('Nexus::MCUBootBootStorage', cmake)
                self.assertIn('flash_map_backend/flash_map_backend.h',
                              (source / 'private_headers.c').read_text())

    def test_full_boot_selects_separate_storage_and_one_layout(self):
        with tempfile.TemporaryDirectory() as tmp:
            _, source, _, _, _ = self.prepared(Path(tmp), 'f407 mcuboot boot')
            cmake = (source / 'CMakeLists.txt').read_text()
            self.assertIn('nexus_add_mcuboot_boot()', cmake)
            self.assertIn('Nexus::MCUBootBootStorage', cmake)
            self.assertTrue((source / 'public_key.c').is_file())
            self.assertEqual((source / 'assembly.toml').read_text().count(
                'layout = "layout.json"'), 1)
            self.assertIn('REFERENCE_PRIMARY_ORIGIN', cmake)

    def test_robotics_profiles_export_public_consumers_without_supplier_headers(self):
        for name in ('native smart components', 'f407 smart components',
                     'f407 mcuboot swap'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                profile = self.profile(name)
                sources = self.consumers.service_sources(ROOT, profile)
                required = set(self.packager.SDK_VERIFICATION_FILES)
                for path in sources.values():
                    self.assertIn(path.relative_to(ROOT).as_posix(), required)
                _, source, _, _, _ = self.prepared(Path(tmp), name)
                cmake = (source / 'CMakeLists.txt').read_text()
                self.assertIn('add_library(installed_public STATIC', cmake)
                if profile.scenario == 'smart_components':
                    for target in ('AlgorithmsCMSISNN', 'ATECC608B',
                                   'LSM6DSOX', 'CANopen', 'LVGL'):
                        self.assertIn('Nexus::' + target, cmake)
                    self.assertNotIn('Nexus::LVGLProfile', cmake)
                    self.assertNotIn('#include "cryptoauthlib.h"',
                                     (source / 'application.c').read_text())
                else:
                    self.assertIn('nexus_add_mcuboot_swap_boot()', cmake)
                    self.assertIn('Nexus::MCUBootSwapBootStorage', cmake)
                    self.assertIn('foreach(role PRIMARY SECONDARY SCRATCH)', cmake)
                    self.assertTrue((source / 'public_key.c').is_file())
                    self.assertNotIn('Nexus::MCUBootBootStorage)', cmake)

    def test_freertos_stop_consumer_exports_the_exact_lowpower_reference(self):
        profile = self.profile('f407 freertos stop')
        package = json.loads((ROOT / 'soc/stm32/stm32f4/package.json').read_text())
        sources = self.consumers.service_sources(ROOT, profile)
        for path in sources.values():
            self.assertIn(path.relative_to(ROOT).as_posix(),
                          package['exports']['verification_files'])
        self.assertEqual(profile.assembly, 'stop.toml')
        self.assertIn('nx_stm32_freertos_stop_arm',
                      self.consumers.service_symbols(profile))

    def test_stop_admits_only_its_two_public_kernel_include_directories(self):
        profile = self.profile('f407 freertos stop')
        resolved = {'enum_abi': 'short-enums', 'soc_family': 'stm32f4',
                    'cpu': {'arch': 'cortex-m4', 'float_abi': 'hard',
                            'fpu': 'fpv4-sp-d16'}}
        flags = ['-std=c11', '-mcpu=cortex-m4', '-mthumb',
                 '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard', '-fshort-enums']
        for option, accepted in (
                ('-I' + str(ROOT / 'ext/freertos/include'), True),
                ('-I' + str(ROOT / 'ext/freertos/portable/GCC/ARM_CM4F'), True),
                ('-I' + str(ROOT / 'ext/freertos/portable/GCC/ARM_CM3'), False),
                ('-I' + str(ROOT / 'ext/mcuboot/boot/bootutil/include'), False),
                ('-include' + str(ROOT / 'ext/freertos/include/FreeRTOS.h'), False)):
            row = {'file': '/consumer/main.c', 'directory': str(ROOT),
                   'arguments': ['cc', *flags, option, '-c', '/consumer/main.c']}
            with self.subTest(option=option):
                if accepted:
                    self.consumers.check_service_compiles(
                        [row], [row], ROOT, resolved, profile)
                    with self.assertRaisesRegex(ValueError, 'private'):
                        self.consumers.check_service_compiles(
                            [row], [row], ROOT, resolved,
                            self.profile('f407 mcuboot boot'))
                else:
                    with self.assertRaisesRegex(ValueError, 'private'):
                        self.consumers.check_service_compiles(
                            [row], [row], ROOT, resolved, profile)

    def test_generic_api_rejects_selected_layout_flags_and_port_paths(self):
        cases = [('f407 loaned rx', '-DNEXUS_LWIP_LOANED_RX=1'),
                 ('native mcuboot control', '-Iext/mcuboot/boot/bootutil/include'),
                 ('native mcuboot control',
                  '-I' + str(ROOT / 'components/protocols/mcuboot/port')),
                 ('f407 mcuboot boot',
                  '-I' + str(ROOT / 'components/protocols/mcuboot/profile'))]
        for name, leaked in cases:
            with self.subTest(name=name):
                profile = self.profile(name)
                flags = ['-std=c11'] + (['-mcpu=cortex-m4', '-mthumb',
                         '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard',
                         '-fshort-enums'] if profile.arm else [])
                row = {'file': '/consumer/application.c', 'directory': str(ROOT),
                       'arguments': ['cc', *flags, leaked, '-c',
                                     '/consumer/application.c']}
                resolved = {'enum_abi': 'short-enums' if profile.arm else
                            'native-int', 'soc_family': 'stm32f4',
                            'cpu': {'arch': 'cortex-m4', 'float_abi': 'hard',
                                    'fpu': 'fpv4-sp-d16'}}
                with self.assertRaisesRegex(ValueError, 'private'):
                    self.consumers.check_service_compiles(
                        [row], [row], ROOT, resolved, profile)

    def test_profile_scope_never_claims_hardware_or_authentication_execution(self):
        for name in NEW_NAMES:
            profile = self.profile(name)
            scope = self.consumers.service_scope(profile).lower()
            self.assertIn('no physical', scope)
            self.assertIn('no measured stack', scope)
        self.assertIn('no image authentication', self.consumers.service_scope(
            self.profile('f407 mcuboot boot')).lower())


if __name__ == '__main__':
    unittest.main()
