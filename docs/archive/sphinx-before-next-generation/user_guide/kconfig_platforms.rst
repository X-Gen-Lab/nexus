Platform Configuration
======================

Only Native, STM32F407VE/VG/ZG and GD32F470ZG are maintained platform choices.
Use an explicit existing configuration fragment and a named CMake preset; each
build directory owns exactly one Board/backend and one effective configuration.
Unsupported choices, unknown symbols, duplicate values, ranges/dependencies or
Board/density contradictions fail configuration. No root ``.config`` fallback
or manually copied generated header is used.

.. code-block:: bash

   cmake --preset linux-gcc-debug
   cmake --preset stm32-qiming-armgcc-freertos-release
   cmake --preset gd32f470-armgcc-baremetal-release

The platform symbols are ``PLATFORM_NATIVE``, ``PLATFORM_STM32`` and
``PLATFORM_GD32F470``. STM32 density is selected by maintained VE/VG/ZG variants;
fixed family/CPU metadata is derived, not duplicated in application fragments.
SoC family directories do not collapse exact chip/Flash identities.

Board and Layout
----------------

Internal Board profiles are Discovery, Qiming V3.1, Sky Youth and Liangshan Pi.
An external Board uses one ``NEXUS_BOARD_DIR`` containing manifest/source/CMake;
inputs are hashed and must stay in that package. Reviewed GPIO/UART/SPI routes,
HSE/density, AF/clock/IRQ/selected DMA consistency are checked. Arbitrary silicon
routes or missing providers cannot be enabled by adding JSON.

An external ``NEXUS_FLASH_LAYOUT_FILE`` chooses image/regions. Default image
owns all physical Flash and reserves no storage. Image offset is zero only.
One parse emits layout/header/linker and binds Board/layout digests into ELF.
Product partitions/main/workers and policy stay external.

The source identities, exact config paths and known limits are maintained in the
root README and ``docs/strategy/support-matrix.yaml``. Other STM32 families,
GD32F303/F407/VF103, ESP32 and nRF52 are not selectable supported platforms.
