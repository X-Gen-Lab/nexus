# GD32 integration status

GD32F407 is the first candidate family. No production GD32 driver is implemented
or hardware-validated in this checkout. Selecting GD32 fails configuration;
a successful interface-only library would misrepresent a working platform.

The independent port requires a pinned official GigaDevice SDK and license,
an exact chip and board revision, a startup/vector table, clock tree and tick,
GPIO/UART, interrupt routing, SPI/DMA, and flash geometry. ST vendor handles,
register definitions and startup objects cannot be used to establish GD32
compatibility. The shared application and HAL contracts remain vendor-neutral.

See `soc/gd32f407/manifest.json` and `profiles/support-matrix.json`. Host STM32
fake tests provide no execution evidence for GD32. BSP-003 and BSP-004 remain
blocked until these dependencies and physical validation are available.
