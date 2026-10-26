# Cross-vendor SoC collaboration

This private directory retains `uart_control.h`, the shared control contract
used by exact SoC providers. It does not own a public I/O API, chip facts or
peripheral algorithms. Public request ownership remains in `io/`.

Reviewed reusable register algorithms are grouped under
[drivers](../../drivers/README.md). STM32 identity access lives in
`soc/stm32/common/stm32_identity.h`; it is not a cross-vendor contract.
SoC wrappers retain chip-specific validation, clock and IRQ/DMA ownership.
