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

if __name__ == "__main__":
    sys.dont_write_bytecode = True

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.configure.metadata import load_policy  # noqa: E402
from tools.dev import process  # noqa: E402
from tools.evidence.common import file_identity, regular_file  # noqa: E402
from tools.maintenance.consumer_profiles import (BASE_CONSUMERS, MODERN_CONSUMERS,
                                     SELECTED_CONSUMERS, SERVICE_CONSUMERS,
                                     ServiceConsumer, consumer_plan,
                                     selected_domain)  # noqa: E402




def clean_environment():
    environment = os.environ.copy()
    for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                 "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH", "LIBRARY_PATH"):
        environment.pop(name, None)
    return environment


def private_compile_inputs(entry, sdk, extra_private=()):
    """Audit actual compiler options against canonical Nexus source boundaries.

    Kernel PUBLIC headers remain valid. F407 SoC APIs live only below the
    maintained soc/stm32/stm32f4/include subtree. Chip component APIs live below
    their declared package's include/nexus directory; package roots and
    implementation directories remain private.
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
        if any(path.is_relative_to(root.resolve()) for root in extra_private):
            return True
        for root in roots:
            if not path.is_relative_to(root):
                continue
            relative = path.relative_to(root)
            parts = relative.parts
            if (not parts or parts[0] in {"drivers", "tests"} or
                    {"vendors", "private"}.intersection(parts)):
                return True
            if (parts[0] == "soc" and
                    parts[:4] != ("soc", "stm32", "stm32f4", "include")):
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
    if chip not in {"native", "stm32"}:
        raise ValueError("Only STM32F4 and explicit host consumers are maintained")
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
            '#include "nexus_factory_strong.h"\n'
            '#include "nexus_config.h"\n'
            '#ifndef NEXUS_PLATFORM_VERSION\n'
            '#error "Missing source interface version"\n'
            '#endif\n'
            '#if NEXUS_FACTORY_ABI_EPOCH != 3U\n'
            '#error "Unsupported factory interface epoch"\n'
            '#endif\n'
            '#if __has_include("stm32f4xx.h") || __has_include("gd32f4xx.h")\n'
            '#error Vendor SDK leaked into product source\n#endif\n'
            '#if __has_include("stm32f407_provider.h") || __has_include("gd32f470_provider.h")\n'
            '#error Concrete provider leaked into product source\n#endif\n'
            'int main(void) {\n'
            ' if (nx_factory_gpio_ref(NX_GPIO_REF_COUNT) != 0) return 7;\n'
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
    assembly = (sdk / "tools/configure/assemblies/sky-baremetal.toml" if arm else
                sdk / "tests/support/native/assemblies/native.toml")
    if not arm:
        arguments.append("-DNEXUS_TEST_SUPPORT=ON")
    arguments.append(f"-DNEXUS_ASSEMBLY_FILE={assembly}")
    if arm:
        arguments.append(f"-DCMAKE_TOOLCHAIN_FILE={sdk / 'cmake/toolchains/arm-gcc.cmake'}")
    return source, build, arguments




def service_profile(specification):
    """Accept exactly one immutable entry from the reviewed transport plan."""
    if (not isinstance(specification, ServiceConsumer)
            or specification not in SERVICE_CONSUMERS):
        raise ValueError("Service consumer differs from the canonical plan")
    return specification


def service_directory(sdk, specification):
    """Use the single owned scenario tree, including shared F4 references."""
    return (Path(sdk) / "tests/contracts" / ("stm32" + specification.scenario)
            if specification.scenario.startswith("f4_") else
            Path(sdk) / "cmake/package/verification" / specification.scenario)


def service_sources(sdk, specification):
    """Return only the exact reviewed scenario's authored consumer sources."""
    verification = service_directory(sdk, specification)
    if specification.scenario.startswith("f4_"):
        authored = tomllib.loads((verification / specification.assembly).read_text())
        board = PurePosixPath(authored["board_package"])
        if (board.is_absolute() or len(board.parts) != 1 or
                board.parts[0] in {"", ".", ".."} or
                board.as_posix() != authored["board_package"] or
                "\\" in authored["board_package"]):
            raise ValueError("F4 reference requires its canonical Board directory")
        name = (board / "board.json").as_posix()
        if specification.scenario == "f4_family":
            consumer = Path(sdk) / "tests/contracts/crc_assembly/consumer.c"
        elif specification.scenario == "f4_graphics_sdram":
            consumer = Path(sdk) / "tests/contracts/stm32f4_graphics/consumer.c"
        else:
            consumer = verification / "consumer.c"
        return {"main.c": consumer,
                name: verification / name}
    if specification.scenario == "secure_services":
        selected = verification / Path(specification.assembly).parent
        return {"application.c": selected / "application.c",
                "application.h": selected / "application.h",
                "composition.c": selected / "composition.c",
                "public_ca.h": verification / "public_ca.h"}
    if specification.scenario == "image_window":
        return {"main.c": verification / "main.c",
                "layout.json": verification / "layout.json"}
    if specification.scenario in {"loaned_rx", "mcuboot_control"}:
        composition = ("composition.c" if specification.scenario == "loaned_rx"
                       else Path(specification.assembly).stem + ".c")
        return {"application.c": verification / "application.c",
                "application.h": verification / "application.h",
                "composition.c": verification / composition}
    if specification.scenario == "mcuboot_boot":
        return {name: verification / name for name in
                ("main.c", "layout.json", "public_key.c")}
    return {"main.c": verification / "services.c"}








