# NUCLEO-F103RB software reference

This Board package selects the exact STM32F103RBT6 and nominal reset HSI8. It
binds PA5/LD2, USART2 PA2/PA3 and PA0/PA1 analog-header raw edge inputs
using UM1724 Rev17. The optional SPI1 route uses PA5/PA6/PA7 with PA4
software CS; PA5 shares the LED load, so SPI1 and LD2 cannot be selected
together. External SPI wiring, voltage, loading and timing require validation. PA13/PA14 remain reserved for
SWD. It does not assume HSE population, inspect the physical PCB, or qualify
bridge and electrical states. The VCP route requires SB13/SB14 ON and
SB62/SB63 OFF on the actual matching Board revision.

`board.json` records source provenance and unknowns separately. Hardware tests
remain `not_executed`; inspect the actual Board, probes, supply and serial path
before running HIL. Product policy, tasks and protocols stay outside this repo.
