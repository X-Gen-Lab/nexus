#!/usr/bin/env python3
"""Verify independent consumers of an existing publishable source SDK prefix."""
import argparse
import concurrent.futures
import json
import re
import shlex
import os
from pathlib import Path, PurePosixPath
import shutil
import signal
import sys
import tomllib
import threading

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.configure.metadata import load_policy  # noqa: E402
from tools.measurement.retention import COMPONENT_OWNER_SMOKES  # noqa: E402
from tools.dev import process  # noqa: E402
from tools.evidence.common import file_identity, regular_file  # noqa: E402
from tools.release.transport import (BASE_CONSUMERS, MODERN_CONSUMERS,
                                     SELECTED_CONSUMERS)  # noqa: E402




def clean_environment():
    environment = os.environ.copy()
    for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                 "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH", "LIBRARY_PATH"):
        environment.pop(name, None)
    return environment


def private_compile_inputs(entry, sdk):
    """Audit actual compiler options against canonical Nexus source boundaries.

    Kernel PUBLIC headers remain valid. Chip component APIs live below their
    declared package's include/nexus directory; neither package roots nor
    implementation directories become public because their path contains soc.
    """
    arguments = entry.get("arguments")
    if arguments is None:
        command = entry.get("command")
        if (not isinstance(command, str) or not command.strip() or
                re.search(r"[$`\r\n;|&<>]", command)):
            raise ValueError("Compiler command requires explicit shell expansion")
        arguments = shlex.split(command)
    if (not isinstance(arguments, list) or not arguments or
            any(not isinstance(item, str) for item in arguments)):
        raise ValueError("Malformed public compiler arguments")
    if re.match(r"[A-Za-z_][A-Za-z0-9_]*=", arguments[0]):
        raise ValueError("Compiler environment requires explicit expansion")
    if Path(arguments[0]).name in {"sh", "bash", "dash", "zsh", "ksh",
                                   "csh", "fish", "cmd", "cmd.exe", "env",
                                   "powershell", "powershell.exe", "pwsh"}:
        raise ValueError("Shell compiler wrappers require explicit expansion")
    if any(item.startswith("@") for item in arguments):
        raise ValueError("Response compiler options require explicit expansion")
    arguments = list(arguments[1:])
    roots = {Path(sdk).resolve(), Path(__file__).resolve().parents[2]}

    def private_path(value):
        if not value or value.startswith(("=", "$SYSROOT")):
            raise ValueError("Include path requires explicit expansion")
        path = Path(value)
        if not path.is_absolute():
            if not entry.get("directory"):
                raise ValueError("Relative include path requires compile directory")
            path = Path(entry["directory"]) / path
        path = path.resolve()
        for root in roots:
            if not path.is_relative_to(root):
                continue
            relative = path.relative_to(root)
            parts = relative.parts
            if (not parts or parts[0] == "soc" or
                    {"vendors", "private"}.intersection(parts)):
                return True
            if parts == ("components",):
                return True
            if parts[:2] == ("components", "soc"):
                if len(parts) <= 4 or parts[4] != "include":
                    return True
                package = root.joinpath(*parts[:4])
                if (not (package / "CMakeLists.txt").is_file() or
                        not (package / "include/nexus").is_dir()):
                    return True
        return False

    def operand():
        if arguments and arguments[0] == "-Xpreprocessor":
            arguments.pop(0)
        if not arguments:
            raise ValueError("Missing public compiler option operand")
        return arguments.pop(0)

    paths = ("-include-pch", "-isystem", "-iquote", "-idirafter",
             "-include", "-imacros", "-I")
    while arguments:
        argument = arguments.pop(0)
        if argument == "-Xpreprocessor":
            arguments.insert(0, operand())
            continue
        if argument.startswith("-Wp,"):
            arguments[:0] = argument[4:].split(",")
            continue
        if argument.startswith("@"):
            raise ValueError("Response compiler options require explicit expansion")
        if argument.startswith(("-iprefix", "-iwithprefix", "-isysroot",
                                "--sysroot", "-imultilib", "-isystem-after",
                                "-include-pth", "-Xclang", "-internal-isystem",
                                "-internal-externc-isystem")):
            raise ValueError("Include search prefix requires explicit expansion")
        if argument in {"-c", "-o", "-MF", "-MT", "-MQ", "-x"}:
            operand()
            continue
        if argument == "-D" or argument.startswith("-D"):
            definition = operand() if argument == "-D" else argument[2:]
            if definition.split("=", 1)[0].startswith(("STM32", "GD32")):
                return True
            continue
        for option in paths:
            if argument == option or argument.startswith(option):
                value = operand() if argument == option else argument[len(option):]
                if private_path(value):
                    return True
                break
    return False


