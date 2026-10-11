# Optional OS adapters

Thin wait/wake and per-object storage contracts. Baremetal has no scheduler;
Native uses POSIX threads; FreeRTOS uses explicit static TCB, stack and queue
storage selected by the caller. No universal maximum pool, hidden worker,
default timer daemon or device dependency on OS.

Notifications are hints: arm/recheck the authoritative request predicate.
Kernel ISR operations validate the selected syscall ceiling. A returning task
parks until another task completes deletion/join; reclaim its TCB/stack only
after the join contract says observers are gone. Applications own scheduler
start, workers, queue depth, fairness and shutdown progress.

The real FreeRTOS POSIX scheduler fixture validates host task/queue lifecycle.
The default privileged runtime selects exact pinned kernel sources from
validated CPU facts: CM0 for M0/M0+, CM3 integer context for M3 and M4 without FP, CM4F for
M4 with FP, CM7/r0p1 for M7 with FP, and each CM23/33/55/85 NTZ port with its
separate assembly file. Integer-only M7 uses a reviewed build-derived CM3 port
with PRIMASK-preserving errata 837070 guards; pinned vendor files remain unchanged.
Host execution does not qualify MCU exception priority, FP/MVE context switches,
stack high-water or timing.

Running APIs require privileged, unmasked Thread mode. Before scheduler start,
the pinned Baseline ports may retain PRIMASK=1 and Mainline ports their exact
syscall BASEPRI; only that port's canonical mask is permitted in that phase.
Matching a value cannot identify its owner. Fault masks and other masks reject
before object mutation; a read-only scheduler query distinguishes bootstrap from
running/suspended tasks. Baseline IRQ wakes use the PRIMASK kernel contract;
Mainline IRQ wakes validate implemented bounds, priority width and syscall ceiling.

Armv8-M supports explicit NTZ, Secure-only, and Nonsecure standalone build modes.
The selected NTZ kernels do not allocate Secure task contexts or expose cross-world
Secure function calls. Secure startup owns SAU/IDAU and Nonsecure coprocessor access;
Nonsecure local masking cannot protect against Secure publishers. The default
task APIs remain privileged; explicit MPU profiles provide a separate static
restricted-task lifecycle and two pointer-free user delay/tick SVC services.
Caller-owned protected metadata and reviewed user linker regions are required;
no generic user-object pool or privileged I/O API is exposed. M0/M0+ and M7 MPU,
and MPU combined with split security, are rejected. See the OS contracts and
current matrix before selecting an optional isolation profile.

Explicit split mode instead selects the real TrustZone-aware Nonsecure ports and
a separately linked Secure baremetal Runtime with `Nexus::SecureContext`. Both
images use matching CPU/ABI facts and the actual CMSE import/SG veneers. Every
authorized task, including Idle, has caller-owned NS TCB/stack and prepared
Secure metadata/stack selected by an immutable trusted mapping. No Secure heap
or maximum context pool is supplied. The NS privileged kernel is trusted;
SAU/IDAU, startup, vectors, NSC memory attribution and interrupt routing remain
consumer/SoC responsibilities. See the
[Secure companion contract](freertos/secure/README.md) before integration.
Two-image linking and host lease tests do not qualify physical attribution,
context retention, stack peaks or adversarial NS-kernel isolation.

The first scheduler start requires cold privileged Thread/MSP state with FPCA
clear; applications must not leave active FP context before bootstrap. The MPU
start guard consumes a protected single-use lease against the real origin/frame.
For v8-M, authored `security = "single"` means Arch state 0, not Nonsecure. It
uses the traditional FD/F9 port return tuple without CMSE permission; explicit
Secure uses the same tuple with its separate Secure facts, while explicit
Nonsecure uses BC/B8. The generated `NEXUS_CPU_SECURE_ONLY` macro selects that
port encoding and does not grant Secure access to a single-world image.

M55/M85 use their actual MVE-aware exception/context ports. The kernel's
`configENABLE_FPU` switch activates the shared coprocessor register bank and
extended/lazy exception frame for either FP or MVE; CPU scalar-FP facts remain
independent. Integer-only MVE compiles with `+nofp` and softfp calling convention.
Upper shared registers are saved by the port. Complete lower-bank and VPR
retention depends on hardware extended exception framing and remains part of
physical context-switch acceptance. This build contract does not qualify that
behavior or guarantee cross-world payload preservation.

The host fixture keeps the pinned kernel and POSIX port unchanged, and uses
`tests/contracts/os_freertos_runtime/posix_event.c` as its owned event support.
The port cancels cond-waiting pthreads before signalling/joining them; upstream
event support has no cancellation cleanup and can leave an exited thread's
mutex locked. Owned waits unlock on both return and cancellation, preventing
repeated task-join deadlocks. This host-only correction does not change the MCU
backend or establish MCU scheduler qualification.

## Maintained execution contracts

See [OS contracts](../docs/design/os-contracts.md),
[delivery scope](../docs/delivery/os.md) and the
[OS01-OS55 execution ledger](../docs/design/os-execution.csv).
New-kernel admission uses the
[backend integration contract](../docs/design/os-backend-integration.md) and its
active execution probe; a probe pass does not register another supported RTOS.

Finite notification deadlines are checked even after a coalesced sequence hint;
continuous unrelated wakes cannot extend a wait. The public predicate receives
one final observation after timeout, so an observed real completion can win.
Queue operations keep immediate try semantics and consume an absolute budget only
when blocking. `NX_DEADLINE_NEVER` uses genuine untimed host waiting.

Task creation validates the selected port's startup frame and alignment before
kernel mutation. Startup minimum and measured runtime stack budget remain distinct.
One external lifecycle owner joins each returning task. Native tasks must not be
cancelled or detached behind the adapter. Permanent tasks and direct notifications
are explicit optional paths with their own storage and receiver/index contracts.

Closable queues use separate caller-owned controls and per-operation waiter
storage; raw queues retain their minimal native blocking implementation. Shutdown
must close admission, keep the owner progressing, settle and join producers, join
the owner, quiesce hardware IRQ publishers and only then destroy notification,
queue and guard storage. The external UART owner example demonstrates that flow.

The optional tickless path needs an explicitly bound timer port and a continuous
clock; without it, normal Tick remains active. Diagnostics use caller-sized rings
and weak no-op sinks. Trace, low power and task isolation do not imply physical
qualification. `tools/hil/os_acceptance.py` prepares or verifies retained OS
hardware records without operating equipment; all unexecuted physical cases stay
`not_executed`.
