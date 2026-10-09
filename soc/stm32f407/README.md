# STM32F407 implementation

This package implements fixed typed C ports for **STM32F407VET6** (LQFP100,
512 KiB Flash) and **STM32F407ZGT6** (LQFP144, 1 MiB Flash). Main SRAM is
128 KiB; the separate 64 KiB CCM domain is described but excluded from the
initial linker and DMA policy. No product Flash partition is reserved.

`system.c` establishes the reviewed 8 MHz HSE / 168 MHz PLL profile, APB1
42 MHz and APB2 84 MHz. Reserved TIM2 runs at 1 MHz, with an overflow IRQ to
extend its 32-bit counter. The overflow IRQ must run at least once every
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
| UART | USART1 PA9/PA10 AF7, 8N1 IRQ TX, true TC completion, cancellation drain and quarantine; per-instance byte or timestamped-event storage |
| SPI | SPI1 PA5/PA6/PA7 AF5, 8-bit master, modes 0–3, 2–256 prescaler, finite absolute deadline and at most 256 bytes; one CS interval; fault abort resets controller |
| I2C | I2C1 PB6/PB7 AF4, 42 MHz / 100 kHz, 7-bit address, at most 8 messages of 256 bytes, finite deadline; repeated START and one/two/final-three-byte ACK handling; explicit NACK/arbitration/STOP failure |
| Flash | Exact VE 8-sector / ZG 12-sector geometry, x32 program at caller-declared 2.7–3.6 V; range/alignment checks, complete pulses, cache invalidation; erase is never preempted by a C deadline |
| IWDG | 4–256 prescaler, 12-bit reload, nominal 32 kHz with 17–47 kHz declared LSI bounds; hardware-option activation and irreversible software enable reported; debug freeze explicit |
| EXTI | Static lines, bounded shared-vector dispatch, per-line event storage and observable overflow; pending hardware bits can coalesce edges; no debounce policy |
| PWM | TIM3 general-purpose channels, fixed shared PSC/ARR, period 1–65535 counts, duty 0–period, shadow compare update; stop restores GPIO inactive level |
| ADC | ADC1 independent 12-bit software shots, sequential low-rate scans of up to 16 external channels; per-channel sample time; nominal reference is not calibration; no DMA/trigger/stream |

Only LED and USART1 external routes are bound by the two reference Board
packages. Internal Flash/IWDG bindings are available for explicit selection;
they neither reserve storage nor enable the watchdog during startup.
Other SoC routes describe controller implementation and silicon AF resources;
they do not establish Board wiring, external pull-ups, device fitting or
physical support. Consumers must supply reviewed external Board bindings to
select those routes. Capture, advanced timers, DMA, I2C GPIO pulse recovery,
internal ADC channels, bootloader relocation and all other modes are rejected
or absent from the selectable schema.

All ports have one execution owner. No heap, registry, string lookup, default
worker, OS lock, generic device reference or reserved copy pool is introduced.
UART IRQ moves bytes and hardware facts; task service observes transfer
deadlines and is required throughout cancel/stop. A late TC may resolve a
quarantined transfer; until then its request remains borrowed. After SETTLED,
no IRQ or provider reference remains. Request owners must preserve storage
until settlement and separate observer/adapter drain.

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
SPI reset/CS, I2C receive profiles/NACK/arbitration/STOP/recovery, Flash bounds
and pulses, irreversible IWDG failure, shared EXTI isolation, PWM and ADC faults.
Host models and ARM ELF linkage remain separate evidence. Physical clock,
IRQ timing, electrical waveform, Flash power loss and long-load qualification
are **not executed** while hardware is disconnected.
The constructor model compiles the same generated C used by firmware and
injects a GPIO clock-readback failure after an earlier pin was changed. It also
checks shared IRQ release and conflicting timer configuration. Separate
software fixtures link every maintained controller for both exact densities;
their external connectors are intentionally not qualified as Board wiring.