def prepare_consumer(prefix, directory, name, run_checked, *, fixture=False,
                     arm=False, chip="stm32", expected_revision=None,
                     version=None, contracts=False):
    """One public consumer template shared by qualification and boundary tests."""
    prefix = Path(prefix)
    sdk = prefix / "share/nexus/src"
    if version is None:
        version = load_policy(sdk)["semantic_version"]
    source = directory / (name + " source")
    build = directory / (name + " build")
    source.mkdir()
    # A parent Git identity must never become the SDK source identity.
    run_checked(["git", "init", "--quiet", str(source)])
    (source / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.31)\n'
        'project(installed_source_consumer LANGUAGES C CXX ASM)\n'
        'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
        f'find_package(Nexus {version} EXACT CONFIG REQUIRED)\n'
        'find_package(Nexus CONFIG REQUIRED)\n'
        'add_executable(parent_plain parent.c)\n'
        'nexus_add_firmware(installed_c SOURCES main.c)\n'
        'nexus_add_firmware(installed_cpp SOURCES main.cpp LIBRARIES '
        'Nexus::Log Nexus::BMP280 Nexus::Storage Nexus::MappedStorage Nexus::AMPIPC Nexus::I3CController Nexus::UserIoBroker'
        + (' Nexus::STM32F407Handoff' if arm and chip == "stm32" else '') + ')\n'
        'target_compile_options(installed_cpp PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>")\n'
        'get_target_property(sdk_root Nexus::Config NEXUS_SOURCE_DIR)\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/resolved-sdk.txt" "${sdk_root}")\n')
    (source / "parent.c").write_text(
        '#ifdef NEXUS_EFFECTIVE_CONFIG\n#error SDK leaked into unrelated parent target\n#endif\n'
        'int main(void) { return 0; }\n')
    code = ('#include "nexus_bindings.h"\n'
            '#include "nexus_factory.h"\n'
            '#include "nexus_config.h"\n'
            '#ifndef NEXUS_PLATFORM_VERSION\n'
            '#error "Missing source interface version"\n'
            '#endif\n'
            '#if NEXUS_FACTORY_ABI_EPOCH != 1U\n'
            '#error "Unsupported factory interface epoch"\n'
            '#endif\n'
            '#if __has_include("stm32f4xx.h") || __has_include("gd32f4xx.h")\n'
            '#error Vendor SDK leaked into product source\n#endif\n'
            '#if __has_include("stm32f407_provider.h") || __has_include("gd32f470_provider.h")\n'
            '#error Concrete provider leaked into product source\n#endif\n'
            'int main(void) {\n'
            ' if (nx_factory_gpio(NX_GPIO_ID_COUNT) != 0) return 7;\n'
            ' nx_platform_start_result_t result = nx_platform_start();\n'
            ' if (result.primary != NX_SUCCESS) return 1;\n'
            ' return nx_platform_stop() == NX_SUCCESS ? 0 : 2;\n}\n')
    (source / "main.c").write_text(code)
    handoff = arm and chip == "stm32"
    (source / "main.cpp").write_text(
        '#include "nexus/components/log.h"\n'
        '#include "nexus/components/bmp280.h"\n'
        '#include "nexus/components/storage.h"\n'
        '#include "nexus/components/mapped_storage.h"\n'
        '#include "nexus/components/mapped_storage_owner.h"\n'
        '#include "nexus/components/amp_ipc_storage.h"\n'
        '#include "nexus/components/i3c_controller_storage.h"\n'
        '#include "nexus/components/user_io_async_storage.h"\n'
        + ('#include "nexus/components/stm32f407_handoff.h"\n' if handoff else '')
        + '#include <cstring>\n' + code.replace(
            ' nx_platform_start_result_t result',
            ' nx_log_sink_port_t sink;\n std::memset(&sink, 0, sizeof(sink));\n'
            ' if (nx_log_init(nullptr, sink, NX_LOG_INFO) != NX_ERROR_INVALID) return 3;\n'
            ' if (nx_bmp280_compensate(nullptr, 0, 0, nullptr) != NX_ERROR_INVALID) return 4;\n'
            ' if (nx_mapped_storage_stop(nullptr) != NX_ERROR_INVALID) return 5;\n'
            ' if (nx_amp_ipc_construct(nullptr, nullptr) != NX_ERROR_INVALID) return 6;\n'
            ' if (nx_i3c_controller_construct(nullptr, nullptr) != NX_ERROR_INVALID) return 9;\n'
            ' if (nx_user_io_async_construct(nullptr, nullptr, 0U, nullptr) '
            '!= NX_ERROR_INVALID) return 10;\n'
            + (' if (nx_stm32f407_handoff_initialize(0) != NX_ERROR_INVALID) return 8;\n'
               if handoff else '') + ' nx_platform_start_result_t result'))
    arguments = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Release", f"-DNexus_DIR={prefix / 'lib/cmake/Nexus'}",
                 "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF"]
    if fixture:
        arguments.append("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON")
    if contracts:
        arguments.append("-DNEXUS_BUILD_WORKLOAD=ON")
    if expected_revision:
        arguments.append("-DNEXUS_EXPECTED_SOURCE_REVISION=" + expected_revision)
    arm_assembly = "liangshan-baremetal.toml" if chip == "gd32" else "sky-baremetal.toml"
    assembly = sdk / "tools/configure/assemblies" / (arm_assembly if arm else "native.toml")
    arguments.append(f"-DNEXUS_ASSEMBLY_FILE={assembly}")
    if arm:
        arguments.append(f"-DCMAKE_TOOLCHAIN_FILE={sdk / 'cmake/toolchains/arm-gcc.cmake'}")
    return source, build, arguments