def prepare_service_consumer(prefix, directory, specification, run_checked, *,
                             expected_revision, fixture=False):
    """Copy an installed public scenario; external packages stay consumer-owned."""
    specification = service_profile(specification)
    sdk = Path(prefix) / "share/nexus/src"
    verification = service_directory(sdk, specification)
    assembly = regular_file(verification / specification.assembly)
    canonical_sources = service_sources(sdk, specification)
    for path in canonical_sources.values():
        regular_file(path)
    source = directory / (specification.name + " source")
    build = directory / (specification.name + " build")
    source.mkdir()
    run_checked(["git", "init", "--quiet", str(source)])
    if specification.domain == "external-package":
        for name, path in external_package_sources(assembly.parent).items():
            target = source / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
    else:
        shutil.copyfile(assembly, source / "assembly.toml")
    for name, path in canonical_sources.items():
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
    secure = specification.scenario == "secure_services"
    boot = specification.scenario == "mcuboot_boot"
    (source / "private_headers.c").write_text(
        '#if __has_include("stm32f4xx.h") || '
        '__has_include("gd32f4xx.h") || __has_include("provider.h")\n'
        '#error Private hardware state leaked into the public consumer\n'
        '#endif\n'
        + ('#if __has_include("mbedtls/ssl.h") || '
           '__has_include("core_mqtt.h") || __has_include("lwip/init.h")\n'
           '#error Supplier headers leaked into public secure APIs\n#endif\n'
           if secure else '')
        + ('#if __has_include("lwip/init.h") || '
           'defined(NEXUS_LWIP_LOANED_RX)\n'
           '#error Selected lwIP layout leaked into generic API\n#endif\n'
           if specification.scenario == "loaned_rx" else '')
        + ('#if __has_include("flash_map_backend/flash_map_backend.h") || '
           '__has_include("bootutil/bootutil.h") || '
           '__has_include("mcuboot_config/mcuboot_config.h")\n'
           '#error MCUboot composition leaked into generic API\n#endif\n'
           if specification.scenario.startswith("mcuboot_") else '')
        + '_Static_assert(1, "Public service include isolation");\n')
    libraries = {
        "common_services": "Nexus::Settings Nexus::OSWait Nexus::LwIPStorage",
        "device_tmp117": "Nexus::TMP117I2C",
        "image_window": "",
        "f4_family": "",
        "f4_crypto": "",
        "f4_sdram": "",
        "f4_qspi": "",
        "f4_graphics": "",
        "f4_graphics_sdram": "",
        "f4_sai": "",
    }
    selected_setup = ""
    if specification.scenario in {"loaned_rx", "mcuboot_control"}:
        generic, storage = (("Nexus::LwIPLoanedRX", "Nexus::LwIPLoanedRXStorage")
                            if specification.scenario == "loaned_rx" else
                            ("Nexus::MCUBootControl", "Nexus::MCUBootPort"))
        visibility = ("PUBLIC" if specification.scenario == "mcuboot_control"
                      else "PRIVATE")
        selected_setup = (('nexus_add_mcuboot_control()\n'
                           if specification.scenario == "mcuboot_control" else '')
            + 'add_library(installed_public STATIC application.c private_headers.c)\n'
            + f'target_link_libraries(installed_public {visibility} {generic})\n'
            + 'nexus_apply_target_policy(installed_public)\n'
            + 'nexus_target_warnings(installed_public STRICT)\n'
            + 'nexus_add_firmware(installed_services SOURCES composition.c\n'
            + f'  LIBRARIES installed_public {storage})\n')
    elif boot:
        selected_setup = (
            'nexus_add_mcuboot_boot()\n'
            'add_library(installed_public STATIC private_headers.c)\n'
            'target_link_libraries(installed_public PRIVATE Nexus::MCUBootControl)\n'
            'nexus_apply_target_policy(installed_public)\n'
            'nexus_target_warnings(installed_public STRICT)\n'
            'file(READ "${CMAKE_CURRENT_SOURCE_DIR}/layout.json" reference_layout)\n'
            'foreach(role PRIMARY SECONDARY)\n'
            '  if(role STREQUAL "PRIMARY")\n'
            '    set(index 1)\n'
            '  else()\n'
            '    set(index 2)\n'
            '  endif()\n'
            '  string(JSON REFERENCE_${role}_ORIGIN GET "${reference_layout}" regions ${index} origin)\n'
            '  string(JSON REFERENCE_${role}_SIZE GET "${reference_layout}" regions ${index} size)\n'
            'endforeach()\n'
            'file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/reference")\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/reference/reference_regions.h"\n'
            '  "#define REFERENCE_PRIMARY_ORIGIN ${REFERENCE_PRIMARY_ORIGIN}U\\n"\n'
            '  "#define REFERENCE_PRIMARY_SIZE ${REFERENCE_PRIMARY_SIZE}U\\n"\n'
            '  "#define REFERENCE_SECONDARY_ORIGIN ${REFERENCE_SECONDARY_ORIGIN}U\\n"\n'
            '  "#define REFERENCE_SECONDARY_SIZE ${REFERENCE_SECONDARY_SIZE}U\\n")\n'
            'nexus_add_firmware(installed_services SOURCES main.c public_key.c\n'
            '  LIBRARIES installed_public Nexus::MCUBootBootStorage)\n'
            'target_include_directories(installed_services PRIVATE "${CMAKE_BINARY_DIR}/reference")\n')
    version = load_policy(sdk)["semantic_version"]
    (source / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.31)\n'
        'project(installed_service_consumer LANGUAGES C ASM)\n'
        'set(CMAKE_C_STANDARD 11)\n'
        'set(CMAKE_C_STANDARD_REQUIRED ON)\n'
        'set(CMAKE_C_EXTENSIONS OFF)\n'
        'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
        f'find_package(Nexus {version} EXACT CONFIG REQUIRED)\n'
        + ('nexus_add_lwip()\n' if secure or specification.scenario in {
            'common_services', 'loaned_rx'} else '')
        + ('nexus_add_tls()\n'
           'nexus_add_coremqtt()\n'
           'add_library(installed_public STATIC application.c private_headers.c)\n'
           'target_link_libraries(installed_public PRIVATE Nexus::TLS Nexus::CoreMQTT)\n'
           'nexus_apply_target_policy(installed_public)\n'
           'nexus_target_warnings(installed_public STRICT)\n'
           + 'nexus_add_firmware(installed_services SOURCES composition.c\n'
           '  LIBRARIES installed_public Nexus::TLSStorage Nexus::CoreMQTTStorage\n'
           '  Nexus::LwIPStorage)\n' if secure else selected_setup if selected_setup else
           'nexus_add_firmware(installed_services SOURCES main.c private_headers.c\n'
           + ('  LIBRARIES ' + libraries[specification.scenario]
              if libraries[specification.scenario] else '') + ')\n')
        + ('target_compile_definitions(installed_services PRIVATE '
           'NX_GRAPHICS_SDRAM_REFERENCE=1)\n'
           if specification.scenario == "f4_graphics_sdram" else '')
        + 'nexus_target_warnings(installed_services STRICT)\n'
        'get_target_property(sdk_root Nexus::Config NEXUS_SOURCE_DIR)\n'
        'get_target_property(config_root Nexus::Platform NEXUS_CONFIG_DIR)\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/resolved-sdk.txt" "${sdk_root}")\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/resolved-config.txt" "${config_root}")\n')
    command = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
               "-DCMAKE_BUILD_TYPE=Release",
               f"-DNexus_DIR={Path(prefix) / 'lib/cmake/Nexus'}",
               "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF",
               f"-DNEXUS_ASSEMBLY_FILE={source / 'assembly.toml'}",
               "-DNEXUS_EXPECTED_SOURCE_REVISION=" + expected_revision]
    if fixture:
        command.append("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON")
    if specification.arm:
        command.append("-DCMAKE_TOOLCHAIN_FILE=" +
                       str(sdk / "cmake/toolchains/arm-gcc.cmake"))
    else:
        command.append("-DNEXUS_TEST_SUPPORT=ON")
    return source, build, command


