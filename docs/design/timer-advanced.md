# Optional timer capture and complementary PWM

These modes are independent of the existing general-purpose `nx_pwm_port_t`.
They keep the same immutable typed face and shared operations model. Selection
owns static controller storage; no registry, pool, allocator, worker or OS lock
is introduced. An unselected mode has no instance storage. Default-image removal
is a separate whole-ELF assembly gate, not a conclusion from an object size.

| Mode | STM32F407 silicon route | GD32F470 silicon route |
|---|---|---|
| Input capture | TIM4 CH1, PB6 AF2 | TIMER3 CH0, PB6 AF2 |
| Complementary PWM | TIM1 CH1 PA8, CH1N PB13, BKIN PB12, AF1 | TIMER0 CH0 PA8, CH0_ON PB13, BRKIN PB12, AF1 |

The reviewed STM32F407 and GD32F470 datasheets establish these alternate
functions. They do not establish a connector, external load, safe electrical
level or PCB revision. External Board facts must authorize the actual route.
The main clock is the established fixed SoC profile; dynamic reclocking is
unsupported. Constructors verify enabled clock gates, APB timer selection and
actual AF fields. They do not measure crystal tolerance.

Cold assembly owns the clock, pins and IRQ vector before construction and keeps
them until stop and publisher quiescence. Its sole immutable configuration
defines exact timer clock, integer prescaler, channel, filter or complementary
output policy. `exclusive` is explicit authority over the whole timer, including
clock-gated peer writers; a disabled register is not proof of absent ownership.
The TIM1/TIMER0 break vectors are shared with TIM9/TIMER8. Assembly must reserve
that shared resource or supply reviewed static dispatch; a provider never changes
the NVIC vector on behalf of a peer. A cold configuration failure must be cleaned
up by its acquisition owner before releasing leases or publishing a factory face.

Capture uses four public controls: start, stop, read and state. Start borrows an
exact caller-owned ring. An admitted IRQ reads one CCR value, which clears its
input capture event flag in hardware, then observes overcapture status. Normal
IRQ handling acknowledges only observed overcapture W0C facts; it preserves a
new unread edge arriving after the data read. A draining IRQ may discard its
observed event without publishing. It publishes at most one value. Consumer read
copies one oldest event under a saved incoming interrupt mask. Queue operations
use conditional cursor wrap and have no division, scanning or variable-length
copy. An observable overcapture or full ring increments a saturating incident
counter; old values are never overwritten. Incidents may coalesce edges, so they
are not an exact count of physical loss.

The raw capture value is a 16-bit modulo timer count. `tick_hz` describes its
post-prescaler unit. `observed_at` comes from the common monotonic clock at IRQ
publication; it is not the edge aperture or a reconstructed absolute hardware
timestamp. Hardware filter codes specify the selected silicon sampling/filter
policy, not a measured debounce interval. Stop first disables and reads back
channel, counter and IRQ sources. Queued events require task drain; a refused
detach keeps the ring borrowed and returns BUSY. Successful stop is the final
ring access. Failed admission has no retained ring, while the execution owner
still checks and cleans any refused hardware-control write before releasing
the outer clock/pin/vector lease.

Complementary PWM has five public controls: set duty, start, stop, state and
clear fault. Period, clock, deadtime, polarity and idle levels are immutable cold
facts. Duty stages CCR shadow against that fixed base. Zero and full-period duty
remain explicit count boundaries; electrical behavior still requires the
reviewed complementary output circuit and physical validation.

Deadtime uses the actual timer tDTS with CKD zero, independently of PSC. Counts
0–127 encode directly, 128–254 by multiples of two, 256–504 by multiples of
eight and 512–1008 by multiples of sixteen. Unrepresentable counts reject;
rounding would silently change a safety-relevant physical interval. Every
hardware encoding is exercised by the production-register model. The repeated
counter, slave clock/gate selection and alternate capture XOR input are cleared
and read back during construction, rather than inherited from a prior owner.

BKIN is enabled in hardware. Automatic output reenabling and irreversible LOCK
are never selected. An active physical input or observed break latches software
fault and disables master output. Start verifies the unchanged deadtime/clock
and break configuration, rechecks pending break before enabling MOE and verifies
actual MOE readback; it cannot clear a fault. These software observations do not
prove behavior for a physical pulse wholly inside a read/write window. Pulse
width, hardware reaction and any required external fault latch are Board/system
qualification facts. Stop
requires actual MOE, both channel sources, counter and break interrupt withdrawal.
A refused write retains responsibility. Explicit clear fault requires disabled
outputs, inactive actual BKIN and a successful pending-break acknowledgment.
It does not start outputs. Products own power-stage recovery policy.

The private cold release controls require successful source withdrawal, task
event drain and an already-quiescent generated IRQ publisher. They clear only
the ended static session under a saved mask. Pins, clocks and vector leases
remain with assembly until it explicitly drives and reads back the Board's
declared inactive levels, then releases resources. Public stop makes a timer
source-withdrawal promise; it does not certify an electrical safety state.

The maintained GoogleTest/GoogleMock targets compile real production C against
locked vendor register layouts. Models provide only MMIO/CPU/clock boundaries,
including input capture read-clear, W0C, rejected stores and asynchronous edges
and hardware break. Strict ARM
compilation and type/object reports are separate evidence. Physical edge rate,
ISR latency, noise/filter response, duty/deadtime tolerance, shared-vector load,
break reaction time and power-stage safety remain **not executed** until a real
station supplies measured results. Nexus makes no safety certification claim.
