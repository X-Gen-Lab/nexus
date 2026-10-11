# ThreadX backend

This backend uses the official Eclipse ThreadX kernel, not a scheduler model.
The reviewed source is `v6.5.2.202603_rel`, commit
`93387b0a60382a29579bbad5045c68328e3751df`, under the original MIT license.
The intended first integration scope is the Linux GNU port and the Cortex-M4 GNU
port for STM32F407 and GD32F470. Other Cortex profiles, MPU/TZ task isolation,
tickless sleep, direct notification slots and runtime scheduler shutdown are not
implemented by this adapter. Arch and FreeRTOS support remain separate.

`nexus/os/threadx.h` exposes caller-owned, backend-specific static services. The
portable wait face is still the existing four-pointer `nx_wait_port_t`; device
faces, typed factory and ordinary I/O calls do not acquire a ThreadX dependency.
Selecting a different OS does not change the generated device identities.
Firmware that does not select ThreadX creates no production ThreadX target or
linked kernel objects/RAM. Host contracts explicitly select their own isolated
kernel fixture, as the FreeRTOS host contracts do.

## Startup and lifetime

The product first initializes the platform and its continuous monotonic clock,
then calls `nx_threadx_kernel_enter(startup, context)`. It must enter privileged,
unmasked Thread mode using MSP with CONTROL.FPCA clear. The attempted kernel entry
is irreversible. The adapter's actual `tx_application_define` callback opens the
only bootstrap scope, invokes product startup, then seals that scope before
returning to the kernel. Callback entry also validates actual privileged Thread
context and canonical bootstrap masks before the product runs. Only the trusted
kernel may call the adapter callback; this check is not a privilege boundary.
The product does not define a competing callback.

Startup may initialize notifications/queues, create tasks, publish a wake hint
and reverse-discard a created returning task that has never executed. It may not
block, join or operate queues. The narrow pinned internal terminate/delete
services used by prestart discard are permitted only in this proven callback
scope. Comparing a scheduler state or interrupt mask elsewhere never reopens it.
Partial startup failure must either complete proven reverse discard, or retain
all outstanding effects and enter the product's safe failure policy.

At runtime, returning tasks cooperate with stop/admission closure, publishers
quiesce, accepted I/O drains or remains quarantined, and an external single joiner
joins tasks before reclaiming controls, borrowed names, stacks and contexts. A
completion semaphore is only a hint. Successful real ThreadX thread deletion
establishes that scheduler references are detached; timeout retains all storage.
Task creation may run a higher-priority entry before creation returns. Failed
resume retains a live control, rather than guessing that the entry never ran.

Permanent tasks have no completion latch, join or destruction. Control, name,
stack and context stay valid until reset. A returning permanent entry invokes
retaining fail-stop. An unexpected `tx_kernel_enter` return also invokes
fail-stop; it does not authorize prestart rollback. The weak default spins
forever; product output safety and reset are external responsibilities, with no
promised completion time. Kernel entry/shutdown examples must retain platform
clocks and IRQ infrastructure while a scheduler is still alive.

Every live control is adapter-owned. One external owner serializes object
init/destroy and task join. It must stop new users and quiesce existing task/IRQ
publishers before destruction; a caller-visible `live` field is not a lock or a
permission to reclaim concurrent users. Controls, task stacks, complete borrowed
names/contexts, queue payloads, item buffers and waiters must be disjoint. Cold
admission rejects wrapping ranges and known control/stack/payload overlap; it
also rejects name/context starting inside a control or stack. Opaque context
extents cannot be inferred and remain the caller's explicit responsibility.

## Wait and queues

A notification uses a real binary ceiling-put semaphore and a release-published
sequence hint. There is one concurrent blocking waiter; each publisher needs its
own valid task or reviewed IRQ context. Blocking finite waits compare the Core
absolute deadline before observing a changed sequence on every iteration.
`NX_DEADLINE_NEVER` uses a real unbounded kernel wait. Bounded kernel tick chunks
preserve the original absolute budget; no retry obtains a new relative budget.
The portable caller still rechecks its authoritative completion predicate.

The byte queue uses two real counting semaphores and caller payload storage.
ThreadX's native word queue only accepts 1–16 `ULONG` words; using it directly
would narrow Nexus's arbitrary-byte contract. This queue supports exact 3-byte
and larger non-word payloads without rounding or undocumented padding. Caller
capacity is exactly `depth * item_size`; all products must validate their actual
payload and depth budgets. Immediate available data/space wins over an expired
queue deadline, while failed receive leaves output unchanged.

A task preemption-threshold guard serializes queue commit and close registries.
It saves/restores the incoming threshold and leaves IRQ delivery enabled. Payload
copy size and the explicit maximum simultaneous waiters determine scheduling
pause. They do not turn the entire copy or broadcast into an IRQ-masked region.
The selected port must support preemption thresholds. Foreign priority-zero
threads with a nonzero time slice are rejected before mutation because threshold
zero cannot exclude their equal-priority rotation. Nexus-created tasks have no
time slice and meet the guard contract. Foreign callers must follow the same
single-core privileged lifecycle/context rules and must not mutate another
caller's scheduling parameters.

The optional closable queue adds two caller-linked waiter lists. Each simultaneous
operation supplies its own initialized waiter. Registration, admission and close
share the task guard; sequence arm before releasing it closes wake-before-block.
Close is idempotent, rejects future sends, immediately latches every registered
waiter and preserves old items for draining. There is no polling worker or global
waiter pool. Destroy requires closed, drained, inactive and externally quiesced
users. Raw queues require drained/quiesced users but have no close broadcast. Never mix
raw operations on a closable queue's inner object with closable operations; raw
waiters are not registered and cannot receive close broadcast.

