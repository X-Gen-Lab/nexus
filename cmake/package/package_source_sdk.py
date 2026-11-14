#!/usr/bin/env python3
"""Prepare/verify a relocatable source SDK with fixed dependency provenance.

No compiler or effective build configuration is embedded as a reusable binary
ABI. This package is an unsigned source snapshot, not a promoted release.
"""
import argparse
from dataclasses import dataclass
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
        cmsis_device_lock, stm32f4_hal_lock,
        threadx_lock, freertos_transport, lwip_lock, mbedtls_lock,
        coremqtt_lock, mcuboot_lock, st_usb_lock,
    )
    from tools.configure.metadata import parse_json, snapshot
    from tools.configure import dependencies as configuration_dependencies
    from tools.configure import packages as source_packages
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
SDK_TOOL_FILES = (
    'tools/configure/documentation.py',
    'tools/configure/matrix.py',
    'scripts/ci/vendor_import_identity.py',
    'scripts/ci/verify_vendor_source.py',
    'scripts/validation/junit.py',
    'tools/__init__.py',
    'tools/dev/artifacts.py',
    'tools/dev/build_lock.py',
    'tools/dev/cache.py',
    'tools/dev/change_paths.py',
    'tools/dev/dependencies.py',
    'tools/dev/dev.py',
    'tools/dev/device.py',
    'tools/dev/presets.py',
    'tools/dev/process.py',
    'tools/dev/projects.py',
    'tools/dev/provenance.py',
    'tools/dev/serial.py',
    'tools/dev/timings.py',
    'tools/evidence/common.py',
    'tools/evidence/identity.py',
    'tools/maintenance/capabilities.py',
    'tools/maintenance/cmsis_device_lock.py',
    'tools/maintenance/consumer_profiles.py',
    'tools/maintenance/coremqtt_lock.py',
    'tools/maintenance/freertos_transport.py',
    'tools/maintenance/lwip_lock.py',
    'tools/maintenance/mbedtls_lock.py',
    'tools/maintenance/mcuboot_lock.py',
    'tools/maintenance/source_catalogue.py',
    'tools/maintenance/st_usb_lock.py',
    'tools/maintenance/stm32f4_hal_lock.py',
    'tools/maintenance/threadx_lock.py',
    'tools/measurement/elf.py',
    'tools/measurement/resources.py',
    'tools/measurement/retention.py',
    'tools/measurement/workload.c',
)

