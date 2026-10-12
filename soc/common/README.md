# Shared silicon algorithms

This private directory owns algorithms whose register ordering is shared by
reviewed silicon implementations. It is not a public device API or a provider
registry. Public interfaces and request ownership contracts remain in `io/`.

The I2C V1 engine is shared by STM32F407 and GD32F470. Each SoC wrapper verifies
its actual SDK register offsets and bit definitions, supplies exact clock,
DMA/IRQ and visible-memory facts, and binds the selected static state. A common
algorithm must not infer a second chip's DMA routing, pin wiring or clock rate
from matching register names.

Keep per-byte operations in the shared engine when the hardware protocol is
identical. Cold validation, NVIC withdrawal and chip-specific resources stay in
SoC wrappers. A differing protocol or hardware erratum belongs in a reviewed
implementation or an explicit narrow capability; do not accumulate speculative
family switches inside a common hot path.

The selected SoC CMake target owns the source graph. A shared source is compiled
only for modes that need it, using private includes and the target's exact ABI.
Do not add another source list, runtime dispatcher, hidden queue or task. Review
actual linked retention and resources when extracting an algorithm.

Use register boundary models to test the real production engine with each
wrapper, including rejection, error, cancel, failed drain and instance isolation.
Compile both selected ARM implementations. Native models and linked ARM images
prove different properties; timing and electrical claims require real hardware.
See [execution budgets](../../docs/design/soc-execution-budgets.md).
