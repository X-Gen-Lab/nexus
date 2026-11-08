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


if __name__ == "__main__":
    sys.dont_write_bytecode = True
SOURCE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(SOURCE_ROOT))
try:
    from tools.maintenance.source_catalogue import (
        children, declared_modules,
        dependency, relative_path, verify_alias_identities,
        verify_gitlinks,
    )
    from tools.maintenance import (
        cmsis_device_lock, stm32f1_hal_lock, stm32f4_hal_lock,
        stm32g4_hal_lock, stm32h5_device_lock, stm32h5_hal_lock,
        stm32h7_hal_lock, stm32u5_hal_lock,
        stm32g4_lock, threadx_lock, freertos_transport, lwip_lock, mbedtls_lock,
        coremqtt_lock, mcuboot_lock,
    )
    from tools.configure.metadata import load_policy, parse_json, snapshot
    from tools.configure import dependencies as configuration_dependencies
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
REQUIRED_DEPENDENCIES = ("vendors/arm/CMSIS_6", "vendors/st/cmsis_device_f4",
                         "vendors/st/stm32f4_hal")
CMSIS_CORE_DIRECTORY = "vendors/arm/CMSIS_6"
CMSIS_CORE_REQUIRED_HEADERS = (
    "core_cm0.h", "core_cm0plus.h", "core_cm3.h", "core_cm4.h",
    "core_cm7.h", "core_cm23.h", "core_cm33.h", "core_cm55.h",
    "core_cm85.h", "cmsis_compiler.h", "cmsis_gcc.h", "cmsis_version.h",
    "m-profile/armv7m_mpu.h", "m-profile/armv8m_mpu.h",
    "m-profile/armv7m_cachel1.h", "m-profile/cmsis_gcc_m.h", "tz_context.h")
OFFICIAL_DRIVERS = (stm32f4_hal_lock,)
FREERTOS_REQUIRED_FILES = (
    'ext/freertos/.gitmodules',
    'ext/freertos/LICENSE.md',
    'ext/freertos/include/FreeRTOS.h',
    'ext/freertos/include/deprecated_definitions.h',
    'ext/freertos/include/event_groups.h',
    'ext/freertos/include/list.h',
    'ext/freertos/include/mpu_prototypes.h',
    'ext/freertos/include/mpu_syscall_numbers.h',
    'ext/freertos/include/mpu_wrappers.h',
    'ext/freertos/include/portable.h',
    'ext/freertos/include/projdefs.h',
    'ext/freertos/include/queue.h',
    'ext/freertos/include/semphr.h',
    'ext/freertos/include/stack_macros.h',
    'ext/freertos/include/stream_buffer.h',
    'ext/freertos/include/task.h',
    'ext/freertos/include/timers.h',
    'ext/freertos/list.c',
    'ext/freertos/portable/GCC/ARM_CM0/port.c',
    'ext/freertos/portable/GCC/ARM_CM0/portasm.c',
    'ext/freertos/portable/GCC/ARM_CM0/portasm.h',
    'ext/freertos/portable/GCC/ARM_CM0/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM23/non_secure/port.c',
    'ext/freertos/portable/GCC/ARM_CM23/non_secure/portasm.c',
    'ext/freertos/portable/GCC/ARM_CM23/non_secure/portasm.h',
    'ext/freertos/portable/GCC/ARM_CM23/non_secure/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM23/non_secure/portmacrocommon.h',
    'ext/freertos/portable/GCC/ARM_CM23/secure/secure_context.h',
    'ext/freertos/portable/GCC/ARM_CM23/secure/secure_context_port.c',
    'ext/freertos/portable/GCC/ARM_CM23/secure/secure_init.h',
    'ext/freertos/portable/GCC/ARM_CM23/secure/secure_port_macros.h',
    'ext/freertos/portable/GCC/ARM_CM23_NTZ/non_secure/mpu_wrappers_v2_asm.c',
    'ext/freertos/portable/GCC/ARM_CM23_NTZ/non_secure/port.c',
    'ext/freertos/portable/GCC/ARM_CM23_NTZ/non_secure/portasm.c',
    'ext/freertos/portable/GCC/ARM_CM23_NTZ/non_secure/portasm.h',
    'ext/freertos/portable/GCC/ARM_CM23_NTZ/non_secure/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM23_NTZ/non_secure/portmacrocommon.h',
    'ext/freertos/portable/GCC/ARM_CM3/port.c',
    'ext/freertos/portable/GCC/ARM_CM3/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM33/non_secure/port.c',
    'ext/freertos/portable/GCC/ARM_CM33/non_secure/portasm.c',
    'ext/freertos/portable/GCC/ARM_CM33/non_secure/portasm.h',
    'ext/freertos/portable/GCC/ARM_CM33/non_secure/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM33/non_secure/portmacrocommon.h',
    'ext/freertos/portable/GCC/ARM_CM33/secure/secure_context.h',
    'ext/freertos/portable/GCC/ARM_CM33/secure/secure_context_port.c',
    'ext/freertos/portable/GCC/ARM_CM33/secure/secure_init.h',
    'ext/freertos/portable/GCC/ARM_CM33/secure/secure_port_macros.h',
    'ext/freertos/portable/GCC/ARM_CM33_NTZ/non_secure/mpu_wrappers_v2_asm.c',
    'ext/freertos/portable/GCC/ARM_CM33_NTZ/non_secure/port.c',
    'ext/freertos/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c',
    'ext/freertos/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.h',
    'ext/freertos/portable/GCC/ARM_CM33_NTZ/non_secure/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM33_NTZ/non_secure/portmacrocommon.h',
    'ext/freertos/portable/GCC/ARM_CM3_MPU/mpu_wrappers_v2_asm.c',
    'ext/freertos/portable/GCC/ARM_CM3_MPU/port.c',
    'ext/freertos/portable/GCC/ARM_CM3_MPU/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM4F/port.c',
    'ext/freertos/portable/GCC/ARM_CM4F/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM4_MPU/mpu_wrappers_v2_asm.c',
    'ext/freertos/portable/GCC/ARM_CM4_MPU/port.c',
    'ext/freertos/portable/GCC/ARM_CM4_MPU/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM55/non_secure/port.c',
    'ext/freertos/portable/GCC/ARM_CM55/non_secure/portasm.c',
    'ext/freertos/portable/GCC/ARM_CM55/non_secure/portasm.h',
    'ext/freertos/portable/GCC/ARM_CM55/non_secure/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM55/non_secure/portmacrocommon.h',
    'ext/freertos/portable/GCC/ARM_CM55/secure/secure_context.h',
    'ext/freertos/portable/GCC/ARM_CM55/secure/secure_context_port.c',
    'ext/freertos/portable/GCC/ARM_CM55/secure/secure_init.h',
    'ext/freertos/portable/GCC/ARM_CM55/secure/secure_port_macros.h',
    'ext/freertos/portable/GCC/ARM_CM55_NTZ/non_secure/mpu_wrappers_v2_asm.c',
    'ext/freertos/portable/GCC/ARM_CM55_NTZ/non_secure/port.c',
    'ext/freertos/portable/GCC/ARM_CM55_NTZ/non_secure/portasm.c',
    'ext/freertos/portable/GCC/ARM_CM55_NTZ/non_secure/portasm.h',
    'ext/freertos/portable/GCC/ARM_CM55_NTZ/non_secure/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM55_NTZ/non_secure/portmacrocommon.h',
    'ext/freertos/portable/GCC/ARM_CM7/r0p1/port.c',
    'ext/freertos/portable/GCC/ARM_CM7/r0p1/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM85/non_secure/port.c',
    'ext/freertos/portable/GCC/ARM_CM85/non_secure/portasm.c',
    'ext/freertos/portable/GCC/ARM_CM85/non_secure/portasm.h',
    'ext/freertos/portable/GCC/ARM_CM85/non_secure/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM85/non_secure/portmacrocommon.h',
    'ext/freertos/portable/GCC/ARM_CM85/secure/secure_context.h',
    'ext/freertos/portable/GCC/ARM_CM85/secure/secure_context_port.c',
    'ext/freertos/portable/GCC/ARM_CM85/secure/secure_init.h',
    'ext/freertos/portable/GCC/ARM_CM85/secure/secure_port_macros.h',
    'ext/freertos/portable/GCC/ARM_CM85_NTZ/non_secure/mpu_wrappers_v2_asm.c',
    'ext/freertos/portable/GCC/ARM_CM85_NTZ/non_secure/port.c',
    'ext/freertos/portable/GCC/ARM_CM85_NTZ/non_secure/portasm.c',
    'ext/freertos/portable/GCC/ARM_CM85_NTZ/non_secure/portasm.h',
    'ext/freertos/portable/GCC/ARM_CM85_NTZ/non_secure/portmacro.h',
    'ext/freertos/portable/GCC/ARM_CM85_NTZ/non_secure/portmacrocommon.h',
    'ext/freertos/queue.c',
    'ext/freertos/tasks.c',
)

