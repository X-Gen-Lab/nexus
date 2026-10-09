# GD32 controller compile/link fixtures

These development software fixtures explicitly assemble the actual GD32F470ZGT6
providers for SPI4, I2C0, internal FMC, FWDGT, EXTI3, TIMER2 PWM, and ADC0 single
or two-channel low-rate scan. Each independent firmware avoids conflicting pins
and uses the public typed API. I2C has two fixed 7-bit address endpoints sharing
one controller; SPI uses the provider's reviewed built-in active-low PF6 CS.

The `software_board` is not a physical PCB package. It does not qualify external
pull-ups, fitted devices, oscillators, analog input, VDD, or actual electrical
wiring. These firmware images are ineligible for physical HIL and product
promotion. Volatile function references retain real operation implementations
without automatically erasing/programming Flash or enabling the watchdog.
Production-source host fault models exercise operation behavior separately.

Configure this directory as an independent consumer with `NEXUS_SOURCE_ROOT`,
one explicit JSON `NEXUS_ASSEMBLY_FILE`, and the platform ARM toolchain. Actual
ELF links demonstrate API/ABI/construction and selected resource allocation.
They do not establish cycle counts or inherit physical reference-Board
qualification.