def prepare_modern_consumer(prefix, directory, family, backend, run_checked,
                            *, fixture=False, expected_revision=None):
    """Prepare exact MCU public consumers, with actual selected kernel use."""
    choices = {
        "h743": ("stm32h743_nucleo_zi2", "hsi64-fixed-cache-off", "led0", "uart3", 0),
        "u575": ("stm32u575_nucleo_zi_q", "msis4", "led0", "uart1", 7),
        "f103": ("stm32f103_reference", "hsi8-fixed", "led0", "uart1", 5),
        "gd303": ("gd32f303_reference", "irc8", "gpio0", "uart0", 5),
        "g474": ("stm32g474re_hse24_reference", "hse24-pll170", "led0", "uart1", 5),
    }
    if family not in choices or backend not in {"baremetal", "freertos"}:
        raise ValueError("Unreviewed modern consumer family/backend")
    board, clock, gpio, uart, pin = choices[family]
    prefix = Path(prefix)
    sdk = prefix / "share/nexus/src"
    version = load_policy(sdk)["semantic_version"]
    source = directory / (family + " " + backend + " source")
    build = directory / (family + " " + backend + " build")
    source.mkdir()
    run_checked(["git", "init", "--quiet", str(source)])
    libraries = ["Nexus::OSFreeRTOS"] if backend == "freertos" else []
    if family == "h743":
        libraries.append("Nexus::H743FrameDMA2D")
    kernel = " LIBRARIES " + " ".join(libraries) if libraries else ""
    (source / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.31)\n'
        'project(installed_modern_consumer LANGUAGES C ASM)\n'
        'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
        f'find_package(Nexus {version} EXACT CONFIG REQUIRED)\n'
        f'nexus_add_firmware(installed_modern SOURCES main.c{kernel})\n'
        + ('set_target_properties(installed_modern PROPERTIES C_STANDARD 11 '
           'C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)\n'
           'nexus_target_warnings(installed_modern STRICT)\n'
           if family == "h743" else '') +
        'get_target_property(sdk_root Nexus::Config NEXUS_SOURCE_DIR)\n'
        'get_target_property(config_root Nexus::Platform NEXUS_CONFIG_DIR)\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/resolved-sdk.txt" "${sdk_root}")\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/resolved-config.txt" "${config_root}")\n')
    assembly = source / "assembly.toml"
    assembly.write_text(
        f'schema = 2\nboard_package = "{sdk}/boards/{board}"\n'
        f'backend = "{backend}"\nclock = "{clock}"\n'
        '[factory]\ncapabilities = true\n'
        '[memory]\nmain_stack_bytes = 2048\nlibc_heap_bytes = 0\n'
        'static_ram_limit_bytes = 16384\nflash_load_limit_bytes = 65536\n'
        f'[gpio.led]\nbinding = "{gpio}"\nmode = "output"\n'
        f'[uart.console]\nbinding = "{uart}"\n'
        'mode = "irq-bytes"\nbaud = 115200\n'
        '[uart.console.rx]\nformat = "bytes"\ncapacity = 64\n'
        '[uart.console.irq]\npriority = 5\ncalls_os = false\n')
    code = (
        '#include "nexus_bindings.h"\n#include "nexus_factory.h"\n'
        '#if __has_include("stm32h7xx.h") || __has_include("stm32u5xx.h") || '
        '__has_include("stm32f1xx.h") || __has_include("gd32f30x.h") || '
        '__has_include("stm32g4xx.h")\n'
        '#error Vendor SDK leaked into the product\n#endif\n'
        '#if __has_include("stm32h743_provider.h") || '
        '__has_include("stm32u575_provider.h") || '
        '__has_include("stm32f103_provider.h") || '
        '__has_include("gd32f303_provider.h") || '
        '__has_include("stm32g474_provider.h") || '
        '__has_include("rtc_lptim_provider.h")\n'
        '#error Provider implementation leaked into the product\n#endif\n'
        'static nx_uart_tx_request_t request;\n'
        'static uint8_t data[2] = {0x12U, 0x34U};\n')
    if backend == "freertos":
        code += (
            '#include "nexus/os/freertos.h"\n'
            'static nx_freertos_task_t task;\n'
            '_Alignas(8) static StackType_t task_stack[128];\n'
            'static void task_entry(void* context) {\n'
            '    (void)context;\n    for (;;) {}\n}\n')
    if family == "h743":
        code += (
            '#include "nexus/components/frame_dma2d.h"\n'
            'static nx_h743_frame_dma2d_storage_t frame_storage;\n'
            'static const nx_frame_blitter_t frame_blitter = {\n'
            '    &nx_h743_frame_dma2d_ops, &frame_storage};\n'
            'static const nx_h743_frame_dma2d_config_t frame_configuration = {\n'
            '    NX_H743_FRAME_DMA2D_CONTROLLER, 4U};\n'
            'static nx_stream_slot_t frame_slot;\n'
            'static nx_stream_t frame_queue;\n'
            'static nx_frame_stream_t frame_stream;\n'
            'static nx_frame_dma_loan_t frame_source;\n'
            'static nx_frame_blit_request_t frame_request;\n'
            'static uint32_t frame_pixels[24];\n'
            'static uint16_t frame_output[48];\n')
    code += (
        'int main(void) {\n'
        '    nx_platform_start_result_t start = nx_platform_start();\n'
        '    if (start.primary != NX_SUCCESS) return (int)start.primary;\n'
        '    const nx_gpio_port_t* led = nx_factory_gpio(NX_GPIO_ID_LED);\n'
        '    const nx_uart_port_t* console = nx_factory_uart(NX_UART_ID_CONSOLE);\n'
        '    nx_request_initialize(&request.base);\n'
        f'    (void)nx_gpio_port_write(led, 1U << {pin}U, 0U);\n'
        '    (void)nx_uart_tx_prepare(&request, data, sizeof(data),\n'
        '        nx_deadline_after(nx_time_now_us(), 1000U));\n'
        '    (void)nx_uart_port_submit(console, &request);\n'
        '    (void)nx_uart_port_cancel(console, &request);\n'
        '    nx_uart_port_service(console);\n')
    if family == "h743":
        code += (
            '    /* Public link reference only: no physical DMA/timing result. */\n'
            '    const nx_frame_plane_t frame_plane = {0U, 16U, 24U, 4U};\n'
            '    nx_frame_layout_t frame_layout;\n'
            '    frame_slot.data = (uint8_t*)frame_pixels;\n'
            '    frame_slot.capacity = sizeof(frame_pixels);\n'
            '    if (nx_frame_layout_planar(&frame_plane, 1U, &frame_layout) != NX_SUCCESS ||\n'
            '        nx_stream_initialize(&frame_queue, &frame_slot, 1U) != NX_SUCCESS ||\n'
            '        nx_frame_stream_initialize(&frame_stream, &frame_queue, &frame_layout) != NX_SUCCESS ||\n'
            '        nx_h743_frame_dma2d_initialize(&frame_storage, &frame_blitter,\n'
            '                                      &frame_configuration) != NX_SUCCESS) return 9;\n'
            '    nx_stream_fill_t frame_fill;\n'
            '    if (nx_stream_reserve(&frame_queue, &frame_fill) != NX_SUCCESS ||\n'
            '        nx_frame_stream_publish(&frame_stream, &frame_fill, 0U) != NX_SUCCESS ||\n'
            '        nx_frame_stream_acquire(&frame_stream, &frame_source) != NX_SUCCESS) return 10;\n'
            '    nx_request_initialize(&frame_request.base);\n'
            '    if (nx_request_prepare(&frame_request.base, NX_DEADLINE_NEVER) != NX_SUCCESS) return 11;\n'
            '    frame_request.source = &frame_source;\n'
            '    frame_request.width_bytes = 16U;\n'
            '    frame_request.rows = 4U;\n'
            '    frame_request.format = NX_FRAME_BLIT_ARGB8888_TO_RGB565;\n'
            '    frame_request.destination = (uint8_t*)frame_output;\n'
            '    frame_request.destination_stride = 24U;\n'
            '    frame_request.destination_capacity = sizeof(frame_output);\n'
            '    if (frame_blitter.ops->submit(frame_blitter.context, &frame_request,\n'
            '                                  nx_time_now_us()) == NX_SUCCESS) {\n'
            '        (void)frame_blitter.ops->service(frame_blitter.context, nx_time_now_us());\n'
            '        (void)frame_blitter.ops->cancel(frame_blitter.context, &frame_request);\n'
            '        (void)frame_blitter.ops->service(frame_blitter.context, nx_time_now_us());\n'
            '    } else {\n'
            '        const nx_dma_drain_facts_t unsubmitted = {frame_layout.bytes, 0U, true, true, true};\n'
            '        (void)nx_frame_dma_release(&frame_stream, &frame_source, &unsubmitted);\n'
            '    }\n'
            '    /* Quarantine retains all static storage; this image is never run. */\n'
            '    (void)frame_blitter.ops->stop(frame_blitter.context);\n')
    if backend == "freertos":
        code += (
            '    /* Link the actual kernel; software qualification never runs it. */\n'
            '    (void)nx_freertos_task_start(&task, "sdk", task_stack, 128U, 1U,\n'
            '                                 task_entry, NULL);\n'
            '    vTaskStartScheduler();\n    for (;;) {}\n}\n')
    else:
        code += '    return (int)nx_platform_stop();\n}\n'
    (source / "main.c").write_text(code)
    arguments = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Release",
                 f"-DNexus_DIR={prefix / 'lib/cmake/Nexus'}",
                 "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF",
                 f"-DNEXUS_ASSEMBLY_FILE={assembly}",
                 f"-DCMAKE_TOOLCHAIN_FILE={sdk / 'cmake/toolchains/arm-gcc.cmake'}"]
    if fixture:
        arguments.append("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON")
    if expected_revision:
        arguments.append("-DNEXUS_EXPECTED_SOURCE_REVISION=" + expected_revision)
    return source, build, arguments