## Cortex-M4 integration

Use the pinned `ports/cortex_m4/gnu` C/ASM headers and all seven upstream ASM
sources listed by that port's CMake file. The official schedule object owns
both strong `PendSV_Handler` and `__tx_PendSVHandler` aliases at the same naked
entry; an extra wrapper would conflict or change the exception frame.
`m4_vectors.S` supplies only SysTick wiring to the actual `_tx_timer_interrupt`.
This port does not require SVC. The port's real FP context handling remains intact.

`m4.c` supplies `_tx_initialize_low_level` instead of the upstream demo. It uses
resolved `NEXUS_CORE_HZ`, `NEXUS_OS_TICK_HZ`, `NEXUS_OS_SYSTICK_RELOAD`,
`NEXUS_OS_EXTERNAL_TICK`, `NEXUS_IRQ_PRIORITY_BITS` and
`NEXUS_IRQ_SYSCALL_PRIORITY`. No 6 MHz sample clock, DWT clock or guessed heap is
used. PendSV receives the lowest implemented priority; SysTick receives the
selected kernel ceiling, or the lowest priority with PRIMASK policy. Unrelated
SHPR3 fields are preserved. Startup masks PRIMASK; the real upstream scheduler
releases its bootstrap mask.

BASEPRI selection must set `TX_PORT_USE_BASEPRI` and the same numeric literal
`TX_PORT_BASEPRI` for both C and ASM. Its encoded value must equal the resolved
logical syscall priority shifted by the implemented IRQ width. A generated C
macro with a `U` suffix is not a valid GAS immediate. PRIMASK selection omits
these macros. `nx_threadx_isr_allowed` rejects system exceptions, out-of-range
external IRQs, unprivileged/masked contexts, incompatible priority grouping and
BASEPRI IRQ priorities above the syscall ceiling before kernel wake mutation.
Native cannot claim a hardware ISR and returns false.

Explicit external Tick requires the product's strong
`nx_threadx_external_tick_setup(tick_hz)` implementation. Missing implementation
must fail linking; failure parks with effects retained. That hook binds a real
bounded periodic IRQ at the correct kernel ceiling and does not replace the
continuous monotonic clock. Existing STM32 TIM2 and GD32 TIMER1 platform clocks
remain the Core deadline domain. The default config processes timer callbacks in
ISR and creates no hidden timer task/stack; optional kernel timer callbacks must
therefore meet ISR bounds. ThreadX has no automatically allocated idle task;
scheduler idle consumes the application-supplied startup/MSP budget.

## Linux qualification scope

The reviewed Linux GNU port runs real ThreadX pthread scheduler, task and timer
threads. The host port allocates memory internally and uses its own pthread
stacks; caller stack bytes are kernel metadata capacity, not a Linux or sanitizer
stack budget. Unprivileged hosts may not obtain upstream SCHED_FIFO policy. Test
fixtures bind the first actually permitted CPU; they do not enable the port's
random multi-core affinity option. These tests prove behavior, not MCU RAM,
real-time timing, physical IRQ priority or host privilege.

`prepare_linux.py` produces a fresh build-local overlay from one exact reviewed
source SHA. It preserves upstream copyright/license and leaves the checkout
unchanged. Only the timer clock, corresponding semaphore wait and exact 1e9
nanosecond carry are corrected: CLOCK_MONOTONIC + GNU `sem_clockwait`, and `>=`
normalization. Unknown source/patch/output or a symlink is rejected. A selecting
CMake target must genuinely compile and link a `_GNU_SOURCE`/`Threads::Threads`
`sem_clockwait` capability probe and fail when unavailable. The derivative never
substitutes an unbounded realtime wait or weak fallback.

Actual wall-clock adjustment requires a suitably privileged host and remains
not executed. Source/clock-domain conformance and mocked libc boundary tests do
not establish that physical host experiment.

## Cost and evidence

The development ARM object probe uses GNU 14.3, Cortex-M4F hard-float, `-Os`,
32 priorities, timer processing in ISR and the exact reviewed port header:

| Caller-owned object | Bytes |
| --- | ---: |
| Kernel TCB / semaphore | 180 / 32 |
| Binary notification / portable wait face | 44 / 16 |
| Returning / permanent task control | 232 / 192 |
| Raw / closable byte queue control | 96 / 112 |
| Each close-aware operation waiter | 52 |

Add actual task stacks and `depth * item_size` payload storage. Permanent tasks
save 40 bytes here; closable queues add 16 bytes plus explicit waiter controls.
These are real 32-bit ARM `sizeof` symbols, not total linked firmware RAM or
Flash, timing measurements or a board qualification. Kernel configuration and
compiler ABI changes require a fresh probe and actual ELF/map retention report.

The IO34 development evidence records real official Linux kernel GoogleTest
execution, actual bootstrap callback lifecycle, queue/close/deadline behaviors,
preimplementation RED snapshots, Cortex-M4 register boundary tests and the whole
Linux timer source with mocked external libc boundaries. MCU vector/tick linking
on three boards, installed-source SDK consumption, final clean source CI and
physical HIL are separate acceptance steps. No port name, stub or software model
may mark those steps passed.

The maintained [ARM matrix gate](../../scripts/ci/os_threadx.py) links the actual
three Board graphs under both supported IRQ policies and checks the missing
external-Tick provider as a separate negative link. It verifies same-address
strong official PendSV aliases, all locked compiled kernel objects, resolved
policy, ELF budget and exact evidence identities. Host Google contracts select
an independent real Linux kernel through `tests/google/threadx.cmake`; eight
executables currently exercise 30 runtime cases, six actual initialization
callback cases and 14 external-boundary mechanism cases. Final clean candidate,
SDK and physical qualification remain separate from development execution.
