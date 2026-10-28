# GD32F303 exact software reference

The maintained part is **GD32F303RCT6**, LQFP64, 256 KiB Flash and 48 KiB SRAM.
The source-reviewed reference uses the internal IRC8 at 8 MHz. It does not
assert any PCB, LED, external crystal, connector or UART adapter wiring.
Physical HIL remains **not executed**.

The current provider implements static GPIOA PA4/PA5 input/output and USART0 PA9 TX /
PA10 RX IRQ bytes. GPIO uses the actual legacy CTL0/CTL1 four-bit fields; it does
not reuse GD32F470's incompatible two-bit GPIO/AF layout. USART0 uses its default
AFIO route, 8N1, oversample16, priority5 and a caller-sized byte ring. Shared
readonly typed operations tables receive separate private state. Factory lookup
only returns existing interfaces and never initializes hardware or allocates.

## Clock and time ownership

Official GD32F30X_HD startup invokes Nexus `SystemInit`, which performs CPU-only
FPU, VTOR and priority-group setup and never selects a PLL or starts a scheduler.
The cold transaction rejects an unknown running clock, wrong observed Flash /
SRAM density, an occupied TIMER1 gate and a live/pending reserved vector before
acquiring the timer. Density validation does not prove physical package identity.
The provider retains the fixed reset-like IRC8 domain and does not reset foreign
peripheral state or adopt a bootloader's running platform.

Reserved TIMER1 and TIMER4 are each **16-bit** counters. TIMER1 uses PSC7 and
CAR65535 for nominal 1 MHz low counts. Manual Rev3.4 Table16-1 connects its update
TRGO to TIMER4 ITI0; external clock mode0 SMC7/TRGS0 and PSC0 count every low
overflow in hardware. No IRQ is needed for each carry. TIMER1's low IRQ only
acknowledges its coalesced health flag; TIMER4's overflow IRQ extends the 32-bit
domain. The high IRQ must be serviced within **4,294,967,296 microseconds**,
including Flash stalls and masks. A high/low/high snapshot rechecks the low word
when it observes a carry and corrects one pending full wrap. This snapshot assumes
that a low update becomes visible at the high counter before a later stable
high/low/high sample; a CPU DSB alone does not establish timer-bridge propagation.
HIL must observe the ITI0 carry path and the absence of setup-UG carries. A flag
reasserting exactly after acknowledgement conservatively parks the platform even
if the high count is otherwise correct. Initialization UG
runs while master output follows disabled CEN, before arming the high slave and
selecting low update TRGO. No setup pulse is counted as elapsed time. A failed
update acknowledgement is terminal: the provider isolates each
source, retains ownership, observes the platform fault and parks. Stop disables
and clears the reserved vector again after every timer source and flag has
been withdrawn; a refused final withdrawal retains its clock and owner. Stop/restart
invalidates all existing deadlines. IRQ masks, NMI/HardFault use, IRC8 tolerance,
deep sleep and dynamic frequency are not covered by a stronger timing promise.

## Pins and asynchronous lifetime

The generated Platform owns one exact selected GPIOA mask lease. It enables the
bank, checks every selected powered nibble and lock before the first pin change,
and captures the original mode/latch. On release it withdraws selected output
modes, restores the captured latch and modes, preserves unrelated pins and any
preexisting gate, then withdraws only its added gate. Failed readback retains the
lease for explicit retry. UART and GPIO do not turn off each other's shared bank.
AFIO and USART clocks are likewise tracked and restored by cold Platform code.

An accepted TX request and payload remain borrowed until actual wire TC and
source withdrawal precede SETTLED. Cancel and timeout enter bounded drain; a
failed drain or source withdrawal keeps QUARANTINED storage. The IRQ moves at most
one TX and one RX byte, samples errors using the real STAT0/DATA clearing
sequence, and reports bounded RX overflow without overwriting queued bytes.
Optional wake callbacks are borrowed hints; stop joins captured publishers before
releasing their storage. There is no hidden worker, fallback transport or DMA.

The separate `dma-tx` and `dma-duplex` profiles add reviewed fixed-channel DMA;
the IRQ-byte profile above retains its own smaller implementation.

The concrete C Platform owner admits the exact selected controllers, leases
GPIOA and AFIO resources, and restores pins before returning owned gates.
Generation emits immutable facts, exact storage and typed lifecycle calls.
The Platform receipt uses 20 bytes; each separate pin lease uses 16 bytes.
Readback failure retains unfinished cleanup for retry. I2C0 keeps its own
PB6/PB7 lease, and no second owner captures the same selected pad.