def prepare_selected_consumer(prefix, directory, name, assembly_name, smoke_name,
                              run_checked, *, expected_revision):
    """Reuse installed canonical inputs through a separate public C project."""
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
    (source / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.31)\n'
        'project(installed_selected_consumer LANGUAGES C ASM)\n'
        'set(CMAKE_C_STANDARD 11)\n'
        'set(CMAKE_C_STANDARD_REQUIRED ON)\n'
        'set(CMAKE_C_EXTENSIONS OFF)\n'
        'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
        f'find_package(Nexus {version} EXACT CONFIG REQUIRED)\n'
        'nexus_add_firmware(installed_selected SOURCES main.c private_headers.c)\n'
        'get_target_property(sdk_root Nexus::Config NEXUS_SOURCE_DIR)\n'
        'get_target_property(config_root Nexus::Platform NEXUS_CONFIG_DIR)\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/resolved-sdk.txt" "${sdk_root}")\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/resolved-config.txt" "${config_root}")\n')
    shutil.copyfile(smoke, source / "main.c")
    (source / "private_headers.c").write_text(
        '#if __has_include("stm32f4xx.h")\n'
        '#error Vendor SDK leaked into product source\n#endif\n'
        '#if __has_include("stm32f407_provider.h") || '
        '__has_include("stm32f407_eth_frames.h") || '
        '__has_include("stm32f407_audio.h") || '
        '__has_include("stm32f407_capture.h") || '
        '__has_include("can_controller.h") || '
        '__has_include("usb_device_controller.h")\n'
        '#error Private provider leaked into product source\n#endif\n'
        '_Static_assert(1, "Public source include isolation");\n')
    command = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
               "-DCMAKE_BUILD_TYPE=Release",
               f"-DNexus_DIR={prefix / 'lib/cmake/Nexus'}",
               "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF",
               f"-DNEXUS_ASSEMBLY_FILE={assembly}",
               "-DNEXUS_EXPECTED_SOURCE_REVISION=" + expected_revision]
    if authored["backend"] != "native":
        command.append(f"-DCMAKE_TOOLCHAIN_FILE={sdk / 'cmake/toolchains/arm-gcc.cmake'}")
    else:
        command.append("-DNEXUS_TEST_SUPPORT=ON")
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
                         specification=None):
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
    if specification is not None:
        domain = selected_domain(specification)
        if ((domain == "native-time") !=
                (resolved.get("cpu", {}).get("arch") == "native")):
            raise ValueError("Selected consumer differs from its reviewed domain")
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
    command = [sys.executable, "-B",
                 str(sdk / "tools/measurement/retention.py"),
                 "--elf", artifacts["elf"]["path"],
                 "--bindings", str(config / "bindings.c"),
                 "--resolved", str(config / "resolved.json"),
                 "--output", str(retention_path)]
    run_checked(command)
    retention = json.loads(retention_path.read_text())
    if (retention["status"] != "passed" or
            retention["elf"] != artifacts["elf"]):
        raise ValueError("Selected consumer provider retention failed")
    return {"artifacts": artifacts, "resources": file_identity(resources_path),
            "resolved": file_identity(config / "resolved.json"),
            "retention": file_identity(retention_path)}