def component_owner_selection(specification):
    """Only the exact maintained tuple enables an independent owner audit."""
    if specification[2] not in COMPONENT_OWNER_SMOKES:
        return None
    if tuple(specification) not in SELECTED_CONSUMERS:
        raise ValueError(
            "Owner consumer differs from the canonical transport plan")
    return specification[2]


def prepare_selected_consumer(prefix, directory, name, assembly_name, smoke_name,
                              run_checked, *, expected_revision):
    """Reuse installed canonical inputs through a separate public C project."""
    owner = component_owner_selection((name, assembly_name, smoke_name))
    sdk = prefix / "share/nexus/src"
    verification = sdk / "cmake/package/verification/io_evolution"
    relative = PurePosixPath(assembly_name)
    if (relative.is_absolute() or ".." in relative.parts or
            "\\" in assembly_name or str(relative) != assembly_name or
            relative.suffix != ".toml"):
        raise ValueError("Selected consumer requires a canonical SDK-relative TOML")
    assembly = regular_file(sdk / assembly_name)
    if not re.fullmatch(r"[a-z0-9-]+", smoke_name):
        raise ValueError("Selected consumer smoke name is invalid")
    smoke = regular_file(verification / "consumers" / (smoke_name + ".c"))
    authored = tomllib.loads(assembly.read_text())
    if type(authored.get("schema")) is not int or authored["schema"] != 2:
        raise ValueError("Selected consumer requires assembly schema 2")
    source = directory / ("selected " + name + " source")
    build = directory / ("selected " + name + " build")
    source.mkdir()
    run_checked(["git", "init", "--quiet", str(source)])
    version = load_policy(sdk)["semantic_version"]
    library = {"u575-retained-time": "Nexus::U575RetainedTime",
               "u575-stop2": "Nexus::U575Stop2",
               "u575-lpdma-copy": "Nexus::U575Stop2Copy",
               "u575-lptim1-lpbam": "Nexus::U575Stop2Lptim1",
               "u575-mpu-gpio": "Nexus::U575MpuGpio",
               "f407-i2s2-slave-tx": "Nexus::F407I2S2SlaveTx"}.get(smoke_name)
    component = " LIBRARIES " + library if library else ""
    (source / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.31)\n'
        'project(installed_selected_consumer LANGUAGES C ASM)\n'
        'set(CMAKE_C_STANDARD 11)\n'
        'set(CMAKE_C_STANDARD_REQUIRED ON)\n'
        'set(CMAKE_C_EXTENSIONS OFF)\n'
        'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
        f'find_package(Nexus {version} EXACT CONFIG REQUIRED)\n'
        'nexus_add_firmware(installed_selected SOURCES main.c private_headers.c'
        f'{component})\n'
        'get_target_property(sdk_root Nexus::Config NEXUS_SOURCE_DIR)\n'
        'get_target_property(config_root Nexus::Platform NEXUS_CONFIG_DIR)\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/resolved-sdk.txt" "${sdk_root}")\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/resolved-config.txt" "${config_root}")\n')
    shutil.copyfile(smoke, source / "main.c")
    (source / "private_headers.c").write_text(
        '#if __has_include("stm32h7xx.h") || '
        '__has_include("stm32u5xx.h") || __has_include("stm32g4xx.h") || '
        '__has_include("stm32f4xx.h") || __has_include("gd32f4xx.h")\n'
        '#error Vendor SDK leaked into product source\n#endif\n'
        '#if __has_include("stm32h743_clock.h") || '
        '__has_include("stm32h743_provider.h") || '
        '__has_include("stm32h743_spi_dma.h") || '
        '__has_include("stm32h743_uart_rtscts.h") || '
        '__has_include("stm32h743_uart_continuous.h") || '
        '__has_include("stm32h743_uart_duplex.h") || '
        '__has_include("stm32h743_uart_continuous_impl.h") || '
        '__has_include("stm32h743_uart_dma_parts.h") || '
        '__has_include("stm32u575_uart_gpdma.h") || '
        '__has_include("stm32u575_uart_gpdma_chain.h") || '
        '__has_include("stm32u575_uart_gpdma_impl.h") || '
        '__has_include("stm32u575_provider.h") || '
        '__has_include("stm32u575_uart_rtscts.h") || '
        '__has_include("stm32u575_lptim.h") || '
        '__has_include("stm32u575_gpio_mpu.h") || '
        '__has_include("stm32u575_mpu_user_init.h") || '
        '__has_include("stm32u575_rtc.h") || '
        '__has_include("retained_time.h") || '
        '__has_include("stm32u575_rtc_calendar.h") || '
        '__has_include("stm32u575_stop2_hw.h") || '
        '__has_include("stm32u575_lpdma_copy_hw.h") || '
        '__has_include("stm32u575_lpdma_copy_internal.h") || '
        '__has_include("stm32u575_lptim1_profile.h") || '
        '__has_include("stm32u575_lpdma_lptim1_internal.h") || '
        '__has_include("stm32g474_fdcan.h") || '
        '__has_include("stm32g474_pwm_adc.h") || '
        '__has_include("stm32g474_adc_group.h") || '
        '__has_include("stm32g474_pwm_shared.h") || '
        '__has_include("stm32g474_pin_state.h") || '
        '__has_include("pin_state_impl.h") || '
        '__has_include("stm32f407_eth_frames.h") || '
        '__has_include("stm32f407_i2s2_slave_tx.h") || '
        '__has_include("gd32f470_eth_frames.h") || '
        '__has_include("stm32g474_provider.h") || '
        '__has_include("rtc_lptim_provider.h")\n'
        '#error Private provider leaked into product source\n#endif\n'
        '_Static_assert(1, "Public source include isolation");\n')
    if owner is not None:
        with (source / "private_headers.c").open("a") as output:
            output.write(
                '#include "nexus/core/time.h"\n'
                '/** \\brief External caller reads the injected face. */\n'
                'nx_time_us_t nx_installed_clock_read(\n'
                '    const nx_clock_t* clock) {\n'
                '    return clock->read(clock->context);\n}\n')
    command = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
               "-DCMAKE_BUILD_TYPE=Release",
               f"-DNexus_DIR={prefix / 'lib/cmake/Nexus'}",
               "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF",
               f"-DNEXUS_ASSEMBLY_FILE={assembly}",
               "-DNEXUS_EXPECTED_SOURCE_REVISION=" + expected_revision]
    if authored["backend"] != "native":
        command.append(f"-DCMAKE_TOOLCHAIN_FILE={sdk / 'cmake/toolchains/arm-gcc.cmake'}")
    return source, build, command


