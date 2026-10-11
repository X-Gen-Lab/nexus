STM32F407 Platform Guide
========================

The maintained variants are F407VGT6 Discovery, F407ZGT6 Qiming V3.1 and
F407VET6 Sky Youth. Family implementation is shared in ``soc/stm32f407``;
precise chip/Board identity remains in effective configuration, manifests,
layout and ELF checks. Other STM32F4 variants are not inferred as supported.

.. list-table:: Density and default allocation
   :header-rows: 1
   :widths: 35 30 35

   * - Variant
     - Physical Flash
     - Default memory policy
   * - F407VE
     - 512 KiB / 8 sectors
     - Whole Flash image, 128 KiB main SRAM.
   * - F407VG/ZG
     - 1 MiB / 12 sectors
     - Whole Flash image, 128 KiB main SRAM.

The extra 64 KiB CCM is not DMA-reachable or automatically initialized for
arbitrary sections. No default storage region exists. External layout reserves
complete erase blocks and keeps the image at Flash base; nonzero offset fails.

Build
-----

.. code-block:: bash

   cmake --preset stm32-qiming-armgcc-freertos-release
   cmake --build --preset stm32-qiming-armgcc-freertos-release --parallel 4
   python3 scripts/ci/validate_firmware_elf.py \
     --build-dir build/stm32-qiming-armgcc-freertos-release \
     --report build/stm32-qiming-armgcc-freertos-release/firmware-static.json

Use the pinned ARM GNU 14.3.rel1 toolchain and complete dependency checkout.
Discovery uses ``stm32-armgcc-release`` / ``stm32-armgcc-freertos-release``;
Qiming and Sky each have named baremetal/FreeRTOS Release presets. Root README
lists all eight maintained MCU Release combinations. Contract firmware validates
linkage; external examples own executable product/application entry points.

Implementation Boundary
-----------------------

SoC owns controllers, clock/IRQ/time/system, physical Flash/identity, linker
sections and private SDK. Board owns reviewed HSE, pin/AF, IRQ/DMA and safe levels.
Platform owns startup/lifecycle/object assembly. Normal applications link
``Nexus::Firmware`` and bootstrap common Runtime using Nexus headers.
``Nexus::STM32SDK`` is an explicit raw-SDK bring-up opt-in.

GPIO, interrupt UART, SPI and internal Flash are maintained within reviewed
Board routes. Discovery has selected SPI DMA routes; UART DMA and typed MCU
I2C remain unsupported. No ADC/CAN/Ethernet/USB production support is implied by
silicon availability. No MPU/cache, bootloader/trust chain or full FreeRTOS kernel
restart is implemented. Running/suspended MCU kernel shutdown is BUSY.

No boards have been connected. Oscillator/PLL/supply, actual pin mux/IRQ priority,
wire TC/DE, DMA drain, Flash power loss and worst-load timing need independent
board-identified HIL evidence. Software records retain ``hardware_verified=false``.
