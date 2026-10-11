Peripheral Configuration
========================

Configuration selects already implemented providers and reviewed Board routes.
A Kconfig entry, vendor peripheral or generated draft does not implement a
controller. Each build resolves one explicit ``NEXUS_CONFIG_FILE`` and keeps its
outputs under the build directory.

Maintained MCU Scope
--------------------

GPIO, interrupt UART, SPI and physical internal Flash are maintained. Discovery
has selected SPI DMA bindings; UART DMA explicitly rejects selection. Modern MCU
typed I2C, generic ADC/DAC/timer/PWM/EXTI, CAN/Ethernet/USB and arbitrary pin/DMA
routing are unsupported. Chip Flash prefetch/instruction/data acceleration
options control ST HAL Flash hardware; they do not implement CPU cache/MPU.

Use a matching fragment from ``configs/``. Qiming and Sky stock profiles disable
SPI because their routes have not been reviewed. GD32 Liangshan owns SPI4 W25Q64
binding, but this alone does not qualify external loopback/sensor/transceiver
wiring. GPIO initial level and AF must match the manifest; duplicate pin,
controller, IRQ or DMA ownership and inconsistent priority/density are rejected.

Native Models
-------------

Native typed GPIO/UART/SPI/I2C/Flash and explicitly private legacy peripheral
models are host behavior fixtures. Native ADC/timer/DMA/ISR simulation does not
expand MCU capability or establish physical precision, latency or waveform.
Provider capability queries report actual selected operation support.

Vendor source selection is private in ``soc/*/sdk`` and follows actual
implementation dependencies. A compiled HAL DMA unit may be a UART/SPI vendor
link dependency without enabling UART DMA. Public applications receive Nexus
types and use explicit target dependencies, not vendor include injection.

Read :doc:`kconfig_platforms`, :doc:`hal` and the Board manifests for current
configurations. Source-bound negative/actual-image checks and physical HIL are
separate records.