def check_native_time_image(sdk, source, build, config, commands, run_checked):
    """Observe this Linux host fixture, not MCU RAM or a general ELF loader.

    Canonical C executes real table methods and checks the entire face/ops
    spans against live /proc/self/maps. nm records only selected retained
    software objects. Neither symbol type nor ELF section flags prove runtime
    read-only mappings, and this observation never claims total RAM or HIL.
    """
    resolved = json.loads((config / "resolved.json").read_text())
    if (resolved.get("soc_family") != "native" or
            resolved.get("backend") != "native" or
            resolved.get("cpu", {}).get("arch") != "native"):
        raise ValueError("Native observation requires the actual Native domain")
    verification = sdk / "cmake/package/verification/io_evolution"
    main = file_identity(source / "main.c")
    matches = []
    for profile in ("pair", "empty"):
        canonical = verification / "consumers" / ("native-time-" + profile + ".c")
        if canonical.is_file() and file_identity(canonical)["sha256"] == main["sha256"]:
            matches.append(profile)
    if len(matches) != 1:
        raise ValueError("Native consumer differs from the installed canonical C scenario")
    profile = matches[0]
    assembly = verification / ("native-time-" + profile + ".toml")
    authored = tomllib.loads(assembly.read_text())
    expected = {(kind, name): item for kind in ("rtc", "lptim")
                for name, item in authored.get(kind, {}).items()}
    actual = resolved.get("controllers", [])
    if (not isinstance(actual, list) or len(actual) != len(expected) or
            {(item.get("kind"), item.get("id")) for item in actual} != set(expected)):
        raise ValueError("Native selected controller identities differ from the canonical assembly")
    for item in actual:
        want = expected[(item["kind"], item["id"])]
        if item.get("mode") != want["mode"] or (
                item["kind"] == "lptim" and item.get("tick_hz") != want["tick_hz"]):
            raise ValueError("Native selected mode/rate differs from the canonical assembly")
    for entry in commands:
        file = Path(entry["file"]).resolve()
        if file not in {source / "main.c", source / "private_headers.c", config / "bindings.c"}:
            continue
        argv = entry.get("arguments") or shlex.split(entry["command"])
        if argv.count("-c") != 1:
            raise ValueError("Native compile row does not identify one actual source")
        selected = Path(argv[argv.index("-c") + 1])
        if not selected.is_absolute():
            selected = Path(entry["directory"]) / selected
        if selected.resolve() != file:
            raise ValueError("Native compile row source and -c argument disagree")
        if file.parent == source and private_compile_inputs(entry, sdk):
            raise ValueError("Private Native state leaked into the public consumer")
    if sum(Path(entry["file"]).resolve() == config / "bindings.c" for entry in commands) != 1:
        raise ValueError("Native generated bindings have no unique compile row")
    image = build / "bin/installed_selected"
    with image.open("rb") as stream:
        header = stream.read(20)
    if len(header) != 20 or header[:6] != b"\x7fELF\x02\x01" or header[18:20] != b"\x3e\x00":
        raise ValueError("Native observation supports only actual little-endian x86_64 ELF64")
    inputs = {
        "elf": image, "resolved": config / "resolved.json",
        "bindings": config / "bindings.c", "compile_commands": build / "compile_commands.json",
        "canonical_source": verification / "consumers" / ("native-time-" + profile + ".c"),
        "assembly": assembly, "sdk_manifest": sdk / ".nexus-source-sdk.json",
        "public_source": source / "main.c", "private_probe": source / "private_headers.c",
    }
    before = {name: file_identity(path) for name, path in inputs.items()}
    runtime = run_checked([str(image)])
    if runtime.returncode:
        raise ValueError("Native canonical runtime failed")
    observed = run_checked(["nm", "-S", "--defined-only", "--format=posix", str(image)])
    if observed.returncode or len(observed.stdout) > 4 * 1024 * 1024:
        raise ValueError("Native symbol observation failed or exceeded its bound")
    symbols = {}
    for line in observed.stdout.splitlines():
        fields = line.split()
        if not fields:
            continue
        name = fields[0]
        if not (name.startswith("s_nx_port_") or name in {
                "nx_native_rtc_ops", "nx_native_lptim_ops",
                "nx_native_time_clock_read", "s_nx_native_time_clock"}):
            continue
        if (len(fields) != 4 or name in symbols or
                not re.fullmatch(r"[0-9a-fA-F]+", fields[2]) or
                not re.fullmatch(r"[0-9a-fA-F]+", fields[3])):
            raise ValueError("Native selected symbol identity is ambiguous")
        symbols[name] = {"type": fields[1], "address": int(fields[2], 16),
                         "size": int(fields[3], 16)}
    states = {"s_nx_port_" + name.replace("-", "_") for _, name in expected}
    required = set(states)
    if expected:
        required.update(("nx_native_rtc_ops", "nx_native_lptim_ops",
                         "nx_native_time_clock_read", "s_nx_native_time_clock"))
    if set(symbols) != required or any(item["size"] <= 0 for item in symbols.values()):
        raise ValueError("Native selected provider retention differs from the resolved selection")
    if before != {name: file_identity(path) for name, path in inputs.items()}:
        raise ValueError("Native observation inputs changed during execution")
    return {"kind": "native-time-host-observation", "status": "passed",
            "physical_status": "not_executed", **before,
            "selected_state_count": len(states),
            "selected_state_bytes": sum(symbols[name]["size"] for name in states),
            "symbols": symbols,
            "qualification": "This Linux x86_64 canonical runtime and selected symbol subset only; not total RAM, MCU or hardware qualification."}


