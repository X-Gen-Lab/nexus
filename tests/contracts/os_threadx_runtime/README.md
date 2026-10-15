# ThreadX runtime contract fixtures

The host GoogleTest executable calls `nx_threadx_kernel_enter` and runs its cases
inside an actual official ThreadX Linux kernel thread. Bootstrap tasks are
created/discarded from the real adapter-owned callback before that thread starts.
`clock.c` supplies the actual host CLOCK_MONOTONIC domain. Mock entry functions
represent application boundaries; task/queue/wait mechanisms are real services.
The kernel source and GoogleTest/GoogleMock archive remain exactly pinned.

The M4 and external-Tick fixtures compile production `m4.c` against explicit
software register facts and mocked CPU register/mask or product-Tick boundaries.
The IRQ fixture compiles production admission against mocked Arch/SCB/NVIC
boundaries. These establish rejection/ordering, not physical priority behavior.
The Linux timer fixture uses the whole upstream-derived timer source and mocks
only libc clock/wait plus the scheduler/timer interrupt boundary; the separate
kernel executable still runs real kernel scheduling and blocking.

Use the actual selecting CMake target's kernel ABI definitions for every linked
translation unit. Do not link the M4 ASM or fake low-level prototypes into Linux
kernel cases, or the host kernel into firmware. Do not import the upstream demo's
clock, DWT, heap or product callback. [Backend contracts](../../../os/threadx/README.md)
state the exact callback, priority, borrowed storage and unsupported scopes.

Evidence must retain actual discovery, commands, exit codes, raw logs, fresh
case XML, source/kernel/config/tool identities and object/ELF bytes. A failed
build is not a behavior RED. Retained preimplementation snapshots identify the
source used when a fixture had to be corrected before obtaining its real RED.
A dirty development run does not qualify a clean firmware candidate. Actual
M4 board links and physical stations are recorded separately; no hardware is
connected for these fixtures.

`CMakeLists.txt` links the production selected platform, official locked kernel,
real startup/vector objects and retained task/queue/notification/close/join APIs.
The three maintained M4F Boards are built separately for BASEPRI and PRIMASK.
The external-Tick negative link omits `external_tick.c` and must fail specifically
for the strong provider symbol. Opting into that file merely links a provider
which returns UNSUPPORTED; it does not fabricate a functioning periodic source.
These images and ELF budgets are software evidence and are not flashed. The
retention-only main is not a product startup/error-handling example; products
must check each initialization result and implement their external safe failure
and cleanup policy.
