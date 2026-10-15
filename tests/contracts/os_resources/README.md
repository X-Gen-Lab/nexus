# Production OS resource and instruction reference

`scripts/ci/os_resources.py` builds this consumer through the maintained Runtime
entry and authored CPU/TOML kernel policies. It does not list kernel or platform
sources independently. The matrix compares M0 and M4F, standard and minimal,
with `Os` and `O2` separately; this is a software ABI reference, not a Board.

The ELF contains both compiler-emitted `sizeof` absolute symbols and named,
retained `__bytes` objects. The gate checks agreement for TCB, Wait Port,
semaphore notification, fixed-receiver direct notification, raw and closable
queues, each caller waiter, joinable and permanent tasks. Two 128-word stacks
and two three-item payload arrays have separately measured symbols. Actual
kernel Idle TCB/stack symbols must agree with the authored profile. The report
separates these objects from the remaining fixture/kernel globals and total
ELF RAM; the total image also includes Core/Arch, the consumer and kernel.

Every variant retains production queue, wait, close, join and task creation
paths. Actual instruction counts, call lists, mask/barrier instructions and
baseline save/restore shape are recorded together with ELF/map, compiled object,
source, tools and command identities. Firmware must retain neither allocation
functions nor compiler atomic helpers. Minimal versus standard comparisons use
identical CPU ABI, optimization and fixture; cross-optimization totals are not
combined.

These are static instruction shapes. Branch frequency, exclusive retries,
interrupt nesting, peripheral latency, context-switch cycles and IRQ/task
latency require real HIL and remain unmeasured. The synthetic memory map and
clock retain symbols only. `_start` is never physical startup and this ELF is
not intended for flashing.