def check_selected_image(sdk, source, build, run_checked, *,
                         owner_specification=None):
    """Bind selected artifacts to actual resources and the retained provider."""
    if (build / "resolved-sdk.txt").read_text() != str(sdk):
        raise ValueError("Selected consumer resolved another source SDK")
    commands = json.loads((build / "compile_commands.json").read_text())
    products = [entry for entry in commands
                if Path(entry["file"]).resolve() in
                {source / "main.c", source / "private_headers.c"}]
    if (len(products) != 2 or
            {Path(entry["file"]).name for entry in products} !=
            {"main.c", "private_headers.c"} or
            any(private_compile_inputs(entry, sdk) for entry in products)):
        raise ValueError("Selected consumer private inputs leaked into product")
    config = Path((build / "resolved-config.txt").read_text())
    resolved = json.loads((config / "resolved.json").read_text())
    if resolved.get("backend") == "native":
        return check_native_time_image(sdk, source, build, config, commands, run_checked)
    artifacts = {suffix: file_identity(build / "bin" /
                                      ("installed_selected." + suffix))
                 for suffix in ("elf", "bin", "map")}
    if any(record["size"] == 0 for record in artifacts.values()):
        raise ValueError("Selected consumer artifact is empty")
    config = Path((build / "resolved-config.txt").read_text())
    resources_path = build / "bin/installed_selected.resources.json"
    resources = json.loads(resources_path.read_text())
    if (resources["status"] != "passed" or
            resources["elf"] != artifacts["elf"] or
            resources["resolved"] != file_identity(config / "resolved.json")):
        raise ValueError("Selected consumer resource/configuration binding failed")
    owner = (component_owner_selection(owner_specification)
             if owner_specification is not None else None)
    retention_path = build / "provider-retention.json"
    command = [sys.executable, "-B",
                 str(sdk / "tools/measurement/retention.py"),
                 "--elf", artifacts["elf"]["path"],
                 "--bindings", str(config / "bindings.c"),
                 "--resolved", str(config / "resolved.json"),
                 "--output", str(retention_path)]
    if owner is not None:
        command += ["--component-owner", owner, "--consumer",
                    str(source / "main.c")]
    run_checked(command)
    retention = json.loads(retention_path.read_text())
    if (retention["status"] != "passed" or
            retention["elf"] != artifacts["elf"]):
        raise ValueError("Selected consumer provider retention failed")
    if owner is not None and (
            retention.get("kind") != "linked_component_owner_retention" or
            retention.get("owner_profile") != owner or
            retention.get("consumer") != file_identity(source / "main.c")):
        raise ValueError("Selected component owner retention binding failed")
    return {"artifacts": artifacts, "resources": file_identity(resources_path),
            "resolved": file_identity(config / "resolved.json"),
            "retention": file_identity(retention_path)}