SDK_VERIFICATION_FILES = (
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
    'cmake/package/verification/smbus/board/board.json',
    'cmake/package/verification/smbus/f407.toml',
    'cmake/package/verification/smbus/services.c',
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
    'tests/support/native/drivers/chip.c',
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
_HOST_VERIFICATION_FILES = tuple(name for name in SDK_VERIFICATION_FILES
    if name.startswith("cmake/package/verification/") and (
        "/native/" in name or PurePosixPath(name).name.startswith("native")))
HOST_SUPPORT_FILES = tuple(dict.fromkeys((*HOST_SUPPORT_FILES,
                                          *_HOST_VERIFICATION_FILES)))

# Reviewed software consumers have an explicit scope, separate from the
# production package roots and host test support.
F4_FAMILY_VERIFICATION_FILES = (
    "tests/contracts/crc_assembly/consumer.c",
    "tests/contracts/stm32f4_family/CMakeLists.txt",
    *(f"tests/contracts/stm32f4_family/f{chip}zg.toml"
      for chip in ("405", "407", "415", "417")),
    *(f"tests/contracts/stm32f4_family/f{chip}zg-board/board.json"
      for chip in ("405", "407", "415", "417")),
    *(f"tests/contracts/stm32f4_family/{stem}.toml"
      for stem in ("f411re", "f446ze", "f479ni")),
    *(f"tests/contracts/stm32f4_family/{stem}-board/board.json"
      for stem in ("f411re", "f446ze", "f479ni")),
    *(f"tests/contracts/stm32f4_power/{name}"
      for name in ("consumer.c", "CMakeLists.txt", "README.md",
          *(f"{part}-{mode}.toml" for part in ("f411re", "f446ze", "f479ni")
            for mode in ("adopt-retained", "provision-empty")),
          *(f"{part}-board/board.json" for part in ("f411re", "f446ze", "f479ni")))),
    *(f"tests/contracts/stm32f4_crypto/{name}"
      for name in ("consumer.c", "CMakeLists.txt", "f415zg.toml", "f417zg.toml",
                   "f415zg-dma.toml", "f417zg-dma.toml",
                   "f415zg-board/board.json", "f417zg-board/board.json")),
    *(f"tests/contracts/stm32f4_sdram/{name}"
      for name in ("consumer.c", "CMakeLists.txt", "f479ni-sdram.toml",
                   "f479ni-sdram-board/board.json")),
    *(f"tests/contracts/stm32f4_qspi/{name}"
      for name in ("consumer.c", "CMakeLists.txt", "f479ni-qspi.toml",
                   "f479ni-qspi-dma.toml", "f479ni-qspi-quad.toml",
                   "f479ni-qspi-quad-dma.toml", "f479ni-qspi-board/board.json",
                   "f479ni-qspi-quad-board/board.json",
                   "mapped_consumer.c", "f479ni-qspi-map.toml",
                   "f479ni-qspi-map-board/board.json")),
    *(f"tests/contracts/stm32f4_sdio_hs/{name}"
      for name in ("consumer.c", "CMakeLists.txt", "sd37-dma.toml",
                   "mmc37-irq.toml", "board/board.json")),
    *(f"tests/contracts/f407_capture_audio/{name}"
      for name in ("CMakeLists.txt", "frame_session.c", "controller_main.c",
                   "ve-frames-raw.toml", "ve_board/board.json")),
    *(f"tests/contracts/stm32f4_graphics/{name}"
      for name in ("consumer.c", "CMakeLists.txt", "combined.toml",
                   "dual-rgb888.toml", "image-only.toml", "board/board.json",
                   "sustained_consumer.c", "sustained_sequence.c",
                   "sustained_sequence.h")),
    *(f"tests/contracts/stm32f4_graphics_sdram/{name}"
      for name in ("CMakeLists.txt", "combined.toml", "board/board.json")),
    *(f"tests/contracts/stm32f4_sai/{name}"
      for name in ("consumer.c", "blocks_consumer.c", "CMakeLists.txt", "README.md",
                   "f446ze-master.toml", "f446ze-slave.toml",
                   "f479ni-master.toml", "f479ni-slave.toml",
                   "f446ze-sai2-master-blocks.toml",
                   "f446ze-sai2-slave-blocks.toml",
                   "f479ni-sparse-master-blocks.toml",
                   "f479ni-sparse-slave-blocks.toml",
                   "f446ze-board/board.json", "f479ni-board/board.json")),
)
@dataclass(frozen=True)
class SourceSelection:
    """One input-root selection with frozen package metadata and file receipts."""

    root: Path
    backend: str
    components: tuple
    host_support: bool
    packages: tuple
    inputs: tuple
    semantic_version: str | None
    configuration_files: frozenset

    @property
    def profile(self):
        return {"backend": self.backend, "components": list(self.components),
                "host_support": self.host_support,
                "packages": [item.id for item in self.packages]}


def _requested_ids(value):
    if (not isinstance(value, (tuple, list)) or not value or
            any(not isinstance(item, str) or
                re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]{0,63}", item) is None
                for item in value) or len(set(value)) != len(value)):
        raise SDKError("SDK packages require explicit unique package IDs")
    return tuple(sorted(value))


