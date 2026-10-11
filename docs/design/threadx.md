# ThreadX configuration and integration

ThreadX adds a real additional kernel backend while retaining the static factory,
immutable device faces and portable wait contract. It selects the official
Eclipse ThreadX GNU kernel port; it does not rename FreeRTOS or substitute a host
scheduler model. [Backend contracts](../../os/threadx/README.md) define caller
storage, bootstrap proof, deadlines, close/join and unsupported mechanisms.

One authored assembly selects `backend = "threadx"`. CPU/IRQ facts remain SoC
facts. `tools/configure/threadx.py` validates the kernel-specific policy into an
independent immutable `ThreadXKernelProfileIR`. It has no invented FreeRTOS idle
stack, notification-slot, MPU, TrustZone or tickless capability fields. CMake
owns the source graph and exact dependency; the typed IR supplies facts and
policy, not another source graph.

## Maintained selection scope

The first platform bindings accept the actual Board identities:

| Board identity | SoC family | Port |
| --- | --- | --- |
| `native-reference` | Native | Linux GNU |
| `stm32f407zg-qiming-v31` | STM32F407 | Cortex-M4 GNU |
| `stm32f407ve-sky-qingchun` | STM32F407 | Cortex-M4 GNU |
| `gd32f470zg-liangshan` | GD32F470 | Cortex-M4 GNU |

Directory names are not Board identities. Unknown Board/SoC pairings are rejected.
CPU-only runtime assemblies may select explicit Native or Cortex-M4/M4F CPU
facts; they do not establish a SoC, board route or physical qualification.
Cortex-M0/M0+/M3/M7/M23/M33/M55/M85 remain rejected for this ThreadX backend;
their separate Arch/FreeRTOS support is unaffected.

## Authoring

```toml
schema = 2
board = "stm32f407_qiming_v31"
backend = "threadx"
clock = "hse8-pll168"
optimization = "Os"

[memory]
main_stack_bytes = 2048
libc_heap_bytes = 0

[os]
tick_source = "systick"
tick_hz = 1000
interrupt_mask = "basepri"
syscall_priority = 5
max_priorities = 32
```

The `board` locator selects the reviewed package; its actual identity is then
validated. Product tasks, queue payload/depth, stack budgets and startup/stop
policy stay external. ThreadX priorities are backend-specific: smaller numbers
run before larger ones. Do not copy FreeRTOS's reversed ordering into a product.

| OS field | Native Linux | Cortex-M4/M4F |
| --- | --- | --- |
| `tick_source` | `host` only, default | `systick` default or explicit `external` |
| `tick_hz` | 1–1,000,000; divides 1e9 ns exactly | 1–1,000,000; SysTick must divide actual core clock exactly |
| `interrupt_mask` | `none` only | `basepri` default or explicit `primask` |
| `syscall_priority` | 0 only | 1..implemented maximum with BASEPRI; 0 with PRIMASK |
| `max_priorities` | 32 only | 32 only |

Null/omitted OS choices use these explicit defaults; non-object values, unknown
fields, booleans in integer fields and invalid tuples fail. `profile`,
`idle_stack_words`, `notification_slots`, `tickless`, `memory_protection`,
`security_model`, `runtime_stats` and `lowpower_port` are rejected for ThreadX.
The initial port minimum is 200 caller bytes with 8-byte alignment; it is a
fixed admission fact, not an application peak-stack budget or host pthread stack
budget. Timer processing in ISR is fixed; there is no implicit timer worker.

A PRIMASK policy preserves physical CPU BASEPRI capability but selects PRIMASK
kernel behavior and logical ceiling zero. BASEPRI C and ASM consume the same
numeric encoded ceiling. Native's `none` policy does not authorize simulated
hardware IRQ calls into ThreadX. PendSV is claimed for MCU ThreadX; SysTick is
claimed only when selected. ThreadX uses no SVC claim, and an external Tick hook
must bind a real source without silently retaining SysTick. Existing Core
monotonic time remains the deadline domain.

## Linux source and evidence

The build-derived Linux timer overlay is bound to one exact reviewed upstream
source SHA and preserves its original MIT notice. GNU `sem_clockwait` must pass
a real compile/link capability probe. The timer uses CLOCK_MONOTONIC for both
clock observation and waits; the exact one-billion-nanosecond carry is normalized.
Unknown source or output is rejected instead of falling back to realtime waits.
The original vendor checkout is unchanged.

Development evidence separates 25 GoogleTest cases inside the real official
kernel, production mechanism tests with mocked external CPU/IRQ/libc boundaries,
configuration Python tests and actual ARM object size probes. The initial
[raw evidence record](../../tests/evidence/threadx/README.md) does not qualify
final clean source, three-board firmware links, SDK consumption or HIL. Follow-up
PRIMASK/configuration records retain their own raw bytes and commands; original
RED/GREEN records are not rewritten as the implementation evolves. The
[follow-up record](../../tests/evidence/threadx-integration/README.md) contains
50 fresh cases: 30 real-kernel service cases, six real-kernel bootstrap cases and
14 production mechanisms with mocked external boundaries. It also records six
three-Board policy links, one precise missing-Tick rejection and three CPU-only
integer/softfp links. These are development results, not physical execution.

## FreeRTOS timebase lowpower selector

FreeRTOS keeps its existing profiles. Its additive `lowpower_port` is `custom`
by default, retaining the explicit product port/weak-null behavior. Selecting
`timebase` requires tickless SysTick, privileged single-world operation and the
reviewed STM32F407/GD32F470 timer implementation. It does not authorize deep sleep.
CPU-only runtime/emission rejects this selection because CPU facts do not supply
that SoC timer/alarm contract. This field never selects a ThreadX sleep mechanism.
Actual timer implementation tests, physical wake latency/power and clock behavior
remain separately qualified.

## Repeatable software gate

`python scripts/ci/os_threadx.py --compiler arm-none-eabi-gcc --output <fresh-dir>`
executes the production platform graph for all three maintained Boards with
BASEPRI and PRIMASK. Each ELF retains the real task, queue, close, notification,
join, timer and scheduling entry points; the two official PendSV aliases must
resolve to the same strong instruction entry. Every official common/M4 source
must have a real compiled object and the same numeric C/ASM policy. The gate
retains the exact ELF/BIN/map, resource report, authored/resolved configuration,
compiler database, command results and pre/post source identities.

A seventh build must fail specifically at the ELF link for the absent strong
external-Tick provider. Unrelated compiler failures, an existing ELF, weak or
missing retained entries, host/dynamic symbols, stale output and source changes
fail qualification. No command in this gate flashes or executes an MCU.
`configured_backends` in the compatibility policy lists available selection
names only; the actual resolver still rejects unsupported CPU/Board tuples.
