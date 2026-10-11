# Stack observations and owner lifecycle endurance

Nexus supplies bounded, read-only paint observations. It does not paint a live
stack, create a sampling worker, suspend a task or claim that paint detects every
reserved stack byte. Product stack sizing and physical station sampling remain
external responsibilities.

## Core and MSP provenance

`nx_stack_observe` accepts the original positive byte capacity of a
caller-provenance painted region. It reads at most that capacity, returns the
low-address untouched prefix and the remaining touched bytes, and retains no
pointer. Invalid, wrapped or output-overlapping spans preserve the output.
The original request, stream, owner and task structures gain no fields.

The region must have been painted while it had no live frames. Sampling needs a
quiescent region and excludes the current SP, all live frames and exception
reservation. An MSP observation therefore requires boot paint provenance and a
reviewed quiescent sampling point. This API does not access the MSP register or
repair missing provenance. An all-painted region terminates at its supplied
capacity rather than reading beyond the region.

A touched-pattern observation is not a peak measurement: an unwritten frame
reservation or a write equal to the paint can remain invisible. It is not stack
canary validation or overflow detection. Physical task/MSP sizing still requires
workload, nested exception and station evidence.

## Optional FreeRTOS static-task binding

`nx_freertos_stack_observe` is compiled only when `[os].trace = true`, including
the maintained diagnostic profile. The standard/minimal/lowpower selections do
not compile its translation unit. The caller supplies the original static task
handle, TCB, stack and word capacity. `xTaskGetStaticBuffers` confirms the exact
live TCB/stack identity; it cannot confirm capacity, so capacity remains the
original-creation contract.

Calls require privileged, unmasked running Thread mode. The caller excludes
join, deletion, prestart discard, rebinding, reinitialization and writes to the
sampled stack through the scan. A returned-and-suspended joinable task can be
sampled before its external join; the binding must stop being used before join
reclaims the task. No stale handle is queried after deletion.

The locked FreeRTOS `tasks.c` fills new static task stacks with `0xA5` under the
maintained paint-enabling configuration. The adapter uses that exact paint and
performs a capacity-bounded Core scan. It deliberately does not call the kernel's
unbounded free-space helper. No extra task/TCB state, masks, queues or runtime
registry are introduced.

The POSIX FreeRTOS port stores startup bookkeeping in the supplied static array
and creates a separate pthread execution stack. Its successful static-buffer
query proves API identity and bounded inspection, not an MCU or Linux execution
stack peak.

## Measured software cost

Development evidence uses the existing production Runtime CMake graph with
Cortex-M4F hard-float, GNU Arm 14.3, `-Os`, `-fshort-enums` and section GC. The
standard graph compiles no stack snapshot TU; the diagnostic graph compiles one.
A probe compiled with that exact adapter command's ABI measured a 16-byte
caller binding and a 12-byte output. Each is caller-owned and optional.

Owned object sections measured 242 bytes of adapter `.text` and 64 bytes of Core
scan `.text`, with zero `.data`/`.bss` in both objects. These are object-code
figures before final link alignment and any newly retained kernel identity query;
they are not a whole-image Flash delta. Unreferenced sections can be discarded.
The snapshot costs at most `words * sizeof(StackType_t)` byte comparisons and
copies a fixed-size output. No cycles or interrupt latency are inferred.

## Real kernel lifecycle endurance

Two unfiltered cases run 32 and 256 rounds through the actual pinned FreeRTOS
POSIX kernel, two real producer tasks, one real owner task, the UART owner and
the Native UART provider. Rounds rotate normal completion, cancellation and a
failed-drain quarantine followed by explicit successful drain.

Each round verifies that quarantined requests cannot be prepared/reclaimed,
producer tasks and owner task are joined, the executor/provider have no retained
request/data pointer, the IRQ hint is detached before its sink is destroyed,
provider stop completes, notifications are destroyed and the kernel task count
returns to the pre-round baseline. Fatal cleanup failure aborts the fixture
instead of releasing storage while tasks still borrow it.

The large hosted fixture is explicitly test-owned outside the coordinator's
pthread stack. A bounded scheduler-suspend guard around Native hardware model
steps prevents the signal-driven POSIX scheduler from parking a pthread mutex
holder and running another model user. It is fixture composition and does not
change a production provider or MCU scheduling rule.

The resulting 288 rounds establish this software lifecycle on Linux. Native IRQ
hints and modeled TC/drain are not physical interrupts or wire evidence. Physical
soak, real UART/DMA wake latency and board current measurements remain not
executed until the three-board station is connected.
