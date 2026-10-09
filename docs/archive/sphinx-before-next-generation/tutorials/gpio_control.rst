Typed GPIO and Board Wiring
===========================

The external
`boot_blinky example <https://github.com/X-Gen-Lab/nexus-examples/tree/main/apps/boot_blinky>`_
constructs its GPIO name/polarity from the selected Board header, bootstraps
Runtime, opens a typed GPIO with explicit owner and checks write/toggle/read.
Native has finite cycles; MCU applications own their loop or scheduled worker.

Discovery, Qiming, Sky and Liangshan have different LED pins/polarities. The
Board manifest and effective GPIO configuration must agree. Initial latch is
set before switching output mode. Never substitute a hardcoded LED pin from a
different Board. Typed close failure/BUSY preserves ownership for retry.

GPIO EXTI is not implemented as a generic MCU provider. A silicon EXTI module,
private IRQ manager or legacy host model does not change that capability.
Real initial levels, pin mux and safe-output continuity require physical evidence.