SERVICE_SYMBOLS = frozenset({
    "nx_settings_open", "nx_settings_get", "nx_settings_set",
    "nx_settings_commit", "nx_settings_close", "nx_wait_select",
    "nx_lwip_adapter_start", "nx_lwip_adapter_apply_link",
    "nx_lwip_adapter_service", "nx_lwip_adapter_stop", "lwip_init",
    "tcp_new", "udp_new", "nx_platform_start", "nx_platform_stop",
    "nx_factory_gpio",
})


def secure_service_symbols(specification):
    """Share the authoritative component-retention symbol contract."""
    from tools.measurement.retention import secure_service_symbols as required
    service_profile(specification)
    return required(specification.arm)


def service_symbols(specification):
    """Required linked methods come from the exact immutable scenario entry."""
    service_profile(specification)
    if specification.scenario == "secure_services":
        return secure_service_symbols(specification)
    if specification.scenario == "device_tmp117":
        return frozenset({"nx_tmp117_init", "nx_tmp117_begin", "nx_tmp117_poll",
                          "nx_tmp117_cancel", "nx_tmp117_stop",
                          "nx_tmp117_i2c_transport", "nx_platform_start",
                          "nx_platform_stop", "nx_factory_gpio"})
    if specification.scenario == "image_window":
        return frozenset({"nx_platform_start", "nx_platform_stop", "image_data"})
    if specification.scenario == "loaned_rx":
        return frozenset({"nx_platform_start", "nx_platform_stop", "lwip_init",
                          "nx_factory_gpio", "nx_gpio_port_read",
                          "nx_lwip_loaned_adapter_start",
                          "nx_lwip_adapter_service", "nx_lwip_adapter_stop"})
    if specification.scenario == "mcuboot_control":
        return frozenset({"nx_platform_start", "nx_platform_stop",
                          "nx_mcuboot_query", "nx_mcuboot_request",
                          "boot_swap_type_multi", "boot_set_pending_multi",
                          "flash_area_read", "flash_area_write"})
    if specification.scenario == "mcuboot_boot":
        return frozenset({"nx_platform_start", "nx_platform_stop", "boot_go",
                          "bootutil_img_validate", "bootutil_verify_sig",
                          "flash_area_read", "flash_area_write",
                          "flash_area_erase", "nx_factory_flash", "bootutil_keys",
                          "bootutil_key_cnt"})
    if specification.scenario == "f4_family":
        return frozenset({"nx_platform_start", "nx_platform_stop",
                          "nx_chip_info_read", "nx_factory_crc",
                          "nx_crc_port_compute", "nx_stm32_crc_ops"})
    if specification.scenario == "f4_crypto":
        return frozenset({"nx_platform_start", "nx_platform_stop",
                          "nx_factory_cipher", "nx_factory_hash",
                          "nx_cipher_port_query", "nx_hash_port_query",
                          "nx_stm32f4_cipher_ops", "nx_stm32f4_hash_ops"})
    if specification.scenario == "f4_sdram":
        return frozenset({"nx_platform_start", "nx_platform_stop",
                          "nx_factory_external_bus", "nx_factory_memory_window",
                          "nx_external_bus_port_state", "nx_memory_window_port_acquire",
                          "nx_memory_window_port_release", "nx_memory_window_port_geometry"})
    if specification.scenario == "f4_qspi":
        return frozenset({"nx_platform_start", "nx_platform_stop",
                          "nx_factory_external_bus", "nx_factory_flash_async",
                          "nx_factory_flash_async_flash",
                          "nx_external_bus_port_state", "nx_external_bus_port_stop",
                          "nx_flash_geometry_sector_count",
                          "nx_flash_port_geometry", "nx_flash_port_read",
                          "nx_flash_port_recover", "nx_flash_async_prepare",
                          "nx_flash_async_submit", "nx_flash_async_cancel",
                          "nx_flash_async_service", "nx_flash_async_stop",
                          "nx_stm32_qspi_bus_ops", "nx_stm32_qspi_async_ops",
                          "nx_stm32_qspi_flash_ops", "HAL_QSPI_Init"})
    if specification.scenario in {"f4_graphics", "f4_graphics_sdram"}:
        symbols = {"nx_platform_start", "nx_platform_stop", "nx_factory_image",
                   "nx_image_port_query", "nx_stm32_image_ops"}
        if specification.assembly != "image-only.toml":
            symbols.update({"nx_factory_display", "nx_display_port_query",
                            "nx_display_port_snapshot", "nx_stm32_ltdc_ops"})
        if specification.scenario == "f4_graphics_sdram":
            symbols.update({
                "nx_image_prepare", "nx_image_port_submit", "nx_image_port_service",
                "nx_display_prepare", "nx_display_port_submit",
                "nx_display_port_service", "nx_display_port_stop",
                "nx_request_initialize", "nx_request_state", "nx_request_result",
                "nx_deadline_finite_after", "nx_time_now_us",
                "nx_stm32_sdram_prepare", "nx_stm32_sdram_stop",
                "nx_stm32_sdram_bus_ops", "nx_stm32_sdram_graphics_attach",
                "nx_stm32_sdram_graphics_check", "nx_stm32_sdram_graphics_detach",
                "nx_stm32_sdram_graphics_overlaps",
            })
        return frozenset(symbols)
    if specification.scenario == "f4_sai":
        return frozenset({"nx_platform_start", "nx_platform_stop",
                          "nx_factory_audio", "nx_audio_port_layout",
                          "nx_frame_layout_audio", "nx_stm32_sai_ops",
                          "nx_stm32_sai_prepare", "HAL_SAI_Init", "SAI1_IRQHandler"})
    return SERVICE_SYMBOLS


