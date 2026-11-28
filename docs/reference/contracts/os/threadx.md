---
id: reference.contracts.os.threadx
type: reference
audience: maintainer
source_language: en
---

(reference.contracts.os.threadx)=
# ThreadX configuration and integration

ThreadX adds a real additional kernel backend while retaining the static factory,
immutable device faces and common OS interfaces. It selects the official
Eclipse ThreadX GNU kernel port; it does not rename FreeRTOS or substitute a host
scheduler model. [Backend contracts](../../../../os/threadx/README.md) define caller
storage, bootstrap proof, deadlines, close/join and unsupported mechanisms.
Business code uses common opaque references; assembly selects real static
layouts through `nexus/os/storage.h`. Only explicit ThreadX extensions depend
on `threadx.h`. Current implementation does not itself establish final software
or physical qualification.

One authored assembly selects `backend = "threadx"`. CPU/IRQ facts remain SoC
facts. `tools/configure/threadx.py` validates the kernel-specific policy into an
independent immutable `ThreadXKernelProfileIR`. It has no invented FreeRTOS idle
stack, notification-slot, MPU, TrustZone or tickless capability fields. CMake
owns the source graph and exact dependency; the typed IR supplies facts and
policy, not another source graph.

## Maintained selection scope

The maintained software assemblies use these Board identities.

| Board identity | SoC family | Port |
| --- | --- | --- |
| `native-reference` | Explicit host-test scope | Linux GNU |
| `stm32f407zg-qiming-v31` | STM32F407 | Cortex-M4 GNU |
| `stm32f407ve-sky-qingchun` | STM32F407 | Cortex-M4 GNU |
| `stm32f103rb-threadx-software` | STM32F103RB HSI8 software reference | Cortex-M3 GNU |

The resolver checks the Board identity and its explicit backend declaration.
CPU-only runtime assemblies can select Native, Cortex-M3 or Cortex-M4/M4F
profiles. Board assemblies supply the SoC, route and startup facts.
The F103RB software reference uses HSI8, SysTick at 1 kHz, BASEPRI and the
Cortex-M3 soft-float ABI. Its physical qualification is pending.

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
policy stay external. Public task priorities increase with their numeric value.
The adapter reverses that order into ThreadX native numbering, where smaller
numbers run first. Pass logical priorities to common task APIs, never raw
ThreadX priority numbers. All controls start zeroed and stacks use byte capacity.
Call `nx_os_run` for the real bootstrap; successful entry never returns, and
Platform startup does not enter the scheduler.

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


## FreeRTOS timebase lowpower selector

FreeRTOS keeps its existing profiles. Its additive `lowpower_port` is `custom`
by default, retaining the explicit product port/weak-null behavior. Selecting
`timebase` requires tickless SysTick, privileged single-world operation and the
reviewed STM32F407 timer implementation. It does not authorize deep sleep.
CPU-only runtime/emission rejects this selection because CPU facts do not supply
that SoC timer/alarm contract. This field never selects a ThreadX sleep mechanism.
Actual timer implementation tests, physical wake latency/power and clock behavior
remain separately qualified.

## Repeatable software gate

`python scripts/ci/os_threadx.py --compiler arm-none-eabi-gcc --output <fresh-dir>`
links two M4 Boards with BASEPRI and PRIMASK, and the F103RB reference with
BASEPRI. Each ELF retains the actual task, queue, close, notification, join,
timer and scheduling entries. The official PendSV aliases resolve to the same
strong entry. Every selected kernel source has a compiled object with the
resolved C and ASM policy. The F103RB check also verifies vector ownership and
the alignment of the initial hardware frame restored through PSP. The gate
retains each ELF, BIN, map, budget, configuration, compiler database and log.

Five Board and mask combinations link successfully. The M4 external Tick case must
fail at the ELF link for the absent strong external-Tick provider. Unrelated compiler failures, an existing ELF, weak or
missing retained entries, host/dynamic symbols, stale output and source changes
fail qualification. No command in this gate flashes or executes an MCU.
`configured_backends` in the compatibility policy lists available selection
names only; the actual resolver still rejects unsupported CPU/Board tuples.