MANIFEST = ".nexus-source-sdk.json"
REQUIRED_FILES = (
    'soc/stm32/stm32f4/devices.json',
    '.gitmodules',
    'CMakeLists.txt',
    'LICENSE',
    'arch/CMakeLists.txt',
    'arch/arm/cortex_m/nx_arch_cache.c',
    'arch/arm/cortex_m/nx_arch_cache_control.c',
    'arch/arm/cortex_m/nx_arch_cortex_m.c',
    'arch/arm/cortex_m/nx_arch_mpu.c',
    'arch/arm/cortex_m/nx_arch_nonsecure.c',
    'arch/arm/cortex_m/nx_arch_security.c',
    'arch/arm/cortex_m/nx_arch_sleep.c',
    'arch/arm/cortex_m/private/cache_control.h',
    'arch/arm/cortex_m/private/compiler.h',
    'arch/arm/cortex_m/private/mechanisms.h',
    'arch/arm/cortex_m/private/nonsecure.h',
    'arch/arm/cortex_m/private/registers.h',
    'arch/arm/cortex_m/private/sleep.h',
    'arch/include/nexus/arch/arch.h',
    'arch/include/nexus/arch/atomic.h',
    'arch/include/nexus/arch/cache.h',
    'arch/include/nexus/arch/cache_control.h',
    'arch/include/nexus/arch/features.h',
    'arch/include/nexus/arch/mpu.h',
    'arch/include/nexus/arch/nonsecure.h',
    'arch/include/nexus/arch/security.h',
    'arch/include/nexus/arch/sleep.h',
    'arch/native/nx_arch_native.c',
    'boards/stm32f407_qiming_v31/board.json',
    'boards/stm32f407ve_sky_qingchun/board.json',
    'boards/stm32f407zg_qiming_core/README.md',
    'boards/stm32f407zg_qiming_core/assemblies/minimal.toml',
    'boards/stm32f407zg_qiming_core/assemblies/touch-nor.toml',
    'boards/stm32f407zg_qiming_core/board.json',
    'boards/stm32f407zg_qiming_v41/README.md',
    'boards/stm32f407zg_qiming_v41/assemblies/minimal.toml',
    'boards/stm32f407zg_qiming_v41/assemblies/sdio.toml',
    'boards/stm32f407zg_qiming_v41/assemblies/touch-nor.toml',
    'boards/stm32f407zg_qiming_v41/board.json',
    'cmake/package/NexusConfig.cmake.in',
    'cmake/package/NexusConfigVersion.cmake.in',
    'cmake/package/package_source_sdk.py',
    'cmake/package/verification/common_services/f407.toml',
    'cmake/package/verification/common_services/native.toml',
    'cmake/package/verification/common_services/services.c',
    'cmake/package/verification/device_tmp117/f407.toml',
    'cmake/package/verification/device_tmp117/native.toml',
    'cmake/package/verification/device_tmp117/services.c',
    'cmake/package/verification/image_window/assembly.toml',
    'cmake/package/verification/image_window/layout.json',
    'cmake/package/verification/image_window/main.c',
    'cmake/package/verification/io_evolution/boards/f407zg-eth-frames/board.json',
    'cmake/package/verification/io_evolution/consumers/ethernet-frames.c',
    'cmake/package/verification/io_evolution/consumers/f407-handoff.c',
    'cmake/package/verification/io_evolution/consumers/f407-i2s2-slave-tx.c',
    'cmake/package/verification/io_evolution/consumers/native-time-empty.c',
    'cmake/package/verification/io_evolution/consumers/native-time-pair.c',
    'cmake/package/verification/io_evolution/f407zg-eth-frames.toml',
    'cmake/package/verification/io_evolution/native-time-empty.toml',
    'cmake/package/verification/io_evolution/native-time-pair.toml',
    'cmake/package/verification/loaned_rx/application.c',
    'cmake/package/verification/loaned_rx/application.h',
    'cmake/package/verification/loaned_rx/composition.c',
    'cmake/package/verification/loaned_rx/f407.toml',
    'cmake/package/verification/mcuboot_boot/assembly.toml',
    'cmake/package/verification/mcuboot_boot/layout.json',
    'cmake/package/verification/mcuboot_boot/main.c',
    'cmake/package/verification/mcuboot_boot/public_key.c',
    'cmake/package/verification/mcuboot_control/application.c',
    'cmake/package/verification/mcuboot_control/application.h',
    'cmake/package/verification/mcuboot_control/f407.c',
    'cmake/package/verification/mcuboot_control/f407.toml',
    'cmake/package/verification/mcuboot_control/native.c',
    'cmake/package/verification/mcuboot_control/native.toml',
    'cmake/package/verification/secure_services/f407/application.c',
    'cmake/package/verification/secure_services/f407/application.h',
    'cmake/package/verification/secure_services/f407/assembly.toml',
    'cmake/package/verification/secure_services/f407/composition.c',
    'cmake/package/verification/secure_services/native/application.c',
    'cmake/package/verification/secure_services/native/application.h',
    'cmake/package/verification/secure_services/native/assembly.toml',
    'cmake/package/verification/secure_services/native/composition.c',
    'cmake/package/verification/secure_services/public_ca.h',
    'cmake/package/verify_consumers.py',
    'cmake/platform/CMSISCore.cmake',
    'cmake/platform/Components.cmake',
    'cmake/platform/Configure.cmake',
    'cmake/platform/ControllerModes.cmake',
    'cmake/platform/Firmware.cmake',
    'cmake/platform/Options.cmake',
    'cmake/platform/Provider.cmake',
    'cmake/platform/Runtime.cmake',
    'cmake/platform/SDK.cmake',
    'cmake/platform/STM32FSMC.cmake',
    'cmake/platform/STM32Official.cmake',
    'cmake/platform/STM32SDIO.cmake',
    'cmake/platform/TargetPolicy.cmake',
    'cmake/platform/Workload.cmake',
    'cmake/toolchains/arm-gcc.cmake',
    'components/CMakeLists.txt',
    'components/amp_ipc/CMakeLists.txt',
    'components/amp_ipc/include/nexus/components/amp_ipc.h',
    'components/amp_ipc/include/nexus/components/amp_ipc_storage.h',
    'components/amp_ipc/src/amp_ipc.c',
    'components/at24c02/CMakeLists.txt',
    'components/at24c02/include/nexus/components/at24c02.h',
    'components/at24c02/references.json',
    'components/at24c02/src/at24c02.c',
    'components/bmp280/include/nexus/components/bmp280.h',
    'components/bmp280/include/nexus/components/bmp280_spi.h',
    'components/bmp280/src/bmp280.c',
    'components/bmp280/src/spi_transport.c',
    'components/bus_owner/include/nexus/components/bus_owner.h',
    'components/bus_owner/include/nexus/components/spi_async_owner.h',
    'components/bus_owner/include/nexus/components/spi_dma_owner.h',
    'components/bus_owner/include/nexus/components/spi_owner.h',
    'components/bus_owner/include/nexus/components/uart_owner.h',
    'components/bus_owner/src/bus_owner.c',
    'components/bus_owner/src/spi_async_owner.c',
    'components/bus_owner/src/spi_dma_owner.c',
    'components/bus_owner/src/spi_owner.c',
    'components/bus_owner/src/uart_owner.c',
    'components/clock_domain/CMakeLists.txt',
    'components/clock_domain/include/nexus/components/clock_domain.h',
    'components/clock_domain/private/clock_domain_impl.h',
    'components/clock_domain/src/clock_domain.c',
    'components/dp83848/CMakeLists.txt',
    'components/dp83848/include/nexus/dp83848/dp83848.h',
    'components/dp83848/src/dp83848.c',
    'components/ds18b20/CMakeLists.txt',
    'components/ds18b20/include/nexus/components/ds18b20.h',
    'components/ds18b20/src/ds18b20.c',
    'components/i3c_controller/CMakeLists.txt',
    'components/i3c_controller/include/nexus/components/i3c_controller.h',
    'components/i3c_controller/include/nexus/components/i3c_controller_storage.h',
    'components/i3c_controller/src/i3c_controller.c',
    'components/ili9341/CMakeLists.txt',
    'components/ili9341/include/nexus/components/ili9341.h',
    'components/ili9341/src/ili9341.c',
    'components/infrared/CMakeLists.txt',
    'components/infrared/include/nexus/components/infrared.h',
    'components/infrared/src/infrared.c',
    'components/log/include/nexus/components/log.h',
    'components/log/src/log.c',
    'components/mapped_storage/CMakeLists.txt',
    'components/mapped_storage/include/nexus/components/mapped_storage.h',
    'components/mapped_storage/include/nexus/components/mapped_storage_owner.h',
    'components/mapped_storage/include/nexus/components/native_mapped_storage.h',
    'components/mapped_storage/src/mapped_storage.c',
    'components/mapped_storage/src/mapped_storage_owner.c',
    'components/mapped_storage/src/native_mapped_storage.c',
    'components/mcuboot/CMakeLists.txt',
    'components/mcuboot/README.md',
    'components/mcuboot/include/nexus/components/mcuboot_control.h',
    'components/mcuboot/port/flash_map_backend/flash_map_backend.h',
    'components/mcuboot/private/upstream_profile.cmake',
    'components/mcuboot/profile/mcuboot_config/mcuboot_assert.h',
    'components/mcuboot/profile/mcuboot_config/mcuboot_config.h',
    'components/mcuboot/profile/mcuboot_config/mcuboot_logging.h',
    'components/mcuboot/profile/os/os_malloc.h',
    'components/mcuboot/profile/sysflash/sysflash.h',
    'components/mcuboot/src/flash_map_backend.c',
    'components/mcuboot/src/mcuboot_control.c',
    'components/modbus/include/nexus/components/modbus_rtu.h',
    'components/modbus/src/modbus_rtu.c',
    'components/mqtt/CMakeLists.txt',
    'components/mqtt/README.md',
    'components/mqtt/include/nexus/mqtt/client.h',
    'components/mqtt/include/nexus/mqtt/core_mqtt_storage.h',
    'components/mqtt/port/core_mqtt_config.h',
    'components/mqtt/src/client.c',
    'components/network/CMakeLists.txt',
    'components/network/Profiles.cmake',
    'components/network/include/nexus/network/byte_stream.h',
    'components/network/include/nexus/network/lwip_adapter.h',
    'components/network/include/nexus/network/lwip_loaned_rx.h',
    'components/network/include/nexus/network/lwip_loaned_rx_storage.h',
    'components/network/include/nexus/network/lwip_storage.h',
    'components/network/include/nexus/network/lwip_tcp.h',
    'components/network/include/nexus/network/lwip_tcp_storage.h',
    'components/network/port/arch/cc.h',
    'components/network/port/lwipopts.h',
    'components/network/src/lwip_adapter.c',
    'components/network/src/lwip_tcp.c',
    'components/onewire/CMakeLists.txt',
    'components/onewire/include/nexus/components/onewire.h',
    'components/onewire/src/crc8.c',
    'components/onewire/src/onewire.c',
    'components/owner_observer/CMakeLists.txt',
    'components/owner_observer/include/nexus/components/owner_observer.h',
    'components/owner_observer/src/owner_observer.c',
    'components/settings/include/nexus/components/settings.h',
    'components/settings/src/settings.c',
    'components/soc/CMakeLists.txt',
    'components/soc/stm32/CMakeLists.txt',
    'components/soc/stm32/stm32f407_handoff/CMakeLists.txt',
    'components/soc/stm32/stm32f407_handoff/include/nexus/components/stm32f407_handoff.h',
    'components/software_i2c/CMakeLists.txt',
    'components/software_i2c/README.md',
    'components/software_i2c/include/nexus/components/software_i2c.h',
    'components/software_i2c/include/nexus/components/software_sccb.h',
    'components/software_i2c/references.json',
    'components/software_i2c/src/software_i2c.c',
    'components/software_spi/CMakeLists.txt',
    'components/software_spi/include/nexus/components/software_spi.h',
    'components/software_spi/src/software_spi.c',
    'components/storage/include/nexus/components/file_flash.h',
    'components/storage/include/nexus/components/flash_storage.h',
    'components/storage/include/nexus/components/storage.h',
    'components/storage/src/file_flash.c',
    'components/storage/src/flash_storage.c',
    'components/storage/src/storage.c',
    'components/tls/CMakeLists.txt',
    'components/tls/README.md',
    'components/tls/include/nexus/tls/storage.h',
    'components/tls/include/nexus/tls/tls.h',
    'components/tls/profile/crypto-user.h',
    'components/tls/profile/tls-user.h',
    'components/tls/src/tls.c',
    'components/tmp117/CMakeLists.txt',
    'components/tmp117/include/nexus/components/tmp117.h',
    'components/tmp117/include/nexus/components/tmp117_i2c.h',
    'components/tmp117/src/i2c_transport.c',
    'components/tmp117/src/tmp117.c',
    'components/usb_control/CMakeLists.txt',
    'components/usb_control/include/nexus/components/usb_acm.h',
    'components/usb_control/include/nexus/components/usb_control.h',
    'components/usb_control/include/nexus/components/usb_descriptors.h',
    'components/usb_control/src/usb_acm.c',
    'components/usb_control/src/usb_control.c',
    'components/usb_control/src/usb_descriptors.c',
    'components/user_io_broker/CMakeLists.txt',
    'components/user_io_broker/include/nexus/components/user_io_async.h',
    'components/user_io_broker/include/nexus/components/user_io_async_storage.h',
    'components/user_io_broker/include/nexus/components/user_io_broker.h',
    'components/user_io_broker/private/user_io_broker_impl.h',
    'components/user_io_broker/src/user_io_async.c',
    'components/user_io_broker/src/user_io_broker.c',
    'components/w25/CMakeLists.txt',
    'components/w25/include/nexus/components/w25.h',
    'components/w25/src/w25.c',
    'components/xpt2046/CMakeLists.txt',
    'components/xpt2046/include/nexus/components/xpt2046.h',
    'components/xpt2046/src/xpt2046.c',
    'core/CMakeLists.txt',
    'core/include/nexus/core/fault.h',
    'core/include/nexus/core/lifecycle.h',
    'core/include/nexus/core/memory.h',
    'core/include/nexus/core/platform_lifecycle.h',
    'core/include/nexus/core/request.h',
    'core/include/nexus/core/request_observer.h',
    'core/include/nexus/core/silicon.h',
    'core/include/nexus/core/stack_observation.h',
    'core/include/nexus/core/status.h',
    'core/include/nexus/core/time.h',
    'core/src/fault.c',
    'core/src/lifecycle.c',
    'core/src/platform_lifecycle.c',
    'core/private/platform_lifecycle_impl.h',
    'core/src/request.c',
    'core/src/request_observer.c',
    'core/src/silicon.c',
    'core/src/stack_observation.c',
    'core/src/time.c',
    'dependencies/coremqtt.lock.json',
    'dependencies/environment.lock.json',
    'dependencies/lwip.lock.json',
    'dependencies/mbedtls.lock.json',
    'dependencies/mcuboot.lock.json',
    'dependencies/platform-compatibility.json',
    'dependencies/stm32f1-hal.lock.json',
    'dependencies/stm32f1.lock.json',
    'dependencies/stm32f4-hal.lock.json',
    'dependencies/stm32g4-hal.lock.json',
    'dependencies/stm32g4.lock.json',
    'dependencies/stm32h5-hal.lock.json',
    'dependencies/stm32h5.lock.json',
    'dependencies/stm32h7-hal.lock.json',
    'dependencies/stm32h7.lock.json',
    'dependencies/stm32h747.lock.json',
    'dependencies/stm32u5-hal.lock.json',
    'dependencies/stm32u5.lock.json',
    'dependencies/threadx.lock.json',
    'dependencies/toolchains.lock.json',
    'io/CMakeLists.txt',
    'io/include/nexus/io/adc.h',
    'io/include/nexus/io/adc_format.h',
    'io/include/nexus/io/adc_group.h',
    'io/include/nexus/io/adc_observer.h',
    'io/include/nexus/io/advanced_pwm.h',
    'io/include/nexus/io/block.h',
    'io/include/nexus/io/can.h',
    'io/include/nexus/io/can_fd.h',
    'io/include/nexus/io/cycle_probe.h',
    'io/include/nexus/io/diagnostic.h',
    'io/include/nexus/io/dac.h',
    'io/include/nexus/io/dma.h',
    'io/include/nexus/io/encoder.h',
    'io/include/nexus/io/entropy.h',
    'io/include/nexus/io/ethernet.h',
    'io/include/nexus/io/ethernet_frames.h',
    'io/include/nexus/io/external_bus.h',
    'io/include/nexus/io/exti.h',
    'io/include/nexus/io/flash.h',
    'io/include/nexus/io/frame_stream.h',
    'io/include/nexus/io/fwd.h',
    'io/include/nexus/io/gpio.h',
    'io/include/nexus/io/i2c.h',
    'io/include/nexus/io/i2c_bus_clear.h',
    'io/include/nexus/io/i2c_long.h',
    'io/include/nexus/io/mdio.h',
    'io/include/nexus/io/memory_window.h',
    'io/include/nexus/io/native/model.h',
    'io/include/nexus/io/native/spi_dma_model.h',
    'io/include/nexus/io/observation.h',
    'io/include/nexus/io/parallel16.h',
    'io/include/nexus/io/pwm_adc.h',
    'io/include/nexus/io/pwm_group.h',
    'io/include/nexus/io/spi.h',
    'io/include/nexus/io/spi_dma_owner_binding.h',
    'io/include/nexus/io/spi_dma_transaction.h',
    'io/include/nexus/io/spi_transaction.h',
    'io/include/nexus/io/stream.h',
    'io/include/nexus/io/stream_budget.h',
    'io/include/nexus/io/stream_profile.h',
    'io/include/nexus/io/timer.h',
    'io/include/nexus/io/timer_capture.h',
    'io/include/nexus/io/uart.h',
    'io/include/nexus/io/uart_format.h',
    'io/include/nexus/io/uart_rs485.h',
    'io/include/nexus/io/uart_word.h',
    'io/include/nexus/io/usb_activation.h',
    'io/include/nexus/io/usb_device.h',
    'io/include/nexus/io/usb_profile.h',
    'io/include/nexus/io/wake.h',
    'io/include/nexus/io/watchdog.h',
    'io/private/can_fd_impl.h',
    'io/private/can_validation.h',
    'io/private/eth_frames_validation.h',
    'io/private/eth_validation.h',
    'io/private/spi_dma_sequence.h',
    'io/private/spi_dma_sequence_owner.h',
    'io/private/stream_profile_storage.h',
    'io/src/api/adc.c',
    'io/src/api/adc_group.c',
    'io/src/api/advanced_pwm.c',
    'io/src/api/block.c',
    'io/src/api/can.c',
    'io/src/api/can_fd.c',
    'io/src/api/dac.c',
    'io/src/api/encoder.c',
    'io/src/api/entropy.c',
    'io/src/api/ethernet.c',
    'io/src/api/ethernet_frames.c',
    'io/src/api/external_bus.c',
    'io/src/api/exti.c',
    'io/src/api/flash.c',
    'io/src/api/gpio.c',
    'io/src/api/i2c.c',
    'io/src/api/mdio.c',
    'io/src/api/memory_window.c',
    'io/src/api/parallel16.c',
    'io/src/api/pwm_adc.c',
    'io/src/api/pwm_group.c',
    'io/src/api/spi.c',
    'io/src/api/timer.c',
    'io/src/api/timer_capture.c',
    'io/src/api/uart.c',
    'io/src/api/uart_word.c',
    'io/src/api/usb_device.c',
    'io/src/api/watchdog.c',
    'io/src/mechanisms/adc_format.c',
    'io/src/mechanisms/adc_observer.c',
    'io/src/mechanisms/adc_stream_profile.c',
    'io/src/mechanisms/cycle_probe.c',
    'io/src/mechanisms/diagnostic.c',
    'io/src/mechanisms/dma.c',
    'io/src/mechanisms/flash_region.c',
    'io/src/mechanisms/frame_stream.c',
    'io/src/mechanisms/i2c_bus_clear.c',
    'io/src/mechanisms/i2c_long.c',
    'io/src/mechanisms/observation.c',
    'io/src/mechanisms/spi_dma_owner_binding.c',
    'io/src/mechanisms/spi_dma_sequence.c',
    'io/src/mechanisms/spi_dma_transaction.c',
    'io/src/mechanisms/spi_transaction.c',
    'io/src/mechanisms/stream.c',
    'io/src/mechanisms/stream_budget.c',
    'io/src/mechanisms/stream_profile.c',
    'io/src/mechanisms/stream_profile_storage.c',
    'io/src/mechanisms/uart_rs485.c',
    'io/src/mechanisms/usb_profile.c',
    'io/src/mechanisms/wake.c',
    'os/CMakeLists.txt',
    'os/baremetal/notify.c',
    'os/diagnostic.c',
    'os/freertos/CMakeLists.txt',
    'os/freertos/diagnostics.c',
    'os/freertos/freertos.c',
    'os/freertos/hooks.c',
    'os/freertos/include/FreeRTOSConfig.h',
    'os/freertos/include/nexus/os/freertos.h',
    'os/freertos/include/nexus/os/freertos_diagnostics.h',
    'os/freertos/include/nexus/os/freertos_mpu.h',
    'os/freertos/include/nexus/os/freertos_stack.h',
    'os/freertos/include/nexus/os/freertos_user.h',
    'os/freertos/include/nexus/os/lowpower.h',
    'os/freertos/include/nexus/os/secure_context.h',
    'os/freertos/include/nexus/os/tick.h',
    'os/freertos/lowpower.c',
    'os/freertos/mpu/guard.c',
    'os/freertos/mpu/guard.h',
    'os/freertos/mpu/guard_arm.c',
    'os/freertos/mpu/layout.c',
    'os/freertos/mpu/patches/arm_cm3_mpu-port.patch',
    'os/freertos/mpu/patches/arm_cm4_mpu-port.patch',
    'os/freertos/mpu/patches/v7-veneers.patch',
    'os/freertos/mpu/patches/v8-baseline-assembly.patch',
    'os/freertos/mpu/patches/v8-baseline-veneers.patch',
    'os/freertos/mpu/patches/v8-mainline-assembly.patch',
    'os/freertos/mpu/patches/v8-mainline-veneers.patch',
    'os/freertos/mpu/patches/v8-port.patch',
    'os/freertos/mpu/prepare_port.py',
    'os/freertos/mpu/private.h',
    'os/freertos/mpu/syscalls.c',
    'os/freertos/mpu/task.c',
    'os/freertos/patches/m7-integer-port.c.patch',
    'os/freertos/patches/m7-integer-portmacro.h.patch',
    'os/freertos/prepare_m7_integer.py',
    'os/freertos/private/timebase_lowpower.h',
    'os/freertos/secure/CMakeLists.txt',
    'os/freertos/secure/Integration.cmake',
    'os/freertos/secure/config/FreeRTOSConfig.h',
    'os/freertos/secure/context.c',
    'os/freertos/secure/init.c',
    'os/freertos/secure/private/hardware.h',
    'os/freertos/stack_snapshot.c',
    'os/freertos/tick.c',
    'os/freertos/timebase_lowpower.c',
    'os/include/nexus/os/baremetal.h',
    'os/include/nexus/os/diagnostic.h',
    'os/include/nexus/os/wait.h',
    'os/native/include/nexus/os/native.h',
    'os/native/native.c',
    'os/reviewed_patch.py',
    'os/threadx/CMakeLists.txt',
    'os/threadx/Kernel.cmake',
    'os/threadx/config/tx_user.h',
    'os/threadx/include/nexus/os/threadx.h',
    'os/threadx/m4.c',
    'os/threadx/m4_vectors.S',
    'os/threadx/patches/linux-lowlevel.patch',
    'os/threadx/patches/linux-scheduler.patch',
    'os/threadx/prepare_linux.py',
    'os/threadx/private/irq_model.h',
    'os/threadx/private/m4_model.h',
    'os/threadx/threadx.c',
    'os/wait.c',
    'scripts/ci/io_lifecycle_matrix.py',
    'scripts/ci/native_contracts.py',
    'scripts/ci/tdd_gate.py',
    'scripts/ci/vendor_import_identity.py',
    'scripts/ci/verify_vendor_source.py',
    'scripts/validation/junit.py',
    'soc/stm32/stm32f4/CMakeLists.txt',
    'soc/stm32/stm32f4/Entropy.cmake',
    'soc/stm32/stm32f4/README.md',
    'soc/stm32/stm32f4/drivers/adc.c',
    'soc/stm32/stm32f4/drivers/adc_continuous.c',
    'soc/stm32/stm32f4/drivers/adc_stream.c',
    'soc/stm32/stm32f4/drivers/advanced_pwm.c',
    'soc/stm32/stm32f4/drivers/can.c',
    'soc/stm32/stm32f4/drivers/entropy.c',
    'soc/stm32/stm32f4/drivers/ethernet.c',
    'soc/stm32/stm32f4/drivers/ethernet_frames.c',
    'soc/stm32/stm32f4/drivers/exti.c',
    'soc/stm32/stm32f4/drivers/flash.c',
    'soc/stm32/stm32f4/drivers/fsmc.c',
    'soc/stm32/stm32f4/drivers/fsmc_boot.c',
    'soc/stm32/stm32f4/drivers/gpio.c',
    'soc/stm32/stm32f4/drivers/i2c.c',
    'soc/stm32/stm32f4/drivers/i2c_async.c',
    'soc/stm32/stm32f4/drivers/i2c_cold.c',
    'soc/stm32/stm32f4/drivers/i2s2_slave_tx.c',
    'soc/stm32/stm32f4/drivers/ip/advanced_timer/advanced_pwm_impl.h',
    'soc/stm32/stm32f4/drivers/ip/advanced_timer/timer_advanced_controller.h',
    'soc/stm32/stm32f4/drivers/ip/advanced_timer/timer_capture_impl.h',
    'soc/stm32/stm32f4/drivers/ip/classic_can/can_controller.c',
    'soc/stm32/stm32f4/drivers/ip/classic_can/can_controller.h',
    'soc/stm32/stm32f4/drivers/ip/classic_can/cortex_m_port.h',
    'soc/stm32/stm32f4/drivers/ip/fs_device_pio/usb_device_controller.c',
    'soc/stm32/stm32f4/drivers/ip/fs_device_pio/usb_device_controller.h',
    'soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.c',
    'soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.h',
    'soc/stm32/stm32f4/drivers/ip/legacy_mac/eth_frames_mac.c',
    'soc/stm32/stm32f4/drivers/ip/legacy_mac/eth_frames_mac.h',
    'soc/stm32/stm32f4/drivers/ip/legacy_mac/eth_mac.c',
    'soc/stm32/stm32f4/drivers/ip/legacy_mac/eth_mac.h',
    'soc/stm32/stm32f4/drivers/platform_cold.c',
    'soc/stm32/stm32f4/drivers/rtc.c',
    'soc/stm32/stm32f4/drivers/sdio.c',
    'soc/stm32/stm32f4/drivers/spi.c',
    'soc/stm32/stm32f4/drivers/spi_dma.c',
    'soc/stm32/stm32f4/drivers/timer.c',
    'soc/stm32/stm32f4/drivers/timer_capture.c',
    'soc/stm32/stm32f4/drivers/uart.c',
    'soc/stm32/stm32f4/drivers/uart_cold.c',
    'soc/stm32/stm32f4/drivers/uart_dma.c',
    'soc/stm32/stm32f4/drivers/uart_duplex.c',
    'soc/stm32/stm32f4/drivers/uart_hal.c',
    'soc/stm32/stm32f4/drivers/uart_receive.c',
    'soc/stm32/stm32f4/drivers/uart_stream.c',
    'soc/stm32/stm32f4/drivers/uart_word.c',
    'soc/stm32/stm32f4/drivers/usb_device.c',
    'soc/stm32/stm32f4/drivers/watchdog.c',
    'soc/stm32/stm32f4/entropy_instance.c',
    'soc/stm32/stm32f4/handoff.c',
    'soc/stm32/stm32f4/include/nexus/soc/stm32f407_entropy.h',
    'soc/stm32/stm32f4/memory_init.c',
    'soc/stm32/stm32f4/private/fsmc_hal/README.md',
    'soc/stm32/stm32f4/private/fsmc_hal/stm32f4xx_hal_conf.h',
    'soc/stm32/stm32f4/private/hal/stm32f4xx_hal_conf.h',
    'soc/stm32/stm32f4/private/memory_init.h',
    'soc/stm32/stm32f4/private/sdio_hal/stm32f4xx_hal_conf.h',
    'soc/stm32/stm32f4/private/stm32_identity.h',
    'soc/stm32/stm32f4/private/stm32f407_basic_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_can.h',
    'soc/stm32/stm32f4/private/stm32f407_can_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_control_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_dma.h',
    'soc/stm32/stm32f4/private/stm32f407_entropy.h',
    'soc/stm32/stm32f4/private/stm32f407_eth.h',
    'soc/stm32/stm32f4/private/stm32f407_eth_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_eth_frames.h',
    'soc/stm32/stm32f4/private/stm32f407_eth_frames_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_fsmc.h',
    'soc/stm32/stm32f4/private/stm32f407_fsmc_profile.h',
    'soc/stm32/stm32f4/private/stm32f407_hal_atomic.h',
    'soc/stm32/stm32f4/private/stm32f407_hal_profile.h',
    'soc/stm32/stm32f4/private/stm32f407_handoff_hw.h',
    'soc/stm32/stm32f4/private/stm32f407_i2c_async.h',
    'soc/stm32/stm32f4/private/stm32f407_i2c_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_i2c_port.h',
    'soc/stm32/stm32f4/private/stm32f407_i2s2_slave_tx.h',
    'soc/stm32/stm32f4/private/stm32f407_lowpower.h',
    'soc/stm32/stm32f4/private/stm32f407_packet_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_platform_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_provider.h',
    'soc/stm32/stm32f4/private/stm32f407_rtc.h',
    'soc/stm32/stm32f4/private/stm32f407_sdio.h',
    'soc/stm32/stm32f4/private/stm32f407_sleep.h',
    'soc/stm32/stm32f4/private/stm32f407_spi_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_sram_boot.h',
    'soc/stm32/stm32f4/private/stm32f407_stream.h',
    'soc/stm32/stm32f4/private/stm32f407_system.h',
    'soc/stm32/stm32f4/private/stm32f407_timer_advanced.h',
    'soc/stm32/stm32f4/private/stm32f407_timer_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_uart_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_uart_hal.h',
    'soc/stm32/stm32f4/private/stm32f407_uart_receive.h',
    'soc/stm32/stm32f4/private/stm32f407_uart_word.h',
    'soc/stm32/stm32f4/private/stm32f407_usb_clock.h',
    'soc/stm32/stm32f4/private/stm32f407_usb_cold.h',
    'soc/stm32/stm32f4/private/stm32f407_usb_device.h',
    'soc/stm32/stm32f4/private/uart_control.h',
    'soc/stm32/stm32f4/references.json',
    'soc/stm32/stm32f4/routes.json',
    'soc/stm32/stm32f4/sleep.c',
    'soc/stm32/stm32f4/soc.json',
    'soc/stm32/stm32f4/sram_boot.c',
    'soc/stm32/stm32f4/system.c',
    'soc/stm32/stm32f4/timebase_lowpower.c',
    'soc/stm32/stm32f4/usb_clock.c',
    'tools/__init__.py',
    'tools/configure/__init__.py',
    'tools/configure/adc.py',
    'tools/configure/assemblies/catalogue.json',
    'tools/configure/assemblies/qiming-baremetal-uart-dma.toml',
    'tools/configure/assemblies/qiming-i2s2-slave-tx.toml',
    'tools/configure/assemblies/sky-baremetal.toml',
    'tools/configure/assemblies/sky-i2s2-slave-tx.toml',
    'tools/configure/assembly.schema.json',
    'tools/configure/authored.py',
    'tools/configure/bindings.py',
    'tools/configure/board_hardware.py',
    'tools/configure/bundle.py',
    'tools/configure/calls.py',
    'tools/configure/can.py',
    'tools/configure/clocks.py',
    'tools/configure/configure.py',
    'tools/configure/contracts.py',
    'tools/configure/coupled_io.py',
    'tools/configure/cpu.py',
    'tools/configure/dependencies.py',
    'tools/configure/dma_topology.py',
    'tools/configure/documentation.py',
    'tools/configure/emission.py',
    'tools/configure/enhanced_endpoints.py',
    'tools/configure/ethernet.py',
    'tools/configure/experience.py',
    'tools/configure/exti.py',
    'tools/configure/fact_references.py',
    'tools/configure/factory.py',
    'tools/configure/gpio_electrical.py',
    'tools/configure/gpio_phase.py',
    'tools/configure/i2c_timing.py',
    'tools/configure/i2c_timing_selection.py',
    'tools/configure/i2s2.py',
    'tools/configure/integration_package.py',
    'tools/configure/io_traits.py',
    'tools/configure/ir.py',
    'tools/configure/records.py',
    'tools/configure/kernel.py',
    'tools/configure/lowpower.py',
    'tools/configure/matrix.py',
    'tools/configure/memory.py',
    'tools/configure/metadata.py',
    'tools/configure/onboarding.py',
    'tools/configure/physical_facts.py',
    'tools/configure/pin_states.py',
    'tools/configure/providers/__init__.py',
    'tools/configure/providers/common.py',
    'tools/configure/providers/descriptor.py',
    'tools/configure/providers/hardware.py',
    'tools/configure/resource_graph.py',
    'tools/configure/rtc.py',
    'tools/configure/runtime.py',
    'tools/configure/runtime_power.py',
    'tools/configure/sdk.py',
    'tools/configure/secondary_controllers.py',
    'tools/configure/selected_contracts.py',
    'tools/configure/selection.py',
    'tools/configure/soc-package.schema.json',
    'tools/configure/stm32f4/__init__.py',
    'tools/configure/stm32f4/bindings.py',
    'tools/configure/stm32f4/block.py',
    'tools/configure/stm32f4/can.py',
    'tools/configure/stm32f4/contract.py',
    'tools/configure/stm32f4_descriptor.py',
    'tools/configure/stm32f4/emission.py',
    'tools/configure/stm32f4/fsmc.py',
    'tools/configure/stm32f4/gpio.py',
    'tools/configure/stm32f4/hardware.py',
    'tools/configure/stm32f4/i2s2.py',
    'tools/configure/stm32f4/integration.py',
    'tools/configure/stm32f4/packet.py',
    'tools/configure/stm32f4/preflight.py',
    'tools/configure/stm32f4/rtc.py',
    'tools/configure/stm32f4/sram.py',
    'tools/configure/stm32f4/timer.py',
    'tools/configure/stream_profiles.py',
    'tools/configure/threadx.py',
    'tools/configure/timer_advanced.py',
    'tools/configure/trigger_graph.py',
    'tools/configure/uart_formats.py',
    'tools/configure/usb_device.py',
    'tools/configure/usb_profiles.py',
    'tools/dev/artifacts.py',
    'tools/dev/cache.py',
    'tools/dev/change_paths.py',
    'tools/dev/dependencies.py',
    'tools/dev/dev.py',
    'tools/dev/device.py',
    'tools/dev/plan.py',
    'tools/dev/presets.py',
    'tools/dev/process.py',
    'tools/dev/projects.py',
    'tools/dev/provenance.py',
    'tools/dev/qualification.py',
    'tools/dev/timings.py',
    'tools/evidence/candidate.py',
    'tools/evidence/common.py',
    'tools/evidence/consumer.py',
    'tools/evidence/container.py',
    'tools/evidence/environment.py',
    'tools/evidence/identity.py',
    'tools/evidence/inputs.py',
    'tools/evidence/legacy_sdk.py',
    'tools/evidence/native_proof.py',
    'tools/evidence/proof.py',
    'tools/evidence/qualification.py',
    'tools/evidence/recipe.py',
    'tools/evidence/reproduce.py',
    'tools/evidence/storage.py',
    'tools/evidence/store.py',
    'tools/hil/admit.py',
    'tools/hil/measure.py',
    'tools/hil/power.py',
    'tools/hil/prepare.py',
    'tools/hil/threadx.py',
    'tools/hil/workloads.py',
    'tools/maintenance/capabilities.py',
    'tools/maintenance/cmsis_device_lock.py',
    'tools/maintenance/coremqtt_lock.py',
    'tools/maintenance/dependency_proposals.py',
    'tools/maintenance/freertos_transport.py',
    'tools/maintenance/lwip_lock.py',
    'tools/maintenance/mbedtls_lock.py',
    'tools/maintenance/mcuboot_lock.py',
    'tools/maintenance/source_catalogue.py',
    'tools/maintenance/stm32_ll_lock.py',
    'tools/maintenance/stm32f1_hal_lock.py',
    'tools/maintenance/stm32f4_hal_lock.py',
    'tools/maintenance/stm32g4_hal_lock.py',
    'tools/maintenance/stm32g4_lock.py',
    'tools/maintenance/stm32h5_device_lock.py',
    'tools/maintenance/stm32h5_hal_lock.py',
    'tools/maintenance/stm32h7_hal_lock.py',
    'tools/maintenance/stm32u5_hal_lock.py',
    'tools/maintenance/support.py',
    'tools/maintenance/threadx_lock.py',
    'tools/measurement/elf.py',
    'tools/measurement/resources.py',
    'tools/measurement/retention.py',
    'tools/measurement/workload.c',
    'tools/release/transport.py',
)
REQUIRED_FILES += (
    'components/soc/stm32/f407_i2s2_slave_tx/CMakeLists.txt',
    'components/soc/stm32/f407_i2s2_slave_tx/include/nexus/components/f407_i2s2_slave_tx.h',
    'io/src/api/rtc.c',
    'io/include/nexus/io/lptim.h',
    'io/include/nexus/io/rtc.h',
    'io/include/nexus/io/native/rtc_lptim_model.h',
    'io/src/api/lptim.c',
    'io/private/rtc_lptim_internal.h',
)

