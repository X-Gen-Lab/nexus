"""Execute the common firmware assembly rule using host ELF objects, without MCU claims."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(sys.platform.startswith('linux'), 'Fixture uses GNU ELF KEEP semantics')
class PlatformObjectAssemblyTests(unittest.TestCase):
    def test_makefiles_unreferenced_records_and_strong_callbacks_reach_final_image(self):
        self.check_image('Unix Makefiles')

    def test_ninja_unreferenced_records_and_strong_callbacks_reach_final_image(self):
        self.assertIsNotNone(shutil.which('ninja'), 'Ninja is required')
        self.check_image('Ninja')

    def check_image(self, generator):
        self.assertIsNotNone(shutil.which('cmake'), 'CMake is required')
        with tempfile.TemporaryDirectory(prefix='nexus object assembly ') as directory:
            source = Path(directory) / 'source'
            build = Path(directory) / 'build'
            source.mkdir()
            (source / 'startup.c').write_text(
                'extern int sdk_call(void);\n'
                'int Reset_Handler(void) { return sdk_call(); }\n')
            (source / 'sdk.c').write_text(
                '__attribute__((weak)) int completion(void) { return 3; }\n'
                'int sdk_call(void) { return completion(); }\n')
            record = ('__attribute__((used, section(".nx_device"), aligned(4)))\n'
                      'static const unsigned registration = {value};\n')
            (source / 'soc.c').write_text(
                record.replace('{value}', '1') + 'int completion(void) { return 42; }\n')
            (source / 'controller.c').write_text(record.replace('{value}', '2'))
            (source / 'board.c').write_text(record.replace('{value}', '4'))
            (source / 'registry.ld').write_text(
                'SECTIONS { .nx_device : ALIGN(4) {\n'
                '  __nx_device_start = .; KEEP(*(.nx_device)) __nx_device_end = .;\n'
                '} } INSERT AFTER .rodata;\n')
            (source / 'main.c').write_text(
                '#include <stddef.h>\n#include <stdint.h>\n'
                'extern const unsigned __nx_device_start[], __nx_device_end[];\n'
                'extern int Reset_Handler(void);\n'
                'int main(void) {\n'
                '  uintptr_t begin = (uintptr_t)__nx_device_start;\n'
                '  uintptr_t end = (uintptr_t)__nx_device_end; unsigned mask = 0;\n'
                '  if (end < begin || end - begin != 3 * sizeof(unsigned) ||\n'
                '      begin % _Alignof(unsigned)) return 1;\n'
                '  for (size_t i = 0; i < 3; ++i)\n'
                '    mask |= *(const unsigned *)(begin + i * sizeof(unsigned));\n'
                '  return mask == 7 && Reset_Handler() == 42 ? 0 : 1;\n'
                '}\n')
            helper = ROOT / 'cmake/modules/NexusComponentObjects.cmake'
            (source / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.21)\n'
                'project(assembly_contract C)\n'
                'add_compile_options(-ffunction-sections -fdata-sections)\n'
                'add_library(sdk STATIC sdk.c)\n'
                'foreach(component soc controller board)\n'
                '  add_library(${component} OBJECT ${component}.c)\n'
                'endforeach()\n'
                'add_library(platform OBJECT startup.c)\n'
                'target_link_libraries(platform PRIVATE sdk)\n'
                f'include("{helper.as_posix()}")\n'
                'nexus_forward_component_objects(platform soc controller board)\n'
                'add_executable(firmware main.c)\n'
                'target_link_libraries(firmware PRIVATE platform)\n'
                # The former nested SOURCES design must demonstrably lose the
                # unreferenced records and strong callback in the same fixture.
                'add_library(broken_platform OBJECT startup.c)\n'
                'target_sources(broken_platform PRIVATE $<TARGET_OBJECTS:soc> '
                '$<TARGET_OBJECTS:controller> $<TARGET_OBJECTS:board>)\n'
                'target_link_libraries(broken_platform PRIVATE sdk)\n'
                'add_executable(broken_firmware main.c)\n'
                'target_link_libraries(broken_firmware PRIVATE broken_platform)\n'
                'foreach(image firmware broken_firmware)\n'
                '  target_link_options(${image} PRIVATE -Wl,--gc-sections '
                '"-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/registry.ld")\n'
                'endforeach()\n')
            for arguments in (
                ['cmake', '-S', str(source), '-B', str(build), '-G', generator],
                ['cmake', '--build', str(build), '--parallel', '2', '--target', 'firmware'],
                [str(build / 'firmware')],
                ['cmake', '--build', str(build), '--parallel', '2', '--target', 'broken_firmware'],
            ):
                result = subprocess.run(arguments, capture_output=True, text=True, timeout=60)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            broken = subprocess.run([str(build / 'broken_firmware')], timeout=10)
            self.assertEqual(broken.returncode, 1, 'Regression fixture must detect missing objects')


if __name__ == '__main__':
    unittest.main()
