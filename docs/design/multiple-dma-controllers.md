# Independent DMA controllers

The existing typed faces, shared immutable operations tables and caller-owned
request/state storage remain authoritative. A cold instance selects one exact
reviewed register/IRQ/DMA tuple. There is no stream allocator, runtime registry,
extra mutable route pointer or hidden copy pool. The mutable DMA state layout
is unchanged; the additional routes consume only selected instances.

| Provider | First route | Second route |
|---|---|---|
| STM32 UART | USART1, APB2 84 MHz, DMA2 stream7 channel4 | USART2, APB1 42 MHz, DMA1 stream6 channel4 |
| STM32 SPI | SPI1, APB2 84 MHz, DMA2 RXstream0/TXstream3 channel3 | SPI2, APB1 42 MHz, DMA1 RXstream3/TXstream4 channel0 |

Reviewed silicon AFs are USART2 PA2/PA3 AF7, USART1 PA9/PA10 AF7,
SPI2 PB13/PB14/PB15 AF5,
and SPI1 PA5/PA6/PA7 AF5. These facts do not supply external Board wiring,
connectors, electrical pull-ups or qualification. Typed configuration remains
the single assembly authority. The second UART TX conflicts with the first
I2C TX engine and must be rejected in resource resolution.

STM UART initialization uses retained exact IRQ, DMA, stream and clock facts.
STM SPI initialization validates the selected bus, clock and engine pointers;
an idle bus whose DMA vectors remain enabled cannot be acquired by another
cold object. Existing primary initializers remain thin wrappers. Duplicate/live hardware, retained enabled DMA publishers and unsupported
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

The table describes finite UART TX/SPI DMA paths. Receive modes and additional
controllers have their own selected contracts in the support matrix; a TX route
does not itself authorize RX. Actual GoogleTest/GoogleMock MMIO
regressions and strict ARM SDK object/layout reports are development evidence.
Factory/selected whole-image validation is recorded separately. Physical DMA,
IRQ latency, CS/wire timing and Board HIL remain **not executed**.


## Independent ADC timer triggers

ADC1/2/3 retain their reviewed DMA2 stream4/channel0, stream2/channel1 and
stream1/channel2 respectively. Both finite and continuous modes select one of
these regular trigger events from RM0090 Rev22 Table68:

| Timer | Selected events | Kernel clock |
|---|---|---|
| TIM1 | CH1, CH2, CH3 | 168 MHz |
| TIM3 | CH1, TRGO | 84 MHz |
| TIM4 | CH4 | 84 MHz |
| TIM5 | CH1, CH2, CH3 | 84 MHz |
| TIM8 | CH1, TRGO | 168 MHz |

The optional TOML `trigger` defaults to `tim3-trgo`. Three ADCs may run
independently with distinct timer owners; this is not simultaneous or
interleaved multimode conversion. Each selection exclusively claims the entire
timer, conflicting with its PWM, capture, encoder or another ADC owner. TIM2
remains the Core monotonic clock. EXTI11 needs a separate physical pin/event
owner and is not admitted through these internal routes.

The immutable typed option drives the resource claims, preflight clock mask,
trigger graph and generated C selector. Analog Board routes authorize only the
selected ADC pins; they no longer contain a fixed TIM3 policy. One shared
read-only C fact table owns EXTSEL, controller, APB gate and channel identities.
The mutable selector uses existing padding. Public ADC faces and stream buffer
contracts remain unchanged. See the [I/O ownership diagrams](io-evolution.md)
for the producer loan and withdrawal boundary.

The shared C timer program clears inherited slave, output, interrupt, repeat
and compare policy before arming. No timer pad or vector is acquired. TRGO uses
update; compare events occur at ARR with CCxE disabled. Initial compare phase
differs from the first update, so neither choice promises PWM phase or an
aperture timestamp. UG occurs while ADC external detection is disabled; after
ADC warmup, external detection and the selected counter are enabled. The full
scan budget still uses 21 MHz ADCCLK and the actual selected resolution/sample
codes.

After memory and publisher drain, close withdraws ADC resources and then the
selected timer gate. A refused gate release retains the TIMER receipt and its
original identity even after the ADC clock is gone. Rebind stays BUSY; retry
releases only the retained timer. A failed gate enable records that identity
before the write. Startup and cleanup remain serialized privileged task work,
with no scheduler, allocation or hidden sampling task.

The VE/baremetal and ZG/FreeRTOS `*adc-timers*.toml` references select all three
converters with three independent timers. Model tests execute real finite and
continuous C paths, including invalid sources and refused acquisition/release.
ARM linking, source SDK consumption and physical acquisition are separate
checks; physical edge rate, jitter and analog behavior require an actual board.
