# Shared silicon algorithms

This private directory owns algorithms whose register ordering is shared by
reviewed silicon implementations. It is not a public device API or a provider
registry. Public interfaces and request ownership contracts remain in `io/`.

The I2C V1 engine is shared by STM32F407, GD32F470, STM32F103 and GD32F303.
The first pair retains its reviewed 100 kHz profile. The 8 MHz low-end pair
uses nominal 80 kHz, priority-zero non-kernel IRQs and no I2C DMA. ES096
2.8.1/2.8.5 and GD32F30x limitations 2.8.1 require these narrower choices.
The F4 stream-DMA layout is never reused for a fixed-channel controller.
Captured wake publishers join before stop returns borrowed notification storage;
a refused vector withdrawal retains the initialized owner for explicit retry.
Each SoC wrapper verifies
its actual SDK register offsets and bit definitions, supplies exact clock,
DMA/IRQ and visible-memory facts, and binds the selected static state. A common
algorithm must not infer a second chip's DMA routing, pin wiring or clock rate
from matching register names.

The UART V1 channel-DMA engine is shared by the exact F103 and GD303 providers.
Its wrappers assert legacy USART and fixed-channel DMA masks and layout.
`dma-tx` owns one TX channel and an explicit IRQ byte ring; `dma-duplex` owns
both channels and caller-provided finite RX blocks. Actual wire TC remains
independent of DMA memory completion. The engine detaches each source, observes
a stable remaining count and acknowledges flags before returning a buffer.
Unprovable withdrawal retains the request or producer loan. Normal RX restarts
through owner service, with explicit gaps and backpressure; no consumer block
is overwritten. A byte discarded from DATA at a finite boundary records loss.
Neither circular reception nor a cache contract is inferred from this engine.

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
