# Reviewed shallow tickless timebase

`[os].lowpower_port = "timebase"` selects a maintained private FreeRTOS
binding for STM32F407 and GD32F470. The default is `"custom"`: existing
application-provided ports and the weak NULL fallback keep their behavior.
Selection also requires `tickless = true`, SysTick, a single privileged
security domain and no MPU profile. CPU-only Runtime configurations reject this
selection because they have no reviewed SoC timer, clock or IRQ facts.

This binding uses shallow Sleep. The fixed clock and 1 MHz monotonic timer keep
running. It does not enter STOP, suspend clocks, select a different tick source,
configure a wake pin or make board electrical claims.

| SoC | Clock / timer | Private compare | Existing shared owner |
| --- | --- | --- | --- |
| STM32F407 | HSE8 / PLL168, TIM2 PSC83 | CC1, CCR1 | TIM2 update / 64-bit epoch |
| GD32F470 | HXTAL25 / PLL200, TIMER1 PSC99 | CH0, CH0CV | TIMER1 update / 64-bit epoch |

The timer is already exclusively reserved by the platform timebase. No output
channel is connected to a pin. The getter only inspects the established profile:
the actual PLL tuple, oscillator/source ready bits, mux/bus dividers, Flash
latency and disabled spread spectrum; the free-running upward timer mode,
prescaler, full counter range, disabled compare preload/output, sleep clock,
overflow interrupt and enabled NVIC route. Missing facts return NULL;
there is no repair or hidden initialization. Clock consumers must remain
quiescent throughout platform shutdown. A pending pause or alarm additionally
rejects clock release before any register or NVIC effect.

## Phase, admission and failure

The generic Idle adapter owns scheduler suspension and incoming PRIMASK
restoration. The private timer callbacks never block or call the kernel. Alarm
IRQ dispatch runs under the SoC's saved interrupt guard. SysTick calls the
renamed real kernel tick handler once per real periodic interrupt.

Pause rejects a pending periodic tick and a still-active first short interval.
The writable snapshot must be disjoint from private retained state, immutable
bindings, MMIO and opaque clock-context storage. Known spans, alignment and
address wrap are checked before effects; the private caller proves the opaque
clock-context extent. A successful pause retains the periodic-source loan until
resume. The paused
counter records whole microseconds and the remaining core-cycle fraction. Each
capture and restore rejects a clock reversal or an observed sampling window
larger than four continuous-clock microseconds.

An alarm uses an absolute deadline in that same boot domain. Admission requires
strictly future time, at most `UINT32_MAX` microseconds ahead. The low 32 bits
match the free-running counter across one wrap. Late arm, compare readback
failure or disabled-source readback failure never become successful admission.
Rejected alarms remove only their own interrupt source. Timer status is W0C:
ones preserve all unrelated sources, including a compare event arriving between
an overflow flag read and acknowledgement.

Resume programs one short SysTick interval. It proves that VAL consumed the
short reload, then restores the full LOAD before the first underflow. Restoring
LOAD only in the first ISR would allow a second short interval. The wrapper
also restores the full LOAD before kernel dispatch. A sub-microsecond remainder
is rounded up by less than one microsecond, rather than relying on an unprovable
near-zero reload race.

The precision contract is quantized. A 1 MHz clock cannot represent arbitrary
core-cycle timestamps, and register operations have latency. The model executes
15 phase/latency combinations with a continuously advancing timer and proves
those executions fall in a conservative minus-one to plus-eleven microsecond
phase interval. The independent capture and restore window checks are the
runtime bound; the model is not a proof of physical MCU instruction latency or
all hardware executions. Repeated sleeps can accumulate bounded phase bias.
Applications needing tighter synchronization must qualify a separate timer
compare clock profile rather than assuming exact sub-microsecond phase.

A pending tick, COUNTFLAG, late first underflow, unconsumed reload or incorrect
restored control bits makes resume fail stop. Failed pause rollback also fails
stop if the source cannot be restored. Failed alarm disable retains both pause
and alarm ownership. Fault state never returns a borrowed source to the caller,
continues the kernel with missing ticks or attempts automatic reclamation.

## Cost and validation scope

Selection adds one static private context. For the maintained 32-bit ABI its
layout is 16 bytes; the read-only register binding and port face are 52 and
24 bytes. These are structure layouts, not a linked image cost claim. The
existing timebase, default request/stream state and OS objects gain no fields.
Default source selection omits the common mechanism, SoC binding and selected
IRQ branches. Four actual production CMake ARM graphs verify zero default new
mechanism TUs/symbols, versus two selected TUs and a strong getter. In the same
empty assembly retaining kernel startup, no-LTO Os/GNUArm14.3.1, selection adds
1532 bytes of Flash / 16 bytes RAM on STM32 and 1516 bytes Flash / 16 bytes RAM
on GD32. These are development fixture deltas, not application or hardware costs.

The GoogleTest / GoogleMock suites execute production common C, actual SDK
register layouts, real SoC timebase IRQ and shutdown code. Tests cover pending
arrival, reversed time, late compare, counter wrap, failed disable/restore,
first-reload consumption, second-pause rejection and shared IRQ flag races.
The two chip suites use external CPU, clock-bootstrap and register semantics
models; they are software evidence.

Physical HIL has not run. Acceptance still requires the real boards and probes:
verify shallow-Sleep clock continuity, NVIC wake, actual capture/restore windows,
SysTick phase and catch-up across repeated sleeps, wrap, competing interrupts
and load. Neither model timing nor ARM linking establishes power savings,
physical WCET, IRQ latency or board readiness.