def _packages_for_root(root, identifiers, package_records=None):
    """Load defaults directly; explicit discovery occurs only for other IDs."""
    sdk_manifest = root / MANIFEST
    policy_path = root / "dependencies/platform-compatibility.json"
    policy_inputs = []
    semantic_version = None
    installed_scope = package_records is not None or sdk_manifest.is_file()
    if package_records is not None:
        paths = tuple(root / safe_relative(item["manifest"]) for item in package_records)
    elif sdk_manifest.is_file():
        identity = parse_json(sdk_manifest.read_bytes())
        if not isinstance(identity, dict) or "source_exports" not in identity:
            raise SDKError("Missing installed source SDK package declaration")
        exports = identity["source_exports"]
        check_source_exports(exports, identity.get("files_sha256", {}))
        paths = tuple(root / safe_relative(item["manifest"])
                      for item in exports["packages"])
    elif policy_path.is_file():
        raw, receipt = stable_file(root, policy_path.relative_to(root).as_posix())
        from tools.configure.metadata import policy_contract
        policy = policy_contract(parse_json(raw))
        semantic_version = policy["semantic_version"]
        paths = tuple(root / safe_relative(name) for name in
                      policy["source_sdk"]["default_package_manifests"])
        policy_inputs.append((policy_path.relative_to(root).as_posix(), receipt))
    elif identifiers is not None:
        paths = ()
    else:
        raise SDKError("The input root has no default SDK package policy")
    defaults = tuple(source_packages.load(path) for path in paths)
    if len({item.id for item in defaults}) != len(defaults):
        raise SDKError("Duplicate default SDK package ID")
    requested = (_requested_ids(identifiers) if identifiers is not None else
                 _requested_ids([item.id for item in defaults]))
    by_id = {item.id: item for item in defaults}
    if not set(requested).issubset(by_id):
        # A maintained SDK is closed to its recorded packages. A checkout may
        # explicitly select another declared package, without expanding defaults.
        if installed_scope:
            raise SDKError("SDK package is outside the installed source scope")
        by_id = {item.id: item for item in source_packages.discover(root)}
    if not set(requested).issubset(by_id):
        raise SDKError("Unknown selected SDK package ID")
    return tuple(by_id[name] for name in requested), tuple(policy_inputs), semantic_version


def resolve_selection(root, profile=None, *, backend="baremetal", components=(),
                      host_support=False, packages=None, entries=None,
                      development_fixture=False, package_records=None):
    """Capture one selection; callers pass it through all packaging stages."""
    root = Path(root).absolute()
    if profile is not None:
        if not isinstance(profile, dict) or set(profile) != {
                "backend", "components", "host_support", "packages"}:
            raise SDKError("Source SDK requires its exact dependency selection")
        backend, components = profile["backend"], profile["components"]
        host_support, packages = profile["host_support"], profile["packages"]
    if type(host_support) is not bool:
        raise SDKError("SDK host support requires an explicit boolean")
    chosen, policy_inputs, semantic_version = _packages_for_root(
        root, packages, package_records)
    from tools.maintenance.source_catalogue import runtime_selection
    try:
        for package in chosen:
            runtime_selection(package.family, backend=backend,
                              components=components, package=package)
    except ValueError as error:
        raise SDKError("Invalid source SDK selection: " + str(error)) from error
    inputs = dict(policy_inputs)
    for package in chosen:
        # Only the manifest controls SDK selection. Facts and CMake source
        # bytes are copied from the source tree and independently verified.
        for path in (package.manifest_path, package.fact("soc")):
            name = path.relative_to(root).as_posix()
            _, receipt = stable_file(root, name)
            if path == package.manifest_path and receipt[0] != package.manifest_sha256:
                raise SDKError("Package manifest changed during selection")
            if path == package.fact("soc") and receipt[0] != package.source_fact_sha256:
                raise SDKError("Package source fact changed during selection")
            if name in inputs and inputs[name] != receipt:
                raise SDKError("Shared package metadata changed during selection")
            inputs[name] = receipt
    if entries is not None and not development_fixture:
        selected_entries = {}
        for name in inputs:
            row = entries.get(name)
            if row is None or row[0] not in {"100644", "100755"} or row[1] != "blob":
                raise SDKError("Package metadata is outside the committed source tree")
            selected_entries[name] = row
        blobs = committed_blobs(root, selected_entries)
        if any(digest(blobs[name]) != receipt[0] for name, receipt in inputs.items()):
            raise SDKError("Package metadata differs from the committed source tree")
    configuration_files = set(CONFIGURATION_FILES)
    for package in chosen:
        # source_names owns the transition from a selected legacy adapter to
        # a pure-data package. Unknown/data families keep the common closure
        # without loading any maintained provider or enumerating families.
        configuration_files.update(configuration_dependencies.source_names(
            "platform", package.family, sdk=True))
    selection = SourceSelection(root, backend, tuple(sorted(components)), host_support,
                                chosen, tuple(sorted(inputs.items())), semantic_version,
                                frozenset(configuration_files))
    if profile is not None and selection.profile != profile:
        raise SDKError("Source SDK dependency selection is not canonical")
    return selection


