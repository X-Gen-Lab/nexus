# STM32F407 implementation

This package implements fixed typed C ports for **STM32F407VET6** (LQFP100,
512 KiB Flash) and **STM32F407ZGT6** (LQFP144, 1 MiB Flash). Main SRAM is
128 KiB; the separate 64 KiB CCM domain is described but excluded from the
initial linker and DMA policy. No product Flash partition is reserved.

`system.c` establishes the reviewed 8 MHz HSE / 168 MHz PLL profile, APB1
42 MHz and APB2 84 MHz. Reserved TIM2 runs at 1 MHz, with an overflow IRQ to
extend its 32-bit counter. USB FS explicitly acquires a separate 48 MHz
clock-domain owner; it is not enabled by base clock startup. The overflow IRQ must run at least once every
2^32 microseconds. It does not use the RTOS SysTick. Initialization records
primary failure, cleanup failure and remaining clock effects independently;
failed HSI switching retains responsibility instead of disabling a live PLL.
Bootstrap clock phases have explicit poll bounds because the final monotonic
clock does not exist yet. Dynamic reclocking is unsupported.

The official ST device package and Arm CMSIS submodules provide register
layout, masks and the complete GCC vector/startup. Their pinned Git identities
are checked by the repository dependency lock. Vendor headers are private to
the implementation and generated static binding. The HAL driver package is
not linked into these direct-register typed providers.

| Provider | Maintained execution and limits |
|---|---|
| GPIO | One-port 16-bit authorization mask; atomic BSRR set/reset; serialized toggle; initial level precedes mode |
| UART | USART1 IRQ bytes/events, IRQ RX blocks, finite TX DMA, RX DMA/IDLE, finite/continuous duplex and continuous RX blocks; USART2 finite TX DMA on its reviewed route; true TC, explicit loss, drain and quarantine |
| SPI | SPI1 short polling or finite full-duplex DMA; SPI2 finite DMA; 8-bit MSB-first master, modes 0–3, complete CS interval; polling at most 256 bytes and DMA at most 65535 bytes |
| I2C | I2C1 polling/IRQ/DMA and I2C2 IRQ/DMA messages, 42 MHz / 100 kHz; 7-bit address, at most 8 read/write messages of 1–256 bytes; repeated START, protected final-byte ACK handling, explicit NACK/arbitration/STOP failure |
| Flash | Exact VE 8-sector / ZG 12-sector geometry, x32 program at caller-declared 2.7–3.6 V; range/alignment checks, complete pulses, cache invalidation; erase is never preempted by a C deadline |
| IWDG | 4–256 prescaler, 12-bit reload, nominal 32 kHz with 17–47 kHz declared LSI bounds; hardware-option activation and irreversible software enable reported; debug freeze explicit |
| EXTI | Static lines, bounded shared-vector dispatch, per-line event storage and observable overflow; pending hardware bits can coalesce edges; no debounce policy |
| PWM | TIM3 general-purpose channels, fixed shared PSC/ARR, period 1–65535 counts, duty 0–period, shadow compare update; stop restores GPIO inactive level |
| ADC | ADC1 12-bit software shots/scans, TIM3-triggered finite or continuous DMA blocks; explicit sample time/reference, complete scan alignment and consumer-loan backpressure |
| CAN | CAN1/CAN2 Classic CAN IRQ, three mailboxes and shared filter domain; no CAN FD |
| USB | OTG FS device PIO endpoints with explicit USB48 ownership; no host/HS/DMA/class stack |
| Ethernet | Raw RMII frame/MDIO and caller-owned descriptor/RX loans; IP/protocol policy remains external |
| Timer capture | Reviewed TIM4 edge-capture route and bounded events |
| Advanced PWM | Reviewed TIM1 complementary outputs/deadtime/break; external electrical safety policy |

