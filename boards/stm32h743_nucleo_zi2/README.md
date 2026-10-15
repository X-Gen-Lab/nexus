# NUCLEO-H743ZI2 software reference

This reusable Board package selects STM32H743ZIT6 and source-reviewed MB1364
routes from UM2407 Rev6 (April 2026). Its physical PCB revision, regulator
fitting and solder bridges are unknown. No physical HIL has executed.

The initial profile uses the internal HSI at 64 MHz, LDO supply, Cache off and
one reserved TIM2 time domain. LED1 uses PB0 active high, with the documented
SB39 ON/SB47 OFF route. USART3 PD8/PD9 AF7 uses the documented ST-LINK VCP
SB19/SB12 path. Alternate PA5 LED fitting requires a distinct reviewed Board
route. External products must inspect their actual PCB and own LED, console,
supply, safety and recovery policy.

The package inventory preserves PC2_C/PC3_C as conditional analog pads. They
are not direct PC2/PC3 GPIO routes. No analog-switch configuration or electrical
qualification is provided. Physical DMA-engine memory reachability does not
open DMA modes, cache ownership or TCM placement in this reference.

[SoC ownership and limits](../../soc/stm32h743/README.md) describe the software
scope. Host models and ARM linking do not establish hardware clock accuracy,
power-entry latency, serial reliability or safe electrical outputs.