def source_profile(*, backend="baremetal", components=(), host_support=False,
                   packages=None, root=SOURCE_ROOT):
    """Resolve explicit package IDs against this input root's default policy."""
    return resolve_selection(root, backend=backend, components=components,
                             host_support=host_support, packages=packages).profile


def checked_profile(value, *, root=SOURCE_ROOT):
    return resolve_selection(root, value).profile


def dependency_required_files(root, profile, *, selection=None):
    """Derive supplier payload requirements from the same reviewed locks."""
    names = set()
    for path, item in runtime_dependency_closure(
            profile, root=root, selection=selection).items():
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
        elif path in {"vendors/st/usb_host", "vendors/st/usb_device"}:
            lock = parse_json(regular(root, item.source_lock).read_bytes())
            names.update(path + "/" + name for name in lock["files"])
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


def required_files(root, profile=None, *, selection=None):
    """Require the selected root's sources, tool closure and suppliers."""
    root = Path(root).absolute()
    if selection is None:
        selection = resolve_selection(root, profile)
        members = _required_source_members(root, selection=selection)
    else:
        # prepare already selected every owned Git source member; verify checks
        # its frozen source_exports. Avoid a second live discovery/selection.
        members = tuple(name for name, _ in selection.inputs) + tuple(
            path.relative_to(root).as_posix() for package in selection.packages
            for path in package.inputs)
    return tuple(sorted(set(members) | selection.configuration_files | set(SDK_TOOL_FILES) |
        set(ROOT_FILES) | set(HOST_SUPPORT_FILES if selection.host_support else ()) |
        set(dependency_required_files(root, selection.profile, selection=selection))))


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


def verify_preparation_snapshot(root, revision, index, *, selection=None, files=()):
    """Reject source or staged dependency changes across package preparation."""
    if git(root, "rev-parse", "HEAD").decode().strip() != revision:
        raise SDKError("Source commit changed while preparing the package")
    if index is not None and git(root, "write-tree", parent_index=True).decode().strip() != index:
        raise SDKError("Source index changed while preparing the development fixture")
    if index is None and git(root, "status", "--porcelain",
                             "--untracked-files=normal", parent_index=True).strip():
        raise SDKError("Publishable source changed while preparing the package")
    for name, receipt in (*(selection.inputs if selection is not None else ()), *files):
        if stable_file(root, name)[1] != receipt:
            raise SDKError("Source input changed while preparing the package: " + name)


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