def positive_parallel(value):
    """Require a canonical positive job count, without a policy maximum."""
    try:
        count = int(value)
    except (TypeError, ValueError) as error:
        raise argparse.ArgumentTypeError(
            "parallel must be a positive integer") from error
    if count < 1 or str(count) != value:
        raise argparse.ArgumentTypeError("parallel must be a positive integer")
    return count


def verify_consumer(kind, specification, prefix, output, identity, run_checked):
    """Execute one complete independent chain from the shared transport plan."""
    sdk = prefix / "share/nexus/src"
    if kind == "base":
        name, arm, chip = specification
        source, build, command = prepare_consumer(
            prefix, output, name, run_checked, arm=arm, chip=chip,
            expected_revision=identity["source_revision"])
        run_checked(command)
        run_checked(["cmake", "--build", str(build), "--parallel", "1",
                     "--target", "installed_c", "installed_cpp"])
        commands = json.loads((build / "compile_commands.json").read_text())
        for entry in commands:
            if Path(entry["file"]).name not in {"main.c", "main.cpp"}:
                continue
            if private_compile_inputs(entry, sdk):
                raise ValueError(
                    "Private implementation input leaked into product source")
        if arm:
            for application in ("installed_c", "installed_cpp"):
                resources = json.loads((build / "bin" /
                    (application + ".resources.json")).read_text())
                if resources["status"] != "passed":
                    raise ValueError(
                        "MCU consumer resource/configuration binding failed")
        else:
            for application in ("installed_c", "installed_cpp"):
                run_checked([str(build / "bin" / application)])
    elif kind == "modern":
        family, backend = specification
        source, build, command = prepare_modern_consumer(
            prefix, output, family, backend, run_checked,
            expected_revision=identity["source_revision"])
        run_checked(command)
        run_checked(["cmake", "--build", str(build), "--parallel", "1",
                     "--target", "installed_modern"])
        commands = json.loads((build / "compile_commands.json").read_text())
        products = [entry for entry in commands
                    if Path(entry["file"]).resolve() == source / "main.c"]
        if len(products) != 1 or private_compile_inputs(products[0], sdk):
            raise ValueError("Modern consumer private inputs leaked into product")
        image = build / "bin/installed_modern.elf"
        resources = json.loads(
            (build / "bin/installed_modern.resources.json").read_text())
        if resources["status"] != "passed" or image.stat().st_size == 0:
            raise ValueError("Modern consumer linked resource binding failed")
        config = Path((build / "resolved-config.txt").read_text())
        retention_path = build / "provider-retention.json"
        run_checked([sys.executable, "-B",
            str(sdk / "tools/measurement/retention.py"),
            "--elf", str(image), "--bindings", str(config / "bindings.c"),
            "--resolved", str(config / "resolved.json"),
            "--output", str(retention_path)])
        if json.loads(retention_path.read_text())["status"] != "passed":
            raise ValueError("Modern consumer provider retention failed")
    else:
        name, assembly, smoke = specification
        source, build, command = prepare_selected_consumer(
            prefix, output, name, assembly, smoke, run_checked,
            expected_revision=identity["source_revision"])
        run_checked(command)
        run_checked(["cmake", "--build", str(build), "--parallel", "1",
                     "--target", "installed_selected"])
        image = check_selected_image(sdk, source, build, run_checked,
                                     owner_specification=specification)
        image.update({"name": name, "assembly": str(sdk / assembly)})
        return image
    return None


class ConsumerCancelled(Exception):
    """A peer already failed; this chain must stop without replacing its cause."""