HOST_SUPPORT_FILES = (
    'tests/__init__.py',
    'tests/support/__init__.py',
    'tests/support/native/CMakeLists.txt',
    'tests/support/native/README.md',
    'tests/support/native/__init__.py',
    'tests/support/native/assemblies/native-empty.toml',
    'tests/support/native/assemblies/native-gpio.toml',
    'tests/support/native/assemblies/native-spi.toml',
    'tests/support/native/assemblies/native-step-debug.toml',
    'tests/support/native/assemblies/native-uart.toml',
    'tests/support/native/assemblies/native.toml',
    'tests/support/native/board/board.json',
    'tests/support/native/configure/__init__.py',
    'tests/support/native/configure/bindings.py',
    'tests/support/native/configure/contract.py',
    'tests/support/native/configure/emission.py',
    'tests/support/native/descriptor.py',
    'tests/support/native/drivers/adc.c',
    'tests/support/native/drivers/can.c',
    'tests/support/native/drivers/can.h',
    'tests/support/native/drivers/can_access.h',
    'tests/support/native/drivers/ethernet.c',
    'tests/support/native/drivers/ethernet.h',
    'tests/support/native/drivers/ethernet_access.h',
    'tests/support/native/drivers/ethernet_frames.c',
    'tests/support/native/drivers/ethernet_frames.h',
    'tests/support/native/drivers/ethernet_frames_access.h',
    'tests/support/native/drivers/exti.c',
    'tests/support/native/drivers/flash.c',
    'tests/support/native/drivers/gpio.c',
    'tests/support/native/drivers/i2c.c',
    'tests/support/native/drivers/provider.h',
    'tests/support/native/drivers/rtc_lptim.c',
    'tests/support/native/drivers/rtc_lptim_provider.h',
    'tests/support/native/drivers/spi.c',
    'tests/support/native/drivers/spi_async.c',
    'tests/support/native/drivers/spi_dma_provider.h',
    'tests/support/native/drivers/spi_dma_transaction.c',
    'tests/support/native/drivers/stream.c',
    'tests/support/native/drivers/time.c',
    'tests/support/native/drivers/timer.c',
    'tests/support/native/drivers/uart.c',
    'tests/support/native/drivers/uart_word.c',
    'tests/support/native/drivers/uart_word_provider.h',
    'tests/support/native/drivers/usb_device.c',
    'tests/support/native/drivers/usb_device.h',
    'tests/support/native/drivers/usb_device_access.h',
    'tests/support/native/drivers/watchdog.c',
    'tests/support/native/routes.json',
    'tests/support/native/soc.json',
)
IO_EVOLUTION_VERIFICATION_FILES = (
    'cmake/package/verification/io_evolution/boards/f407zg-eth-frames/board.json',
    'cmake/package/verification/io_evolution/consumers/ethernet-frames.c',
    'cmake/package/verification/io_evolution/consumers/f407-handoff.c',
    'cmake/package/verification/io_evolution/consumers/f407-i2s2-slave-tx.c',
    'cmake/package/verification/io_evolution/consumers/native-time-empty.c',
    'cmake/package/verification/io_evolution/consumers/native-time-pair.c',
    'cmake/package/verification/io_evolution/f407zg-eth-frames.toml',
    'cmake/package/verification/io_evolution/native-time-empty.toml',
    'cmake/package/verification/io_evolution/native-time-pair.toml',
)