def _selected(name, root, profile, packages, configuration_files):
    if any(package.import_lock is not None and
           PurePosixPath(name).is_relative_to(PurePosixPath(package.import_lock).parent)
           for package in packages):
        return True
    if name in F4_FAMILY_VERIFICATION_FILES:
        return True
    if name in HOST_SUPPORT_FILES or name.startswith("tests/"):
        return profile["host_support"] and name in HOST_SUPPORT_FILES
    if name.startswith("tools/configure/assemblies/"):
        return name.endswith((".toml", ".json"))
    if name.startswith("tools/configure/"):
        return (name in configuration_files or name in SDK_TOOL_FILES or
                name.endswith(".schema.json")
                or name == "tools/configure/README.md")
    if name.startswith(("tools/", "scripts/")):
        return name in SDK_TOOL_FILES or name in configuration_files
    if name.startswith("soc/"):
        return name == "soc/README.md" or any(
            package.exports(root / name) for package in packages)
    if name.startswith("cmake/package/verification/external_provider/") or name.startswith(
            "cmake/package/verification/common_services/external/"):
        return False
    if name.startswith("cmake/package/verification/io_evolution/"):
        if "native-time-" in name and not profile["host_support"]:
            return False
        return name in IO_EVOLUTION_VERIFICATION_FILES
    if name.startswith(("dependencies/source/", "cmake/testing/", "cmake/presets/")):
        return False
    return name in ROOT_FILES or name.split("/", 1)[0] in ROOT_DIRS


def selected(name, *, root=SOURCE_ROOT, profile=None):
    """Apply the selected export ownership, independently of compiled units."""
    root = Path(root).absolute()
    selection = resolve_selection(root, profile)
    return _selected(name, root, selection.profile, selection.packages,
                     selection.configuration_files)


def owned_source_files(root, entries, *, profile=None, selection=None):
    """Expand an exact source-tree view using declared production ownership."""
    root = Path(root).absolute()
    selection = resolve_selection(root, profile) if selection is None else selection
    profile, packages = selection.profile, selection.packages
    return tuple(sorted(name for name, (_, kind, _) in entries.items()
                        if kind == "blob" and
                        not any(part in {".git", "__pycache__"}
                                for part in PurePosixPath(name).parts) and
                        _selected(name, root, profile, packages,
                                  selection.configuration_files)))


def check_source_exports(value, files):
    """Require the frozen source member set before opening payload bytes.

    This is unsigned snapshot consistency. It does not authenticate a source
    when an actor can rewrite the checker and all of its declarations.
    """
    if not isinstance(value, dict) or set(value) != {"members", "packages"}:
        raise SDKError("Invalid source SDK export declaration")
    members, packages = value["members"], value["packages"]
    if (not isinstance(members, list) or not members or
            any(not isinstance(name, str) for name in members) or
            members != sorted(set(members)) or not isinstance(packages, list)):
        raise SDKError("Invalid source SDK export members")
    for name in members:
        safe_relative(name)
    if any(name not in files for name in members):
        raise SDKError("Installed source SDK is missing required sources/licenses")
    ids, manifests = set(), set()
    for package in packages:
        if (not isinstance(package, dict) or set(package) != {"id", "manifest"}
                or not isinstance(package["id"], str) or
                re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]{0,63}", package["id"]) is None):
            raise SDKError("Invalid source SDK package declaration")
        manifest = str(safe_relative(package["manifest"]))
        if (manifest not in members or not manifest.endswith("/package.json") or
                package["id"] in ids or manifest in manifests):
            raise SDKError("Missing or duplicate source SDK package manifest")
        ids.add(package["id"])
        manifests.add(manifest)
    return tuple(members)


def check_selected_source_scope(selection, members, files):
    """Keep unselected SoC sources and undeclared platform files out of SDKs."""
    members = set(members)
    for name in files:
        safe_relative(name)
        if not name.startswith(("vendors/", "ext/")) and name not in members:
            raise SDKError("Unlisted platform source payload in SDK: " + name)
        if (name.startswith("soc/") and name != "soc/README.md" and
                not any(package.exports(selection.root / name)
                        for package in selection.packages)):
            raise SDKError("Unselected SoC package payload in SDK: " + name)