def service_scope(specification):
    """Describe the executed software boundary without physical claims."""
    if specification.scenario == "f4_family":
        return ("Selected exact F4 internal HSI16/CRC C11 link, "
                "resources and method retention only. No external PCB, "
                "startup execution, measured stack or physical timing.")
    if specification.scenario == "f4_graphics_sdram":
        return ("Selected F479NI public DMA2D to LTDC composition with an "
                "explicit graphics-owned SDRAM endpoint; ARM C11 link, "
                "resources and graphics-memory bridge retention only. "
                "No physical SDRAM, panel, startup execution, bandwidth, "
                "measured stack or physical timing qualification.")
    if specification.scenario in {"f4_crypto", "f4_sdram", "f4_graphics",
                                   "f4_sai", "f4_qspi"}:
        return ("Selected exact F4 public factory C11 link, resources and "
                "method retention only. No physical hardware execution, "
                "memory validation, cryptographic known-answer qualification, "
                "measured stack or physical timing.")
    if specification.scenario == "loaned_rx":
        return ("Selected 2-slot/4-piece RX profile and two 512-byte spare pbufs; "
                "ARM link/resource observation only. No physical MAC/PHY, packet "
                "execution or usable randomness. No measured stack or timing.")
    if specification.scenario == "mcuboot_control":
        return (("Positive official trailer calls on the public nonpersistent "
                 "Native NOR model. " if not specification.arm else
                 "ARM control link; NULL operation prevents persistent access. ")
                + "No physical Flash or image authentication. "
                "No measured stack or timing; no reset or rollback qualification.")
    if specification.scenario == "mcuboot_boot":
        return ("Separate full boot ARM link with an attributed public test key "
                "and one explicit layout. No image authentication execution, "
                "physical Flash, handoff, reset or rollback qualification. "
                "No physical execution. No measured stack or timing.")
    if specification.scenario == "image_window":
        return ("Executable window only; no bootloader, signing, activation "
                "or physical execution. No measured stack or timing.")
    if specification.scenario == "device_tmp117":
        return ("Software I2C endpoint model only; Native runtime or ARM link. "
                "No physical sensor, bus, measured stack or timing.")
    return (("Public CA only; no client identity, authenticated connection "
             "or broker exchange. "
             if specification.scenario == "secure_services" else "")
            + "Canonical SDK scenario and linked implementation only; "
            "no measured stack, timing, physical network or Flash.")


