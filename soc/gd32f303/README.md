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

Reserved TIMER1 is a **16-bit** counter with PSC7 and CAR65535, giving nominal
1 MHz microseconds. Its overflow vector extends the domain, and one pending wrap
is accounted for during a mask-preserving snapshot. The overflow must be serviced
within **65,536 microseconds**, including interrupt masking and higher-priority
load. A failed update acknowledgement is terminal: the provider isolates each
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
select exact static software references. I2C, Flash program/erase, EXTI, PWM,
ADC, DMA, USB/CAN/Ethernet and other controllers/packages remain **unsupported**.
Their presence in the official SDK is not provider support. The Flash page
geometry is 128 physical 2 KiB pages; products own explicit reservations when a
future Flash provider is implemented. Additional clocks, routes and modes require
separate reviewed facts, production code, RED/GREEN contracts, ARM and SDK exits.