def source_export_declaration(root, members, *, selection=None):
    """Freeze the package identities and exact selected Git source members."""
    root = Path(root).absolute()
    selection = resolve_selection(root) if selection is None else selection
    return {"members": sorted(name for name in members
                              if not name.startswith(("vendors/", "ext/"))),
            "packages": [
        {"id": package.id,
         "manifest": package.manifest_path.relative_to(root).as_posix()}
        for package in selection.packages]}


def snapshot_files(source, entries, development_fixture=False, *, profile=None,
                   selection=None):
    """Select committed sources, or an explicit nonpublishable worktree view.

    Staged additions are tracked by the index but absent from HEAD and from
    Git's untracked list. Include both index and untracked paths only in the
    development overlay; publishable payloads retain committed-blob identity.
    """
    if selection is None:
        selection = resolve_selection(source, profile, entries=entries,
                                      development_fixture=development_fixture)
    names = set(owned_source_files(source, entries, selection=selection))
    if development_fixture:
        current = {name.decode(): ("100644", "blob", "")
                   for name in git(source, "ls-files", "--cached", "--others",
                                   "--exclude-standard", "-z",
                                   parent_index=True).split(b"\0") if name}
        names.update(owned_source_files(source, current, selection=selection))
    return names


def runtime_dependency_closure(profile=None, *, root=SOURCE_ROOT, selection=None):
    """Expand declared package suppliers and explicitly selected services."""
    from tools.maintenance.source_catalogue import runtime_selection
    selection = resolve_selection(root, profile) if selection is None else selection
    profile = selection.profile
    records = {}

    def append(item):
        if item.scope != "runtime":
            raise SDKError("Invalid runtime supplier selection")
        if item.path in records:
            if records[item.path] != item:
                raise SDKError("Conflicting runtime supplier selection")
            return
        records[item.path] = item
        # F4 uses the built-in kernel port, not third-party port repositories.
        if item.identifier != "freertos":
            for child in children(item.path):
                append(child)

    for package in selection.packages:
        for item in runtime_selection(package.family, backend=profile["backend"],
                                      components=profile["components"], package=package):
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


def dependency_records(root, entries, profile=None, *, selection=None):
    declared_modules(regular(root, ".gitmodules").read_bytes())
    try:
        verify_gitlinks({name: row[2] for name, row in entries.items()
                         if row[1] == "commit"}, None)
    except ValueError as error:
        raise SDKError("Unregistered or missing top-level dependency Gitlink") from error
    closure = runtime_dependency_closure(profile, root=root, selection=selection)
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


def reviewed_import_directories(*, root=SOURCE_ROOT, profile=None, selection=None):
    """Use only frozen source-lock facts from the selected package set."""
    selection = resolve_selection(root, profile) if selection is None else selection
    return tuple(sorted({str(safe_relative(package.import_lock).parent)
                         for package in selection.packages
                         if package.import_lock is not None}))


def validate_import_records(identity, *, root=SOURCE_ROOT, selection=None):
    """Require every reviewed import exactly once and bind its source tree."""
    records = identity["reviewed_imports"]
    expected = set(reviewed_import_directories(root=root, selection=selection))
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
    for role in ("device", "host"):
        path = "vendors/st/usb_" + role
        if path not in records:
            continue
        identity = st_usb_lock.verify(root, role, source_sdk=source_sdk)
        if (records[path]["commit"], records[path]["tree"]) != (
                identity["commit"], identity["tree"]):
            raise SDKError("ST USB package identity differs from reviewed source")
    for official in OFFICIAL_DRIVERS:
        if official.DIRECTORY not in records:
            continue
        arguments = ({"packaged_commit": records[official.DIRECTORY]["commit"],
                      "packaged_cmsis_commit": records[official.CMSIS_DIRECTORY]["commit"]}
                     if source_sdk else {})
        official.verify(root / official.DIRECTORY, root / official.LOCK_NAME,
                        root / official.CMSIS_DIRECTORY, **arguments)