def check_service_compiles(commands, public, sdk, resolved, specification):
    """Audit actual C11/CPU/ABI argv and the narrow public-only API boundary."""
    for entry in public:
        argv = entry.get("arguments") or shlex.split(entry["command"])
        # Public-only callers may not opt into either supplier's storage layout.
        private = tuple(root / "components/mcuboot" / directory
                        for root in (Path(sdk), Path(__file__).resolve().parents[2])
                        for directory in ("port", "profile"))
        private += tuple(root / "ext" for root in
                         (Path(sdk), Path(__file__).resolve().parents[2]))
        if (any(str(sdk / "ext") in argument for argument in argv) or
                private_compile_inputs(entry, sdk, private) or
                any(argument.split("=")[0] == "-DNEXUS_LWIP_LOANED_RX"
                    or (argument == "-D" and index + 1 < len(argv) and
                        argv[index + 1].split("=")[0] == "NEXUS_LWIP_LOANED_RX")
                    for index, argument in enumerate(argv))):
            raise ValueError("Service private inputs leaked into product")
    expected = {"-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16",
                "-mfloat-abi=hard", "-fshort-enums"} if specification.arm else set()
    if (resolved.get("enum_abi") != ("short-enums" if specification.arm
                                    else "native-int")):
        raise ValueError("Service enum ABI differs from its profile")
    if specification.arm and (resolved.get("soc_family") != "stm32f4" or
            resolved.get("cpu", {}).get("arch") != "cortex-m4" or
            resolved["cpu"].get("float_abi") != "hard" or
            resolved["cpu"].get("fpu") != "fpv4-sp-d16"):
        raise ValueError("Service CPU ABI differs from its profile")
    for entry in commands:
        if Path(entry["file"]).suffix in {".cpp", ".cc", ".cxx", ".C"}:
            raise ValueError("Service firmware must remain C11")
        if Path(entry["file"]).suffix != ".c":
            continue
        argv = entry.get("arguments") or shlex.split(entry["command"])
        if [item for item in argv if item.startswith("-std=")] != ["-std=c11"]:
            raise ValueError("Service requires actual C11 translation units")
        options = {item for item in argv if item.startswith((
            "-mcpu=", "-mfpu=", "-mfloat-abi=")) or item in {
                "-mthumb", "-marm", "-fshort-enums", "-fno-short-enums"}}
        if options != expected:
            raise ValueError("Service compiled CPU/enum ABI differs")