## Finite channel DMA and I2C IRQ messages

USART0 TX uses DMA0 channel3. `dma-tx` retains an explicit byte ring and never
acquires RX DMA0 channel4. `dma-duplex` additionally receives caller-supplied
normal-mode blocks through channel4. DMA flags have four bits per channel;
F470 stream selectors, FIFO registers and flag layouts are not reused. The
shared legacy production engine checks 1..65535-byte buffers against explicit
48 KiB SRAM domains. This profile does not admit Flash TX buffers.

Generation selects the mode's shared readonly ops table, direct initializer and
IRQ functions. TX-only instances contain the compact TX context and explicit IRQ
byte ring; duplex instances embed their RX block extension after that same TX
prefix. Ordinary section GC removes unselected block-reception code from TX-only
images. The factory still returns pre-existing objects without a runtime
forwarder, new configuration knob or allocation. Representation trimming does
not change duplex loans, deadlines, withdrawal or notification contracts.

GD32F30x limitations Rev1.4 sections2.7.1/2.7.3 require TEN before CHEN. Cold
UART preparation establishes the transmitter and every TX admission verifies
UEN/TEN before retaining a request or enabling DMA. Memory TC cannot settle a
request until the actual USART TC and source withdrawal prove wire drain.
Cancel/timeout retain the accepted borrow; a refused drain remains QUARANTINED.
The first observed completion after the absolute deadline reports timeout.

RX publication follows DMAR/CHEN withdrawal, stable final count and flag
acknowledgement. IDLE returns the observed prefix; it does not promise an exact
frame boundary. Errors discard the affected block and report loss, including a
DATA byte discarded at a finite boundary. Owner service rearms free blocks and
can introduce gaps. Backpressure preserves READY and consumer loans instead of
overwriting them; RX stop remains BUSY while any loan or withdrawal remains.
There is no circular, gap-free or hidden-worker mode.

I2C0 uses default PB6/PB7 AF open-drain at nominal80 kHz, IRC8/APB8 MHz,
CCR50/TRISE9 and event/error IRQ priority0. Device limitations2.8.1 require
higher-priority receive handling; bounded protocol windows preserve the incoming
interrupt mask. Kernel notification calls from these IRQs are rejected. The
shared V1 engine remains the transfer owner; F4 stream DMA is not inferred.
The cold owner independently leases only PB6/PB7 and added GPIOB/AFIO/I2C0
gates. Stop first joins core and notification publishers, then restores the
captured pads and gates. Any refused readback retains cleanup responsibility.
External pullups, bus voltage, capacitance and endpoints are uninspected. Slave,
SMBus, automatic GPIO recovery and I2C DMA remain unsupported.

## SPI and independent watchdog

SPI0 uses default PA5 SCK / PA6 MISO / PA7 MOSI without remap, one explicitly
selected PA4 active-low CS endpoint, and finite 8-bit polling. The controller
and endpoint remain separate static identities. Each call is limited to 256 bytes
and a finite absolute deadline; the 8 MHz input permits divisors 2..256, giving
31.25 kHz..4 MHz SCK, below the datasheet's 30 MHz SPI clock limit. There is no
IRQ, DMA, hidden scratch or fallback. One whole CS interval covers a transaction.
Reset and source readback establish drain after a fault. If an accepted transfer
cannot prove reset drain or inactive CS, it retains its active execution,
controller, CS and caller-buffer borrow, masks IRQs, observes the platform fault
and parks even when the observer returns. A returning transfer has proved drain
and inactive CS before releasing its CPU-only buffers. Separate idle stop/recover
failures retain the concrete controller/CS lease for explicit retry. Physical
wire timing, external endpoint electrical requirements and CS setup/hold remain
unqualified.

FWDGT observes the hardware option without enabling or feeding during startup.
An FMC option-byte read error rejects admission; later observation failures keep
known activation responsibility and never interpret invalid USER option bits.
A product explicitly requests nominal activation and debug freeze. Official
manual Table14-1 and datasheet Table4-49 use nominal duration
`(RLD * divisor + 1) / 40000`, giving 25 us..26.208025 s. This differs from the
STM32 reload formula. The datasheet provides only a typical IRC40K frequency:
physical bounds remain `minimum_timeout_us = maximum_timeout_us = 0`.
Successful nominal programming returns SUCCESS; an enabled state query returns
UNSUPPORTED while still reporting enabled, irreversible and actual debug facts.
Products needing a guaranteed physical bound must reject this uncharacterized
profile before activation. Partial PR/RLD updates likewise establish no physical
bound. Preactivation oscillator/debug leases are restored only after readback;
failed preparation retains explicit retry responsibility. Once the enable key
is issued, errors never disable the watchdog or release its responsibility.
Platform stop reports STATE while it is enabled. Reset causes are boot-latched
without implicit acknowledgement or reboot policy.

