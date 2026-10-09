# STM32F407 runtime implementation

This change implements the first STM32 UART operation runtime and makes xE/xG
physical Flash capacity an enforced resource boundary. It does not count host
models or ARM compilation as electrical, interrupt-latency or power-fail HIL.

## UART ownership and delivery

All public STM32 UART transfer/query/cancel entry points require task context
with configurable interrupts enabled. Synchronous transfers reject a masked
caller before borrowing a buffer, since UART IRQ and HAL tick progress are
required to complete or enforce a timeout. A caller must keep interrupts enabled
while waiting. Legacy async copies validate this contract before their internal
short metadata/copy critical section.

SPI blocking transfers and queue service also reject masked task context before
controller ownership, CS assertion, OSAL waiting or DMA launch. A rejected queue
service retains its pending request for a later valid task call. Flash program
and erase require the same context because vendor busy waits use the HAL tick;
read and cache sync do not wait on that clock and preserve an existing mask.

Cancelling a ticket after its completion IRQ suppresses any unconsumed task
notification before returning success. While a user TX/RX/error callback is
executing, cancellation, polling, new submission and device close return BUSY;
poll settlement is returned only after its dispatched callback has returned.
Close reserves teardown before HAL calls, preventing a new dispatch while
resources are being removed. Successful close invalidates its ticket and clears
pending notifications and registered callbacks. A new owner registers fresh
callbacks after opening; the monotonically increasing sequence is retained.

If a failed TX abort quarantines the UART by disabling UE, RX retains earlier
events and overflow records in order, then publishes a hardware-error
discontinuity event. Further receive calls report hardware error; stale vendor
callbacks cannot restart reception. Reopening requires successful teardown.

- Logical `UART0`, `UART1`, `UART2` select USART1, USART2, USART3 respectively.
  The corresponding IRQ index uses the same zero-based logical convention.
  A board resource port initializes clocks and pins; unsupported wiring fails.
- UART implementation, state, TX copy storage and RX event ring are static per
  enabled descriptor. The RX size setting counts event records, each retaining
  its IRQ acquisition timestamp, clock resolution, data and diagnostic status.
  Registry construction reports status and never starts hardware.
- Optional `nx_uart_operations_t` submits a borrowed zero-copy TX buffer and
  returns a monotonically increasing nonzero ticket. `poll` returns a result;
  successful query does not mean successful transmission. The typed registry
  holds the device owner until settlement is observed. Closed references and
  stale tickets cannot operate a reopened instance.
- The maintained UART transport is interrupt-driven. DMA configuration is
  rejected until a real per-instance SoC/Board mapping and ownership port exist.
  Legacy async TX copies into driver storage, so it cannot retain caller memory.
- Completion requires UART TC and zero TX count. TXE or a fabricated early
  callback does not report wire completion. User TX notification is deferred
  until task-context poll and occurs once. Deadline enforcement also requires
  poll; no hidden timer or worker is claimed.
- Cancellation synchronously disables IT buffer access through HAL abort. The
  last physical byte may still be shifting; `settled=true` and `wire_idle=false`
  distinguish safe memory reuse from RS485 direction release. New submissions
  wait for TC. Abort failure masks TX sources and disables UE, clears the
  borrowed pointer, and quarantines the device with a visible hardware error.
  The typed core retains its lease after failed cancel until settled poll.
- Production RX arms a one-byte IT request during hardware open, receives its
  byte into instance-owned storage, records the event, and rearms in IRQ. It does
  not read HAL's already advanced `pRxBuffPtr`. Foreign SDK handles are ignored
  through an explicit connected-handle table rather than unsafe container casts.
  ORE, PE, FE and NE produce diagnostic events. RX overrun generates an ordered
  loss marker after retained events; later bytes are dropped until that marker
  is delivered. No unrelated TX is completed by an RX error.
