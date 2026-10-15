# STM32F407 user-board resource bindings

These are software bring-up bindings, not physical qualification. Physical PCB
revision, fitted options, jumpers, voltage, probe and UART adapter must be recorded
before execution. The source document revision is not a measured PCB revision.

| Profile | Verified minimal resources | Enabled software baseline |
|---|---|---|
| Qiming Xinxin STM32F407ZGT6 high-spec V3.1 | HSE 8 MHz; LED0 PE3, LED1 PE4, LED2 PG9, all active low; USART1 PA9/PA10 | Early inactive LED latches, PE3 typed GPIO, UART logical instance 0; SPI disabled |
| LCKFB Sky STM32F407VET6 youth | HSE 8 MHz; PB2 LED active high; USART1 PA9/PA10; 512 KiB internal Flash | Early inactive LED latch, PB2 typed GPIO, UART logical instance 0; SPI disabled |

Qiming evidence is the user-supplied [V3.1 manual](https://github.com/petercwq/STM32407/blob/master/启明欣欣407开发板(高配版)V3.1例程使用手册.pdf)
and matching example repository tree `08c5f513f1677388fcacaa519dca0c9ef00b6d2e`.
`led.h` blob `63ed6144e8d33f947fbe2784a9962d2913028ac8` identifies the
three LED pins; `led.c` blob `4adeb6f04323f6c8ae4eabf3308334e3e6910277`
initializes their inactive high levels. The manual independently describes low
as on. The matching header sets HSE_VALUE to 8 MHz and system-clock source blob
`e99736d005f731db67300447d77ce96901d4c3ec` uses HSE with PLL_M 8,
PLL_N 336 and PLL_P 2. A legacy 25 MHz template comment is not the board clock.
The matching RS232 `usart1.c` blob `e0f9a0fdf6633de6ef5f1c451c2966f22dae204b`
binds USART1 to PA9/PA10 AF7. The manual describes MAX232 and P14 jumpers;
verify their physical state and use the correct RS232 adapter at that connector.
MCU 3.3 V UART levels must not be confused with RS232 connector voltage levels.
Exact external Flash part, RS485 direction and other peripheral allocations
are not inferred from the presence of example folders.

Sky evidence is the first-party [edition table](https://wiki.lckfb.com/zh-hans/fdb/skystar.html)
and [MCU resource table](https://wiki.lckfb.com/zh-hans/fdb/micropython/introduction.html).
The youth edition leaves the LSE, SPI Flash and debug header unpopulated. USB
Type-C is MCU USB/power, not an assumed USB-UART bridge. UART requires a correctly
soldered debug header and external 3.3 V adapter; RS485 requires a separate
transceiver resource profile. Compare the [official schematic](https://wiki.lckfb.com/zh-hans/tkx/hardware/schematic.html)
with the physical unit before HIL. Do not silently use the VG high-spec edition's
1 MiB layout or fitted W25Q128 on this VE youth board.

Each selected board forwards a strong `HAL_MspInit` object. Its narrow public,
vendor-free `nx_board_prepare_safe_outputs()` preloads the LED inactive value
before enabling output mode, including before the product opens the GPIO. No
external actuator is configured or declared safe by this development-board
baseline. The typed GPIO profile must retain the same inactive initial value.

The Cortex-M4 blocking-context predicate now reads PRIMASK, BASEPRI and FAULTMASK.
It conservatively rejects any active exception mask, preventing a caller holding
a FreeRTOS critical region from entering an IRQ/tick-dependent blocking driver.
Saved architecture state still restores only PRIMASK and never modifies kernel
BASEPRI. Native concurrency, host register fixtures, ARM code generation and
physical interrupt/electrical tests remain separate evidence classes.

Executed checks for this change: four production board/UART translation units
passed strict C11 `-Wall -Wextra -Werror` ARM GCC 14.3 syntax compilation with
the pinned actual STM32 SDK. Both vendor-free board headers passed C11/C++17
compilation without SDK headers. Two host fixtures compile the actual board
sources and validate clock/latch/output ordering, untouched unrelated outputs,
UART binding/release and unsupported controller/DE rejection. Cortex-M4 object
disassembly contains all three mask reads and only PRIMASK writes. Full firmware
linking, artifact identity and physical HIL are separate integration gates.
