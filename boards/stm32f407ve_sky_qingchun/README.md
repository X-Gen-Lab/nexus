# LCKFB Sky STM32F407VE youth reference wiring

Exact selected part: **STM32F407VET6**, LQFP100, 512 KiB internal Flash.
The youth edition has an 8 MHz HSE and separate 64 KiB CCM; the maintained
profile uses 168 MHz and the main 128 KiB SRAM. Physical PCB revision remains
`null` until inspection; VG high-spec and populated-header editions are
separate fixtures.

`board.json` binds PB2 as an active-high LED, preloading inactive low before
output mode. USART1 uses PA9/PA10 AF7 at MCU 3.3 V logic. The youth debug header
is unpopulated, so HIL requires a fitted connection and external probe/UART
bridge. PA13/PA14 remain reserved for SWD. First-party wiki/schematic references
are retained as fact provenance.

Onboard external Flash is not assumed populated. No SPI/I2C/RS485/PWM/ADC/EXTI
external resource or product partition is selected by this package. A consumer
may add a separately reviewed Board package and electrical profile. Physical
clock, fitted options, reset levels, UART/IRQ and loaded operation have not been
executed; software evidence establishes compile/model scope only.
