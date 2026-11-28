# NUCLEO-F103RB WWDG early-warning witness

The HSI8 bare-metal reference uses the unchanged window watchdog I/O.
The Board author explicitly assigns BKP_DR1 by selecting
`[hardware] backup_word = "wwdg-early-warning-dr1"`. The selected route
claims the backup word, PWR control and WWDG IRQ0. The product links
`Nexus::STM32F103RBWatchdogWarning` to use the package's C API and its
official interrupt handler.

After Platform starts, prepare backup access and consume the previous
warning marker. Read `nx_reset_cause()` independently. F1 reset flags
remain sticky across resets, so the marker and reset flags report
separate facts. Existing unknown data in DR1 is preserved. The
reference expects the ordinary WWDG activation followed by explicit
warning arm. Firmware may release the backup lease before an EWI
write attempt. Once the reset-only write is attempted, only system
reset releases ownership, including when NVIC activation fails.

The handler records one marker in DR1, verifies the write and clears
EWIF. It never reloads the watchdog. The marker survives the reviewed
system reset while the backup domain retains power. Physical warning
latency, VBAT operation, power-loss persistence and board behavior
remain unverified.

中文说明：该装配仅在授权备份寄存器 DR1 后开放提前告警。迟到或失败
均交由看门狗自然复位。下次上电路径分别读取备份见证与 RCC 复位标志。
选用时才接入 IRQ，普通轮询及无 WWDG 装配没有该路径。

[RM0008 Rev 21 §§6.4.1, 7.1.1, 7.1.3, 20.6.2–20.6.3](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf)
