# GD32F303RCT6 bare-chip reference fixture

This package is a software fixture for the exact GD32F303RCT6 LQFP64 package.
It defines PA4/PA5 GPIO, default USART0 PA9/PA10 and SPI0 PA5/PA6/PA7 silicon
routes from the official datasheet. The SPI software fixture uses separate PA4
active-low CS, without asserting an attached device. Internal FWDGT has no pins. It does not describe a manufactured PCB or claim that any connector,
LED, transceiver, supply, crystal or external debugger is attached.

The assembly selects the internal IRC8 source and therefore invents no external
oscillator frequency. PA13/PA14 and the remaining SWJ pads stay reserved; no
option-byte, SWJ-remap or boot-strap changes are performed. Actual reset, power,
voltage levels, pulls, routing, clock tolerance and physical tests remain the
responsibility of a separately identified Board package and HIL station.

See [the provider contract](../../soc/gd32f303/README.md) for exact implemented
modes, static ownership, finite time-domain limits and unsupported scope.