# Host consumption is an explicit SDK feature, not another production SoC.
_HOST_VERIFICATION_FILES = tuple(name for name in REQUIRED_FILES
    if name.startswith("cmake/package/verification/") and (
        "/native/" in name or PurePosixPath(name).name.startswith("native")))
REQUIRED_FILES = tuple(name for name in REQUIRED_FILES
                       if name not in _HOST_VERIFICATION_FILES)
HOST_SUPPORT_FILES = tuple(dict.fromkeys((*HOST_SUPPORT_FILES,
                                          *_HOST_VERIFICATION_FILES)))

# Generator code identity has one authority; packaging consumes its closure.
REQUIRED_FILES = tuple(dict.fromkeys((*REQUIRED_FILES,
    *(name for mode in ("platform", "runtime")
      for name in configuration_dependencies.source_names(mode, None, sdk=True)))))

for _provider in provider_descriptors.families():
    REQUIRED_FILES = tuple(dict.fromkeys((*REQUIRED_FILES,
        *_provider.fact_files, *_provider.owned_payload)))


def source_profile(*, backend="baremetal", components=(), host_support=False):
    """One canonical, explicit supplier scope for a relocatable F4 SDK."""
    if type(host_support) is not bool:
        raise SDKError("SDK host support requires an explicit boolean")
    try:
        from tools.maintenance.source_catalogue import runtime_selection
        runtime_selection("stm32f4", backend=backend, components=components)
    except ValueError as error:
        raise SDKError("Invalid source SDK selection: " + str(error)) from error
    return {"backend": backend, "components": sorted(components),
            "host_support": host_support}


