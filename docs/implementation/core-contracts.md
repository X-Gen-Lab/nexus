# HAL, OSAL and logging execution contracts

This implementation addresses HAL-001/HAL-002, OS-001/OS-002 and MEM-001 for
industrial control and connected-device products. It changes public lifecycle
contracts deliberately; callers must inspect cleanup status before releasing
owned buffers, callback arguments or backend contexts.

## Implemented boundaries

| Area | Implemented contract |
| --- | --- |
| Critical sections | Cortex-M HAL saves/restores PRIMASK using architecture selection; Native OSAL uses a recursive thread lock. Nesting preserves the initial state. Allocation, sleep and blocking calls are forbidden inside critical regions. |
| Power manager | Static interface initialization; NULL/foreign instances fail explicitly, invalid queries return `NX_POWER_UNKNOWN`, and unimplemented SLEEP/STOP return NOT_SUPPORTED without claiming a hardware transition. |
| HAL atomic access | HAL atomic operations serialize the complete access using the architecture primitive. Ordinary/volatile field access does not implement synchronization. |
| Devices | Static device descriptors are initialized once under a protected state transition. Concurrent initialization returns NULL for retry. Vendor initialization executes outside the common lock. ISR initialization is rejected. GPIO factories resolve dedicated read/write descriptors first, then the capabilities of a combined GPIO device. |
| Handles | Native, FreeRTOS and baremetal use type-tagged opaque lifetime tokens. Validation does not dereference user handles. A stale token cannot identify a reused slot. Token exhaustion fails rather than wrapping. |
| Finite waits | Native preserves one monotonic deadline across notifications. FreeRTOS uses overflow-safe positive millisecond round-up and reserves the forever sentinel. Baremetal requires an installed board clock and uses unsigned elapsed time across tick wrap. |
| Queues | Checked size/count multiplication, configured per-item/per-queue limits, FIFO/front order and atomic overwrite of the oldest entry. `osal_queue_send_overwrite` is a nonblocking task operation and does not mutate queue mode. |
| Mutexes | Recursive ownership is enforced. Unlock by a different thread/task fails; deletion while held returns BUSY and retains ownership. |
| Events | Portable bits are 0..23. WAIT_ALL/WAIT_ANY are explicit. Barrier synchronization requires WAIT_ALL with auto-clear; waiters observe the same matching snapshot before bits are cleared. |
| Tasks | Stop is cooperative. A running delete requests stop and returns BUSY. The function must return; join confirms completion before handle deletion. Native still enters a function when stop was requested before first scheduling, allowing accepted work to drain. |
| Timers | Successful task-context stop/delete settles callback ownership. Native callback-in-progress operations return BUSY. FreeRTOS confirms commands using timer-daemon barriers, retains timed-out lifetimes for retry, and atomically invalidates the final deleting pin and slot. |
| Memory | Heap/custom allocation is a task-management service. HAL static pools carry actual buffer and bitmap capacities, aligned strides, protected allocation/accounting, and double-free rejection. Allocator replacement is BUSY while an allocation or allocation attempt is live. |

FreeRTOS operations pin kernel objects across blocking calls. Destruction returns
BUSY while a waiter/owner pins an object. Native semaphore/queue/event destruction
instead cancels blocked waiters with `OSAL_ERROR_CANCELLED`; permanent slot
condition variables let old waiters observe deletion even after immediate reuse.
Products should quiesce users before destruction across either backend.

FreeRTOS event updates deferred from ISR retain an object pin until the timer
daemon executes the update. Timer `*_from_isr` operations acknowledge command
acceptance only; they do **not** return callback-argument ownership. Complete
settlement in a management task. Cortex-M FreeRTOS APIs require an eligible IRQ
priority; the supported F407 reference uses four preemption bits, syscall
threshold 5, and SysTick/PendSV priority 15. IRQs 0..4 must not call OSAL.

## Backend capabilities and configuration

Baremetal owns one product control thread and hardware ISRs. It provides static
mutex/semaphore/queue/event services, not a fabricated stackful scheduler. Task,
software-timer and heap APIs explicitly report unsupported capabilities. Install
`osal_baremetal_set_clock` once after board timebase initialization. Before that,
finite timed operations and `osal_get_time_ms` fail explicitly. The native
baremetal executable models these board hooks; it does not validate electrical
or Cortex-M interrupt behavior.

Native `*_from_isr` calls simulate nonblocking task operations. They are not
POSIX signal-safe and do not prove MCU ISR execution. Native stack watermark and
host heap totals are simulation information, not measured MCU budgets. Custom
HAL allocators must return suitably aligned storage and keep their captured
context alive until all corresponding allocations are freed; manually supplied
static pools must provide aligned backing storage and strides.

