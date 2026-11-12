# Multiple controller ownership

The public I/O faces keep their existing operation tables and caller-owned
request contracts. Selecting another physical controller creates another
private provider instance, not another transport abstraction or global
registry. An instance's cold register, clock, DMA and IRQ facts remain immutable
until its publisher set has been withdrawn and the instance stopped.

## I2C V1 IRQ and finite DMA

STM32F407 I2C1 and I2C2 execute the same maintained
V1 register algorithm. Per-controller cold wrappers validate all register
pointers, DATA offsets, line inputs, bus clocks and DMA selectors against the
actual SDK. Their EV, ER, RX DMA and TX DMA IRQs come from the selected
controller identity. Initialization of an already enabled physical controller
returns `BUSY` before changing its control registers or IRQ priorities.

For every maintained F407 polling, IRQ and DMA master, `max_hz` is a nominal
upper bound. Standard mode uses the smaller of that bound and 88 kHz before
rounding the 12-bit CCR upward. This applies ES0182 Rev19 section 2.10.4 to
seven-bit multi-message requests and ten-bit internal repeated STARTs alike.
The default 100 kHz bound therefore selects CCR 239 at 42 MHz PCLK, about
87.866 kHz; it can serve both endpoint widths without changing the authored
mode. A bound above 100 kHz explicitly selects Fast mode, whose existing
divider and electrical requirements remain unchanged. Repeated-START admission
also checks the actual CCR before borrowing storage or touching the transfer
registers. It never changes the divider inside a transaction.

Readonly traits retain `configured_max_hz` and report `nominal_hz`, rounded down
to integer Hz from the selected nominal PCLK and CCR. Zero means the fixture
has no physical clock integration; this field is never a measured bus rate.
F4 V1 does not accept the V2 `bus_hz_min` timing envelope. Oscillator tolerance,
target timing and Board pull-ups still require electrical qualification.

| SoC controller | GPIOB pins and AF | Peripheral clock | Finite RX / TX route |
| --- | --- | --- | --- |
| STM32F407 I2C1 | PB6/PB7 or PB8/PB9, AF4 | 42 MHz | DMA1 stream0 / stream6, channel1 |
| STM32F407 I2C2 | PB10/PB11, AF4 | 42 MHz | DMA1 stream2 / stream7, channel7 |

A DMA request
selector never identifies or allocates a memory engine. Static cold assembly
must reject other devices that select an already owned physical engine.

Setting the private state's DMA descriptor to `NULL` selects IRQ-only messages
and acquires no DMA vector. Exact immutable macros
`NX_STM32_I2C2_DMA(regions, count)` selects
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

The reviewed silicon route source is STM32 RM0090 Rev22 Table 43. These facts do not establish a board's
external wiring. Secondary-board routes require an explicit reviewed board
configuration. Host tests model external CPU/MMIO behavior, and real ARM
compilation uses actual vendor SDK headers. Bus timing, electrical levels and
DMA contention still need hardware qualification; no physical result is
claimed here.