The two reference Board packages bind their reviewed LED and USART1 routes;
Qiming also declares reviewed CAN bindings. Selection remains explicit. Internal Flash/IWDG bindings are available for explicit selection;
they neither reserve storage nor enable the watchdog during startup.
Other SoC routes describe controller implementation and silicon AF resources;
they do not establish Board wiring, external pull-ups, device fitting or
physical support. Consumers must supply reviewed external Board bindings to
select those routes. Selected capture, advanced PWM and continuous double-buffer RX/ADC modes are
maintained as listed below. Explicit I2C GPIO bus-clear is a separate common
component. Internal ADC channels, bootloader relocation and undeclared modes
remain rejected or absent from the selectable schema. The
[derived matrix](../../docs/delivery/capabilities.md) records exact selectable
mode names, reference Board bindings and resolver-selected production source.

UART/SPI finite DMA uses caller-owned buffers in reviewed DMA-accessible domains;
CCM is excluded. DMA TC alone does not release a borrow: UART TC or SPI idle,
memory-channel disable and IRQ-reference detachment must also be observed.
Failed drain retains QUARANTINED storage. `irq-blocks` uses per-byte IRQ and
IDLE boundaries; DMA receive uses distinct finite or continuous double-buffer
providers. `dma-tx` with IRQ blocks is rejected; explicit `dma-duplex` and
`dma-duplex-continuous` select the implemented bidirectional providers.
Continuous modes never overwrite READY/BORROWED blocks; missing replacements
stop capture and report loss without inventing a missed-event count.

Finite ADC `trigger-dma` uses ordinary finite DMA. It stops trigger/conversion/DMA before
publishing each complete block, then requires explicit service to rearm after
stabilization. Consumer loans are never overwritten; stop remains BUSY until
they are released. TIM3 is exclusive with PWM, and finite rearming introduces gaps. The separate
`trigger-dma-continuous` provider maintains double-buffer ownership and requires
replacement/service progress; it stops rather than overwrite a consumer loan.

All ports have one execution owner. No heap, registry, string lookup, default
worker, OS lock, generic device reference or reserved copy pool is introduced.
UART IRQ moves bytes and hardware facts; task service observes transfer
deadlines and is required throughout cancel/stop. A late TC may resolve a
quarantined transfer; until then its request remains borrowed. After SETTLED,
no IRQ or provider reference remains. Request owners must preserve storage
until settlement and separate observer/adapter drain.

I2C recovery checks the controller BUSY flag and both retained SCL/SDA GPIO
input levels from the reviewed route. A low line returns IO even if BUSY is
clear. Local reset does not emit GPIO pulses, replay a transaction or establish
future bus availability; missing line facts reject before register writes.

Construction is a cold task operation with no other writers of the selected
controllers. GPIO levels and reviewed AF fields are installed explicitly.
Advanced constructors capture only affected pin fields and clock bits on the
startup stack; a later acquisition failure restores those fields before the
outer platform rolls back its earlier interfaces. Existing enabled channels
or EXTI lines are rejected before pin changes. Shutdown drains interfaces in
reverse order, preserves declared CS/PWM inactive levels, releases other
controller pins to inputs and disables only the last EXTI vector owner. An
enabled IWDG remains an explicit irreversible effect and prevents a clean
platform shutdown. These are ownership guarantees, not physical timing proof.

`tests/contracts/stm32_model` compiles the real production providers against
controlled official register structs and explicit model event hooks. It tests
clock failure/rollback, timer wrap, GPIO masks, UART TC/cancel/late IRQ/RX loss,
SPI reset/CS, I2C receive profiles/repeated-START auto-clear/NACK/arbitration/STOP/recovery, Flash bounds
and pulses, irreversible IWDG failure, shared EXTI isolation, PWM and ADC faults.
Host models and ARM ELF linkage remain separate evidence. Physical clock,
IRQ timing, electrical waveform, Flash power loss and long-load qualification
are **not executed** while hardware is disconnected.
The constructor model compiles the same generated C used by firmware and
injects a GPIO clock-readback failure after an earlier pin was changed. It also
checks shared IRQ release and conflicting timer configuration. Separate
software fixtures link every maintained controller for both exact densities;
their external connectors are intentionally not qualified as Board wiring.
