"""Products select the admitted MQTT helper without vendor path knowledge."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


COMPOSITION = r'''
#include "nexus/mqtt/core_mqtt_storage.h"
#include <stdlib.h>
static nx_result_t write_bytes(void* c, const uint8_t* p, size_t n,
                               size_t* done, nx_time_us_t d) {
    (void)c; (void)p; (void)n; (void)d; *done = 0U; return NX_ERROR_BUSY;
}
static nx_result_t read_bytes(void* c, uint8_t* p, size_t n,
                              size_t* done, nx_time_us_t d) {
    (void)c; (void)p; (void)n; (void)d; *done = 0U; return NX_ERROR_BUSY;
}
static void event(void* c, const nx_mqtt_event_t* e) { (void)c; (void)e; }
nx_time_us_t nx_mqtt_port_now_us(void) { return 0U; }
void nx_mqtt_port_assert(const char* file, unsigned line) {
    (void)file; (void)line; abort();
}
int main(void) {
    nx_mqtt_t client = {0}; uint8_t buffer[256];
    MQTTPubAckInfo_t outgoing[2], incoming[2];
    nx_mqtt_control_t controls[2];
    const nx_byte_stream_ops_t ops = {write_bytes, read_bytes};
    const nx_mqtt_config_t config = {{&ops, NULL}, event, NULL, NULL, NULL};
    const nx_mqtt_memory_t memory = {
        buffer, sizeof(buffer), outgoing, 2U, incoming, 2U, controls, 2U};
    return nx_mqtt_init(&client, &config, &memory) == NX_SUCCESS &&
           nx_mqtt_deinit(&client) == NX_SUCCESS ? 0 : 1;
}
'''


class CoreMQTTConsumerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='MQTT product ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.environment = {key: value for key, value in os.environ.items()
                            if not key.startswith('GIT_')}
        self.environment.update(PYTHONDONTWRITEBYTECODE='1', NEXUS_BUILD_JOBS='1')
        self.cmake = shutil.which('cmake') or str(ROOT / '.venv/bin/cmake')

    def configure(self, body, *, assembly=True):
        source = self.root / 'source'
        source.mkdir()
        build = self.root / 'build'
        setup = ''
        if assembly:
            setup = (
                'set(NEXUS_BUILD_TESTS OFF CACHE BOOL "Host tests")\n'
                'set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Host workload")\n'
                f'set(NEXUS_ASSEMBLY_FILE "{ROOT}/tools/configure/assemblies/native-gpio.toml")\n'
                f'add_subdirectory("{ROOT}" nexus EXCLUDE_FROM_ALL)\n')
        (source / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(mqtt_product LANGUAGES C ASM)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            'set(CMAKE_C_STANDARD 11)\nset(CMAKE_C_STANDARD_REQUIRED ON)\n'
            'set(CMAKE_C_EXTENSIONS OFF)\n' + setup + body)
        (source / 'generic.c').write_text(
            '#include "nexus/mqtt/client.h"\n'
            '#if __has_include("core_mqtt.h") || '
            '__has_include("core_mqtt_config.h")\n'
            '#error Private vendor or profile header leaked\n#endif\n'
            'int main(void) { return 0; }\n')
        (source / 'composition.c').write_text(COMPOSITION)
        result = subprocess.run([self.cmake, '-S', str(source), '-B', str(build),
                                 '-G', 'Ninja'], env=self.environment,
            capture_output=True, text=True, check=False, timeout=60)
        return source, build, result

    def test_real_product_consumes_zero_arg_helper_and_keeps_vendor_in_composition(self):
        source, build, configured = self.configure(
            'nexus_add_coremqtt()\nnexus_add_coremqtt()\n'
            'add_executable(generic generic.c)\n'
            'target_link_libraries(generic PRIVATE Nexus::CoreMQTT)\n'
            'add_executable(composition composition.c)\n'
            'target_link_libraries(composition PRIVATE Nexus::CoreMQTTStorage)\n')
        self.assertEqual(configured.returncode, 0,
                         configured.stdout + configured.stderr)
        built = subprocess.run([self.cmake, '--build', str(build), '--parallel',
                                '1', '--target', 'generic', 'composition'],
            env=self.environment, capture_output=True, text=True, check=False,
            timeout=120)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        rows = json.loads((build / 'compile_commands.json').read_text())
        generic = next(row for row in rows
                       if Path(row['file']) == source / 'generic.c')
        self.assertNotIn('/ext/coremqtt/', generic['command'])
        self.assertNotIn('/components/mqtt/port', generic['command'])
        for name in ('generic', 'composition'):
            result = subprocess.run([str(build / name)], capture_output=True,
                text=True, check=False, timeout=5)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_public_arguments_cannot_replace_vendor_or_include_source(self):
        _, _, result = self.configure('nexus_add_coremqtt("unreviewed")\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('accepts no', result.stdout + result.stderr)

    def test_missing_assembly_identity_rejects_before_selecting_targets(self):
        _, _, result = self.configure(
            f'include("{ROOT}/components/mqtt/CMakeLists.txt")\n'
            'nexus_add_coremqtt()\n', assembly=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('configured Nexus assembly', result.stdout + result.stderr)

    def test_forged_sdk_scope_cannot_turn_repository_bytes_into_an_sdk(self):
        _, _, result = self.configure(
            f'set_property(GLOBAL PROPERTY NEXUS_SOURCE_SDK_SOURCE "{ROOT}")\n'
            'nexus_add_coremqtt()\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('coreMQTT source rejected', result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
