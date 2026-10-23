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

Selected transport extensions add USART2 finite DMA TX and duplex finite RX
blocks, plus default PB6/PB7 I2C1 IRQ messages. The exact source and ownership
limits below remain distinct from physical Board validation.

## Source and electrical boundary

The private CMSIS and GPIO/USART LL headers use the exact pair pinned by official
STM32CubeF1 v1.8.7. The two dependency locks retain original notices and selected
file identities. Vendor types do not enter public Nexus interfaces. Full HAL C
engines, mutable Handles, HAL tick and `HAL_Init` are not linked. Finite direct
control writes preserve request ownership and avoid LL exclusive retry loops.
F1 CRL/CRH nibbles and SR/DR acknowledgement differ from modern STM32 register
layouts; neither modern MODER/AFR nor USART ICR/TEACK is inferred. The shared
layer boundaries follow [the platform contracts](../../../docs/design/README.md).

The NUCLEO-F103RB Board is a source reference from UM1724 Rev17. PA5/LD2 and
USART2 PA2/PA3 VCP are documented routes; SB13/SB14 ON and SB62/SB63 OFF must be
checked on the actual Board. The physical PCB revision and bridge population
remain unknown. No external oscillator is assumed. Nominal HSI8 frequency and
serial baud retain the datasheet tolerance; no calibrated measurement is claimed.

Standalone startup reads the factory Flash-size halfword at `0x1FFFF7E0` and
requires 128 KiB for the declared STM32F103RBT6 target. RM0008 Rev21 section30.1.1
permits this CPU read. ES096 Rev15 section2.2.2 states that normal software
cannot read `DBGMCU_IDCODE` or `DBGMCU_CR`; it reads zero outside debug mode.
Startup therefore does not use debug visibility as admission. Device group and
silicon revision remain unobserved, and matching density does not prove the
orderable part, package or Board. A strict revision policy cannot accept an
unavailable debug read as revision A, accept an assembly literal as an
observation, or infer permission from a newer-looking code.

The same ES096 limitation excludes ordinary-firmware `DBGMCU_CR` access.
IWDG enable supports `debug_freeze=false`; a request for `true` returns
`UNSUPPORTED` after context and output validation, before any hardware access
or activation. Enable, feed, state and failed-preparation cleanup never read
or write debug control. The reported false is the supported CPU configuration,
not an observation of a debugger's setting. An external debugger owns its own
freeze setup; this provider neither adopts nor clears that setup. The HAL/LL
debug freeze macros directly access `DBGMCU_CR` and do not remove this erratum.

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

## Finite UART channel DMA and I2C

`dma-tx` uses DMA1 channel7 and the selected IRQ byte ring; it does not acquire
channel6. `dma-duplex` additionally uses channel6 for normal RX blocks supplied
by the caller. TX and each RX block contain 1..65535 bytes inside the declared
20 KiB SRAM DMA domain. TX payloads must be placed in that admitted SRAM;
Flash source reachability is not opened by this profile. The SDK wrapper checks
the actual USART/DMA layout, gates and every selected vector. A foreign channel
is rejected before admission. Generation selects one shared readonly ops table
for the authored mode and one exact static instance; factory lookup returns that
object. `dma-tx` owns a compact TX context and its IRQ byte ring. `dma-duplex`
embeds the RX block extension after the same TX prefix. Direct selected
initializers and vectors let ordinary section GC remove unused block-reception
code from TX-only images; no runtime forwarding layer or configuration switch
is added. Both modes retain the same request, drain and wake contracts.

DMA completion only ends memory movement. Accepted TX retains its request and
payload until the DMA request/channel/IRQ sources detach and real wire TC is
observed. A completion first observed after the absolute deadline reports timeout.
A refused drain retains QUARANTINED storage. RX publishes only after DMAR/EN
withdrawal, a stable final count and acknowledgement. IDLE reports the observed
prefix, not an exact timestamped byte boundary. Hardware errors discard the
affected block and report loss. A DATA byte discarded during a finite boundary
also reports loss. Owner service rearms a free block; gaps are possible and a
borrowed consumer block is never reused. RX stop returns BUSY until producer,
READY and consumer loans are drained; it never revokes them.

