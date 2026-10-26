# Reusable IP implementations

This private source directory groups reviewed register-protocol algorithms by
controller kind and IP implementation. A selected SoC target compiles the required
sources directly with private includes and its exact ABI. Public faces and
request ownership remain in `io/`; chip clocks, routes, startup, memory and
provider state remain with the exact SoC. This grouping adds no runtime layer,
registry, allocation, queue or worker.

| Source directory | Maintained algorithm |
| --- | --- |
| `uart/uart_v1_channel_dma/` | Legacy USART with fixed-channel DMA |
| `i2c/i2c_v1_async/` | Reviewed V1 IRQ/DMA message sequencing |
| `i2c/i2c_v2/` | Reviewed V2 polling message sequencing |
| `adc/legacy_adc/` | Legacy single-shot ADC |
| `timer/legacy_pwm/` | Legacy fixed PWM |
| `flash/legacy_flash/` | Legacy halfword Flash programming |

The I2C V1 engine is shared by STM32F407, GD32F470, STM32F103 and GD32F303.
The first pair retains its reviewed 100 kHz profile. The 8 MHz low-end pair
uses nominal 80 kHz, priority-zero non-kernel IRQs and no I2C DMA. ES096
2.8.1/2.8.5 and GD32F30x limitations 2.8.1 require these narrower choices.
The F4 stream-DMA layout is never reused for a fixed-channel controller.
Captured wake publishers join before stop returns borrowed notification storage;
a refused vector withdrawal retains the initialized owner for explicit retry.
Each SoC wrapper verifies its actual SDK register offsets and bit definitions,
supplies exact clock, DMA/IRQ and visible-memory facts, and binds the selected
static state. Matching register names cannot establish another chip's routing,
pin wiring, clock rate or errata policy.

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

Keep per-byte operations shared only when their hardware protocol is identical.
Cold validation, NVIC withdrawal and chip resources stay in SoC wrappers.
A differing protocol or erratum requires a reviewed implementation or explicit
narrow capability; do not add speculative family switches to a shared hot path.
STM32 identity access remains in `soc/stm32/common/stm32_identity.h` and the
cross-vendor SoC control contract remains in `soc/common/uart_control.h`.

CMake remains the only source graph. Unselected algorithms must not compile
merely because their files exist here. Review the actual linked retention and
resource costs of selected sources. Register-boundary models exercise the real
engine and wrappers, including rejection, errors, cancel, failed drain and
instance isolation. Compile each selected ARM implementation separately.
Native models and linked ARM images establish different software properties;
timing and electrical claims still require physical execution.
See [execution budgets](../docs/design/soc-execution-budgets.md) and the
[provider guide](../docs/guides/provider-porting.md).
