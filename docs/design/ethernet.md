# Raw Ethernet MAC, MDIO and DP83848

This optional mechanism keeps network protocols, tasks, MAC-address policy,
negotiation policy, PHY power/reset circuitry and product recovery outside Nexus.
Lookup returns immutable Ethernet and MDIO faces; it never starts hardware.
The default platform does not instantiate descriptors, buffers, a PHY or a worker.

The first mode is RMII, normal four-word chained descriptors, contiguous untagged
Ethernet II frames, 14–1514 bytes excluding FCS. Length-coded IEEE 802.3,
802.1Q/802.1ad tags, jumbo frames, checksum offload, PTP and scatter/gather are
rejected or remain separate unsupported modes. TX padding and FCS insertion stay
in hardware. RX storage has at least 1518 bytes; automatic CRC/pad/type stripping
is disabled, so the reported RX descriptor length includes its four-byte FCS.
The public loan subtracts exactly four bytes after complete-frame bounds checks.
No software claim establishes cable, waveform or throughput qualification.

## Static storage and data ownership

Caller assembly supplies exact TX/RX descriptor arrays, TX slots, RX metadata
and buffers. No SDK default five-buffer pool or global cursor is linked. The
provider validates aligned descriptors, complete Ethernet-bus-master read/write
spans, array/control/payload separation and independent buffers before DMA reset
or OWN publication. TX payloads need four-byte alignment and may not overlap a
request, any provider/control/descriptor/RX storage or another borrowed request.
Read-only TX payload sharing is allowed while every corresponding borrow lives.
Current STM32/GD32 bindings accept their reviewed main SRAM only: respectively
128/192 KiB at 0x20000000. STM CCM and GD TCM are excluded; GD additional SRAM
is not yet admitted by this Ethernet binding. CPU reachability or a SPI DMA
selector does not establish Ethernet reachability.

A TX request is borrowed only after all validation and atomic admission. Buffer,
length and chained pointer are written before DMB and OWN. Normal completion
requires a CPU-owned last/first single-frame descriptor with its DMA-written TX
status. Both manuals state the last descriptor receives MAC transmit status
before ownership returns. The buffer/control reference is cleared, DSB executes,
the provider slot is detached and only then SETTLED is release-published. Success
reports the submitted bytes transmitted by the MAC, not delivery to a peer
application. Returned error status reports IO; no request is retried.

Each RX descriptor has DMA, READY or HELD metadata and a monotonically increasing
64-bit incarnation. One complete descriptor is published only after OWN returned
and an acquire barrier. READY slots form a caller-metadata FIFO: enqueue, acquire
and matching release are constant-time and never scan a variable ring under the
mask. A held descriptor remains CPU-owned at ring wrap. No copy pool or overwrite
can satisfy backpressure. CRC, length, fragmented, tagged/unsupported and other
DMA error frames are discarded with latched flags and saturating discard counts.
RBU/overflow observations are visible separately; they do not invent an exact
lost-frame count. A stale, foreign or double token cannot rearm a buffer. Epochs
survive a successful stop/restart; exhaustion closes admission rather than wraps.

## IRQ, cancellation and stop

The ISR processes at most one TX and one RX descriptor. Coalesced completions
repend the selected actual vector when another CPU-owned completed descriptor is
ready. Hardware and descriptor accesses are direct volatile loads/stores on the
MCU, with no per-register operations table. Optional cold callbacks handle NVIC
source control, repending and current-priority validation, rather than payload
bytes. The wake is a bounded IRQ-only hint; task service does not invoke it.
Its actual current priority/group must satisfy the configured kernel ceiling.
A captured wake stays borrowed until its callback returns. Attach/stop reports
BUSY while a publisher exists; a callback cannot release its own live context.

One serialized privileged task owns control methods, with incoming masks clear.
Metadata guards save/restore the incoming PRIMASK. No task, RTOS lock or worker
is created. Consumers process their loan outside those short guards. Cold storage
validation can scale with descriptor count; it runs before hardware acquisition.

Cancellation closes the shared TX engine and is explicitly controller-wide.
Other pending TX requests are cancelled too; an already returned successful wire
status remains success. Changing speed/duplex requires the external owner to
apply link-down first, then observe both DMA processes and MAC/FIFO state idle
before applying the new up fact. Until then BUSY preserves the old mode; an
unchanged up fact is idempotent. Restarting drained TX in the same mode does not
require discarding live RX. This follows the independent process and MAC enable
semantics in RM0090 §33.6.1/§33.8 and GD32F4xx manual §27.3.3/§27.4.1.
Link-down and fatal DMA/underflow errors close admission
and report IO without replay. Task service advances absolute request and drain
deadlines. Cancel/timeout alone never revoke request or payload ownership.

