"""Optional public targets carry I/O dependencies without device instances."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]

ARCH_SOURCE = r'''
#include "nexus/arch/arch.h"
static uint32_t mask;
nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t saved = {mask};
    mask = 1U;
    return saved;
}
void nx_arch_irq_restore(nx_arch_irq_state_t saved) { mask = saved.value; }
bool nx_arch_in_isr(void) { return false; }
bool nx_arch_is_privileged(void) { return true; }
bool nx_arch_cycle_snapshot(uint32_t* cycles) { (void)cycles; return false; }
'''

USB_SOURCE = r'''
#include "nexus/components/usb_descriptors.h"
static nx_result_t packet(void* context, nx_usb_request_t* request) {
    (void)request;
    ++*(unsigned*)context;
    return NX_ERROR_UNSUPPORTED;
}
static nx_result_t stall(void* context, bool enabled) {
    (void)enabled;
    ++*(unsigned*)context;
    return NX_ERROR_UNSUPPORTED;
}
static nx_result_t poll(void* context) {
    ++*(unsigned*)context;
    return NX_ERROR_UNSUPPORTED;
}
static nx_result_t state(void* context, nx_usb_device_state_t* value) {
    (void)value;
    return poll(context);
}
static nx_result_t reject(void* context, uint64_t epoch) {
    (void)epoch;
    return poll(context);
}
int main(void) {
    unsigned hardware_calls = 0U;
    const nx_usb_endpoint_ops_t packet_ops = {packet, packet, stall};
    const nx_usb_device_ops_t device_ops = {.service = poll, .state = state};
    const nx_usb_setup_decision_ops_t decision_ops = {reject};
    const nx_usb_device_port_t device = {&device_ops, &hardware_calls};
    const nx_usb_endpoint_port_t ports[] = {
        {&packet_ops, &hardware_calls}, {&packet_ops, &hardware_calls}};
    const nx_usb_endpoint_profile_t endpoints[] = {
        {&ports[0], 64U, 0U, true, NX_USB_PROFILE_CONTROL},
        {&ports[1], 64U, 0U, false, NX_USB_PROFILE_CONTROL}};
    const nx_usb_device_profile_t profile = {
        .port = &device, .endpoints = endpoints, .endpoint_count = 2U,
        .phy_clock_hz = 48000000U, .fifo_words = 320U, .rx_fifo_words = 96U,
        .endpoint_numbers = 4U, .role = NX_USB_PROFILE_DEVICE,
        .speed = NX_USB_PROFILE_FULL_SPEED, .phy = NX_USB_PROFILE_INTERNAL_FS,
        .engine = NX_USB_PROFILE_PIO,
        .setup_decision = {&decision_ops, &hardware_calls}};
    const uint8_t bytes[18] = {18U, 1U, 0U, 2U, 0U, 0U, 0U, 64U,
                              0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U};
    const nx_usb_descriptor_view_t descriptor = {bytes, sizeof(bytes)};
    return nx_usb_descriptor_validate(&profile, &descriptor) != NX_SUCCESS ||
           hardware_calls != 0U;
}
'''

PROBE_SOURCE = r'''
#include "nexus/io/cycle_probe.h"
#include "nexus/io/spi_dma_transaction.h"
#include "nexus/io/uart_word.h"
static void service(void* context) { ++*(unsigned*)context; }
int main(void) {
    unsigned uart_calls = 0U, spi_calls = 0U;
    const nx_uart_word_ops_t uart_ops = {.service = service};
    const nx_spi_dma_transaction_port_ops_t spi_ops = {.service = service};
    const nx_uart_word_port_t uart = {&uart_ops, &uart_calls};
    const nx_spi_dma_transaction_port_t spi = {&spi_ops, &spi_calls};
    nx_io_cycle_probe_t probe = {0};
    nx_io_cycle_observation_t out = {0};
    if (nx_uart_word_port_service_observed(&uart, &probe) != NX_SUCCESS ||
        nx_spi_dma_transaction_service_observed(&spi, &probe) != NX_SUCCESS ||
        nx_io_cycle_probe_snapshot(&probe, &out) != NX_SUCCESS)
        return 1;
    return uart_calls != 1U || spi_calls != 1U || out.attempts != 2U ||
           out.unavailable_pairs != 2U || out.elapsed_known ||
           out.last_available || out.raw_pairs != 0U;
}
'''


class OptionalIODomainTests(unittest.TestCase):
    def prepare(self, root, kind):
        """Use production targets with GPIO selected and narrow Arch leaves."""
        component = (
            f'add_subdirectory("{ROOT / "components/usb_control"}" usb)'
            if kind == "usb" else "")
        target = ("Nexus::USBDescriptors" if kind == "usb"
                  else "Nexus::IOCycleProbe")
        project = f'''cmake_minimum_required(VERSION 3.31)
project(optional_io_consumer LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NEXUS_SOC_FAMILY stm32f407)
set(NEXUS_SELECTED_KINDS gpio)
add_library(nexus_abi_options INTERFACE)
add_library(nexus_build_options INTERFACE)
add_library(nexus_owned_warnings INTERFACE)
target_compile_options(nexus_owned_warnings INTERFACE
    -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
    -Wstrict-prototypes -Wmissing-prototypes)
add_library(nexus_arch STATIC arch.c)
target_include_directories(nexus_arch PUBLIC "{ROOT / 'arch/include'}")
target_link_libraries(nexus_arch PRIVATE nexus_owned_warnings)
add_library(Nexus::Arch ALIAS nexus_arch)
add_subdirectory("{ROOT / 'core'}" core)
add_subdirectory("{ROOT / 'io'}" io)
{component}
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE {target} nexus_owned_warnings)
'''
        root.mkdir(parents=True, exist_ok=True)
        (root / "CMakeLists.txt").write_text(project)
        (root / "arch.c").write_text(ARCH_SOURCE)
        (root / "main.c").write_text(
            USB_SOURCE if kind == "usb" else PROBE_SOURCE)

    def build_and_run(self, root):
        environment = dict(os.environ)
        environment["PATH"] = (
            str(ROOT / ".venv/bin") + os.pathsep + environment.get("PATH", ""))
        cmake = shutil.which("cmake", path=environment["PATH"])
        self.assertIsNotNone(cmake, "Prepared CMake is required")
        commands = [
            ("configure", [cmake, "-S", str(root), "-B", str(root / "build"),
                           "-G", "Ninja"]),
            ("build", [cmake, "--build", str(root / "build"), "--target",
                       "consumer", "--parallel", "2"]),
            ("run", [str(root / "build/consumer")]),
        ]
        for stage, command in commands:
            result = subprocess.run(command, env=environment,
                                    capture_output=True, text=True, timeout=60)
            output = result.stdout + result.stderr
            (root / f"{stage}.log").write_text(output)
            self.assertEqual(result.returncode, 0, output)

    def consumer(self, kind):
        with tempfile.TemporaryDirectory(prefix="nexus optional IO ") as folder:
            root = Path(folder)
            self.prepare(root, kind)
            self.build_and_run(root)

    def test_usb_descriptors_validate_without_a_selected_usb_controller(self):
        self.consumer("usb")

    def test_cycle_probe_dispatches_caller_ports_with_gpio_only(self):
        self.consumer("probe")


if __name__ == "__main__":
    unittest.main()