def checked_profile(value):
    if not isinstance(value, dict) or set(value) != {
            "backend", "components", "host_support"}:
        raise SDKError("Source SDK requires its exact dependency selection")
    result = source_profile(**value)
    if result != value:
        raise SDKError("Source SDK dependency selection is not canonical")
    return result


def dependency_required_files(root, profile):
    """Derive supplier payload requirements from the same reviewed locks."""
    names = set()
    for path, item in runtime_dependency_closure(profile).items():
        names.add(path + "/" + item.notice)
        if path == CMSIS_CORE_DIRECTORY:
            names.update(path + "/CMSIS/Core/Include/" + name
                         for name in CMSIS_CORE_REQUIRED_HEADERS)
        elif path == "vendors/st/cmsis_device_f4":
            names.update(path + "/" + name for name in (
                "Include/stm32f4xx.h", "Include/stm32f407xx.h",
                "Include/system_stm32f4xx.h",
                "Source/Templates/gcc/startup_stm32f407xx.s"))
        elif path == stm32f4_hal_lock.DIRECTORY:
            lock = parse_json(regular(root, stm32f4_hal_lock.LOCK_NAME).read_bytes())
            names.update(path + "/" + name for name in stm32f4_hal_lock.contract(lock))
        elif path == "ext/freertos":
            names.update(FREERTOS_REQUIRED_FILES)
        elif path == "ext/threadx":
            lock = parse_json(regular(root, "dependencies/threadx.lock.json").read_bytes())
            names.update(path + "/" + name for name in threadx_lock.contract(lock))
        elif path in {"ext/lwip", "ext/coremqtt", "ext/mcuboot"}:
            verifier = {"ext/lwip": lwip_lock, "ext/coremqtt": coremqtt_lock,
                        "ext/mcuboot": mcuboot_lock}[path]
            lock_path = dependency(path).source_lock
            lock = parse_json(regular(root, lock_path).read_bytes())
            names.update(path + "/" + name for name in verifier.contract(lock)["files"])
        elif path in {mount.path for mount in mbedtls_lock.mounts()}:
            lock = mbedtls_lock.load_lock(root / mbedtls_lock.LOCK_NAME)
            names.update(path + "/" + name for name in mbedtls_lock.record(lock, path)["files"])
    return tuple(sorted(names))