I2C1 uses default PB6 SCL/PB7 SDA, legacy AF open-drain, nominal 80 kHz at
APB8 MHz (CCR50/TRISE9), and event/error IRQ priority0. ES096 2.8.1 requires
highest-priority service; 2.8.2 receive windows remain under the saved interrupt
mask; 2.8.5 excludes the affected 88..100 kHz repeated-START range. Kernel
callbacks are rejected from these priority-zero IRQs. A FreeRTOS owner may
poll the latched request or use a non-kernel notification mechanism. Products
must budget global interrupt masking and inspect physical bus timing.

The I2C cold owner separately leases only PB6/PB7 and added GPIOB/AFIO/I2C1
gates. It preloads released lines, preserves unrelated pins and restores the
captured nibbles/latches only after core IRQ/wake withdrawal. External pullups,
bus voltage, capacitance, endpoint wiring and silicon revision remain external
qualification inputs. A low/busy bus is rejected; no implicit GPIO bus clear,
product recovery, SMBus, slave mode or I2C channel-DMA support is introduced.

The external `tests/contracts/legacy_transport_assembly` consumer selects each
new mode independently for baremetal and FreeRTOS. Its ARM images and host
register models establish software execution and linking, not MCU waveforms.

## Physical Flash, ADC and PWM

The exact Flash face exposes all 128 physical 1 KiB pages and a 2-byte program
unit. The existing `poll-word` mode name does not imply 32-bit programming.
Authored Board bindings declare `supply_mv`; the software reference uses 3300 mV,
which is an input assumption and does not measure the actual supply. The driver
admits 2700..3600 mV and exposes no product partition. Consumers must select an
explicit writable, erase-aligned region and keep running code, vectors and live
data outside destructive operations. Erase/program wait until hardware BUSY is
clear even after the absolute deadline; timeout only prevents the next pulse.
Neither cancellation nor a timeout aborts a physical pulse. Each halfword must
be erased, except programming all zero. All-0xff input halfwords issue no
program pulse. ES096 section 2.2.11's delayed BSY is
handled by an intervening control-register read and barrier after erase start.
Failed relock retains the initialized owner for explicit close retry.

Same-bank instruction/vector fetch can stall during the pulse. DS5319 maximum
1 KiB page erase is 40 ms; the 65.536 ms TIM2 service bound still includes that
stall, other interrupt latency and masks. This mode establishes no tight IRQ
deadline, RAM execution closure or watchdog feeding policy. A hardware pulse
that never becomes idle never returns its caller's storage. FreeRTOS SysTick can
coalesce during same-bank fetch stalls; this reference does not provide kernel
tick catch-up.

ADC1 selects only PA0/channel0, independent 12-bit software shots, APB2/2 = 4 MHz
and sample code7 (239.5 sample + 12.5 conversion cycles). It waits four nominal
microseconds, resets calibration and completes calibration before admitting
samples. The converter stays powered until close. The integer sequence API
rounds both half-cycle values upward and reports a conservative 253-cycle
scheduling bound; the physical conversion sum is 252 cycles. The nominal
reference is caller-declared; input impedance, PA0 glitch applicability,
accuracy and wiring require physical validation. No DMA/stream mode is implied.

TIM3 CH1 uses PA6 default mapping with one fixed 8 MHz divisor and a 16-bit
period of 1..65535 counts. Board `initial` declares the inactive level. Start
loads shadow registers while TRGO follows disabled CEN, then enables alternate
output and the timer. Stop preloads and selects the declared GPIO inactive
level, then independently attempts each timer withdrawal. A refused timer leaf
does not prevent pad detachment; failed cleanup retains the timer and pin lease.
Platform restores the original pin only after complete withdrawal. Zero and full
duty are explicit CCR0 and CCR=period. ADC/PWM/Flash reuse one private reviewed
legacy engine with GD303; each exact family checks its own official register ABI
and static resource facts. There is no extra runtime forwarding table.

## Validation scope

GoogleTest/GoogleMock execute actual production C against external hardware
models. Python tests check exact facts, routes, rejected modes and construction.
ARM fixtures consume the typed factory in baremetal and FreeRTOS images. These
establish software behavior and linking only. Physical HIL is `not_executed`.
The original manuals and exact observations are indexed in `references.json`.
