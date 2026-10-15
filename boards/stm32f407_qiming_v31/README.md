# Qiming Xinxin STM32F407ZG V3.1 reference wiring

Exact selected part: **STM32F407ZGT6**, LQFP144, 1 MiB internal Flash.
HSE is 8 MHz; the maintained clock profile is 168 MHz. `V3.1` identifies the
supplied manual, not a verified physical PCB revision. The PCB revision remains
`null` until fixture inspection.

The read-only `board.json` declares PE3, PE4 and PG9 as active-low LEDs. Each
binding preloads its inactive high bit before output mode. USART1 uses PA9/PA10
AF7. MCU UART pins are 3.3 V logic. The Board RS232 connector is behind the
MAX232/P14 jumper path and requires a matching RS232 adapter rather than a TTL
adapter. Actual connector/jumper selection belongs to the fixture record.
PA13/PA14 remain reserved for SWD.

Provenance includes the supplied V3.1 manual and exact example repository
Git tree/blob identities for LED, HSE and UART facts. SPI/I2C/RS485, external
Flash fitting, PWM, ADC and EXTI external wiring are not selected by this Board
package. No industrial actuator output or product storage is claimed safe.
Physical board, startup, UART waveform, reset levels and loaded behavior have
not been tested. Software link/model evidence must not be read as physical
qualification.
