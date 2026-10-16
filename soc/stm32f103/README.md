# STM32F103RB provider

The maintained exact part is STM32F103RBT6, LQFP64, 128 KiB Flash and 20 KiB
SRAM. This is a Cortex-M3 without FPU, DSP or MPU. Its 43 external IRQ slots
and four implemented priority bits come from the official device header. Other
STM32F1 densities, packages and connectivity-line devices are not implied.

The current vertical slice consists of official startup, CPU-only pre-RAM
`SystemInit`, reset HSI8 admission, independent TIM2 time, GPIOA PA5 and USART2
PA2/PA3 IRQ bytes, independent IWDG, PA0/PA1 EXTI events and bounded SPI1 polling with PA4 CS.
The static typed factory returns pre-existing faces. Private
states and bounded RX rings are generated once; no heap, task, registry, lock or
scheduler is introduced. Other modes fail configuration admission.

## Source and electrical boundary

The private CMSIS and GPIO/USART LL headers use the exact pair pinned by official
STM32CubeF1 v1.8.7. The two dependency locks retain original notices and selected
file identities. Vendor types do not enter public Nexus interfaces. Full HAL C
engines, mutable Handles, HAL tick and `HAL_Init` are not linked. Finite direct
control writes preserve request ownership and avoid LL exclusive retry loops.
F1 CRL/CRH nibbles and SR/DR acknowledgement differ from modern STM32 register
layouts; neither modern MODER/AFR nor USART ICR/TEACK is inferred. The shared
layer boundaries follow [the platform contracts](../../docs/design/README.md).

The NUCLEO-F103RB Board is a source reference from UM1724 Rev17. PA5/LD2 and
USART2 PA2/PA3 VCP are documented routes; SB13/SB14 ON and SB62/SB63 OFF must be
checked on the actual Board. The physical PCB revision and bridge population
remain unknown. No external oscillator is assumed. Nominal HSI8 frequency and
serial baud retain the datasheet tolerance; no calibrated measurement is claimed.

## Clock and ownership

Cold admission rejects a foreign clock profile, timer, vector, USART gate,
remapped route, locked pad or driven pin before taking ownership. TIM2 is a
**16-bit** counter on F103. PSC7 and ARR65535 yield nominal 1 MHz and a 65.536 ms
wrap interval. The overflow handler must execute at least once per interval;
long IRQ masks, NMI/HardFault reads and deep STOP continuity are unsupported.
One pending wrap is corrected in the timestamp snapshot. Prescaler UG is
issued with MMS=CEN while CEN=0, so it cannot export an unselected TRGO pulse. An observed W0C failure
invalidates the time epoch, retains every owner, observes the fault and parks.

Generated assembly leases only selected CRL fields and output latches. GPIO
preloads the latch before enabling output. Timer stop withdraws the source and
observes cleared flags before its final NVIC disable/pending proof. Stop first quiesces controller and
notification references, then restores selected pins and owned gates. Failed
readback retains unfinished cleanup responsibility. Every consumer joins before
the timer is stopped; old deadlines are invalid after a new epoch starts.

USART TX borrows request and payload until actual TC and IRQ source withdrawal
precede release-publication of SETTLED. Timeout and cancel stop loading bytes;
failed drain retains QUARANTINED storage. RX has an explicit byte ring and reports
loss without overwriting unread bytes. IRQ notification is only a hint; captured
callbacks must drain before their borrowed target can be returned.

EXTI0/1 use independent vectors 6/7 and AFIO EXTICR GPIOA selection. PA0 and PA1
are documented CN8 A0/A1 pins 1/2; external stimulus, voltage, pull and debounce
remain external responsibilities. Each IRQ acknowledges W1C once and records
one coalesced timestamped fact. Both-edge direction is a sampled level, not an
edge history. A full ring preserves old facts and then reports a loss boundary.
Stop observes source/vector withdrawal and restores only its selected mux field.

IWDG uses DS5319 nominal 40 kHz and 30..60 kHz LSI uncertainty, not F4's different
oscillator bounds. PR and RLR can latch independently, so a failed update retains
a conservative cross-product timeout envelope. Activation is irreversible;
Platform stop reports STATE while enabled. Failed preactivation LSI/debug
rollback retains private leases and requires ordinary stop retry. Hardware
watchdog option bytes are observed without rewriting them. Pending automatic
PR/RLR updates report enabled/irreversible with unknown bounds and UNSUPPORTED. Products own feed,
health, reset and debug policy. No destructive hardware enable has been tested.

SPI1 uses default PA5 SCK, PA6 MISO and PA7 MOSI, with one explicit PA4
active-low endpoint. The selected endpoint frequency is 31,250..4,000,000 Hz,
rounded down by the actual 8 MHz APB2 divisor. Each privileged task call has a
finite absolute deadline, one whole-CS interval and 1..256 full-duplex bytes.
Modes 0..3 and exact in-place buffers are supported; partial overlap is rejected.
Success follows actual BSY drain, CS-high readback and a final deadline check.
A failed transfer resets and checks the controller before releasing buffers;
recovery is explicit. An unresettable wire or unprovable CS release retains the
executor and caller buffers, masks interrupts, observes the fault and parks if
the observer returns. This terminal case never reports completion. PA5 is also
LD2, so the LED and SPI clock cannot be selected together; its electrical load
requires physical validation. No SPI DMA or asynchronous execution is claimed.

## Validation scope

GoogleTest/GoogleMock execute actual production C against external hardware
models. Python tests check exact facts, routes, rejected modes and construction.
ARM fixtures consume the typed factory in baremetal and FreeRTOS images. These
establish software behavior and linking only. Physical HIL is `not_executed`.
The original manuals and exact observations are indexed in `references.json`.
