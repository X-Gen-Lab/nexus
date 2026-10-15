# Multiple controller ownership

The public I/O faces keep their existing operation tables and caller-owned
request contracts. Selecting another physical controller creates another
private provider instance, not another transport abstraction or global
registry. An instance's cold register, clock, DMA and IRQ facts remain immutable
until its publisher set has been withdrawn and the instance stopped.

## I2C V1 IRQ and finite DMA

STM32F407 I2C1 and I2C2, and GD32F470 I2C0 and I2C1, execute the same maintained
V1 register algorithm. Per-controller cold wrappers validate all register
pointers, DATA offsets, line inputs, bus clocks and DMA selectors against the
actual SDK. Their EV, ER, RX DMA and TX DMA IRQs come from the selected
controller identity. Initialization of an already enabled physical controller
returns `BUSY` before changing its control registers or IRQ priorities.

| SoC controller | GPIOB pins and AF | Peripheral clock | Finite RX / TX route |
| --- | --- | --- | --- |
| STM32F407 I2C1 | PB6/PB7 or PB8/PB9, AF4 | 42 MHz | DMA1 stream0 / stream6, channel1 |
| STM32F407 I2C2 | PB10/PB11, AF4 | 42 MHz | DMA1 stream2 / stream7, channel7 |
| GD32F470 I2C0 | PB6/PB7 or PB8/PB9, AF4 | 50 MHz | DMA0 channel0 / channel6, selector1 |
| GD32F470 I2C1 | PB10/PB11, AF4 | 50 MHz | DMA0 channel2 / channel7, selector7 |

GD32 names in this table are the vendor's literal DMA0/DMA1 names. The platform
configuration's opaque controller IDs use a separate mapping. A DMA request
selector never identifies or allocates a memory engine. Static cold assembly
must reject other devices that select an already owned physical engine.

Setting the private state's DMA descriptor to `NULL` selects IRQ-only messages
and acquires no DMA vector. Exact immutable macros
`NX_STM32_I2C2_DMA(regions, count)` and `NX_GD32_I2C1_DMA(regions, count)` select
the secondary finite routes. The initialization API is unchanged. Pin, bus
clock, DMA clock and shared resource leases remain cold assembly's
responsibility; the provider does not silently allocate these resources.

Each request retains its descriptor, messages, buffers and endpoint until that
controller's engine, interrupt sources and wire have drained. Completing or
cancelling one instance cannot acknowledge another stream's W1C flags, disable
its vectors, release its request or change its wake sink. A refused DMA disable
retains only the affected instance's borrow in `DRAINING` or `QUARANTINED`;
elapsed time does not prove hardware quiescence.

The shared DMA acknowledge leaf compiles to a direct selected register write.
Its test override models the real W1C side effect rather than treating a clear
register as ordinary RAM. There is no per-byte MMIO operation table or new
mutable field. Actual Cortex-M4F compilation measures the unchanged state at
56 bytes, endpoint at 8 bytes and optional immutable DMA descriptor at 80 bytes.
The provider and shared algorithm objects contain zero global DATA/BSS.
Selected object text size is evidence of a compiled object, not an entire
image's retained Flash size or a cycle measurement.

The reviewed silicon route sources are STM32 RM0090 Rev22 Table 43 and GD32F4xx
User Manual Rev3.3 Tables 10-2 and 10-3. These facts do not establish a board's
external wiring. Secondary-board routes require an explicit reviewed board
configuration. Host tests model external CPU/MMIO behavior, and real ARM
compilation uses actual vendor SDK headers. Bus timing, electrical levels and
DMA contention still need hardware qualification; no physical result is
claimed here.