class ConsumerCommandError(ValueError):
    """Retain a real command failure, separately from coordinator cancellation."""

    def __init__(self, record, output):
        self.exit_code = record["exit_code"]
        if (self.exit_code == 130 and
                isinstance(record.get("child_returncode"), int) and
                record["child_returncode"] > 0):
            self.exit_code = record["child_returncode"]
        super().__init__(f"Consumer command failed; see {output / record['log']}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--parallel", type=positive_parallel, default=2,
                        help="Total concurrent consumer jobs (default: 2); "
                             "each build uses one job")
    args = parser.parse_args(argv)
    prefix, output = args.prefix.absolute(), args.output.absolute()
    records = []
    report = {"schema_version": 1, "kind": "source_sdk_consumer_verification",
              "prefix": str(prefix), "status": "failed",
              "physical_status": "not_executed", "commands": records,
              "selected_images": [], "parallel_jobs": args.parallel}
    output_prepared = False
    cancel = threading.Event()
    failure_lock = threading.Lock()
    failures = []

    def remember_failure(index, error):
        failure = {"consumer_index": index, "error": str(error),
                   "exit_code": getattr(error, "exit_code", None)}
        with failure_lock:
            failures.append(failure)
            cancel.set()
        return failure

    def command_runner(index, commands):
        def run_checked(command):
            if cancel.is_set():
                raise ConsumerCancelled("Consumer cancelled after an earlier failure")
            phase = len(commands)
            log = output / f"command-{index:02d}-{phase:02d}.log"
            environment = clean_environment()
            # The worker owns one global job, including nested configure/check
            # tools. NEXUS_BUILD_JOBS takes precedence in the shared runner.
            environment.update(NEXUS_BUILD_JOBS="1",
                               CMAKE_BUILD_PARALLEL_LEVEL="1",
                               CTEST_PARALLEL_LEVEL="1")
            result = process.run(command, cwd=Path.cwd(), env=environment,
                                 log=log, timeout=180,
                                 stage="source-sdk-consumer",
                                 cancel_event=cancel)
            receipt = json.loads(result.receipt_path.read_text())
            record = {"argv": [str(value) for value in command],
                      "exit_code": result.returncode, "log": log.name,
                      "consumer_index": index, "phase_index": phase,
                      "allocated_jobs": 1,
                      "receipt": str(result.receipt_path.relative_to(output)),
                      **{key: receipt[key] for key in (
                          "started_ns", "finished_ns", "duration_seconds",
                          "outcome", "child_returncode", "withdrawal_complete")}}
            commands.append(record)
            if result.returncode:
                if (result.returncode == 130 and cancel.is_set() and
                        not (receipt.get("child_returncode") is not None and
                             receipt["child_returncode"] > 0)):
                    raise ConsumerCancelled(
                        "Running consumer withdrawn after an earlier failure")
                raise ConsumerCommandError(record, output)
            if not receipt["withdrawal_complete"]:
                raise ValueError("Consumer process withdrawal was incomplete")
            return result
        return run_checked

    try:
        if (prefix.resolve() != prefix or output.resolve() != output or
                output.is_relative_to(prefix) or prefix.is_relative_to(output)):
            raise ValueError("Consumer SDK and output must be canonical and disjoint")
        if output.exists():
            if not (output / ".nexus-sdk-consumer-check").is_file():
                raise ValueError(
                    "Existing output is not an owned consumer verification bundle")
            shutil.rmtree(output)
        output.mkdir(parents=True)
        (output / ".nexus-sdk-consumer-check").write_text(
            "owned SDK consumer verification\n")
        output_prepared = True
        sdk = prefix / "share/nexus/src"
        command_runner(0, records)([
            sys.executable, "-B", str(sdk / "cmake/package/package_source_sdk.py"),
            "--verify", str(sdk)])
        identity = json.loads((sdk / ".nexus-source-sdk.json").read_text())
        if identity["publishable"] is not True or identity["source_dirty"]:
            raise ValueError(
                "Formal consumer verification requires a publishable committed "
                "source SDK")
        report["source_revision"] = identity["source_revision"]
        report["source_tree"] = identity["source_tree"]
        # The maintained transport tuples remain the sole plan authority.
        plan = ([("base", specification) for specification in BASE_CONSUMERS] +
                [("modern", specification) for specification in MODERN_CONSUMERS] +
                [("selected", specification) for specification in SELECTED_CONSUMERS])
        results = [None] * len(plan)
        local_records = [[] for _ in plan]
        workers = min(args.parallel, len(plan))
        report["consumer_workers"] = workers

        def work(position):
            kind, specification = plan[position]
            name = (specification[0] + " " + specification[1]
                    if kind == "modern" else
                    ("selected " if kind == "selected" else "") + specification[0])
            outcome = {"consumer_index": position + 1, "name": name,
                       "status": "cancelled"}
            try:
                if cancel.is_set():
                    raise ConsumerCancelled("Consumer cancelled before admission")
                image = verify_consumer(kind, specification, prefix, output,
                                        identity, command_runner(
                                            position + 1, local_records[position]))
                outcome["status"] = "passed"
                if image is not None:
                    outcome["selected_image"] = image
            except ConsumerCancelled as error:
                outcome["error"] = str(error)
            except Exception as error:
                outcome["status"] = "failed"
                outcome["failure"] = remember_failure(position + 1, error)
            return outcome

        executor = concurrent.futures.ThreadPoolExecutor(max_workers=workers)
        started = {}
        pending = set()
        next_position = 0
        interrupted = False
        previous_handlers = {}

        def interrupt(signum, frame):
            nonlocal interrupted
            del signum, frame
            # Raising here could interrupt shutdown itself before its report.
            interrupted = True
            cancel.set()

        # Signals reach only the coordinator; set cancellation before waiting
        # for workers so every started process tree can be withdrawn and joined.
        if threading.current_thread() is threading.main_thread():
            for signum in (signal.SIGINT, signal.SIGTERM):
                previous_handlers[signum] = signal.signal(signum, interrupt)
        try:
            while (next_position < len(plan) and len(pending) < workers and
                   not cancel.is_set()):
                future = executor.submit(work, next_position)
                started[future] = next_position
                pending.add(future)
                next_position += 1
            while pending:
                completed, pending = concurrent.futures.wait(
                    pending, return_when=concurrent.futures.FIRST_COMPLETED)
                for future in completed:
                    results[started[future]] = future.result()
                while (next_position < len(plan) and len(pending) < workers and
                       not cancel.is_set()):
                    future = executor.submit(work, next_position)
                    started[future] = next_position
                    pending.add(future)
                    next_position += 1
        except KeyboardInterrupt:
            interrupted = True
            cancel.set()
        finally:
            # Unexpected coordinator errors must also release running children.
            if pending or failures:
                cancel.set()
            executor.shutdown(wait=True, cancel_futures=True)
            for future, position in started.items():
                if future.cancelled():
                    results[position] = {"consumer_index": position + 1,
                                         "status": "cancelled"}
                else:
                    results[position] = future.result()
            for signum, handler in previous_handlers.items():
                signal.signal(signum, handler)
            for commands in local_records:
                records.extend(commands)
            report["consumers"] = [item if item is not None else {
                "consumer_index": index + 1, "status": "not_started"}
                for index, item in enumerate(results)]
            report["selected_images"] = [item["selected_image"] for item in results
                if item is not None and "selected_image" in item]
        if interrupted:
            error = ValueError("Consumer verification interrupted")
            error.exit_code = 130
            remember_failure(0, error)
        if failures:
            report["failures"] = failures
            report["first_failure"] = failures[0]
            raise ValueError(failures[0]["error"])
        if len(results) != len(plan) or any(
                item is None or item["status"] != "passed" for item in results):
            raise ValueError("Consumer plan was not completely joined and verified")
        report["status"] = "passed"
    except Exception as error:
        if not failures:
            remember_failure(0, error)
        report["failures"] = failures
        report["first_failure"] = failures[0]
        report["error"] = failures[0]["error"]
        print(f"Source SDK consumers rejected: {report['error']}",
              file=sys.stderr)
    if (output_prepared and output.is_dir() and
            (output / ".nexus-sdk-consumer-check").is_file()):
        (output / "verification.json").write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
