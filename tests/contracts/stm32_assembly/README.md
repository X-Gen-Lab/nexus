# STM32 controller compile/link fixtures

These are **development software fixtures**, not physical Board packages or
HIL firmware. They explicitly select reviewed silicon routes separately for
VE and ZG densities, so every maintained controller is compiled and linked with
its actual fixed constructor and public type. Images for SPI, I2C, Flash, IWDG,
shared EXTI5/6, PWM and ADC single/scan are independent to avoid pin conflicts.

The contexts are generated from external Board-package inputs. The fixtures
claim no fitted external device, I2C pull-ups, measured VDD, actual PCB, physical
waveform or product qualification. They are ineligible for hardware admission
and product promotion. Volatile reads of every selected typed factory face
retain concrete operations tables and their initialized methods without calling
Flash programming or watchdog enable methods. A post-link audit checks the
actual selected faces, public table layout, provider initializer methods, local
function source identity and LOAD bytes in `bin/provider-retention.json`. Host
register models separately execute operation success/fault behavior.
The generated-constructor host model additionally checks partial acquisition
rollback, shared EXTI vector release and PWM idle restoration using these same
schema inputs and generator functions.

Configure this directory as an independent consumer, with explicit
`NEXUS_SOURCE_ROOT`, one `NEXUS_ASSEMBLY_FILE` from this directory and the
platform ARM toolchain. The link and retention report establish software
API/ABI/construction, exact geometry and retained method bytes. They do not
establish operation execution, timing or electrical behavior. No result is
inherited by the three real
reference Boards' electrical support matrix.
