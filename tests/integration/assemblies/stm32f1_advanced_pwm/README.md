# STM32F103 complementary PWM

## NUCLEO-F103RB TIM1

The reviewed route uses PA8 (CH1, CN10-23), PB13 (CH1N, CN10-30), and
PB12 (BKIN, CN10-16). The assembly owns TIM1, its break vector and these
pads under the HSI8 bare-metal System. No AFIO remap is programmed.

This logic-level reference uses 8 MHz timer and counter clocks, 800 ticks
per period, and 8 tDTS ticks of deadtime. The intended duty update is
preloaded for the next period. The nominal frequency is 10 kHz and nominal
deadtime is 1 µs. The reviewed DTG codes are the linear range 1–127.
Physical timing and electrical behavior remain unmeasured.

BKIN is active low with an internal pull-down so an open input requests a
break. Drive a valid inactive high before attempting start. Cold preparation
and stop park both outputs as low GPIO. On break, TIM1 asynchronously
clears MOE and forces idle low. The interrupt leaves BIF latched and masks
its own source. Stop detaches both channels. Explicit fault clear after
stop and a separate start are required to resume. AOE and LOCK stay zero.

The example checks the public API and leaves the outputs parked by default.
A debugger-controlled logic probe can execute start and stop for a bench
test. The assembly defines no power stage or motor-drive qualification.

[ST RM0008 Rev21, §§14.3.11–12, 14.4.18](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf)
· [ST UM1724 Rev17, Table 27](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)

## STM32F103ZE TIM8

Select `f103ze-tim8.toml` for the LQFP144 logic reference. CH1 is PC6
(pin 96), CH1N is PA7 (pin 43), and BKIN is PA6 (pin 42). The configuration
selects TIM8 APB2, its exclusive TIM8_BRK interrupt (43), and these three pads.
TIM8 has no AFIO remap in this route. It uses the same fixed 8 MHz counter,
active-low break, explicit fault clear and inactive-low outputs as TIM1.
The driver enables only CH1 and CH1N, leaving the CH4 output covered by the
SDIO pin-conflict erratum disabled. The referenced one-bit SDIO configuration
owns different pads. Bench logic behavior and break latency remain unmeasured.

[DS5792 Rev13, Table 5](https://www.st.com/resource/en/datasheet/stm32f103ze.pdf)
· [ES0340 Rev17, §§2.3.6, 2.7.1 and 2.7.3–4](https://www.st.com/resource/en/errata_sheet/es0340-stm32f101xcde-stm32f103xcde-device-errata-stmicroelectronics.pdf)