def prepare(source, output, development_fixture=False, *, profile=None,
            backend="baremetal", components=(), host_support=False, packages=None):
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
    entries = tree_entries(source, revision)
    selection = resolve_selection(source, profile, backend=backend,
        components=components, host_support=host_support, packages=packages,
        entries=entries, development_fixture=development_fixture)
    profile = selection.profile
    dependency_graph, index = dependency_snapshot(source, development_fixture)
    dependencies = dependency_records(source, dependency_graph, profile,
                                      selection=selection)
    verify_selected_dependencies(source, dependencies)
    sys.path.insert(0, str(source / "scripts/ci"))
    from vendor_import_identity import reviewed_import
    imports = [reviewed_import(source / directory, source)
               for directory in reviewed_import_directories(
                   root=source, selection=selection)]
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".nexus-source-sdk-", dir=output.parent))
    sdk = temporary / "share/nexus/src"
    sdk.mkdir(parents=True)
    files = {}
    source_receipts = []
    try:
        names = snapshot_files(source, entries, development_fixture,
                               profile=profile, selection=selection)
        source_blobs = {} if development_fixture else committed_blobs(source, {name: entries[name] for name in names})
        for name in sorted(names):
            entry = entries.get(name)
            if not (source / name).exists() and development_fixture:
                continue
            if entry and entry[0] not in ("100644", "100755"):
                raise SDKError("Source SDK requires regular Git blobs")
            data = source_blobs.get(name)
            if development_fixture:
                data, receipt = stable_file(source, name)
                source_receipts.append((name, receipt))
            files[name] = copy_file(source, sdk, name, data,
                                    entry[0] == "100755" if entry else None)
        # A development overlay can delete a committed path. Freeze the files
        # actually copied, rather than retaining that old tree member as live.
        source_members = tuple(sorted(files))
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
        for name in required_files(source, profile, selection=selection):
            if name not in files:
                raise SDKError(f"Source SDK is incomplete: {name}")
        verify_preparation_snapshot(source, revision, index,
                                    selection=selection, files=source_receipts)
        manifest = {"schema": 3, "profile": profile, "kind": "nexus-installed-source-sdk",
                    "source_exports": source_export_declaration(source, source_members, selection=selection),
                    "source_revision": revision, "source_tree": source_tree,
                    "publishable": not development_fixture, "source_dirty": dirty,
                    "files_sha256": files, "dependencies": dependencies,
                    "reviewed_imports": imports,
                    "toolchains_lock_sha256": files["dependencies/toolchains.lock.json"],
                    "snapshot_sha256": digest(json.dumps(files, sort_keys=True, separators=(",", ":")).encode())}
        semantic_version = selection.semantic_version
        if semantic_version is None:
            raise SDKError("SDK preparation requires its source compatibility policy")
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
        if dependency_records(source, dependency_graph, profile,
                              selection=selection) != dependencies:
            raise SDKError("Source dependencies changed while preparing the package")
        verify_preparation_snapshot(source, revision, index,
                                    selection=selection, files=source_receipts)
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
              "package_files_sha256", "source_exports"}
    if (set(identity) != fields or type(identity["schema"]) is not int or
            identity["schema"] != 3 or identity["kind"] != "nexus-installed-source-sdk"):
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
    check_source_exports(identity["source_exports"], files)
    # Missing mandatory names already disqualify the manifest. Reject before
    # reading payload bytes; accepted manifests still undergo every check below.
    raw_profile = identity["profile"]
    if not isinstance(raw_profile, dict) or set(raw_profile) != {
            "backend", "components", "host_support", "packages"} or type(
                raw_profile["host_support"]) is not bool:
        raise SDKError("Source SDK requires its exact dependency selection")
    early_required = (*ROOT_FILES, *SDK_TOOL_FILES, *CONFIGURATION_FILES,
        *(HOST_SUPPORT_FILES if raw_profile["host_support"] else ()))
    if any(name not in files for name in early_required):
        raise SDKError("Installed source SDK is missing required sources/licenses")
    selection = resolve_selection(root, raw_profile,
        package_records=identity["source_exports"]["packages"])
    profile = selection.profile
    check_selected_source_scope(selection, identity["source_exports"]["members"], files)
    if any(name not in files for name in required_files(root, profile, selection=selection)):
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
    declarations = source_export_declaration(root, identity["source_exports"]["members"],
                                              selection=selection)
    if declarations != identity["source_exports"]:
        raise SDKError("Source SDK package declarations differ from source exports")
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
    closure = runtime_dependency_closure(profile, root=root, selection=selection)
    if paths != set(closure):
        raise SDKError("Source SDK dependency set differs from the reviewed catalogue")
    import_directories = reviewed_import_directories(root=root, selection=selection)
    for name in files:
        if name.startswith(("vendors/", "ext/")) and not any(
                name.startswith(path + "/") for path in (*closure, *import_directories)):
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
    for record in validate_import_records(identity, root=root, selection=selection):
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
    for name in required_files(root, profile, selection=selection):
        if name not in files:
            raise SDKError("Installed source SDK is missing required "
                           "sources/licenses: " + name)
    for name, receipt in selection.inputs:
        if stable_file(root, name)[1] != receipt:
            raise SDKError("Source SDK package metadata changed during verification")
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
    parser.add_argument("--package", action="append", default=None,
                        help="Explicit package ID; repeat to select several")
    args = parser.parse_args(argv)
    try:
        if args.verify:
            if (args.source or args.output or args.development_fixture or args.component or
                    args.host_support or args.package or args.backend != "baremetal"):
                raise SDKError("Verification cannot also prepare a source package")
            result = verify(args.verify)
        elif args.source and args.output:
            result = prepare(args.source, args.output, args.development_fixture,
                             backend=args.backend, components=args.component,
                             host_support=args.host_support, packages=args.package)
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