The FreeRTOS reference consumes effective CPU clock, tick rate, heap size,
priority count, timer priority and command-queue length. Preemption and the timer
daemon are required capabilities and disabled inputs fail configuration. The
F407 reference currently shares a 1,000 Hz SysTick with the HAL millisecond clock;
other rates require a separate HAL timebase. The platform owns the unique
SysTick wrapper, while the kernel owns SVC/PendSV. Assertions remain enabled in
release builds to validate IRQ priority and kernel invariants. Weak stack/assert
hooks fail-stop; the malloc hook reports failure and permits NULL to propagate.
Board applications may replace the fault hooks with a reviewed safe-output/reset
policy. Fault callbacks must not block, allocate or recursively call OSAL.

Resource counts and queue copy/storage ceilings come from the effective build
configuration. FreeRTOS self-deleted task stacks/TCBs are reclaimed by the idle
task; management code must let idle execute. Function completion returns user
argument ownership, while actual kernel heap reclamation may be deferred to
idle. No universal worst-case execution time or RAM claim is made without the
selected board's map, timing and high-water measurements.

## Logging lifecycle

Both synchronous and asynchronous logging are serialized task-context services.
They may format strings, allocate management resources, wait for locks and invoke
backends. They are outside deterministic control loops and ISRs. DROP_OLDEST and
DROP_NEWEST bound queue operations; BLOCK uses a finite operation budget.

A short gate pins the logger mutex before a caller can wait on it. Shutdown
rejects new producers, waits for existing users, requests cooperative worker
stop, drains accepted entries, joins the worker and then deletes its queue and
mutex. BUSY/TIMEOUT retains resources for management retry; a fixed delay is
never used as proof of thread completion. Memory backends have independent
ring-buffer locks so diagnostic reads/clears can coexist with worker writes.
Dynamic factory allocation uses OSAL's management allocator.

Async flush enqueues a FIFO barrier carrying an opaque reply-queue token. Its
reply follows previous backend writes and flush callbacks and returns backend
flush errors. Queue empty alone is not completion. After timeout a late reply
targets a rejected stale token, never an expired caller stack pointer. Queue
storage policy can discard entries; submission success alone is not durable
storage or an audit-log delivery guarantee.

Backend callback reentry into writes, unregister, flush or shutdown returns
BUSY. Backend cleanup failure preserves registration/ownership for retry.
Logger cleanup failure retains closing state until retry succeeds. Destroying a
registered or in-use built-in backend returns BUSY. A caller must unregister and
stop independent users before destruction. Borrowed format strings and backend
structures/contexts must remain valid for their documented lifetime.

## API migration

- Replace forced task deletion with `osal_task_request_stop`, a function that
  checks `osal_task_should_stop`, bounded `osal_task_join`, then deletion.
- Check `nx_mutex_destroy`, `nx_mem_init`, `nx_mem_free_to_pool`, built-in log
  backend destroy and other cleanup results. BUSY does not relinquish ownership.
- Use `NX_MEM_POOL_DEFINE` or provide real `buffer_bytes` and `bitmap_words` for a
  manually supplied pool; requested block sizes round up to aligned strides.
- Use WAIT_ALL plus auto-clear for `osal_event_sync`; upper event bits are invalid.
- Use `osal_get_time_ms` for monotonic deadlines with unsigned elapsed subtraction
  and budgets below 2^31 milliseconds.

## Executed regression evidence

The real Native CMake build ran these contracts with assertions enabled:

| Executable | Executed cases | Evidence |
| --- | --- | --- |
| `osal_contract_tests` | 14 groups | Eight-thread critical/atomic/pool contention; once-only device init; 1,000 stale generations; cancelled blocked queue/sem/event users; fixed deadlines; cooperative task/timer ownership; event barriers; overflow/alignment; invalid power-manager instances and unsupported mode preservation; zero live OSAL objects. |
| `baremetal_contract_tests` | 7 groups | Saved interrupt-mask model; absent-clock failure; tick wrap; injected ISR boundary; stale handles; bounded queues/events; unsupported capabilities; zero live objects. |
| `freertos_contract_tests` | 10 groups | Pinned real kernel/POSIX port; waiter pins; positive sub-tick round-up at 100 Hz; daemon settlement; size-aware realloc; event barrier; 30 concurrent timer-delete/retry lifetimes with new-slot isolation and actual heap restoration after idle. Natural process exit is required. |
| `posix_event_contract_tests` | 5 groups | Local host-port helper regression includes cancellation/unlock and timed-wait behavior; see `freertos-posix-host-port.md`. |
| `log_contract_tests` | 8 groups | Immediate-stop drain; blocked callbacks/flush; bounded BUSY ownership; callback reentry; concurrent producers/shutdown/ring reads; oversized metadata; backend cleanup failures/retry; late reply after flush timeout; no live objects/allocations. |

The initial integrated Native build passed **1,696/1,696 CTest entries** in 49.93
seconds, including **271 OSAL** and **137 log** entries (136 existing log tests
and one eight-group executable). Existing task tests were migrated to cooperative
cleanup. The final run includes the unified logger-flush deadline patch. Host latency measurements remain benchmark output; deterministic
filter tests verify no backend bytes rather than demanding a timing order from
an externally scheduled host. The build maintainer records the complete execution in `build-config.md`.
The complete Native run wrote `build/linux-gcc-debug/ctest-results.xml` with
zero failures/errors/skips. A subsequently added four-task `freertos_demo`
application smoke also passed separately; its evidence is
`build/evidence/native-freertos-demo-final.xml`. The expanded suite therefore
has 1,697 registrations, while the executed complete run above had 1,696.

