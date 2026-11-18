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
    def test_uart_wire_public_surface_excludes_provider_register_settings(self):
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "Prepared C compiler is required")
        with tempfile.TemporaryDirectory(prefix="nexus wire boundary ") as folder:
            root = Path(folder)
            sources = {
                "public": ('#include "nexus/io/uart_wire.h"\n'
                           'nx_uart_wire_mode_t mode = NX_UART_WIRE_LIN;\n'),
                "private": ('#include "drivers/uart/stm32f4_uart_wire_config.h"\n'
                            'const nx_stm32_uart_wire_config_t wire = {\n'
                            ' .mode = NX_UART_WIRE_SMARTCARD,\n'
                            ' .prescaler = 21U, .guard_time = 16U };\n'),
                "leaked": ('#include "nexus/io/uart_wire.h"\n'
                           'nx_uart_wire_config_t wire;\n'),
            }
            for name, source in sources.items():
                path = root / (name + ".c")
                path.write_text(source)
                command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                           "-pedantic", "-I" + str(ROOT / "io/include"),
                           "-I" + str(ROOT / "core/include"),
                           "-I" + str(ROOT / "soc/stm32/stm32f4"),
                           "-c", str(path), "-o", str(root / (name + ".o"))]
                result = subprocess.run(command, capture_output=True,
                                        text=True, timeout=30)
                with self.subTest(name=name):
                    if name == "leaked":
                        self.assertNotEqual(result.returncode, 0,
                                            "Public wire API leaks raw settings")
                    else:
                        self.assertEqual(result.returncode, 0, result.stderr)

    def sources(self, kinds=(), *, family="stm32f4", components=(),
                external_component=False, required_target=None, broker=False,
                network=False, modes=("irq-byte-event",), build=False):
        with tempfile.TemporaryDirectory(prefix="nexus io sources ") as folder:
            root = Path(folder)
            controllers = ";".join(f"{kind}/fixture/{mode}"
                                   for kind in kinds for mode in modes)
            project = f'''cmake_minimum_required(VERSION 3.31)
project(io_source_contract LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NEXUS_SOURCE_DIR "{ROOT}")
set(NEXUS_SOC_FAMILY "{family}")
set(NEXUS_TEST_SUPPORT {"ON" if family == "native" else "OFF"})
set(NEXUS_SELECTED_KINDS "{';'.join(kinds)}")
set(NEXUS_SELECTED_COMPONENTS "{';'.join(components)}")
set(NEXUS_SELECTED_CONTROLLERS "{controllers}")
add_library(nexus_core INTERFACE)
add_library(Nexus::Core ALIAS nexus_core)
add_library(nexus_arch INTERFACE)
add_library(Nexus::Arch ALIAS nexus_arch)
add_library(nexus_build_options INTERFACE)
add_library(nexus_owned_warnings INTERFACE)
target_include_directories(nexus_core INTERFACE "{ROOT / 'core/include'}")
target_include_directories(nexus_arch INTERFACE "{ROOT / 'arch/include'}")
target_compile_options(nexus_build_options INTERFACE -ffunction-sections)
add_subdirectory("{ROOT / 'io'}" io)
'''
            if broker:
                project += f'''
add_library(nexus_os_wait INTERFACE)
add_subdirectory("{ROOT / 'components/runtime/bus_owner'}" owner EXCLUDE_FROM_ALL)
add_subdirectory("{ROOT / 'components/runtime/user_io_broker'}" broker EXCLUDE_FROM_ALL)
'''
            if network:
                project += f'''
add_library(nexus_abi_options INTERFACE)
function(nexus_apply_target_policy target)
endfunction()
set(nexus_lwip_source "{ROOT / 'ext/lwip'}")
include("{ROOT / 'components/protocols/network/Profiles.cmake'}")
nexus_lwip_profile("" LwIP LwIPTCP FALSE)
'''
            if external_component:
                project += f'''
function(nexus_apply_target_policy target)
endfunction()
add_subdirectory("{ROOT / 'components/devices/tmp117'}" tmp117)
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
    messages[1].buffer.rx[0] = 0x12U;
    messages[1].buffer.rx[1] = 0x34U;
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
            command_output = result.stdout
            if build:
                result = subprocess.run(
                    [cmake, "--build", str(root / "build"), "--target", target,
                     "--parallel", "2"], env=environment,
                    capture_output=True, text=True, timeout=60)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)
            sources = set()
            for line in command_output.splitlines():
                arguments = shlex.split(line)
                if "-c" not in arguments:
                    continue
                source = Path(arguments[arguments.index("-c") + 1])
                if source.is_relative_to(ROOT / "io"):
                    sources.add(source.relative_to(ROOT / "io").as_posix())
                    if build:
                        self.assertIn("-o", arguments)
                        self.assertTrue((root / "build" / arguments[
                            arguments.index("-o") + 1]).is_file(), source)
            return sources

    def test_empty_platform_has_no_device_api_or_transport_mechanism(self):
        self.assertEqual(self.sources(), {
            "src/mechanisms/diagnostic.c", "src/mechanisms/observation.c"})

    def test_gpio_does_not_compile_communication_adc_or_streams(self):
        self.assertEqual(self.sources(("gpio",)), {
            "src/api/gpio.c", "src/api/pin_state.c",
            "src/mechanisms/diagnostic.c", "src/mechanisms/observation.c"})

    def test_irq_uart_builds_only_byte_api_format_and_wake(self):
        self.assertEqual(self.sources(("uart",), build=True), {
            "src/api/uart.c", "src/api/uart_format.c",
            "src/mechanisms/wake.c", "src/mechanisms/diagnostic.c",
            "src/mechanisms/observation.c"})

    def test_word_uart_builds_its_own_api_without_stream_or_rs485(self):
        sources = self.sources(("uart",), modes=("irq-words",), build=True)
        self.assertIn("src/api/uart_word.c", sources)
        self.assertIn("src/api/uart_format.c", sources)
        self.assertNotIn("src/mechanisms/stream.c", sources)
        self.assertNotIn("src/mechanisms/uart_rs485.c", sources)

    def test_block_uart_keeps_stream_without_rs485_or_word(self):
        sources = self.sources(("uart",), modes=("irq-blocks",), build=True)
        self.assertIn("src/mechanisms/stream.c", sources)
        self.assertIn("src/mechanisms/stream_profile.c", sources)
        self.assertNotIn("src/api/uart_word.c", sources)
        self.assertNotIn("src/mechanisms/uart_rs485.c", sources)

    def test_explicit_rs485_target_needs_no_selected_uart(self):
        sources = self.sources(("gpio",), required_target="Nexus::IOUARTRS485",
                               build=True)
        self.assertIn("src/mechanisms/uart_rs485.c", sources)
        self.assertIn("src/api/gpio.c", sources)
        self.assertIn("src/api/uart.c", sources)
        self.assertNotIn("src/api/uart_word.c", sources)
        self.assertNotIn("src/mechanisms/stream.c", sources)

    def test_spi_builds_without_unrelated_stream_storage(self):
        sources = self.sources(("spi",), modes=("short-poll",), build=True)
        self.assertIn("src/api/spi.c", sources)
        self.assertNotIn("src/mechanisms/stream.c", sources)
        self.assertNotIn("src/mechanisms/frame_stream.c", sources)
        self.assertNotIn("src/mechanisms/dma.c", sources)

    def test_scalar_adc_builds_without_stream_storage(self):
        sources = self.sources(("adc",), modes=("low-rate-scan",), build=True)
        self.assertIn("src/api/adc.c", sources)
        self.assertIn("src/mechanisms/adc_format.c", sources)
        self.assertNotIn("src/mechanisms/stream.c", sources)
        self.assertNotIn("src/mechanisms/adc_stream_profile.c", sources)

    def test_adc_blocks_keep_stream_profile_storage(self):
        sources = self.sources(("adc",), modes=("trigger-dma",), build=True)
        self.assertIn("src/mechanisms/stream.c", sources)
        self.assertIn("src/mechanisms/adc_stream_profile.c", sources)
        self.assertNotIn("src/mechanisms/stream_profile.c", sources)

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
        self.assertEqual(self.sources(("gpio",), external_component=True), {
            "src/api/i2c.c", "src/mechanisms/dma.c",
            "src/mechanisms/i2c_bus_clear.c", "src/mechanisms/wake.c"})

    def test_scalar_broker_does_not_select_uart_or_aggregate_io(self):
        self.assertEqual(self.sources(("gpio", "uart"), broker=True,
                                     required_target="Nexus::UserIoBroker"), {
            "src/api/gpio.c", "src/api/pin_state.c"})

    def test_async_broker_explicitly_selects_existing_uart_owner(self):
        sources = self.sources(("gpio",), broker=True,
                               required_target="Nexus::UserIoAsync")
        self.assertIn("src/api/uart.c", sources)
        self.assertNotIn("src/mechanisms/stream.c", sources)
        self.assertNotIn("src/api/i2c.c", sources)

    def test_tcp_keeps_ethernet_without_selected_uart(self):
        sources = self.sources(("uart",), network=True,
                               required_target="Nexus::LwIPTCP")
        self.assertIn("src/api/ethernet_frames.c", sources)
        self.assertNotIn("src/api/uart.c", sources)


if __name__ == "__main__":
    unittest.main()
