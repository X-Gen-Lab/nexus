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
Maintained ARM builds select exact pinned kernel sources from validated CPU
facts: CM0 for M0/M0+, CM3 integer context for M3 and M4 without FP, CM4F for
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
Nonsecure local masking cannot protect against Secure publishers. MPU user-task
gateways are not supplied: task creation and APIs remain privileged.

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