- Hardware close refuses an active transfer and checks abort, IRQ detach,
  vendor deinit and board release. Failed close retains ownership and may be
  retried. Failed open also tracks partial ownership so failed cleanup is not
  silently discarded. Suspend/resume and power methods perform actual hardware
  transitions rather than changing a Boolean.

`stm32_uart_resource.h` is a vendor-free narrow board port. MB997's fixture
binding uses USART2 PA2/PA3; new product boards supply their independently
verified bindings. No DE pin or onboard RS485 transceiver is invented. A real
RS485 product must supply direction wiring and transceiver timing; the ordinary
TTL UART contract does not advertise RS485 support.

## Acquisition clock and platform ownership

The SoC acquisition clock samples the 1 kHz HAL/SysTick phase and pending tick,
then extends the 32-bit millisecond epoch. Its 1 microsecond numerical resolution
is not an accuracy, jitter or ISR-latency measurement. It requires the supported
fixed 1 kHz tick, no tickless idle, and interrupt masking shorter than one full
tick; phase reprogramming cannot publish a decreasing timestamp. Hardware timing
and long critical-section budgets remain product HIL acceptance items.

SoC IRQ preparation now uses the effective `NX_CONFIG_OSAL_FREERTOS` selection
and actual private `FreeRTOSConfig.h` threshold when clamping IRQ priorities.
The product owns scheduler startup. STM32 platform startup checks physical
Flash capacity before peripheral opening and cleans HAL up on clock failure.
Global in-process platform teardown remains explicitly unsupported after boot;
a strong `nx_platform_deinit` prevents the generic weak success from dropping
MCU ownership. Individual devices can still be closed through their contracts.

## SPI board boundary

SPI's existing bounded device arbitration, generations, cancellation and DMA
settlement remain intact. Its board port now receives only a transient immutable
binding request containing physical instance, DMA selection and private SDK
handle; it receives no controller jobs, phases, owner tokens or locks. CS and
clock queries take the physical instance ID. Vendor types stay in implementation
targets. This removes full mutable-controller coupling; static Board DMA wiring
and IRQ allocation have not yet been moved into a generated SoC resource graph.
DMA Flash-read validation uses the configured physical xE/xG Flash end.

## Physical Flash geometry

The shared F407 implementation supports 512 KiB xE and 1 MiB xG geometry. xE
reserves physical sectors 6/7 at `0x08040000..0x08080000`; xG reserves 10/11 at
`0x080C0000..0x08100000`. Each partition is two 128 KiB sectors. Runtime admission
checks the 16-bit silicon Flash-size field (KiB), effective configuration (bytes),
and both linker symbols before any data access. Misconfigured silicon or linker
layout exposes no port. Erase selection uses this checked geometry; all offset
arithmetic is bounded. The linker separately constrains firmware to its image
partition. Programming/erase can stall instruction fetch and remain maintenance
operations requiring supply and power-fail evidence.

## Executed software verification

The production UART host fixture covers three real driver instances, IRQ route,
RX rearming/pointer handling/acquisition timing, RX errors and ordered overflow,
TC versus early callback, deferred single notification, timeout/cancel, stale
sequence, invalid length/ISR calls, nested mask preservation, abort quarantine,
failed hardware close, reopen, and failed-open cleanup. The acquisition clock
fixture covers phase, pending tick, millisecond wrap and monotonic reprogramming.

The actual production Flash port runs against fixed-address host memory and a
faulting HAL model for xE and xG plus a mismatched-linker image. It covers silicon
mismatch, range/alignment/ISR fences, NOR zero-to-one rejection, physical erase
sector selection, controller exclusion, unlock/program/erase/lock failures,
post-erase verification and cache state. These are production-control-flow
checks, separate from the pure geometry regression. SPI production host
fixtures retain 10 suites, six DMA fault/race outcomes and 1,000 seeded operations
in each bare-metal and OSAL configuration. Final combined source identity,
CMake/CTest/ARM matrix and physical execution must be recorded by integration.