def required_files(root, profile=None):
    """Require current platform sources and the explicitly selected suppliers."""
    profile = source_profile() if profile is None else checked_profile(profile)
    return tuple(dict.fromkeys((*REQUIRED_FILES,
        *(HOST_SUPPORT_FILES if profile["host_support"] else ()),
        *dependency_required_files(Path(root), profile))))


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


def git_environment(*, parent_index=False):
    """Read actual objects; only parent snapshot operations inherit its index."""
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("GIT_")}
    environment.update({"GIT_CONFIG_NOSYSTEM": "1",
                        "GIT_CONFIG_GLOBAL": os.devnull,
                        "GIT_OPTIONAL_LOCKS": "0",
                        "GIT_NO_REPLACE_OBJECTS": "1"})
    if parent_index and "GIT_INDEX_FILE" in os.environ:
        environment["GIT_INDEX_FILE"] = os.environ["GIT_INDEX_FILE"]
    return environment


def git(root, *args, parent_index=False):
    result = subprocess.run(["git", "-c", "core.fsmonitor=false", "-C",
                             str(root), *args], capture_output=True,
                            env=git_environment(parent_index=parent_index))
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
    index = (git(root, "write-tree", parent_index=True).decode().strip()
             if development_fixture else None)
    return tree_entries(root, index if index is not None else "HEAD"), index