def _required_source_members(root, *, selection=None):
    """Project checkout ownership or the installed snapshot's frozen members."""
    if (root / MANIFEST).is_file():
        identity = json.loads((root / MANIFEST).read_bytes(), object_pairs_hook=object_pairs)
        if identity.get("schema") != 3:
            raise SDKError("Unsupported installed source SDK identity")
        return check_source_exports(identity.get("source_exports"),
                                    identity.get("files_sha256", {}))
    selection = resolve_selection(root) if selection is None else selection
    entries = {}
    for name in ROOT_FILES:
        if (root / name).is_file():
            entries[name] = ("100644", "blob", "")
    directories = [root / directory for directory in ROOT_DIRS if directory != "soc"]
    directories.extend(scope for package in selection.packages
                       for scope in package.export_roots)
    for directory in directories:
        for path in directory.rglob("*"):
            if path.is_file() and not path.is_symlink():
                entries[path.relative_to(root).as_posix()] = ("100644", "blob", "")
    if (root / "soc/README.md").is_file():
        entries["soc/README.md"] = ("100644", "blob", "")
    # Verification sources intentionally live outside production roots.
    for name in (*SDK_VERIFICATION_FILES, *F4_FAMILY_VERIFICATION_FILES):
        if (root / name).is_file():
            entries[name] = ("100644", "blob", "")
    return owned_source_files(root, entries, selection=selection)


CONFIGURATION_FILES = frozenset(name for mode in ("platform", "runtime")
    for name in configuration_dependencies.source_names(mode, None, sdk=True))
REQUIRED_FILES = tuple(sorted(set(_required_source_members(SOURCE_ROOT)) |
                              CONFIGURATION_FILES | set(SDK_TOOL_FILES)))


if __name__ == "__main__":
    raise SystemExit(main())
