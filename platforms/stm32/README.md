# STM32F407 platform assembly

The maintained STM32 implementation is **STM32F407VE/VG/ZG**, on Sky Youth,
Discovery and Qiming V3.1 respectively. A vendor header or a silicon peripheral
is not a supported Nexus route. Other STM32 families are not selectable.

`arch/cortex_m4/` owns CPU exception/mask/barrier primitives.
`soc/stm32f407/` owns controllers, clock/interrupt/system implementations,
physical Flash/identity, private headers, selected SDK translation units and
linker sections. `boards/` owns reviewed wiring, HSE, safe initial levels and
GPIO/UART/SPI resources. `platforms/stm32/` owns startup, platform lifecycle
and final object assembly, using `nexus_forward_component_objects()`.

The family directory serves all three densities without changing precise chip
identity: VE has 512 KiB Flash/8 sectors; VG/ZG has 1 MiB/12 sectors. Default
firmware uses the whole physical Flash and creates no storage reservation.
Main SRAM is 128 KiB; 64 KiB CCM is not DMA-reachable or automatically used.
`NEXUS_FLASH_LAYOUT_FILE` lets the external consumer reserve complete erase
blocks. Image offset must be zero.

## Build and consume

Use the pinned ARM GNU 14.3.rel1 toolchain and complete dependency checkout:

```sh
cmake --preset stm32-qiming-armgcc-freertos-release
cmake --build --preset stm32-qiming-armgcc-freertos-release --parallel 4
python3 scripts/ci/validate_firmware_elf.py \
  --build-dir build/stm32-qiming-armgcc-freertos-release \
  --report build/stm32-qiming-armgcc-freertos-release/firmware-static.json
```

Discovery presets are `stm32-armgcc-release` and
`stm32-armgcc-freertos-release`; Qiming/Sky use their named baremetal/FreeRTOS
Release presets listed in the [root README](../../README.md).
The platform contract ELF is an independent link fixture. Actual applications
live in [nexus-examples](https://github.com/X-Gen-Lab/nexus-examples), choose
`Nexus::Firmware` and call `nx_runtime_bootstrap()` from their own main.
Application workers, scheduler, partitions and recovery policy stay external.

Vendor SDK headers/macros are private to implementation targets. Explicit
raw-SDK bring-up can select `Nexus::STM32SDK`; those callers own raw hardware
quiescence before infrastructure shutdown. Ordinary consumers use Nexus types.

## Capability and validation boundary

GPIO, IRQ-based UART, SPI and physical internal Flash are maintained within
reviewed Board resources. Discovery SPI has selected DMA routes; this does not
provide UART DMA or arbitrary DMA routing. Typed MCU I2C, CAN/Ethernet, MPU/cache,
bootloader/nonzero image relocation and complete FreeRTOS kernel restart are
not implemented. Exact HSE/PLL/supply, IRQ priority/preemption, wire TC, DMA,
Flash power loss and worst-load behavior require real hardware qualification.

The [support matrix](../../docs/strategy/support-matrix.yaml) links exact software
evidence and preserves `hardware_verified=false`. Current directory or source
changes require new software checks; old build counts do not qualify them.
