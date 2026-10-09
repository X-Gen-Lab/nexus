# STM32F407 linker sections

Only maintained GNU linker inputs for F407VE/VG/ZG live here. The generated
`firmware.ld` derives physical density, image bounds and explicit regions from
one effective configuration and one optional `NEXUS_FLASH_LAYOUT_FILE`.
Default firmware owns the whole physical Flash and reserves no parameter area.
Image offset must be zero; bootloader/VTOR relocation is not implemented.

The section input retains the real `.isr_vector`, startup/SP/reset symbols,
text/rodata, registration objects, data load/copy, zeroed bss, main stack and
libc heap. Board/layout digests are bound to absolute ELF symbols. Firmware
linkage explicitly retains SoC/Board objects and strong IRQ entries; archive
creation alone does not prove those entries reached the image.

F407VE has 512 KiB Flash; VG/ZG has 1 MiB. Main RAM is 128 KiB SRAM. The extra
64 KiB CCM is not DMA-accessible and is not automatically allocated/initialized
by adding an arbitrary section. Do not increase a linker region beyond real
silicon to silence overflow. External applications own main stack/libc heap
budgets separately from OSAL object/task pools and measured runtime high-water
marks.

Run `scripts/ci/validate_firmware_elf.py` against the actual build directory to
check segment permissions, vector/SP/reset, strong IRQ/registry retention,
density and Board/layout identity. Physical startup, DMA and power loss remain
separate HIL gates. Current commands and consumer-owned layout examples are in
[the CMake SDK guide](../../../cmake/README.md).