The final ASan/UBSan selection passed **23/23 registrations** (18 contracts and
five application smokes) in 11.56 seconds; evidence is
`build/linux-gcc-sanitizers/ctest-results.xml`. It includes the Native/HAL,
logging and independent POSIX-event contracts. The real FreeRTOS kernel runtime
was validated separately and is not claimed as sanitizer-instrumented. Leak
checking was disabled (`ASAN_OPTIONS=detect_leaks=0`) because the sandbox's
ptrace restrictions prevent reliable LeakSanitizer execution. Resource-count
and actual FreeRTOS heap-restoration assertions are additional behavioral
evidence, not a claim that LeakSanitizer passed.

Official STM32/CMSIS/pinned FreeRTOS host-GCC syntax checks covered the boot
bridge and IRQ configurations for baremetal and FreeRTOS. The environment has
no ARM GCC/newlib toolchain: actual Cortex-M compilation, linking and linker-map
measurement have not been executed.

These host/kernel/model results are not STM32/GD32 HIL evidence. This change does
not claim measured MCU interrupt latency, board stack watermarks, brownout
behavior, safe-output fault recovery, DMA/electrical correctness or hardware
worst-case execution time. Those require the supported board/profile evidence.

## Online CI follow-up

The subsequent online ARM GCC 13.2.1 build exposed a `taskYIELD()` macro shape
that the POSIX port did not expose: a bare single-line if/else produced a dangling
else after the ARM macro expansion. `osal_task_delay` now brackets both branches;
the real kernel contract also executes its zero-delay yield branch.

Cppcheck 2.13 reported two nullable `NX_CONTAINER_OF` paths in the old common
power manager. Assertions were not a runtime NULL guard. The manager now uses
static interface initialization and validates the singleton instance before
access. Invalid queries return `NX_POWER_UNKNOWN`; unsupported hardware power
transitions fail without changing the reported RUN mode. This also removes the
unsynchronized first-use interface writes and the status-only simulated sleep.

The existing Native/HAL contract executable now executes 14 groups. The updated
Native/HAL and real FreeRTOS kernel executables both passed (2/2 CTest entries,
0.49 seconds), and the updated 14-group Native/HAL executable passed ASan/UBSan
with leak checking disabled (1/1, 0.40 seconds). Evidence:
`/tmp/nexus-online-core-fix-tests.log` and
`/tmp/nexus-online-core-fix-asan-tests.log`. These are targeted follow-up runs;
they do not restate the earlier complete repository run as execution of the
follow-up source. Online ARM/static-analysis revalidation is recorded separately
when the next CI run actually completes.

### Correctness review of the online clang-tidy report

The report from revision `3cbfe18` exposed real boundary defects alongside
diagnostics about deliberate wire-format and runtime-panic operations:

- History navigation now represents all 255 entries accepted by its public
  capacity type. Zero-byte history entries reject insertion before subtracting
  a terminator or copying data. Both cases have executable regressions in the
  existing shell test target.
- Native heap statistics describe a simulated diagnostic budget. Host malloc
  may exceed it; remaining/minimum budget saturates at zero, while integrity
  checks validate allocation-list/count/byte consistency rather than treating
  a budget overrun as corruption. The existing 14-group contract now exercises
  an allocation larger than the model budget and its cleanup.
- Console flush returns backend failure when stdout fails. JSON export checks
  formatter failures/truncation, and binary hex emission uses bounded two-byte
  pairs. Optional diagnostic task/timer names use explicitly bounded copies.
- OpenSSL provider output lengths are checked before final-output pointer
  arithmetic; separate size conversions and a subtraction check avoid signed
  addition overflow. Storage/update magic is explicitly a four-byte binary
  array, preserving the wire format without adding string terminators.
- Version digit casts operate only on decimal digits 0..9. Heap constants and
  namespace arithmetic use the receiving size type before multiplication.
  String/memory comparisons state their equality test explicitly.
- `configASSERT(0)` remains a deliberate runtime panic. Its single local
  `cert-dcl03-c` exclusion explains why a compile-time assertion is unsuitable;
  it does not suppress runtime correctness checks for other code.

The new logging contract was independently compiled into `/tmp` against the
updated libraries and executed all nine groups with exit zero. Its Linux
subprocess replaces stdout with `/dev/full`, buffers a successful write, and
requires the eventual flush to report backend failure; it releases its backend
and verifies OSAL resource counts before the fixture exits. Evidence:
`/tmp/nexus-tidy-log-contracts.log`. Final shared Debug/Release/sanitizer and
online CI results for this review batch are recorded by the integration owner;
this targeted result is not a substitute for those runs.
