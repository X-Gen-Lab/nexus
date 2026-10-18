#!/usr/bin/env python3
"""Verify independent consumers of an existing publishable source SDK prefix."""
import argparse
import json
import re
import shlex
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tomllib

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.configure.metadata import load_policy  # noqa: E402
from tools.evidence.common import file_identity  # noqa: E402
from tools.release.transport import (BASE_CONSUMERS, MODERN_CONSUMERS,
                                     SELECTED_CONSUMERS)  # noqa: E402




def clean_environment():
    environment = os.environ.copy()
    for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                 "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH", "LIBRARY_PATH"):
        environment.pop(name, None)
    return environment


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
        'nexus_add_firmware(installed_cpp SOURCES main.cpp LIBRARIES Nexus::Log Nexus::BMP280 Nexus::Storage)\n'
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
    (source / "main.cpp").write_text(
        '#include "nexus/components/log.h"\n'
        '#include "nexus/components/bmp280.h"\n'
        '#include "nexus/components/storage.h"\n'
        '#include <cstring>\n' + code.replace(
            ' nx_platform_start_result_t result',
            ' nx_log_sink_port_t sink;\n std::memset(&sink, 0, sizeof(sink));\n'
            ' if (nx_log_init(nullptr, sink, NX_LOG_INFO) != NX_ERROR_INVALID) return 3;\n'
            ' if (nx_bmp280_compensate(nullptr, 0, 0, nullptr) != NX_ERROR_INVALID) return 4;\n'
            ' nx_platform_start_result_t result'))
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
    kernel = " LIBRARIES Nexus::OSFreeRTOS" if backend == "freertos" else ""
    (source / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.31)\n'
        'project(installed_modern_consumer LANGUAGES C ASM)\n'
        'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
        f'find_package(Nexus {version} EXACT CONFIG REQUIRED)\n'
        f'nexus_add_firmware(installed_modern SOURCES main.c{kernel})\n'
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


def prepare_selected_consumer(prefix, directory, name, assembly_name, smoke_name,
                              run_checked, *, expected_revision):
    """Reuse installed canonical inputs through a separate public C project."""
    sdk = prefix / "share/nexus/src"
    verification = sdk / "cmake/package/verification/io_evolution"
    assembly = verification / (assembly_name + ".toml")
    smoke = verification / "consumers" / (smoke_name + ".c")
    if not assembly.is_file() or not smoke.is_file():
        raise ValueError("Installed selected-consumer input is missing")
    source = directory / ("selected " + name + " source")
    build = directory / ("selected " + name + " build")
    source.mkdir()
    run_checked(["git", "init", "--quiet", str(source)])
    version = load_policy(sdk)["semantic_version"]
    (source / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.31)\n'
        'project(installed_selected_consumer LANGUAGES C ASM)\n'
        'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
        f'find_package(Nexus {version} EXACT CONFIG REQUIRED)\n'
        'nexus_add_firmware(installed_selected SOURCES main.c private_headers.c)\n'
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
        '__has_include("stm32h743_uart_rtscts.h") || '
        '__has_include("stm32u575_uart_gpdma.h") || '
        '__has_include("stm32u575_provider.h") || '
        '__has_include("stm32u575_uart_rtscts.h") || '
        '__has_include("stm32g474_fdcan.h") || '
        '__has_include("stm32g474_pwm_adc.h") || '
        '__has_include("stm32g474_adc_group.h") || '
        '__has_include("stm32g474_pwm_shared.h") || '
        '__has_include("stm32f407_eth_frames.h") || '
        '__has_include("gd32f470_eth_frames.h") || '
        '__has_include("stm32g474_provider.h") || '
        '__has_include("rtc_lptim_provider.h")\n'
        '#error Private provider leaked into product source\n#endif\n'
        '_Static_assert(1, "Public source include isolation");\n')
    command = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
               "-DCMAKE_BUILD_TYPE=Release",
               f"-DNexus_DIR={prefix / 'lib/cmake/Nexus'}",
               "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF",
               f"-DNEXUS_ASSEMBLY_FILE={assembly}",
               "-DNEXUS_EXPECTED_SOURCE_REVISION=" + expected_revision]
    authored = tomllib.loads(assembly.read_text())
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
        if file.parent == source and any(value in " ".join(argv)
                for value in ("/io/native", "/io/private")):
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


