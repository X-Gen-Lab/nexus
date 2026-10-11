# Independent finite UART and SPI DMA controllers

The existing typed faces, shared immutable operations tables and caller-owned
request/state storage remain authoritative. A cold instance selects one exact
reviewed register/IRQ/DMA tuple. There is no stream allocator, runtime registry,
extra mutable route pointer or hidden copy pool. The mutable DMA state layout
is unchanged; the additional routes consume only selected instances.

| Provider | First route | Second route |
|---|---|---|
| STM32 UART | USART1, APB2 84 MHz, DMA2 stream7 channel4 | USART2, APB1 42 MHz, DMA1 stream6 channel4 |
| GD32 UART | USART0, APB2 100 MHz, literal DMA1 channel7 selector4 | USART1, APB1 50 MHz, literal DMA0 channel6 selector4 |
| STM32 SPI | SPI1, APB2 84 MHz, DMA2 RXstream0/TXstream3 channel3 | SPI2, APB1 42 MHz, DMA1 RXstream3/TXstream4 channel0 |
| GD32 SPI | SPI4, APB2 100 MHz, literal DMA1 RXchannel3/TXchannel4 selector2 | SPI0, APB2 100 MHz, literal DMA1 RXchannel2/TXchannel5 selector3 |

Reviewed silicon AFs are USART2/USART1 PA2/PA3 AF7, SPI2 PB13/PB14/PB15 AF5,
and SPI0 PA5/PA6/PA7 AF5. These facts do not supply external Board wiring,
connectors, electrical pull-ups or qualification. Typed configuration remains
the single assembly authority. The second UART TX conflicts with the first
I2C TX engine and must be rejected in resource resolution. GD literal DMA0/1
is distinct from public opaque controller IDs 1/2.

STM UART initialization uses retained exact IRQ, DMA, stream and clock facts.
STM SPI initialization validates the selected bus, clock and engine pointers;
an idle bus whose DMA vectors remain enabled cannot be acquired by another
cold object. GD cold `_initialize_at` methods select only the two maintained
readonly controller descriptors. Existing primary initializers remain thin
wrappers. Duplicate/live hardware, retained enabled DMA publishers and unsupported
tuples reject before effects. A disabled engine still has an owner while its
vector lease is enabled; cold acquisition preserves that owner's flags/priority.

Finite DMA completion is independent from wire completion. Hot W1C clears only
observed completion/half-completion bits; fresh error bits remain latched.
The provider reads errors after actual engine withdrawal and again before
settlement. It never reports success after a detected DMA error. Both engines,
IRQ sources and SPI wire must drain before CS release and SETTLED; UART also
requires true TC. Failed withdrawal retains QUARANTINED requests and buffers.
Peer flags, clocks, vectors, CS and loans remain independent. Shared DMA clock
ownership belongs to outer assembly and is never released by an individual stop.

Explicit SPI fixed-memory flags preserve caller-owned dummy/discard scalar
storage, exact one-byte DMA domains and MINC identity. There is no implicit
scratch fallback. STM SPI2 polling abort/recovery pulses APB1 SPI2 reset;
it does not reset the neighboring APB2 SPI1 controller.

Only finite second-controller UART TX/SPI DMA is implemented here. Existing
primary receive modes remain covered by their own suites; secondary continuous
RX is not admitted through these new facts. Actual GoogleTest/GoogleMock MMIO
regressions and strict ARM SDK object/layout reports are development evidence.
Factory/selected whole-image validation is recorded separately. Physical DMA,
IRQ latency, CS/wire timing and Board HIL remain **not executed**.
