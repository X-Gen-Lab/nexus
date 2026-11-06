"""Actual C11 reference images retain private transports behind public headers."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest

from tools.dev.process import run
from tools.measurement.elf import Elf32

ROOT = Path(__file__).resolve().parents[2]


class ReferencePortImageTests(unittest.TestCase):
    def image(self, fixture, required, private_headers, target="reference", assembly=None):
        environment = dict(os.environ)
        environment.pop('GIT_PAGER', None)
        environment['PATH'] = str(ROOT / '.venv/bin') + os.pathsep + environment.get('PATH', '')
        environment['NEXUS_BUILD_JOBS'] = environment['CMAKE_BUILD_PARALLEL_LEVEL'] = '1'
        tools = {name: shutil.which(name, path=environment['PATH']) for name in
                 ('cmake', 'ninja', 'arm-none-eabi-gcc', 'arm-none-eabi-nm')}
        self.assertTrue(all(tools.values()), 'Prepared locked CMake/Ninja/ARM tools required')
        retained = environment.get('NEXUS_REFERENCE_PORT_OUTPUT')
        if retained:
            Path(retained).mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=fixture + '-', dir=retained) as value:
            folder = Path(value)
            if retained:
                # Keep an independent fresh run, including failed tool receipts.
                folder = Path(tempfile.mkdtemp(prefix=fixture + '-', dir=retained))
            observations = []

            def execute(label, command):
                result = run(command, cwd=ROOT, env=environment, timeout=180,
                             stage=label, log=folder / (label + '.log'))
                observations.append({'argv': command, 'exit_code': result.returncode})
                (folder / 'commands.json').write_text(json.dumps(observations, indent=2) + '\n')
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result.stdout

            lock = json.loads((ROOT / 'dependencies/toolchains.lock.json').read_text())
            version = execute('compiler-version', [tools['arm-none-eabi-gcc'], '--version'])
            self.assertIn(lock['arm_gnu']['version'].lower(), version.lower())
            compiler_target = execute('compiler-target',
                                      [tools['arm-none-eabi-gcc'], '-dumpmachine'])
            self.assertEqual(compiler_target.strip(), lock['arm_gnu']['target'])
            build = folder / 'build'
            configure = [tools['cmake'], '-S', str(ROOT / 'tests/contracts' / fixture),
                    '-B', str(build), '-G', 'Ninja', '-DNEXUS_SOURCE_ROOT=' + str(ROOT),
                    '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/toolchains/arm-gcc.cmake')]
            if assembly:
                configure.append('-DNEXUS_ASSEMBLY_FILE=' + str(ROOT / assembly))
            execute('configure', configure)
            execute('build', [tools['cmake'], '--build', str(build), '--target',
                              target, '--parallel', '1'])
            images = list(build.rglob(target + '.elf'))
            self.assertEqual(len(images), 1, 'One actual selected firmware ELF required')
            elf_path = images[0]
            elf = Elf32(elf_path.read_bytes())
            self.assertFalse(set(required) - elf.symbols.keys())
            names = {item['name'] for item in elf.defined_symbols}
            self.assertFalse(names & {'malloc', 'calloc', 'realloc', 'free', '_sbrk',
                    '_sbrk_r', '_malloc_r', '_free_r', 'pvPortMalloc', 'tx_byte_allocate',
                    'vTaskStartScheduler', 'tx_kernel_enter', 'pthread_create'})
            self.assertFalse(any(name.startswith(('_Z', '__cxa', '_Unwind', 'testing::'))
                                 for name in names))
            undefined = execute('undefined', [tools['arm-none-eabi-nm'], '-u', str(elf_path)])
            self.assertFalse(undefined.strip(), undefined)
            commands = execute('link-commands', [tools['ninja'], '-C', str(build),
                                                '-t', 'commands', target])
            link = commands.splitlines()[-1]
            self.assertGreater(link.rfind('--no-gc-sections'), link.rfind('--gc-sections'))
            records = json.loads((build / 'compile_commands.json').read_text())
            deps = execute('actual-deps', [tools['ninja'], '-C', str(build), '-t', 'deps'])

            def headers(entry):
                marker = entry['output'] + ': #deps '
                self.assertIn(marker, deps)
                block = deps.split(marker, 1)[1].split('\n\n', 1)[0]
                return {Path(line.strip()).resolve() for line in block.splitlines()[1:]
                        if line.startswith('    ')}

            main = next(row for row in records if Path(row['file']).name == 'main.c')
            self.assertNotIn('/vendors/', main['command'])
            self.assertNotIn('/private/', main['command'])
            public = headers(main)
            self.assertTrue(any('/components/' in str(path) and '/include/nexus/' in str(path)
                                for path in public))
            self.assertFalse(any('/vendors/' in str(path) or '/private/' in str(path)
                                 for path in public))
            composition = next(row for row in records if Path(row['file']).name == 'composition.c')
            private = headers(composition)
            self.assertLessEqual(set(private_headers), {path.name for path in private})
            self.assertTrue(any('/vendors/' in str(path) for path in private))
            if fixture == 'h563_i3c_port':
                port = next(row for row in records
                            if Path(row['file']).name == 'stm32h563_i3c_port.c')
                self.assertIn('stm32h5xx_ll_i3c.h', {path.name for path in headers(port)})
            (folder / 'result.json').write_text(json.dumps({
                'scope': 'Real ARM compile/link and actual header closure; no physical execution',
                'elf_sha256': hashlib.sha256(elf_path.read_bytes()).hexdigest(),
                'required_symbols': required, 'main_headers': sorted(map(str, public)),
                'composition_headers': sorted(map(str, private)), 'link': link}, indent=2) + '\n')

    def test_h563_i3c_transport_and_public_controller_image(self):
        self.image('h563_i3c_port', [
            'nx_h563_i3c_port_bind', 'nx_h563_i3c_port_ops', 'nx_i3c_controller_construct',
            'nx_i3c_request_prepare', 'nx_i3c_submit', 'nx_i3c_service', 'nx_i3c_cancel',
            'nx_i3c_endpoint', 'nx_i3c_event_take', 'nx_i3c_event_status', 'nx_i3c_close',
            'nx_i3c_reset', 'nx_i3c_reopen', 'nx_i3c_stop'],
            ['stm32h563_i3c_port.h', 'stm32h563xx.h'])

    def test_h743_mapped_owner_and_official_transport_image(self):
        self.image('h743_w25q128', [
            'nx_h743_w25q128_prepare', 'nx_h743_w25q128_service',
            'nx_h743_w25q128_backend', 'nx_h743_w25q128_stop',
            'nx_mapped_storage_owner_init', 'nx_mapped_storage_prepare',
            'HAL_QSPI_Command_IT', 'HAL_QSPI_Receive_IT', 'HAL_QSPI_Transmit_IT',
            'HAL_QSPI_IRQHandler', 'HAL_QSPI_MemoryMapped', 'HAL_QSPI_Abort_IT',
            'HAL_MDMA_Abort_IT'], ['stm32h743_w25q128.h', 'stm32h7xx_hal.h'],
            target='h743_w25q128_consumer',
            assembly='tools/configure/assemblies/nucleo-h743-baremetal-empty.toml')


if __name__ == '__main__':
    unittest.main()