def check_selected_image(sdk, source, build, run_checked):
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
            any(value in entry["command"] for entry in products for value in
                ("/vendors/", "/soc/", "-DSTM32", "-DGD32"))):
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
    retention_path = build / "provider-retention.json"
    run_checked([sys.executable, "-B", str(sdk / "tools/measurement/retention.py"),
                 "--elf", artifacts["elf"]["path"],
                 "--bindings", str(config / "bindings.c"),
                 "--resolved", str(config / "resolved.json"),
                 "--output", str(retention_path)])
    retention = json.loads(retention_path.read_text())
    if (retention["status"] != "passed" or
            retention["elf"] != artifacts["elf"]):
        raise ValueError("Selected consumer provider retention failed")
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


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--parallel", type=positive_parallel,
                        help="Build jobs per consumer (default: 4 base, 2 modern)")
    args = parser.parse_args(argv)
    prefix, output = args.prefix.absolute(), args.output.absolute()
    records = []
    report = {"schema_version": 1, "kind": "source_sdk_consumer_verification",
              "prefix": str(prefix), "status": "failed",
              "physical_status": "not_executed", "commands": records,
              "selected_images": []}
    try:
        if prefix.resolve() != prefix or output.resolve() != output or output.is_relative_to(prefix):
            raise ValueError("Consumer output must be canonical and outside the SDK")
        if output.exists():
            if not (output / ".nexus-sdk-consumer-check").is_file():
                raise ValueError("Existing output is not an owned consumer verification bundle")
            shutil.rmtree(output)
        output.mkdir(parents=True)
        (output / ".nexus-sdk-consumer-check").write_text("owned SDK consumer verification\n")
        def run_checked(command):
            result = subprocess.run(command, capture_output=True, text=True,
                                    env=clean_environment(), timeout=180)
            index = len(records)
            log = output / f"command-{index:02d}.log"
            log.write_text(result.stdout + result.stderr)
            records.append({"argv": [str(value) for value in command],
                            "exit_code": result.returncode, "log": log.name})
            if result.returncode:
                raise ValueError(f"Consumer command {index} failed; see {log}")
            return result
        sdk = prefix / "share/nexus/src"
        run_checked([sys.executable, "-B", str(sdk / "cmake/package/package_source_sdk.py"),
                     "--verify", str(sdk)])
        identity = json.loads((sdk / ".nexus-source-sdk.json").read_text())
        if identity["publishable"] is not True or identity["source_dirty"]:
            raise ValueError("Formal consumer verification requires a publishable committed source SDK")
        report["source_revision"] = identity["source_revision"]
        report["source_tree"] = identity["source_tree"]
        for name, arm, chip in BASE_CONSUMERS:
            source, build, command = prepare_consumer(prefix, output, name,
                run_checked, arm=arm, chip=chip, expected_revision=identity["source_revision"])
            run_checked(command)
            run_checked(["cmake", "--build", str(build), "--parallel",
                         str(args.parallel) if args.parallel is not None else "4",
                         "--target", "installed_c", "installed_cpp"])
            commands = json.loads((build / "compile_commands.json").read_text())
            for entry in commands:
                if Path(entry["file"]).name not in {"main.c", "main.cpp"}:
                    continue
                if any(value in entry["command"] for value in
                       ("/vendors/", "/soc/", "-DSTM32F407", "-DGD32F470")):
                    raise ValueError("Private implementation input leaked into product source")
            if arm:
                for application in ("installed_c", "installed_cpp"):
                    resources = json.loads((build / "bin" / (application + ".resources.json")).read_text())
                    if resources["status"] != "passed":
                        raise ValueError("MCU consumer resource/configuration binding failed")
            else:
                for application in ("installed_c", "installed_cpp"):
                    run_checked([str(build / "bin" / application)])
        for family, backend in MODERN_CONSUMERS:
            source, build, command = prepare_modern_consumer(
                prefix, output, family, backend, run_checked,
                expected_revision=identity["source_revision"])
            run_checked(command)
            run_checked(["cmake", "--build", str(build), "--parallel",
                         str(args.parallel) if args.parallel is not None else "2",
                         "--target", "installed_modern"])
            commands = json.loads((build / "compile_commands.json").read_text())
            products = [entry for entry in commands
                        if Path(entry["file"]).resolve() == source / "main.c"]
            if len(products) != 1 or any(value in products[0]["command"]
                    for value in ("/vendors/", "/soc/", "-DSTM32", "-DGD32")):
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
        for name, assembly, smoke in SELECTED_CONSUMERS:
            source, build, command = prepare_selected_consumer(
                prefix, output, name, assembly, smoke, run_checked,
                expected_revision=identity["source_revision"])
            run_checked(command)
            run_checked(["cmake", "--build", str(build), "--parallel",
                         str(args.parallel) if args.parallel is not None else "2",
                         "--target", "installed_selected"])
            image = check_selected_image(sdk, source, build, run_checked)
            image.update({"name": name, "assembly": str(
                sdk / "cmake/package/verification/io_evolution" /
                (assembly + ".toml"))})
            report["selected_images"].append(image)
        report["status"] = "passed"
    except (OSError, ValueError, subprocess.TimeoutExpired, KeyError) as error:
        report["error"] = str(error)
        print(f"Source SDK consumers rejected: {error}", file=sys.stderr)
    if output.is_dir() and (output / ".nexus-sdk-consumer-check").is_file():
        (output / "verification.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