Withdrawal clears DMA ST and MAC TE, reads their actual state, requires TX process
state stopped, issues TX FIFO flush and observes its self-clearing acknowledgement
and inactive MAC/FIFO state. Only then may software withdraw an unexecuted OWN
bit; the descriptor is read back before references detach. A refused OWN, running
engine or stuck FIFO flush retains QUARANTINED requests when drain expires.
Later actual quiescence may settle them, but cannot invent a successful transfer.

Stop additionally closes RX/MAC/DMA admission and sources, requires RX process
state stopped and no active MAC receive path, checks MDIO/reset idle and DMA
interrupt-disable readback, detaches and clears the selected NVIC source, then
withdraws RX OWN with readback. It clears descriptor base pointers and W1C status
and checks those writes before releasing wake/storage ownership. Held consumer
loans prevent successful stop and remain immutable. Callers join all task/IRQ
publishers and consumers before reclaiming assembly storage or Board clocks/pins.
Partial startup owns its residual effects and must use this same explicit stop;
a reset/IRQ-enable failure is never replaced with successful cleanup fiction.

## Station management and PHY facts

Clause22 MDIO is a narrow, separate face using the same explicitly owned MAC
clock. Addresses/registers are 0..31. Operations run only in privileged task
context with masks clear, never wait in IRQ/PRIMASK and require a finite absolute
monotonic deadline. The divider comes from actual HCLK: the two supported bindings
use 168/200 MHz and HCLK/102, below 2.5 MHz. BUSY rejects without another command.
Timeout leaves the real hardware state intact and does not publish a late read
value; a timed-out write may already have applied and must not be blindly replayed.

DP83848 state contains an explicit address and expected model ID. TI 0x20005ca0
and NS 0x20005c90 variants are selected explicitly; only the revision nibble is
ignored. No unknown ID falls back to a supported PHY. BMCR soft reset observes
the actual self-clearing bit under the same deadline and invalidates configured
truth. It restraps the PHY but proves neither GPIO/power reset nor fitted straps.
Products select either explicit 10/100 half/full advertisement or fixed speed and
duplex. Configuration success starts negotiation; it does not assert link.
Observation reads latch-low BMSR twice plus PHYSTS and reports link only after
valid link and, when selected, complete negotiation. Remote fault, loopback/reset
or inconsistent fixed mode is not converted to successful link.

## Reviewed sources and qualification limits

Production layout/mask assertions use the locked STM CMSIS device and GD32
standard peripheral headers. Mechanism review uses ST RM0090 Rev22, Ethernet
normal descriptors, TxDMA default flow, DMA process-state/FIFO controls; GD32F4xx
User Manual Rev3.3 chapter27, normal descriptors and station-management clock
range. Both first four descriptor words and all registers used by the common
mechanism match these proofs; enhanced descriptor/PTP mode remains disabled.

DP83848 register truth comes from TI SNLS266E (March2015), Tables6-9/10/11/12/13/18
and https://www.ti.com/lit/ds/symlink/dp83848c.pdf. The documented TI model/revision
is 0x20005ca2. The explicit TI and NS ID distinction is independently confirmed
by Linux v6.12 drivers/net/phy/dp83848.c; that GPL source is review evidence only
and is not linked or copied into the production component.

The Qiming F407ZG optional RMII source profile is reviewed separately: manual
printed31/physical33 and fixed BSP blob a35e981e8a549e1dbaf84bb4522b2f150bc5d51c
show DP83848 address1, an external 50 MHz oscillator and P5/WiFi jumpers; pins
PA1/2/7, PC1/4/5, PG11/13/14 AF11. PG routes do not authorize the VE100 package.
PHY reset GPIO is unproved. Sky and Liangshan have no reviewed PHY route. Current
Board bindings must stay disabled until configuration integration admits an
explicit reviewed route; model code is not a fabricated PCB binding.

GoogleTest/GoogleMock tests execute the actual common algorithm, both SDK-checked
bindings and explicit Native ingress/egress/MDIO hardware, including W1C and stuck
reset/engine/OWN/FIFO/IRQ sources. Native hardware uses caller PHY register files,
synthetic FCS storage and explicit descriptor-completion injection; it never
invents PHY identity, link, real CRC or remote delivery. Host software, strict ARM
compilation, linked image resources and physical/HIL are separate evidence. The
first three establish only their recorded scope. Physical PHY/link/electrical,
actual PCB jumpers, loss under traffic, latency, throughput and EMC are not
executed in this environment.

Independent review regressions protect the known immutable wake-target span
against descriptors, RX DMA buffers, request metadata and writable API outputs.
Opaque callback-context size remains an external lifetime contract. Wake
replacement rechecks in-flight publishers under the same exclusion as replacing
the pointer. Stop rechecks publishers after actual NVIC withdrawal and closes
IRQ dispatch before unmasked ring/table cleanup; failed cleanup retains the
controller. Closing rejects new RX acquire; existing held loans still release.