def verify_preparation_snapshot(root, revision, index):
    """Reject source or staged dependency changes across package preparation."""
    if git(root, "rev-parse", "HEAD").decode().strip() != revision:
        raise SDKError("Source commit changed while preparing the package")
    if index is not None and git(root, "write-tree", parent_index=True).decode().strip() != index:
        raise SDKError("Source index changed while preparing the development fixture")


def dependency_entries(root, dependency):
    """Redistribute only the locked source closure for reviewed imports."""
    entries = tree_entries(root / dependency)
    if dependency == CMSIS_CORE_DIRECTORY:
        prefix = "CMSIS/Core/Include/"
        required = {prefix + name for name in CMSIS_CORE_REQUIRED_HEADERS}
        required.add("LICENSE")
        if not required.issubset(entries):
            raise SDKError("CMSIS Core committed header/notice graph is incomplete")
        # Compiler and architecture headers contain recursive includes. Keep the
        # complete reviewed Include tree, never templates or RTOS components.
        entries = {name: entry for name, entry in entries.items()
                   if name == "LICENSE" or name.startswith(prefix)}
        if any(mode not in ("100644", "100755") or kind != "blob"
               for mode, kind, _ in entries.values()):
            raise SDKError("CMSIS Core payload must contain regular tracked files")
    elif dependency == "ext/threadx":
        raw, _ = snapshot(root / "dependencies/threadx.lock.json")
        lock = parse_json(raw)
        names = threadx_lock.contract(lock) | {lock["license"]["path"]}
        if not names.issubset(entries):
            raise SDKError("ThreadX committed source graph is incomplete")
        entries = {name: entries[name] for name in names}
    elif dependency == "ext/lwip":
        raw, _ = snapshot(root / "dependencies/lwip.lock.json")
        names = set(lwip_lock.contract(parse_json(raw))["files"])
        if not names.issubset(entries):
            raise SDKError("lwIP committed source graph is incomplete")
        entries = {name: entries[name] for name in names}
    elif dependency == "ext/mcuboot":
        raw, _ = snapshot(root / "dependencies/mcuboot.lock.json")
        names = set(mcuboot_lock.contract(parse_json(raw))["files"])
        if not names.issubset(entries):
            raise SDKError("MCUboot committed source graph is incomplete")
        entries = {name: entries[name] for name in names}
    elif dependency == "ext/coremqtt":
        raw, _ = snapshot(root / "dependencies/coremqtt.lock.json")
        names = set(coremqtt_lock.contract(parse_json(raw))["files"])
        if not names.issubset(entries):
            raise SDKError("coreMQTT committed source graph is incomplete")
        entries = {name: entries[name] for name in names}
    elif dependency in {item.path for item in mbedtls_lock.mounts()}:
        lock = mbedtls_lock.load_lock(root / mbedtls_lock.LOCK_NAME)
        names = set(mbedtls_lock.record(lock, dependency)["files"])
        if not names.issubset(entries):
            raise SDKError("TLS committed static source closure is incomplete")
        entries = {name: entries[name] for name in names}
    elif dependency == "vendors/st/cmsis_device_g4":
        raw, _ = snapshot(root / "dependencies/stm32g4.lock.json")
        names = stm32g4_lock.contract(parse_json(raw))
        if not names.issubset(entries):
            raise SDKError("G4 committed source graph is incomplete")
        entries = {name: entries[name] for name in names}
    elif dependency == stm32h5_device_lock.DIRECTORY:
        raw, _ = snapshot(root / stm32h5_device_lock.LOCK_NAME)
        names = stm32h5_device_lock.contract(parse_json(raw))
        if not names.issubset(entries):
            raise SDKError("H563 committed device source graph is incomplete")
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
        selected_names = set()
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            if dependency != review["directory"]:
                continue
            raw, _ = snapshot(root / review["lock_name"])
            lock = parse_json(raw)
            names = cmsis_device_lock.contract(lock)
            if lock["selected_part"] != part or not names.issubset(entries):
                raise SDKError("Reviewed device committed source graph is incomplete")
            selected_names.update(names)
        if selected_names:
            entries = {name: entries[name] for name in selected_names}
    return entries


def selected(name, *, root=SOURCE_ROOT, profile=None):
    profile = source_profile() if profile is None else checked_profile(profile)
    if name in HOST_SUPPORT_FILES or name.startswith("tests/"):
        return profile["host_support"] and name in HOST_SUPPORT_FILES
    if name.startswith("cmake/package/verification/external_provider/") or name.startswith(
            "cmake/package/verification/common_services/external/"):
        return False
    if name.startswith("cmake/package/verification/io_evolution/"):
        if "native-time-" in name and not profile["host_support"]:
            return False
        return name in IO_EVOLUTION_VERIFICATION_FILES
    if name.startswith(("tools/testing/", "tools/configure/tests/",
                        "tools/nxconfig/")):
        return False
    if name.startswith(("tools/configure/test_", "scripts/ci/test_")):
        return False
    if name.startswith(("dependencies/source/", "cmake/testing/", "cmake/presets/")):
        return False
    if re.search(r"(?:^|/)(?:gd32|gd470|g474|h563|h743|h747|u575)", name):
        return False
    return name in ROOT_FILES or name.split("/", 1)[0] in ROOT_DIRS


def snapshot_files(source, entries, development_fixture=False, *, profile=None):
    """Select committed sources, or an explicit nonpublishable worktree view.

    Staged additions are tracked by the index but absent from HEAD and from
    Git's untracked list. Include both index and untracked paths only in the
    development overlay; publishable payloads retain committed-blob identity.
    """
    names = {name for name, (_, kind, _) in entries.items()
             if kind == "blob" and selected(name, root=source, profile=profile)}
    if development_fixture:
        names.update(name.decode() for name in git(
            source, "ls-files", "--cached", "--others", "--exclude-standard",
            "-z", parent_index=True).split(b"\0")
                     if name and selected(name.decode(), root=source, profile=profile))
    return names


def runtime_dependency_closure(profile=None):
    """Expand only the selected F4 suppliers and required nested sources."""
    from tools.maintenance.source_catalogue import runtime_selection
    profile = source_profile() if profile is None else checked_profile(profile)
    records = {}

    def append(item):
        if item.path in records or item.scope != "runtime":
            raise SDKError("Invalid or duplicate runtime supplier selection")
        records[item.path] = item
        # F4 uses the built-in kernel port, not third-party port repositories.
        if item.identifier != "freertos":
            for child in children(item.path):
                append(child)

    for item in runtime_selection("stm32f4", backend=profile["backend"],
                                  components=profile["components"]):
        append(item)
    return records


def dependency_declarations(root, item, entries=None):
    """Bind recursive transport to reviewed declarations and committed links."""
    descendants = children(item.path)
    module_file = root / item.path / ".gitmodules"
    if descendants or module_file.exists() or module_file.is_symlink():
        try:
            declared_modules(regular(root, item.path + "/.gitmodules").read_bytes(),
                             parent=item.path)
        except ValueError as error:
            raise SDKError("Invalid recursive dependency declaration: " +
                           item.path) from error
    if entries is not None:
        links = {name: row[2] for name, row in entries.items()
                 if row[1] == "commit"}
        try:
            verify_gitlinks(links, item.path)
        except ValueError as error:
            raise SDKError("Unregistered or changed recursive dependency Gitlink") from error