## Software scope and extension

`references.json` binds the actual datasheet Rev3.0, manual Rev3.4 and errata
Rev1.4 bytes and section anchors. Official SDK3.0.3 source notices and its minimum
header/startup closure are pinned in the imported source lock. Native register
models execute the real production code with GoogleTest/GoogleMock; strict C11
ARM builds and Source SDK consumption have separate evidence. None proves
clock accuracy, serial waveforms or actual silicon revision applicability.

The six `gd32-f303-{baremetal,freertos}-{empty,io,spi-watchdog}.toml` assemblies
select exact static software references. The separate twelve
`tests/contracts/legacy_transport_assembly` profiles cover both exact parts,
three transport modes and baremetal/FreeRTOS as external factory consumers.
The additional
`tests/contracts/mainstream_assembly/gd303-devices*.toml` fixtures select the
physical devices below and PA1 EXTI. This does not open unimplemented modes,
USB/CAN/Ethernet or other controllers/packages. Presence in the official SDK is
not provider support. Additional clocks, routes and modes require reviewed
facts, production code, RED/GREEN contracts, ARM and SDK exits.

## Physical Flash, ADC, PWM and EXTI

FMC exposes the whole bank0 as 128 physical 2 KiB pages with 2-byte halfword
programming. Board `supply_mv` is mandatory; 3300 mV in the reference is an
authored assumption, with actual voltage unobserved. The supported declaration
is 2700..3600 mV. Products explicitly supply writable erase-aligned regions and
own power loss, watchdog, code placement and recovery. The driver never reserves
a product partition. It checks erased halfwords, except the manual's all-zero
programming exception, skips all-0xff input halfwords without a program pulse,
and reports settled hardware errors. Timeout does not
abort a pulse; the current BUSY pulse settles before return or before starting
another. Failed relock retains the owner for explicit close retry.

Datasheet Rev3.0 Table4-21 specifies **800 ms maximum page erase**. Same-bank
instruction/vector fetch can stall, so no tight IRQ deadline or RAM execution
promise is made. The hardware timer cascade preserves every low wrap across
that interval; software does not assume the old 65.536 ms IRQ service gap.
A pulse that never becomes idle never returns the caller's storage. FreeRTOS
SysTick can coalesce while same-bank fetch stalls; this reference does not infer
kernel tick catch-up from the independently preserved physical clock. No physical
erase/program has been executed.

ADC0 only admits PA0/channel0, independent 12-bit software shots, APB2/2 = 4 MHz
and sample code7. Errata Rev1.4 section2.4.1 requires at least 1 ms after enable
before calibration; the reference waits 2 ms nominal for oscillator margin.
Board VDDA decoupling and bead requirements remain external. The APB/2 clock
avoids section2.4.2's immediate-read hazard at ADC clock <= APB/4. Power stays on
until close. Physical sample/conversion timing is 239.5 + 12.5 = 252 cycles;
integer timing facts conservatively round the components and report253 cycles.
Nominal reference, analog accuracy and input wiring remain unqualified.

TIMER2 CH0 uses default PA6 and a fixed 8 MHz divisor, 1..65535 period counts,
explicit inactive Board level and zero/full duty endpoints. Shutdown first
preloads/selects GPIO inactive, then independently attempts every timer source
withdrawal. A refused controller leaf keeps the lease and cannot skip pad
detachment. Platform restores the original pin only after complete withdrawal.
TIMER2 never substitutes for reserved TIMER1/TIMER4. These ADC/PWM/Flash modes
share a private reviewed legacy engine with F103, with exact SDK ABI checks and
no forwarding-only runtime layer.

EXTI0/1 admit only PA0/PA1, independent vectors6/7, GPIOA mux0 and bounded
caller-sized raw event rings. Each IRQ acknowledges PD W1C once and records one
coalesced timestamped fact. Both-edge direction is a sampled level. Overflow
preserves queued facts and reports a later loss boundary. Stop withdraws masks,
triggers, pending source and vector before restoring the selected mux. Captured
wake publishers must finish before their target or ring can be released. Pulls,
debounce and external electrical routes remain product/Board responsibilities.