def check_service_image(sdk, source, build, specification, run_checked):
    """Check one exact scenario, real runtime or ARM resources, and retention."""
    specification = service_profile(specification)
    if (build / "resolved-sdk.txt").read_text() != str(sdk):
        raise ValueError("Service consumer resolved another source SDK")
    verification = service_directory(sdk, specification)
    canonical = verification / specification.assembly
    canonical_sources = service_sources(sdk, specification)
    secure = specification.scenario == "secure_services"
    split = specification.scenario in {"secure_services", "loaned_rx",
                                       "mcuboot_control"}
    boot = specification.scenario == "mcuboot_boot"
    if ((source / "assembly.toml").read_bytes() != canonical.read_bytes()
            or any((source / name).read_bytes() != path.read_bytes()
                   for name, path in canonical_sources.items())):
        raise ValueError("Service consumer differs from the canonical scenario")
    inputs = {"assembly": source / "assembly.toml",
              "private_probe": source / "private_headers.c",
              "sdk_manifest": sdk / ".nexus-source-sdk.json",
              "compile_commands": build / "compile_commands.json"}
    if specification.scenario != "common_services":
        inputs.update({"consumer/" + name: source / name
                       for name in canonical_sources})
    else:
        inputs["public_source"] = source / "main.c"
    config = Path((build / "resolved-config.txt").read_text())
    if config != build / "nexus-sdk/generated":
        raise ValueError("Service configuration directory is not canonical")
    inputs.update(resolved=config / "resolved.json", bindings=config / "bindings.c")
    resolved = json.loads((config / "resolved.json").read_text())
    if specification.scenario.startswith("f4_"):
        board = next(path for name, path in canonical_sources.items()
                     if name.endswith("/board.json"))
        if resolved.get("part") != json.loads(board.read_text())["soc"]:
            raise ValueError("F4 reference resolved another exact part")
    native = resolved.get("cpu", {}).get("arch") == "native"
    if (native == specification.arm or
            resolved.get("backend") != ("baremetal" if specification.arm
                                         else "native")):
        raise ValueError("Service consumer differs from its reviewed domain")
    commands = json.loads((build / "compile_commands.json").read_text())
    products = ({source / "private_headers.c"} if boot else
                {source / ("application.c" if split else "main.c"),
                 source / "private_headers.c"})
    owned = products | ({source / "composition.c"} if split else set())
    if boot:
        owned.update({source / "main.c", source / "public_key.c"})
    public = [entry for entry in commands
              if Path(entry["file"]).resolve() in products]
    if (len(public) != len(products) or
            {Path(entry["file"]).resolve() for entry in public} != products or
            any(private_compile_inputs(entry, sdk) for entry in public)):
        raise ValueError("Service consumer private inputs leaked into product")
    if split:
        if sum(Path(entry["file"]).resolve() == source / "composition.c"
               for entry in commands) != 1:
            raise ValueError("Service composition has no unique compile row")
    for path in owned:
        if sum(Path(entry["file"]).resolve() == path for entry in commands) != 1:
            raise ValueError("Service owned source has no unique compile row")
    if specification.scenario != "common_services":
        check_service_compiles(commands, public, sdk, resolved, specification)
    for entry in commands:
        file = Path(entry["file"]).resolve()
        if (not file.is_relative_to(sdk) and file not in owned
                and file != config / "bindings.c"):
            raise ValueError("Service compilation consumed an unreviewed source")
        if file in owned or file == config / "bindings.c":
            argv = entry.get("arguments") or shlex.split(entry["command"])
            if argv.count("-c") != 1:
                raise ValueError("Service compile row has no unique -c source")
            selected = Path(argv[argv.index("-c") + 1])
            if not selected.is_absolute():
                selected = Path(entry["directory"]) / selected
            if selected.resolve() != file:
                raise ValueError("Service compile row source and -c disagree")
    if sum(Path(entry["file"]).resolve() == config / "bindings.c"
           for entry in commands) != 1:
        raise ValueError("Service generated bindings have no unique compile row")
    suffix = ".elf" if specification.arm else ""
    executable = build / "bin" / ("installed_services" + suffix)
    with regular_file(executable).open("rb") as stream:
        header = stream.read(20)
    expected = (b"\x7fELF\x01\x01", b"\x28\x00") if specification.arm else (
        b"\x7fELF\x02\x01", b"\x3e\x00")
    if len(header) != 20 or header[:6] != expected[0] or header[18:20] != expected[1]:
        raise ValueError("Service image differs from its reviewed ELF domain")
    inputs["elf"] = executable
    before = {name: file_identity(path) for name, path in inputs.items()}
    runtime = None
    resources_identity = retention_identity = None
    if specification.arm:
        resources_path = build / "bin/installed_services.resources.json"
        resources = json.loads(resources_path.read_text())
        if (resources["status"] != "passed" or resources["violations"] or
                resources["elf"] != before["elf"] or
                resources["resolved"] != before["resolved"]):
            raise ValueError("Service resource/configuration binding failed")
        resources_identity = file_identity(resources_path)
        retention_path = build / "provider-retention.json"
        retention_command = [sys.executable, "-B",
                     str(sdk / "tools/measurement/retention.py"),
                     "--elf", str(executable), "--bindings",
                     str(config / "bindings.c"), "--resolved",
                     str(config / "resolved.json"), "--output", str(retention_path)]
        if secure:
            retention_command += ["--component-owner", "f407-secure-services",
                                  "--consumer", str(source / "application.c")]
        no_provider = specification.scenario in {"image_window", "mcuboot_control"}
        if not no_provider:
            run_checked(retention_command)
        retained = (json.loads(retention_path.read_text())
                    if not no_provider else None)
        if retained is not None and (retained["status"] != "passed" or
                retained["elf"] != before["elf"]):
            raise ValueError("Service provider retention failed")
        if secure and (
                retained.get("kind") != "linked_component_owner_retention" or
                retained.get("owner_profile") != "f407-secure-services" or
                retained.get("consumer") != before["consumer/application.c"]):
            raise ValueError("Secure service component retention identity differs")
        retention_identity = (file_identity(retention_path)
                              if retained is not None else None)
    else:
        runtime = run_checked([str(executable)])
        if runtime.returncode:
            raise ValueError("Service canonical runtime failed")
    observed = run_checked(["arm-none-eabi-nm" if specification.arm else "nm",
                            "-S", "--defined-only", "--format=posix",
                            str(executable)])
    if observed.returncode or len(observed.stdout) > 4 * 1024 * 1024:
        raise ValueError("Service symbol observation failed or exceeded its bound")
    required_symbols = service_symbols(specification)
    forbidden = {"libc_heap": [], "cpp_runtime": []}
    if specification.scenario != "common_services" and specification.arm:
        from tools.measurement.retention import secure_forbidden_symbols
        forbidden = secure_forbidden_symbols({line.split()[0] for line in
            observed.stdout.splitlines() if line.split()})
        if any(forbidden.values()):
            raise ValueError("Service retained libc heap or C++ runtime")
    symbols = {}
    for line in observed.stdout.splitlines():
        fields = line.split()
        if not fields or fields[0] not in required_symbols:
            continue
        if (len(fields) != 4 or fields[0] in symbols or
                re.fullmatch(r"[0-9a-fA-F]+", fields[2]) is None or
                re.fullmatch(r"[0-9a-fA-F]+", fields[3]) is None):
            raise ValueError("Service symbol identity is ambiguous")
        symbols[fields[0]] = {"type": fields[1], "address": int(fields[2], 16),
                              "size": int(fields[3], 16)}
    if set(symbols) != required_symbols or any(not value["size"]
                                            for value in symbols.values()):
        raise ValueError("Service implementation retention is incomplete")
    if before != {name: file_identity(path) for name, path in inputs.items()}:
        raise ValueError("Service observation inputs changed during execution")
    return {"kind": "source-sdk-service-observation", "status": "passed",
            "name": specification.name, "domain": specification.domain,
            "physical_status": "not_executed", "inputs": before,
            "runtime_executed": runtime is not None, "symbols": symbols,
            "resources": resources_identity, "retention": retention_identity,
            **({"forbidden_symbols": forbidden}
               if specification.scenario != "common_services"
               and specification.arm else {}),
            "scope": service_scope(specification)}