def dependency_records(root, entries, profile=None):
    declared_modules(regular(root, ".gitmodules").read_bytes())
    try:
        verify_gitlinks({name: row[2] for name, row in entries.items()
                         if row[1] == "commit"}, None)
    except ValueError as error:
        raise SDKError("Unregistered or missing top-level dependency Gitlink") from error
    closure = runtime_dependency_closure(profile)
    records = []
    parent_entries = {None: entries}
    for path, item in closure.items():
        row = parent_entries[item.parent].get(relative_path(item))
        if not row or row[:2] != ("160000", "commit"):
            raise SDKError(f"Source commit has no required dependency Gitlink: {path}")
        checkout = root / safe_relative(path)
        if (checkout.resolve() != checkout or not (checkout / ".git").exists()
                or Path(git(checkout, "rev-parse", "--show-toplevel").decode()
                        .strip()).resolve() != checkout):
            raise SDKError(f"Initialize the pinned dependency before packaging: {path}")
        actual = git(checkout, "rev-parse", "HEAD").decode().strip()
        if actual != row[2]:
            raise SDKError(f"Initialize the pinned dependency before packaging: {path}")
        if git(checkout, "status", "--porcelain", "--untracked-files=all").strip():
            raise SDKError(f"Dependency checkout is dirty: {path}")
        origins = git(checkout, "config", "--local", "--get-all",
                      "remote.origin.url").decode().splitlines()
        if origins != [item.fetch_url]:
            raise SDKError(f"Dependency fetch origin is not approved: {path}")
        committed = tree_entries(checkout)
        dependency_declarations(root, item, committed)
        parent_entries[path] = committed
        if item.identifier == "freertos":
            freertos_transport.verify(root, selected_commit=actual,
                                      require_git=True, require_children=False)
        records.append({"path": path, "commit": actual,
                        "tree": git(checkout, "rev-parse", "HEAD^{tree}")
                        .decode().strip()})
    try:
        verify_alias_identities(records)
    except ValueError as error:
        raise SDKError("Dependency alias pin/tree differs from canonical mount") from error
    return records


def committed_blobs(root, entries):
    """Read Git blobs without checkout EOL/export transformations."""
    names = sorted(entries)
    request = "".join(entries[name][2] + "\n" for name in names).encode()
    result = subprocess.run(["git", "-c", "core.fsmonitor=false", "-C",
                             str(root), "cat-file", "--batch"],
                            input=request, capture_output=True,
                            env=git_environment())
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


def reviewed_import_directories():
    """Use maintained family declarations, never discover an installed file set."""
    return tuple(sorted({Path(item.import_lock).parent.as_posix()
                         for item in provider_descriptors.families()
                         if item.import_lock is not None}))


def validate_import_records(identity):
    """Require every reviewed import exactly once and bind its source tree."""
    records = identity["reviewed_imports"]
    expected = set(reviewed_import_directories())
    if (not isinstance(records, list) or len(records) != len(expected) or
            any(not isinstance(record, dict) or
                not isinstance(record.get("path"), str) or
                record.get("source_tree") != identity["source_tree"]
                for record in records) or
            {record["path"] for record in records} != expected):
        raise SDKError("Source SDK requires exact reviewed imports and owner tree")
    return records


def verify_selected_dependencies(root, records, *, source_sdk=False):
    """Run full byte/provenance admission only for packaged suppliers."""
    records = {record["path"]: record for record in records}
    if "ext/freertos" in records:
        freertos_transport.verify(root, selected_commit=records["ext/freertos"]["commit"],
                                  require_git=not source_sdk, require_children=False)
    if "ext/threadx" in records:
        threadx_lock.verify(root / "ext/threadx", root / "dependencies/threadx.lock.json",
                            "linux", **({"packaged_commit": records["ext/threadx"]["commit"]}
                                        if source_sdk else {}))
    for path, verifier, lock_name in (
            ("ext/lwip", lwip_lock, "dependencies/lwip.lock.json"),
            ("ext/coremqtt", coremqtt_lock, "dependencies/coremqtt.lock.json"),
            ("ext/mcuboot", mcuboot_lock, "dependencies/mcuboot.lock.json")):
        if path not in records:
            continue
        record = records[path]
        if source_sdk and (record["commit"] != verifier.COMMIT or record["tree"] != verifier.TREE):
            raise SDKError("Source SDK supplier identity differs from reviewed source: " + path)
        verifier.verify(root / path, root / lock_name,
                        **({"source_sdk": True} if source_sdk else {}))
    if "ext/mbedtls" in records:
        mbedtls_lock.verify(root / "ext/mbedtls", root / mbedtls_lock.LOCK_NAME,
                            **({"source_sdk": True, "dependency_records": list(records.values())}
                                if source_sdk else {}))
    for official in OFFICIAL_DRIVERS:
        if official.DIRECTORY not in records:
            continue
        arguments = ({"packaged_commit": records[official.DIRECTORY]["commit"],
                      "packaged_cmsis_commit": records[official.CMSIS_DIRECTORY]["commit"]}
                     if source_sdk else {})
        official.verify(root / official.DIRECTORY, root / official.LOCK_NAME,
                        root / official.CMSIS_DIRECTORY, **arguments)


def prepare(source, output, development_fixture=False, *, profile=None):
    profile = source_profile() if profile is None else checked_profile(profile)
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
    dirty = bool(git(source, "status", "--porcelain", "--untracked-files=normal",
                     parent_index=True).strip())
    if dirty and not development_fixture:
        raise SDKError("Publishable source SDK preparation rejects a dirty source checkout")
    entries = tree_entries(source)
    dependency_graph, index = dependency_snapshot(source, development_fixture)
    dependencies = dependency_records(source, dependency_graph, profile)
    verify_selected_dependencies(source, dependencies)
    sys.path.insert(0, str(source / "scripts/ci"))
    from vendor_import_identity import reviewed_import
    imports = [reviewed_import(source / directory, source)
               for directory in reviewed_import_directories()]
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".nexus-source-sdk-", dir=output.parent))
    sdk = temporary / "share/nexus/src"
    sdk.mkdir(parents=True)
    files = {}
    try:
        names = snapshot_files(source, entries, development_fixture, profile=profile)
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
        for name in required_files(source, profile):
            if name not in files:
                raise SDKError(f"Source SDK is incomplete: {name}")
        verify_preparation_snapshot(source, revision, index)
        manifest = {"schema": 2, "profile": profile, "kind": "nexus-installed-source-sdk",
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
        if dependency_records(source, dependency_graph, profile) != dependencies:
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
    fields = {"schema", "profile", "kind", "source_revision", "source_tree", "publishable", "source_dirty",
              "files_sha256", "dependencies", "reviewed_imports", "toolchains_lock_sha256", "snapshot_sha256",
              "package_files_sha256"}
    if (set(identity) != fields or type(identity["schema"]) is not int or
            identity["schema"] != 2 or identity["kind"] != "nexus-installed-source-sdk"):
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
    # Missing mandatory names already disqualify the manifest. Reject before
    # reading payload bytes; accepted manifests still undergo every check below.
    raw_profile = identity["profile"]
    if not isinstance(raw_profile, dict) or set(raw_profile) != {
            "backend", "components", "host_support"} or type(
                raw_profile["host_support"]) is not bool:
        raise SDKError("Source SDK requires its exact dependency selection")
    early_required = (*REQUIRED_FILES,
        *(HOST_SUPPORT_FILES if raw_profile["host_support"] else ()))
    if any(name not in files for name in early_required):
        raise SDKError("Installed source SDK is missing required sources/licenses")
    profile = checked_profile(raw_profile)
    if any(name not in files for name in required_files(root, profile)):
        raise SDKError("Installed source SDK is missing required sources/licenses")
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
    closure = runtime_dependency_closure(profile)
    if paths != set(closure):
        raise SDKError("Source SDK dependency set differs from the reviewed catalogue")
    for name in files:
        if name.startswith(("vendors/", "ext/")) and not any(
                name.startswith(path + "/") for path in closure):
            raise SDKError("Unselected supplier payload in source SDK: " + name)
    try:
        verify_alias_identities(records)
    except ValueError as error:
        raise SDKError("Dependency alias pin/tree differs from canonical mount") from error
    declared_modules(regular(root, ".gitmodules").read_bytes())
    for item in closure.values():
        dependency_declarations(root, item)
    verify_selected_dependencies(root, records, source_sdk=True)
    sys.path.insert(0, str(root / "scripts/ci"))
    from vendor_import_identity import validate_import_record
    from verify_vendor_source import verify as verify_vendor_import
    for record in validate_import_records(identity):
        imported = root / record["path"]
        notices = {item["path"]: regular(imported, item["path"]).read_bytes()
                   for item in record.get("notices", [])}
        lock_contents = regular(imported, "source.lock.json").read_bytes()
        validate_import_record(record, lock_contents, notices,
                               identity["source_revision"])
        # A rehashed package map cannot replace the reviewed upstream closure.
        # Reuse the same byte verifier used during preparation, then close its
        # auxiliary file set against the recorded origin and license notices.
        verify_vendor_import(imported)
        expected = {"source.lock.json", *notices,
                    *(item["path"] for item in parse_json(lock_contents)["files"])}
        actual_import = {path.relative_to(imported).as_posix()
                         for path in imported.rglob("*")
                         if path.is_file() or path.is_symlink()}
        if actual_import != expected:
            raise SDKError("Source SDK contains files outside the reviewed import")
    for name in required_files(root, profile):
        if name not in files:
            raise SDKError("Installed source SDK is missing required "
                           "sources/licenses: " + name)
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
    parser.add_argument("--backend", choices=("baremetal", "freertos", "threadx"),
                        default="baremetal")
    parser.add_argument("--component", action="append", default=[])
    parser.add_argument("--host-support", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.verify:
            if (args.source or args.output or args.development_fixture or args.component or
                    args.host_support or args.backend != "baremetal"):
                raise SDKError("Verification cannot also prepare a source package")
            result = verify(args.verify)
        elif args.source and args.output:
            result = prepare(args.source, args.output, args.development_fixture,
                             profile=source_profile(backend=args.backend,
                                 components=args.component, host_support=args.host_support))
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
