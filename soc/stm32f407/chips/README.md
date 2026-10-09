# Maintained STM32F407 configuration

This directory describes the maintained **STM32F407VE, STM32F407VG and
STM32F407ZG** variants. `soc/stm32f407/` is the shared family implementation;
`STM32_CHIP_NAME`, density symbols, Board identity and generated layout retain
the exact variant. Other STM32 series and variants are not supported choices.

| Variant | Physical Flash | Erase geometry | Default main SRAM |
|---|---:|---|---:|
| F407VE | 512 KiB | 8 nonuniform sectors | 128 KiB |
| F407VG/ZG | 1 MiB | 12 nonuniform sectors | 128 KiB |

Physical SRAM also includes 64 KiB CCM. It is not a general DMA buffer region
or a promise of initialized extra memory sections. Maintained F407 clocks are
bounded to 168 MHz; HSE is selected by the reviewed Board package.

`platforms/stm32/Kconfig` selects maintained lifecycle/boot settings and sources
SoC-owned chip, controller, clock, interrupt and system configuration. One
explicit `NEXUS_CONFIG_FILE` generates the build-local effective configuration.
Unknown choices, contradictory densities and unsupported Board bindings fail;
there is no source-root `.config` fallback.

A future chip needs actual startup/vector, clock, geometry, provider/Board
routes, negative configuration checks and independent firmware evidence before
it becomes a selectable maintained choice. SDK availability is not sufficient.