def positive_parallel(value):
    """Require a canonical positive job count, without a policy maximum."""
    try:
        return process.positive_integer(value, "parallel")
    except ValueError as error:
        raise argparse.ArgumentTypeError(str(error)) from error


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
    elif kind == "services":
        source, build, command = prepare_service_consumer(
            prefix, output, specification, run_checked,
            expected_revision=identity["source_revision"])
        run_checked(command)
        target = "installed_services"
        run_checked(["cmake", "--build", str(build), "--parallel", "1",
                     "--target", target])
        return check_service_image(sdk, source, build, specification, run_checked)
    elif kind == "selected":
        name, assembly, smoke = specification
        source, build, command = prepare_selected_consumer(
            prefix, output, name, assembly, smoke, run_checked,
            expected_revision=identity["source_revision"])
        run_checked(command)
        run_checked(["cmake", "--build", str(build), "--parallel", "1",
                     "--target", "installed_selected"])
        image = check_selected_image(sdk, source, build, run_checked,
                                     specification=specification)
        image.update({"name": name, "assembly": str(sdk / assembly)})
        return image
    else:
        raise ValueError("Unknown consumer plan domain")
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
    parser.add_argument("--parallel", type=positive_parallel,
                        help="Total concurrent consumer jobs; default inherits "
                             "NEXUS_BUILD_JOBS/CMAKE_BUILD_PARALLEL_LEVEL or 2; "
                             "each child build uses one job")
    args = parser.parse_args(argv)
    try:
        args.parallel = process.parallel_jobs(os.environ, requested=args.parallel)
    except ValueError as error:
        parser.error(str(error))
    prefix, output = args.prefix.absolute(), args.output.absolute()
    records = []
    report = {"schema_version": 1, "kind": "source_sdk_consumer_verification",
              "prefix": str(prefix), "status": "failed",
              "physical_status": "not_executed", "commands": records,
              "selected_images": [], "service_images": [],
              "parallel_jobs": args.parallel}
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
        plan = consumer_plan()
        results = [None] * len(plan)
        local_records = [[] for _ in plan]
        workers = min(args.parallel, len(plan))
        report["consumer_workers"] = workers

        def work(position):
            kind, specification = plan[position]
            name = (specification.name if kind == "services" else
                    specification[0] + " " + specification[1]
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
                    outcome["service_image" if kind == "services" else
                            "selected_image"] = image
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
            report["service_images"] = [item["service_image"] for item in results
                if item is not None and "service_image" in item]
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
