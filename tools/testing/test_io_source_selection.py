"""Inspect the real I/O target without compiling an unrelated device domain."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class IOSourceSelectionTests(unittest.TestCase):
    def sources(self, kinds=(), *, family="stm32f4", components=(),
                external_component=False, required_target=None):
        with tempfile.TemporaryDirectory(prefix="nexus io sources ") as folder:
            root = Path(folder)
            project = f'''cmake_minimum_required(VERSION 3.31)
project(io_source_contract LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NEXUS_SOC_FAMILY "{family}")
set(NEXUS_TEST_SUPPORT {"ON" if family == "native" else "OFF"})
set(NEXUS_SELECTED_KINDS "{';'.join(kinds)}")
set(NEXUS_SELECTED_COMPONENTS "{';'.join(components)}")
add_library(nexus_core INTERFACE)
add_library(nexus_arch INTERFACE)
add_library(Nexus::Arch ALIAS nexus_arch)
add_library(nexus_build_options INTERFACE)
add_library(nexus_owned_warnings INTERFACE)
target_include_directories(nexus_core INTERFACE "{ROOT / 'core/include'}")
target_include_directories(nexus_arch INTERFACE "{ROOT / 'arch/include'}")
target_compile_options(nexus_build_options INTERFACE -ffunction-sections)
add_subdirectory("{ROOT / 'io'}" io)
'''
            if external_component:
                project += f'''
function(nexus_apply_target_policy target)
endfunction()
add_subdirectory("{ROOT / 'components/tmp117'}" tmp117)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE Nexus::TMP117I2C)
target_link_options(consumer PRIVATE -Wl,--gc-sections)
'''
                (root / "main.c").write_text('''
#include "nexus/components/tmp117_i2c.h"
static nx_time_us_t now(void* context) { (void)context; return 1U; }
static nx_result_t exchange(void* context, nx_i2c_message_t* messages,
                           size_t count, nx_time_us_t deadline,
                           size_t* transferred) {
    if (count != 2U || deadline != 10U || messages[0].length != 1U ||
        messages[1].length != 2U || !messages[1].read)
        return NX_ERROR_INVALID;
    ++*(unsigned*)context;
    messages[1].data[0] = 0x12U;
    messages[1].data[1] = 0x34U;
    *transferred = 3U;
    return NX_SUCCESS;
}
int main(void) {
    unsigned calls = 0U;
    const nx_i2c_endpoint_ops_t ops = { .transaction = exchange };
    const nx_i2c_endpoint_t endpoint = { &ops, &calls };
    nx_clock_t clock = { .read = now, .context = &calls };
    nx_tmp117_transport_port_t port = nx_tmp117_i2c_transport(&endpoint, clock);
    const uint8_t tx = 0U;
    uint8_t rx[2] = {0};
    nx_result_t result = port.exchange(port.context, &tx, 1U, rx, 2U, 10U);
    return result != NX_SUCCESS || calls != 1U || rx[0] != 0x12U ||
           rx[1] != 0x34U;
}
''')
            elif required_target is not None:
                (root / "main.c").write_text("int probe(void) { return 0; }\n")
                project += f'''
add_library(consumer STATIC main.c)
target_link_libraries(consumer PRIVATE {required_target})
'''
            (root / "CMakeLists.txt").write_text(project)
            environment = dict(os.environ)
            environment["PATH"] = (str(ROOT / ".venv/bin") + os.pathsep +
                                   environment.get("PATH", ""))
            cmake = shutil.which("cmake", path=environment["PATH"])
            self.assertIsNotNone(cmake, "Prepared CMake is required")
            command = [cmake, "-S", str(root), "-B", str(root / "build"),
                       "-G", "Ninja"]
            result = subprocess.run(command, env=environment,
                                    capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0,
                             result.stdout + result.stderr)
            if external_component:
                result = subprocess.run(
                    [cmake, "--build", str(root / "build"), "--target",
                     "consumer", "--parallel", "1"], env=environment,
                    capture_output=True, text=True, timeout=60)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)
                result = subprocess.run([str(root / "build/consumer")],
                                        capture_output=True, text=True,
                                        timeout=10)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)
            ninja = shutil.which("ninja", path=environment["PATH"])
            self.assertIsNotNone(ninja, "Prepared Ninja is required")
            target = ("consumer" if external_component or required_target
                      else "nexus_io")
            result = subprocess.run(
                [ninja, "-C", str(root / "build"), "-t", "commands", target],
                env=environment, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            sources = set()
            for line in result.stdout.splitlines():
                arguments = shlex.split(line)
                if "-c" not in arguments:
                    continue
                source = Path(arguments[arguments.index("-c") + 1])
                if source.is_relative_to(ROOT / "io"):
                    sources.add(source.relative_to(ROOT / "io").as_posix())
            return sources

    def test_empty_platform_has_no_device_api_or_transport_mechanism(self):
        self.assertEqual(self.sources(), {
            "src/mechanisms/diagnostic.c", "src/mechanisms/observation.c"})

    def test_gpio_does_not_compile_communication_adc_or_streams(self):
        self.assertEqual(self.sources(("gpio",)), {
            "src/api/gpio.c", "src/api/pin_state.c",
            "src/mechanisms/diagnostic.c", "src/mechanisms/observation.c"})

    def test_uart_retains_stream_word_and_rs485_gpio_dependencies(self):
        sources = self.sources(("uart",))
        self.assertTrue({"src/api/uart.c", "src/api/uart_word.c",
                         "src/mechanisms/stream_profile.c",
                         "src/mechanisms/dma.c"} <= sources)
        self.assertEqual({name for name in sources if name.startswith("src/api/")},
                         {"src/api/uart.c", "src/api/uart_word.c",
                          "src/api/gpio.c", "src/api/pin_state.c"})
        self.assertNotIn("src/mechanisms/spi_dma_sequence.c", sources)
        self.assertNotIn("src/mechanisms/adc_stream_profile.c", sources)
        self.assertNotIn("src/mechanisms/adc_format.c", sources)

    def test_explicit_target_keeps_its_public_io_dependency(self):
        sources = self.sources(required_target="Nexus::IOFlash")
        self.assertIn("src/api/flash.c", sources)
        self.assertIn("src/mechanisms/flash_region.c", sources)
        self.assertNotIn("src/api/usb_device.c", sources)

    def test_native_model_retains_the_complete_public_contract(self):
        sources = self.sources(family="native")
        self.assertIn("src/api/usb_device.c", sources)
        self.assertIn("src/api/ethernet_frames.c", sources)
        self.assertIn("src/mechanisms/frame_stream.c", sources)

    def test_explicit_component_accepts_a_caller_owned_provider(self):
        self.sources(("gpio",), external_component=True)


if __name__ == "__main__":
    unittest.main()
