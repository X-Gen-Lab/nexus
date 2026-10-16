#!/usr/bin/env python3
"""Prepare/verify a relocatable source SDK with fixed dependency provenance.

No compiler or effective build configuration is embedded as a reusable binary
ABI. This package is an unsigned source snapshot, not a promoted release.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tempfile

class SDKError(ValueError):
    pass


sys.dont_write_bytecode = True
SOURCE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(SOURCE_ROOT))
try:
    from tools.maintenance.source_catalogue import SOURCE_DEPENDENCIES, declared_modules
    from tools.maintenance import (
        cmsis_device_lock, stm32f1_hal_lock, stm32f4_hal_lock,
        stm32g4_lock, threadx_lock,
    )
    from tools.configure.metadata import load_policy, parse_json, snapshot
    from tools.configure.providers import descriptor as provider_descriptors
except ImportError as error:
    # Missing verifier modules are an incomplete SDK, including after an
    # attacker rehashes the payload map. Never accept a reduced checker graph.
    message = "missing required sources/licenses (source verifier dependencies)"
    if __name__ == "__main__":
        print("Source SDK rejected: " + message, file=sys.stderr)
        raise SystemExit(1) from error
    raise SDKError(message) from error

ROOT_FILES = {"CMakeLists.txt", "LICENSE", "README.md", "AGENTS.md", ".gitmodules",
              ".clang-format", ".editorconfig", ".clang-format-dirs"}
ROOT_DIRS = {"arch", "boards", "cmake", "dependencies", "core", "io", "os",
             "components", "tools", "scripts", "soc"}
REQUIRED_DEPENDENCIES = tuple(item.path for item in SOURCE_DEPENDENCIES)
OFFICIAL_DRIVERS = (stm32f1_hal_lock, stm32f4_hal_lock)
REQUIRED_FILES = (
    "CMakeLists.txt", "LICENSE", ".gitmodules", "dependencies/toolchains.lock.json",
    "dependencies/environment.lock.json", "core/include/nexus/core/request.h",
    "arch/include/nexus/arch/arch.h", "arch/cortex_m/nx_arch_cortex_m.c",
    "arch/cortex_m/private/compiler.h", "arch/native/nx_arch_native.c",
    "arch/include/nexus/arch/atomic.h", "arch/include/nexus/arch/features.h",
    "arch/include/nexus/arch/cache.h", "arch/include/nexus/arch/mpu.h",
    "arch/include/nexus/arch/security.h", "arch/cortex_m/private/mechanisms.h",
    "arch/cortex_m/nx_arch_cache.c", "arch/cortex_m/nx_arch_mpu.c",
    "arch/cortex_m/nx_arch_security.c",
    "arch/include/nexus/arch/cache_control.h",
    "arch/cortex_m/nx_arch_cache_control.c",
    "arch/cortex_m/private/cache_control.h",
    "arch/include/nexus/arch/nonsecure.h",
    "arch/cortex_m/nx_arch_nonsecure.c",
    "arch/cortex_m/private/nonsecure.h",
    "arch/include/nexus/arch/sleep.h", "arch/cortex_m/nx_arch_sleep.c",
    "arch/cortex_m/private/sleep.h",
    "core/include/nexus/core/memory.h",
    "core/include/nexus/core/request_observer.h", "core/src/request_observer.c",
    "io/include/nexus/io/stream_budget.h", "io/src/stream_budget.c",
    "components/owner_observer/CMakeLists.txt",
    "components/owner_observer/include/nexus/components/owner_observer.h",
    "components/owner_observer/src/owner_observer.c",
    "soc/gd32f470/memory_init.c", "soc/gd32f470/private/memory_init.h",
    "core/src/request.c", "io/include/nexus/io/gpio.h",
    "io/include/nexus/io/uart.h", "os/freertos/include/FreeRTOSConfig.h",
    "boards/stm32f407_qiming_v31/board.json",
    "boards/stm32f407ve_sky_qingchun/board.json",
    "boards/gd32f470_liangshan/board.json",
    "soc/stm32f407/soc.json", "soc/gd32f470/soc.json",
    "tools/configure/configure.py", "cmake/platform/Firmware.cmake",
    "cmake/platform/TargetPolicy.cmake",
    "tools/configure/bundle.py", "tools/configure/dependencies.py",
    "tools/configure/contracts.py", "tools/configure/matrix.py",
    "tools/configure/sdk.py",
    "tools/configure/assemblies/catalogue.json",
    "tools/dev/dev.py", "tools/dev/presets.py", "tools/dev/process.py",
    "tools/dev/change_paths.py",
    "tools/dev/cache.py", "tools/dev/dependencies.py",
    "tools/dev/qualification.py", "tools/dev/provenance.py",
    "tools/evidence/store.py",
    "tools/evidence/environment.py", "tools/evidence/recipe.py",
    "tools/configure/authored.py", "tools/configure/ir.py",
    "tools/configure/cpu.py", "tools/configure/runtime.py",
    "tools/configure/kernel.py", "tools/configure/metadata.py",
    "tools/configure/memory.py", "tools/configure/can.py", "dependencies/platform-compatibility.json",
    "os/freertos/tick.c", "os/freertos/include/nexus/os/tick.h",
    "os/CMakeLists.txt", "os/diagnostic.c",
    "os/include/nexus/os/diagnostic.h", "os/freertos/CMakeLists.txt",
    "os/freertos/freertos.c", "os/freertos/hooks.c",
    "os/freertos/include/nexus/os/freertos.h",
    "os/freertos/lowpower.c", "os/freertos/diagnostics.c",
    "os/freertos/include/nexus/os/lowpower.h",
    "os/freertos/include/nexus/os/freertos_diagnostics.h",
    "os/freertos/include/nexus/os/freertos_mpu.h",
    "os/freertos/include/nexus/os/freertos_user.h",
    "os/freertos/include/nexus/os/secure_context.h",
    "os/freertos/mpu/prepare_port.py", "os/freertos/mpu/private.h",
    "os/freertos/mpu/task.c", "os/freertos/mpu/layout.c",
    "os/freertos/mpu/syscalls.c",
    "os/freertos/mpu/guard.c", "os/freertos/mpu/guard_arm.c",
    "os/freertos/mpu/guard.h",
    "os/freertos/secure/Integration.cmake",
    "os/freertos/secure/CMakeLists.txt",
    "os/freertos/secure/config/FreeRTOSConfig.h",
    "os/freertos/secure/private/hardware.h",
    "os/freertos/secure/context.c", "os/freertos/secure/init.c",
    "cmake/platform/Runtime.cmake", "os/freertos/prepare_m7_integer.py",
    "tools/configure/providers/__init__.py",
    "tools/configure/providers/common.py",
    "tools/configure/providers/native/__init__.py",
    "tools/configure/providers/native/bindings.py",
    "tools/configure/providers/native/emission.py",
    "tools/configure/providers/stm32f407/__init__.py",
    "tools/configure/providers/stm32f407/bindings.py",
    "tools/configure/providers/stm32f407/emission.py",
    "tools/configure/providers/gd32f470/__init__.py",
    "tools/configure/providers/gd32f470/bindings.py",
    "tools/configure/providers/gd32f470/emission.py",
    "tools/configure/factory.py", "tools/configure/assemblies/native.toml",
    "tools/configure/assemblies/qiming-baremetal-uart-dma.toml",
    "tools/configure/assemblies/sky-baremetal.toml",
    "tools/configure/assemblies/liangshan-baremetal.toml",
    "tools/measurement/workload.c",
    "ext/freertos/tasks.c", "ext/freertos/queue.c", "ext/freertos/list.c",
    "ext/freertos/LICENSE.md",
    "vendors/arm/CMSIS_5/CMSIS/Core/Include/core_cm4.h", "vendors/arm/CMSIS_5/LICENSE.txt",
    "vendors/st/cmsis_device_f4/Source/Templates/gcc/startup_stm32f407xx.s",
    "vendors/st/cmsis_device_f4/LICENSE.md",
    "vendors/gigadevice/gd32f4xx/Firmware/CMSIS/GD/GD32F4xx/Source/GCC/startup_gd32f450_470.S",
    "vendors/gigadevice/gd32f4xx/source.lock.json",
    "cmake/package/NexusConfig.cmake.in", "cmake/package/NexusConfigVersion.cmake.in",
    "cmake/package/package_source_sdk.py")
MANIFEST = ".nexus-source-sdk.json"
# Required payload identity is separate from CMake's production source graph.
# Every exposed kernel profile must survive a rehashed omission attempt.
for _port in ("ARM_CM0", "ARM_CM3", "ARM_CM4F", "ARM_CM7/r0p1",
              "ARM_CM3_MPU", "ARM_CM4_MPU",
              "ARM_CM23_NTZ/non_secure", "ARM_CM33_NTZ/non_secure",
              "ARM_CM55_NTZ/non_secure", "ARM_CM85_NTZ/non_secure",
              "ARM_CM23/non_secure", "ARM_CM33/non_secure",
              "ARM_CM55/non_secure", "ARM_CM85/non_secure"):
    REQUIRED_FILES += tuple(f"ext/freertos/portable/GCC/{_port}/{name}"
                            for name in ("port.c", "portmacro.h"))
    if _port in {"ARM_CM0", "ARM_CM23_NTZ/non_secure",
                  "ARM_CM33_NTZ/non_secure", "ARM_CM55_NTZ/non_secure",
                  "ARM_CM85_NTZ/non_secure", "ARM_CM23/non_secure",
                  "ARM_CM33/non_secure", "ARM_CM55/non_secure",
                  "ARM_CM85/non_secure"}:
        REQUIRED_FILES += tuple(f"ext/freertos/portable/GCC/{_port}/{name}"
                                for name in ("portasm.c", "portasm.h"))
    if _port.endswith("/non_secure"):
        REQUIRED_FILES += (f"ext/freertos/portable/GCC/{_port}/portmacrocommon.h",)
    if _port in {"ARM_CM3_MPU", "ARM_CM4_MPU", "ARM_CM23_NTZ/non_secure",
                  "ARM_CM33_NTZ/non_secure", "ARM_CM55_NTZ/non_secure",
                  "ARM_CM85_NTZ/non_secure"}:
        REQUIRED_FILES += (f"ext/freertos/portable/GCC/{_port}/mpu_wrappers_v2_asm.c",)
for _core in (23, 33, 55, 85):
    REQUIRED_FILES += tuple(
        f"ext/freertos/portable/GCC/ARM_CM{_core}/secure/{name}"
        for name in ("secure_context_port.c", "secure_context.h",
                     "secure_init.h", "secure_port_macros.h"))
REQUIRED_FILES += tuple(f"ext/freertos/include/{name}" for name in (
    "FreeRTOS.h", "task.h", "queue.h", "list.h", "semphr.h", "portable.h",
    "projdefs.h", "stack_macros.h", "mpu_prototypes.h", "mpu_wrappers.h",
    "mpu_syscall_numbers.h", "deprecated_definitions.h", "timers.h",
    "event_groups.h", "stream_buffer.h"))
REQUIRED_FILES += (
    "dependencies/threadx.lock.json", "tools/maintenance/threadx_lock.py",
    "tools/maintenance/source_catalogue.py", "tools/configure/threadx.py",
    "tools/configure/lowpower.py", "os/threadx/CMakeLists.txt",
    "os/threadx/Kernel.cmake", "os/threadx/threadx.c", "os/threadx/m4.c",
    "os/threadx/m4_vectors.S", "os/threadx/prepare_linux.py",
    "os/threadx/config/tx_user.h", "os/threadx/include/nexus/os/threadx.h")
# The reviewed lock remains the one ThreadX graph authority. Rehashed SDK
# omissions cannot remove required kernel payloads from a different file map.
_threadx_lock = parse_json((SOURCE_ROOT / "dependencies/threadx.lock.json").read_bytes())
threadx_lock.contract(_threadx_lock)
REQUIRED_FILES += tuple("ext/threadx/" + name for name in _threadx_lock["files"])
REQUIRED_FILES += ("ext/threadx/" + _threadx_lock["license"]["path"],)
REQUIRED_FILES += ("dependencies/stm32g4.lock.json",
                   "tools/maintenance/stm32g4_lock.py")
_g4_lock = parse_json((SOURCE_ROOT / "dependencies/stm32g4.lock.json").read_bytes())
stm32g4_lock.contract(_g4_lock)
REQUIRED_FILES += tuple("vendors/st/cmsis_device_g4/" + name
                        for name in _g4_lock["files"])
REQUIRED_FILES += ("cmake/platform/STM32Official.cmake",)
for _official in OFFICIAL_DRIVERS:
    REQUIRED_FILES += (_official.LOCK_NAME,
                       "tools/maintenance/" + _official.__name__.split(".")[-1] + ".py")
    _official_lock = parse_json((SOURCE_ROOT / _official.LOCK_NAME).read_bytes())
    _official.contract(_official_lock)
    REQUIRED_FILES += tuple(_official.DIRECTORY + "/" + name
                            for name in _official_lock["files"])
# Complete declared platform surfaces must remain present after an attacker
# removes a payload and rehashes its file map. This is a required-source identity
# contract, not another build graph: CMake still owns every selected target.
# Explicit names make omissions fail closed; scanning an installed SDK would
# derive a reduced requirement from precisely the missing files being checked.
REQUIRED_FILES += (
    "core/CMakeLists.txt", "core/src/time.c", "core/src/fault.c",
    "core/src/stack_observation.c", "io/CMakeLists.txt",
    "tools/configure/bindings.py", "tools/configure/emission.py",
    "tools/configure/usb_device.py", "tools/configure/ethernet.py",
    "tools/configure/timer_advanced.py", "tools/configure/assembly.schema.json",
    "tools/configure/secondary_controllers.py",
    "cmake/platform/Configure.cmake", "cmake/platform/ControllerModes.cmake",
    "cmake/platform/Options.cmake", "cmake/platform/SDK.cmake",
    "scripts/ci/vendor_import_identity.py", "scripts/ci/verify_vendor_source.py",
    "tools/maintenance/dependency_proposals.py",
    "os/freertos/timebase_lowpower.c",
    "os/freertos/private/timebase_lowpower.h", "os/freertos/stack_snapshot.c",
    "os/freertos/include/nexus/os/freertos_stack.h",
    "os/threadx/private/irq_model.h", "os/threadx/private/m4_model.h",
    "soc/common/i2c_v1_async.c", "soc/common/i2c_v1_async.h")
REQUIRED_FILES += tuple("core/include/nexus/core/" + name + ".h" for name in (
    "fault", "memory", "request", "request_observer", "stack_observation",
    "status", "time"))
REQUIRED_FILES += tuple("io/include/nexus/io/" + name + ".h" for name in (
    "adc", "adc_observer", "advanced_pwm", "can", "dma", "ethernet",
    "exti", "flash", "gpio", "i2c", "i2c_bus_clear", "mdio", "native/model",
    "spi", "stream", "stream_budget", "timer", "timer_capture", "uart",
    "uart_rs485", "usb_device", "wake", "watchdog"))
REQUIRED_FILES += tuple("io/src/" + name + ".c" for name in (
    "adc", "adc_observer", "advanced_pwm", "can", "can_controller", "dma",
    "eth_mac", "ethernet", "exti", "flash", "flash_region", "gpio", "i2c",
    "i2c_bus_clear", "mdio", "spi", "stream", "stream_budget", "timer",
    "timer_capture", "uart", "uart_rs485", "usb_device",
    "usb_device_controller", "wake", "watchdog"))
REQUIRED_FILES += tuple("io/private/" + name + ".h" for name in (
    "advanced_pwm_impl", "can_controller", "eth_mac",
    "timer_advanced_controller", "timer_capture_impl", "usb_device_controller"))
REQUIRED_FILES += tuple("io/native/" + name + ".c" for name in (
    "adc", "can", "ethernet", "exti", "flash", "gpio", "i2c", "spi",
    "spi_async", "stream", "time", "timer", "uart", "usb_device", "watchdog"))
REQUIRED_FILES += tuple("io/native/" + name + ".h" for name in (
    "can", "can_access", "ethernet", "ethernet_access", "provider",
    "usb_device", "usb_device_access"))
for _family in ("stm32f407", "gd32f470"):
    REQUIRED_FILES += tuple("soc/" + _family + "/" + name for name in (
        "CMakeLists.txt", "system.c", "sleep.c", "timebase_lowpower.c",
        "usb_clock.c", "soc.json", "routes.json"))
    REQUIRED_FILES += tuple("soc/" + _family + "/drivers/" + name + ".c"
                            for name in (
        "adc", "adc_continuous", "adc_stream", "advanced_pwm", "can",
        "ethernet", "exti", "flash", "gpio", "i2c", "i2c_async", "spi",
        "spi_dma", "timer", "timer_capture", "uart", "uart_dma",
        "uart_receive", "uart_stream", "usb_device", "watchdog"))
    REQUIRED_FILES += tuple("soc/" + _family + "/private/" + _family + "_"
                            + name + ".h" for name in (
        "can", "dma", "eth", "i2c_async", "lowpower", "provider", "sleep",
        "timer_advanced", "uart_receive", "usb_clock", "usb_device"))
REQUIRED_FILES += (
    "tools/configure/fact_references.py", "tools/configure/preflight.py",
    "tools/configure/gpio_phase.py",
    "soc/stm32f407/references.json", "soc/gd32f470/references.json",
    "soc/stm32f407/private/stm32f407_system.h",
    "soc/stm32f407/private/stm32f407_stream.h",
    "soc/gd32f470/timebase.c", "soc/gd32f470/private/system.h",
    "soc/gd32f470/private/gd32f470_adc_stream.h",
    "soc/gd32f470/private/gd32f470_spi_dma.h",
    "soc/gd32f470/private/gd32f470_uart_stream.h",
    "soc/gd32f470/private/gd32f4xx_libopt.h", "soc/gd32f470/private/usb_conf.h")
for _component, _include, _private in (
        ("clock_domain", "nexus/components/clock_domain.h", "clock_domain_impl.h"),
        ("user_io_broker", "nexus/components/user_io_broker.h", "user_io_broker_impl.h"),
        ("dp83848", "nexus/dp83848/dp83848.h", None)):
    REQUIRED_FILES += tuple("components/" + _component + "/" + name for name in (
        "CMakeLists.txt", "src/" + _component + ".c", "include/" + _include))
    if _private is not None:
        REQUIRED_FILES += ("components/" + _component + "/private/" + _private,)
# G474 owned support joins only after the actual generated factory and four
# bare-metal/FreeRTOS ARM links passed. The official vendor closure stays five
# locked files; no additional MCU headers or unreviewed SDK unit is selected.
REQUIRED_FILES += tuple("soc/stm32g474/" + name for name in (
    "CMakeLists.txt", "system.c", "drivers/gpio.c", "drivers/uart.c",
    "drivers/exti.c",
    "private/stm32g474_system.h", "private/stm32g474_provider.h",
    "soc.json", "routes.json"))
REQUIRED_FILES += ("boards/stm32g474re_hse24_reference/board.json",)
REQUIRED_FILES += tuple("tools/configure/providers/stm32g474/" + name for name in (
    "__init__.py", "bindings.py", "emission.py"))
# The standalone Secure mechanism includes its complete private hardware port.
REQUIRED_FILES += tuple("soc/stm32u575/" + name for name in (
    "security.c", "private/stm32u575_security.h",
    "private/stm32u575_security_hw.h"))
# Public measurement references are part of the supported SDK input surface.
# A rehashed omission must not turn a complete SDK into a reduced profile set.
REQUIRED_FILES += tuple(
    f"tools/configure/assemblies/nucleo-{chip}-{backend}-{mode}.toml"
    for chip in ("h743", "u575")
    for backend in ("baremetal", "freertos")
    for mode in ("empty", "uart"))
# These shared inputs are opened by the default target graph even when no
# optional device is selected. Keep its exported components and actual resource
# observers complete as well; a rehashed map alone cannot prove their presence.
REQUIRED_FILES += (
    "arch/CMakeLists.txt", "cmake/toolchains/arm-gcc.cmake",
    "cmake/package/verify_consumers.py", "components/CMakeLists.txt",
    "os/wait.c", "os/baremetal/notify.c", "os/native/native.c",
    "os/include/nexus/os/wait.h", "os/include/nexus/os/baremetal.h",
    "os/native/include/nexus/os/native.h",
    "tools/measurement/resources.py", "tools/measurement/retention.py",
    "tools/measurement/elf.py", "tools/evidence/common.py",
    "tools/evidence/identity.py")
REQUIRED_FILES += tuple("components/bus_owner/src/" + name + ".c" for name in (
    "bus_owner", "spi_owner", "spi_async_owner", "uart_owner"))
REQUIRED_FILES += tuple("components/bus_owner/include/nexus/components/" + name
                        + ".h" for name in (
    "bus_owner", "spi_owner", "spi_async_owner", "uart_owner"))
REQUIRED_FILES += (
    "components/bmp280/src/bmp280.c", "components/bmp280/src/spi_transport.c",
    "components/bmp280/include/nexus/components/bmp280.h",
    "components/bmp280/include/nexus/components/bmp280_spi.h",
    "components/log/src/log.c", "components/log/include/nexus/components/log.h",
    "components/modbus/src/modbus_rtu.c",
    "components/modbus/include/nexus/components/modbus_rtu.h")
REQUIRED_FILES += tuple("components/storage/src/" + name + ".c" for name in (
    "storage", "flash_storage", "file_flash"))
REQUIRED_FILES += tuple("components/storage/include/nexus/components/" + name
                        + ".h" for name in (
    "storage", "flash_storage", "file_flash"))
REQUIRED_FILES += ("tools/maintenance/cmsis_device_lock.py",)
for _review in cmsis_device_lock.REVIEWED_DEVICES.values():
    REQUIRED_FILES += (_review["lock_name"],)
    REQUIRED_FILES += tuple(_review["directory"] + "/" + name
                            for name in sorted(_review["files"]))
REQUIRED_FILES = tuple(dict.fromkeys(REQUIRED_FILES))
for _descriptor in provider_descriptors.families():
    REQUIRED_FILES += (_descriptor.configuration_modules + _descriptor.fact_files
                       + _descriptor.owned_payload)
REQUIRED_FILES += ("tools/configure/providers/descriptor.py",
                   "tools/configure/providers/hardware.py",
                   "tools/configure/clocks.py", "tools/configure/physical_facts.py")
REQUIRED_FILES = tuple(dict.fromkeys(REQUIRED_FILES))
HEX40 = re.compile(r"[0-9a-f]{40}")
HEX64 = re.compile(r"[0-9a-f]{64}")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def object_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise SDKError("Duplicate identity key")
        result[key] = value
    return result


def safe_relative(name):
    if not isinstance(name, str):
        raise SDKError("Source paths must be strings")
    path = PurePosixPath(name)
    if (not name or "\\" in name or path.is_absolute() or ".." in path.parts
            or path.as_posix() != name or name == "."):
        raise SDKError("Unsafe source SDK path")
    return path


def regular(root, name):
    rel = safe_relative(name)
    path = root / Path(*rel.parts)
    for index in range(1, len(rel.parts) + 1):
        if (root / Path(*rel.parts[:index])).is_symlink():
            raise SDKError(f"Source SDK cannot follow symlinks: {name}")
    if not path.is_file():
        raise SDKError(f"Missing source SDK file: {name}")
    return path


def filesystem_identity(path):
    value = path.stat()
    return (value.st_dev, value.st_ino, value.st_mode, value.st_size,
            value.st_mtime_ns, value.st_ctime_ns)


def stable_file(root, name):
    """Bind content to one unchanged regular file throughout verification."""
    path = regular(root, name)

    def filesystem():
        return filesystem_identity(regular(root, name))

    before = filesystem()
    data = path.read_bytes()
    after = filesystem()
    if before != after or len(data) != after[3]:
        raise SDKError("Source SDK input changed during read: " + name)
    return data, (digest(data), after)


def git(root, *args):
    result = subprocess.run(["git", "-C", str(root), *args], capture_output=True)
    if result.returncode:
        raise SDKError("A complete Git checkout is required: " + result.stderr.decode(errors="replace").strip())
    return result.stdout


def tree_entries(root, revision="HEAD"):
    entries = {}
    for row in git(root, "ls-tree", "-r", "-z", revision).split(b"\0"):
        if not row:
            continue
        meta, name = row.split(b"\t", 1)
        mode, kind, oid = meta.decode().split()
        entries[name.decode()] = (mode, kind, oid)
    return entries


def dependency_snapshot(root, development_fixture=False):
    """Bind only explicit development dependencies to the real stage-0 index.

    A staged new or updated Gitlink is part of the nonpublishable worktree
    overlay. write-tree rejects conflicts and never stages or changes entries.
    Published sources continue to require the exact committed HEAD graph.
    """
    index = (git(root, "write-tree").decode().strip()
             if development_fixture else None)
    return tree_entries(root, index if index is not None else "HEAD"), index


def verify_preparation_snapshot(root, revision, index):
    """Reject source or staged dependency changes across package preparation."""
    if git(root, "rev-parse", "HEAD").decode().strip() != revision:
        raise SDKError("Source commit changed while preparing the package")
    if index is not None and git(root, "write-tree").decode().strip() != index:
        raise SDKError("Source index changed while preparing the development fixture")


def dependency_entries(root, dependency):
    """Redistribute only the locked source closure for reviewed imports."""
    entries = tree_entries(root / dependency)
    if dependency == "ext/threadx":
        raw, _ = snapshot(root / "dependencies/threadx.lock.json")
        lock = parse_json(raw)
        names = threadx_lock.contract(lock) | {lock["license"]["path"]}
        if not names.issubset(entries):
            raise SDKError("ThreadX committed source graph is incomplete")
        entries = {name: entries[name] for name in names}
    elif dependency == "vendors/st/cmsis_device_g4":
        raw, _ = snapshot(root / "dependencies/stm32g4.lock.json")
        names = stm32g4_lock.contract(parse_json(raw))
        if not names.issubset(entries):
            raise SDKError("G4 committed source graph is incomplete")
        entries = {name: entries[name] for name in names}
    elif dependency in {official.DIRECTORY for official in OFFICIAL_DRIVERS}:
        official = next(official for official in OFFICIAL_DRIVERS
                        if official.DIRECTORY == dependency)
        raw, _ = snapshot(root / official.LOCK_NAME)
        names = official.contract(parse_json(raw))
        if not names.issubset(entries):
            raise SDKError("Official committed source graph is incomplete: "
                           + dependency)
        entries = {name: entries[name] for name in names}
    else:
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            if dependency != review["directory"]:
                continue
            raw, _ = snapshot(root / review["lock_name"])
            lock = parse_json(raw)
            names = cmsis_device_lock.contract(lock)
            if lock["selected_part"] != part or not names.issubset(entries):
                raise SDKError("Reviewed device committed source graph is incomplete")
            entries = {name: entries[name] for name in names}
            break
    return entries


def selected(name):
    # Historical audit archives belong to the repository, not the source SDK.
    if name.startswith("tools/testing/evidence/"):
        return False
    # Host test frameworks never become firmware source dependencies. Their
    # reviewed lock stays in the SDK provenance, without host-only payloads.
    if name.startswith("dependencies/source/"):
        return False
    return (name in ROOT_FILES or name.split("/", 1)[0] in ROOT_DIRS
            or name.startswith("vendors/gigadevice/gd32f4xx/"))


def snapshot_files(source, entries, development_fixture=False):
    """Select committed sources, or an explicit nonpublishable worktree view.

    Staged additions are tracked by the index but absent from HEAD and from
    Git's untracked list. Include both index and untracked paths only in the
    development overlay; publishable payloads retain committed-blob identity.
    """
    names = {name for name, (_, kind, _) in entries.items()
             if kind == "blob" and selected(name)}
    if development_fixture:
        names.update(name.decode() for name in git(
            source, "ls-files", "--cached", "--others", "--exclude-standard",
            "-z").split(b"\0") if name and selected(name.decode()))
    return names


def dependency_records(root, entries):
    declared_modules(regular(root, ".gitmodules").read_bytes())
    records = []
    for dep in REQUIRED_DEPENDENCIES:
        row = entries.get(dep)
        if not row or row[:2] != ("160000", "commit"):
            raise SDKError(f"Source commit has no required dependency Gitlink: {dep}")
        checkout = root / dep
        actual = git(checkout, "rev-parse", "HEAD").decode().strip()
        if actual != row[2] or not (checkout / ".git").exists():
            raise SDKError(f"Initialize the pinned dependency before packaging: {dep}")
        output = git(root, "submodule", "status", "--recursive", "--", dep).decode()
        for line in output.splitlines():
            match = re.fullmatch(r" ([0-9a-f]{40}) (.+?)(?: \([^\n]+\))?", line)
            if not match:
                raise SDKError("Required recursive dependencies must be initialized at their recorded commits")
            path, commit = match[2], match[1]
            checkout = root / safe_relative(path)
            if git(checkout, "status", "--porcelain", "--untracked-files=normal").strip():
                raise SDKError(f"Dependency checkout is dirty: {path}")
            records.append({"path": path, "commit": commit,
                            "tree": git(checkout, "rev-parse", "HEAD^{tree}").decode().strip()})
    if len({record["path"] for record in records}) != len(records):
        raise SDKError("Duplicate dependency identity")
    return records


def committed_blobs(root, entries):
    """Read Git blobs without checkout EOL/export transformations."""
    names = sorted(entries)
    request = "".join(entries[name][2] + "\n" for name in names).encode()
    result = subprocess.run(["git", "-C", str(root), "cat-file", "--batch"],
                            input=request, capture_output=True)
    if result.returncode:
        raise SDKError("Cannot read the committed source objects")
    cursor = 0
    blobs = {}
    for name in names:
        end = result.stdout.find(b"\n", cursor)
        header = result.stdout[cursor:end].decode().split()
        if len(header) != 3 or header[0] != entries[name][2] or header[1] != "blob":
            raise SDKError("Source object identity is not a regular Git blob")
        size = int(header[2])
        cursor = end + 1
        data = result.stdout[cursor:cursor + size]
        cursor += size + 1
        if len(data) != size or hashlib.sha1(b"blob " + str(size).encode() + b"\0" + data).hexdigest() != entries[name][2]:
            raise SDKError("Committed source blob identity changed")
        blobs[name] = data
    if cursor != len(result.stdout):
        raise SDKError("Unexpected committed source object data")
    return blobs


def copy_file(source, output, name, data=None, executable=None):
    file = regular(source, name)
    if data is None:
        data = file.read_bytes()
    destination = output / safe_relative(name)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)
    destination.chmod(0o755 if (os.access(file, os.X_OK) if executable is None else executable) else 0o644)
    return digest(data)


def prepare(source, output, development_fixture=False):
    source = source.resolve()
    output = output.absolute()
    if output != output.resolve():
        raise SDKError("Source SDK output requires a canonical path without symlink or parent-directory aliases")
    if output.exists() or output.is_symlink():
        raise SDKError("Source SDK output must not already exist")
    if output.is_relative_to(source):
        raise SDKError("Prepare the relocatable source SDK outside its input checkout")
    if Path(git(source, "rev-parse", "--show-toplevel").decode().strip()).resolve() != source:
        raise SDKError("The source must be the actual Git repository root")
    revision = git(source, "rev-parse", "HEAD").decode().strip()
    source_tree = git(source, "rev-parse", "HEAD^{tree}").decode().strip()
    dirty = bool(git(source, "status", "--porcelain", "--untracked-files=normal").strip())
    if dirty and not development_fixture:
        raise SDKError("Publishable source SDK preparation rejects a dirty source checkout")
    entries = tree_entries(source)
    dependency_graph, index = dependency_snapshot(source, development_fixture)
    dependencies = dependency_records(source, dependency_graph)
    threadx_lock.verify(source / "ext/threadx", source / "dependencies/threadx.lock.json", "linux")
    stm32g4_lock.verify(source / "vendors/st/cmsis_device_g4",
                       source / "dependencies/stm32g4.lock.json")
    for official in OFFICIAL_DRIVERS:
        official.verify(source / official.DIRECTORY,
                        source / official.LOCK_NAME,
                        source / official.CMSIS_DIRECTORY)
    for review in cmsis_device_lock.REVIEWED_DEVICES.values():
        cmsis_device_lock.verify(source / review["directory"],
                                 source / review["lock_name"])
    sys.path.insert(0, str(source / "scripts/ci"))
    from vendor_import_identity import reviewed_import
    imports = [reviewed_import(source / "vendors/gigadevice/gd32f4xx", source)]
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".nexus-source-sdk-", dir=output.parent))
    sdk = temporary / "share/nexus/src"
    sdk.mkdir(parents=True)
    files = {}
    try:
        names = snapshot_files(source, entries, development_fixture)
        source_blobs = {} if development_fixture else committed_blobs(source, {name: entries[name] for name in names})
        for name in sorted(names):
            entry = entries.get(name)
            if not (source / name).exists() and development_fixture:
                continue
            if entry and entry[0] not in ("100644", "100755"):
                raise SDKError("Source SDK requires regular Git blobs")
            files[name] = copy_file(source, sdk, name, source_blobs.get(name),
                                    entry[0] == "100755" if entry else None)
        for record in dependencies:
            dep = record["path"]
            dep_entries = dependency_entries(source, dep)
            blobs = committed_blobs(source / dep, {name: row for name, row in dep_entries.items() if row[1] == "blob"})
            for name, (mode, kind, blob) in dep_entries.items():
                if kind == "commit":
                    continue  # Copied from its own separately verified record.
                if mode not in ("100644", "100755"):
                    raise SDKError("Dependencies require regular tracked files")
                relative = dep + "/" + name
                files[relative] = copy_file(source, sdk, relative, blobs[name], mode == "100755")
        for name in REQUIRED_FILES:
            if name not in files:
                raise SDKError(f"Source SDK is incomplete: {name}")
        verify_preparation_snapshot(source, revision, index)
        manifest = {"schema": 1, "kind": "nexus-installed-source-sdk",
                    "source_revision": revision, "source_tree": source_tree,
                    "publishable": not development_fixture, "source_dirty": dirty,
                    "files_sha256": files, "dependencies": dependencies,
                    "reviewed_imports": imports,
                    "toolchains_lock_sha256": files["dependencies/toolchains.lock.json"],
                    "snapshot_sha256": digest(json.dumps(files, sort_keys=True, separators=(",", ":")).encode())}
        semantic_version = load_policy(sdk)["semantic_version"]
        config = (sdk / "cmake/package/NexusConfig.cmake.in").read_text().replace("@NEXUS_SDK_VERSION@", semantic_version)
        package = temporary / "lib/cmake/Nexus"
        package.mkdir(parents=True)
        (package / "NexusConfig.cmake").write_text(config)
        version = (sdk / "cmake/package/NexusConfigVersion.cmake.in").read_text().replace("@NEXUS_SDK_VERSION@", semantic_version)
        (package / "NexusConfigVersion.cmake").write_text(version)
        manifest["package_files_sha256"] = {
            "lib/cmake/Nexus/NexusConfig.cmake": digest((package / "NexusConfig.cmake").read_bytes()),
            "lib/cmake/Nexus/NexusConfigVersion.cmake": digest((package / "NexusConfigVersion.cmake").read_bytes())}
        (sdk / MANIFEST).write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        verify(sdk)
        verify_preparation_snapshot(source, revision, index)
        if dependency_records(source, dependency_graph) != dependencies:
            raise SDKError("Source dependencies changed while preparing the package")
        temporary.rename(output)
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        raise
    return manifest


def verify(root):
    root = root.absolute()
    if root.is_symlink() or not root.is_dir() or root.resolve() != root:
        raise SDKError("Installed source SDK root must be a regular directory")
    directories = {root, *(path for path in root.rglob("*") if path.is_dir())}
    directory_identity = {path: filesystem_identity(path) for path in directories}
    raw_identity, first_identity = stable_file(root, MANIFEST)
    checked = {(root, MANIFEST): first_identity}
    identity = json.loads(raw_identity, object_pairs_hook=object_pairs)
    fields = {"schema", "kind", "source_revision", "source_tree", "publishable", "source_dirty",
              "files_sha256", "dependencies", "reviewed_imports", "toolchains_lock_sha256", "snapshot_sha256",
              "package_files_sha256"}
    if set(identity) != fields or identity["schema"] != 1 or identity["kind"] != "nexus-installed-source-sdk":
        raise SDKError("Unsupported installed source SDK identity")
    if any(not isinstance(identity[key], str) or not HEX40.fullmatch(identity[key]) for key in ("source_revision", "source_tree")):
        raise SDKError("Invalid source Git identity")
    if any(type(identity[key]) is not bool for key in ("publishable", "source_dirty")):
        raise SDKError("Invalid source SDK provenance flags")
    if identity["publishable"] and identity["source_dirty"]:
        raise SDKError("A dirty source SDK cannot be publishable")
    files = identity["files_sha256"]
    if not isinstance(files, dict) or not files:
        raise SDKError("Source SDK requires a complete file identity")
    for name, expected in files.items():
        if not isinstance(expected, str) or not HEX64.fullmatch(expected):
            raise SDKError("Invalid source SDK file digest")
        _, actual_file = stable_file(root, name)
        checked[root, name] = actual_file
        if actual_file[0] != expected:
            raise SDKError(f"Source SDK file identity changed: {name}")
    actual = {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file() or path.is_symlink()}
    if actual != set(files) | {MANIFEST}:
        raise SDKError("Installed source SDK has missing or additional files")
    if any(name not in files for name in REQUIRED_FILES):
        raise SDKError("Installed source SDK is missing required sources/licenses")
    if identity["snapshot_sha256"] != digest(json.dumps(files, sort_keys=True, separators=(",", ":")).encode()):
        raise SDKError("Source snapshot file-map identity changed")
    if identity["toolchains_lock_sha256"] != files["dependencies/toolchains.lock.json"]:
        raise SDKError("Toolchain lock identity differs from packaged source")
    if root.parts[-3:] != ("share", "nexus", "src"):
        raise SDKError("Relocate the complete installed SDK prefix, not its source subtree")
    prefix = root.parents[2]
    directory_identity.update((path, filesystem_identity(path)) for path in (
        prefix, prefix / "share", prefix / "share/nexus", prefix / "lib",
        prefix / "lib/cmake", prefix / "lib/cmake/Nexus"))
    package_files = identity["package_files_sha256"]
    expected_exports = {"lib/cmake/Nexus/NexusConfig.cmake", "lib/cmake/Nexus/NexusConfigVersion.cmake"}
    if not isinstance(package_files, dict) or set(package_files) != expected_exports:
        raise SDKError("Source SDK requires its exported CMake package identity")
    for name, expected in package_files.items():
        if not isinstance(expected, str) or not HEX64.fullmatch(expected):
            raise SDKError("Exported CMake package identity changed")
        _, exported = stable_file(prefix, name)
        checked[prefix, name] = exported
        if exported[0] != expected:
            raise SDKError("Exported CMake package identity changed")
    records = identity["dependencies"]
    if not isinstance(records, list) or not records:
        raise SDKError("Source SDK requires dependency commit identities")
    paths = set()
    for record in records:
        if not isinstance(record, dict) or set(record) != {"path", "commit", "tree"}:
            raise SDKError("Invalid source SDK dependency record")
        path = str(safe_relative(record["path"]))
        if path in paths or any(not isinstance(record[key], str) or not HEX40.fullmatch(record[key]) for key in ("commit", "tree")):
            raise SDKError("Invalid or duplicate source SDK dependency identity")
        if not any(name.startswith(path + "/") for name in files):
            raise SDKError("Dependency provenance has no packaged source")
        paths.add(path)
    if not set(REQUIRED_DEPENDENCIES).issubset(paths):
        raise SDKError("Source SDK dependency set is incomplete")
    declared_modules(regular(root, ".gitmodules").read_bytes())
    threadx = next(record for record in records if record["path"] == "ext/threadx")
    threadx_lock.verify(root / "ext/threadx", root / "dependencies/threadx.lock.json",
                        "linux", packaged_commit=threadx["commit"])
    g4 = next(record for record in records
              if record["path"] == "vendors/st/cmsis_device_g4")
    stm32g4_lock.verify(root / "vendors/st/cmsis_device_g4",
                       root / "dependencies/stm32g4.lock.json",
                       packaged_commit=g4["commit"])
    for official in OFFICIAL_DRIVERS:
        driver = next(record for record in records
                      if record["path"] == official.DIRECTORY)
        cmsis = next(record for record in records
                     if record["path"] == official.CMSIS_DIRECTORY)
        official.verify(root / official.DIRECTORY,
                        root / official.LOCK_NAME,
                        root / official.CMSIS_DIRECTORY,
                        packaged_commit=driver["commit"],
                        packaged_cmsis_commit=cmsis["commit"])
    for review in cmsis_device_lock.REVIEWED_DEVICES.values():
        matching = [record for record in records
                    if record["path"] == review["directory"]]
        if len(matching) != 1:
            raise SDKError("Source SDK lacks its exact reviewed device dependency")
        cmsis_device_lock.verify(root / review["directory"],
                                 root / review["lock_name"],
                                 packaged_commit=matching[0]["commit"])
    sys.path.insert(0, str(root / "scripts/ci"))
    from vendor_import_identity import validate_import_record
    imports = identity["reviewed_imports"]
    if not isinstance(imports, list) or len(imports) != 1:
        raise SDKError("Source SDK requires its reviewed GD32 source import")
    record = imports[0]
    if not isinstance(record, dict) or record.get("path") != "vendors/gigadevice/gd32f4xx" or record.get("source_tree") != identity["source_tree"]:
        raise SDKError("Source import has a different owner tree")
    imported = root / record["path"]
    notices = {item["path"]: regular(imported, item["path"]).read_bytes() for item in record.get("notices", [])}
    validate_import_record(record, regular(imported, "source.lock.json").read_bytes(), notices, identity["source_revision"])
    # An external verifier can take time. Reject late rewrites, same-content
    # replacements, chmod and file-set changes rather than returning an old
    # proof for the current SDK. Every hash and filesystem identity is retained.
    for (directory, name), first in checked.items():
        try:
            _, final = stable_file(directory, name)
        except (SDKError, OSError) as error:
            raise SDKError("Source SDK changed during verification: " + name) from error
        if final != first:
            raise SDKError("Source SDK changed during verification: " + name)
    final_files = {path.relative_to(root).as_posix() for path in root.rglob("*")
                   if path.is_file() or path.is_symlink()}
    if final_files != actual:
        raise SDKError("Source SDK file set changed during verification")
    # Close the final content sweep as well: a file checked early in that sweep
    # may have changed while a later file was being read. This is an observed
    # stability window; callers still need an immutable SDK during actual use.
    for (directory, name), (_, first_filesystem) in checked.items():
        if filesystem_identity(regular(directory, name)) != first_filesystem:
            raise SDKError("Source SDK input changed during closing proof: " + name)
    final_directories = {root, *(path for path in root.rglob("*") if path.is_dir())}
    if final_directories != directories or any(
            filesystem_identity(path) != first
            for path, first in directory_identity.items()):
        raise SDKError("Source SDK directories changed during verification")
    return identity


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--development-fixture", action="store_true")
    parser.add_argument("--verify", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.verify:
            if args.source or args.output or args.development_fixture:
                raise SDKError("Verification cannot also prepare a source package")
            result = verify(args.verify)
        elif args.source and args.output:
            result = prepare(args.source, args.output, args.development_fixture)
        else:
            raise SDKError("Use --source/--output to prepare, or --verify to inspect")
    except (SDKError, OSError, ValueError, TypeError, KeyError) as exc:
        print(f"Source SDK rejected: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({"kind": "nexus_source_sdk_summary", "sdk_kind": result["kind"],
                      "source_revision": result["source_revision"],
                      "publishable": result["publishable"], "files": len(result["files_sha256"]),
                      "snapshot_sha256": result["snapshot_sha256"]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
