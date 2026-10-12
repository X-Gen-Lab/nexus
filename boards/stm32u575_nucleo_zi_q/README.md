# NUCLEO-U575ZI-Q software reference

This package selects STM32U575ZIT6Q on source-reviewed MB1549. It uses the internal
MSIS4MHz profile, so `clocks` is empty and no Board HSE dependency is invented.
The default RAM is SRAM1 192KiB; clock/security/DMA/cache restrictions are described
in the [SoC contract](../../soc/stm32u575/README.md).

PC7/LD1 is active high with the documented SB21ON/SB23OFF route. USART1 PA9/PA10
AF7 uses the documented STLINK VCP route: SB32/SB34ON and SB24/SB26/SB31/SB33OFF.
PA13/PA14 stay reserved for SWD. These are official source defaults, not observed
connections on a particular physical board.

Physical PCB/silicon revision, actual bridges, SMPS/supply fitting, oscillator
accuracy and TZEN/SWAP_BANK option bytes remain unknown. The boot owner must
observe TZEN0/SWAP_BANK0 without relying on this platform to modify option bytes.
No physical HIL has executed. USART noise-glitch errata may require external
line/filter design; a passing register model does not qualify serial immunity.
Application LED behavior, console protocols, task orchestration and product
recovery policy belong in external repositories.
