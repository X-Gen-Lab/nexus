---
id: tools-hil-threadx
type: reference
audience: maintainer
source_language: en
---

(tools-hil-threadx)=
# ThreadX acquisition and admission contract

The HIL tools prepare independent ThreadX observation protocols. They do not
operate equipment. An unbound plan remains `prepared_unbound`, and its physical
status remains `not_executed`. Synthetic validator tests establish software
validation behavior; their temporary records are never station results.

## Maintained scope

The configure authority owns the supported CPU and platform bindings. The HIL
contract recomputes CPU, IRQ and kernel decisions through the same authority
and verifies the selected Board's exact part in its owned `board.json`.

| Board | ThreadX port | HIL preparation scope |
| --- | --- | --- |
| `native-reference` | `linux/gnu` | Host software only |
| `stm32f407zg-qiming-v31` | `cortex_m4/gnu` | Exact M4 station preparation |
| `stm32f407ve-sky-qingchun` | `cortex_m4/gnu` | Exact M4 station preparation |
| `gd32f470zg-liangshan` | `cortex_m4/gnu` | Exact M4 station preparation |

Native ThreadX support remains available for host software validation. Host
observations cannot qualify MCU hardware. These contracts do not add another
ThreadX core, SoC, board or tickless port. Existing Arch and FreeRTOS support
retains its own scope.

The context pins a separate `backend_contract`: `backend = "threadx"`, the
canonical kernel-profile digest, the selected kernel port, 32 priorities,
`priority_order = "lower_number_higher"`, timer processing in ISR, tick source,
interrupt mask and syscall priority. A FreeRTOS context cannot be relabelled as
ThreadX, and an authored digest cannot replace normal firmware admission.

## Independent observation cases

`workloads.py` publishes the following `threadx_` protocols only for a selected
ThreadX backend. Every protocol retains its actual stimulus, raw observations,
content-hashed instruments, safety review and numeric limits. Existing
FreeRTOS task or low-power protocols do not qualify these cases.

| Workload protocol | OS trace case | Required observations |
| --- | --- | --- |
| `threadx_task` | `task` | Every static thread's priority, preemption threshold, time slice, iterations and start/finish; expected and observed dispatch sequence |
| `threadx_irq_wake` | `irq_wake` | Actual reviewed IRQ, binary-notification ceiling-put service, IRQ latch and resumed-thread timestamps |
| `threadx_context` | `context` | All r4-r11; all s0-s31 when the selected M4 ABI enables FP |
| `threadx_stack` | `stack` | MSP and every selected thread; capacity and observed peak with remaining headroom |
| `threadx_deadline` | `deadline` | Original Core monotonic deadline and each actual timeout return |
| `threadx_lifecycle` | `lifecycle` | Admitted/settled operations, iterations, joins, retained borrows and stale accesses |
| `threadx_fail_stop` | `fail_stop` | Assert and stack-overflow trigger/output timestamps, fault-observer record, incoming masks, retained loans and terminal progress |

The actual Nexus notification wake uses `tx_semaphore_ceiling_put`. A trace of
another ThreadX primitive does not prove this adapter path. `os_irqs` names
every reviewed IRQ that calls the OS; all must exist in `irq_priorities` and
respect the selected logical BASEPRI ceiling. Other non-OS IRQs may have higher
urgency, but their load and nesting must be recorded.

Each IRQ sample names its actual resumed `task` from the complete selected
thread inventory. The context contract preserves exact JSON types: integer
values and booleans cannot substitute for one another.

The `tasks` context is a complete list of unique names and exact numeric
`priority`, `preemption_threshold` and `time_slice_ticks` selections. At least
two threads are required for this interference contract. Thresholds cannot
exceed their thread's priority. `stack_names` contains exactly those threads
plus `MSP`. Per-thread execution and dispatch observations must cover every
declared thread. These recorded patterns do not establish scheduler WCET.

## Station requirements and retained data

Before physical validation, normal `admit.py` admission must bind the reviewed
station, fixture, exact ELF, resolved configuration and recomputed resources.
The context then binds that admission and the complete independent workload
and instrument identities. It records actual CPU frequency, interference,
executor, register features and logical IRQ priorities.

| Requirement | Station budget | Unit |
| --- | --- | --- |
| `task.minimum_iterations` | `os_task_iterations.minimum` | `count` |
| `irq_wake.maximum_us` | `os_irq_latency.maximum` | `us` |
| `deadline.maximum_lateness_us` | `os_deadline_lateness.maximum` | `us` |
| `lifecycle.minimum_iterations` | `os_lifecycle.minimum` | `count` |
| `fail_stop.maximum_response_us` | `os_fail_response.maximum` | `us` |

Both iteration minima are positive. Requirements must exactly equal reviewed
integer station bounds. There is no inherited `os_low_power_lateness` field.
The physical run contains exactly the seven cases, and every retained trace
pins the context digest. Missing, duplicate, foreign or changed traces fail
validation. Every timing sample must meet its bound; averaging cannot hide an
outlier. Deadline samples describe actual timeout returns.

Each fault scenario additionally contains these fields:

```json
{
  "observer_record": {"reason": "assert", "sequence": 1},
  "incoming_masks": {"primask": 0, "basepri": 80, "ipsr": 0},
  "retained_loans": 1,
  "terminal_progress": 0
}
```

`reason` must match the scenario, and the observer sequence must be positive.
Mask values are bounded raw observations. `retained_loans` records outstanding
ownership and may be nonzero; fail-stop does not authorize reclaiming it.
`terminal_progress` must remain zero. Safe external output does not prove DMA
quiescence, borrowed-buffer settlement or a safe restart. Products own isolated
fault stimuli, output safety and recovery policy.

## Commands and evidence boundaries

Generate a plan from an exact generated ThreadX configuration:

```sh
python tools/hil/os_acceptance.py plan \
    --resolved build/threadx-board/generated/resolved.json \
    --report build/threadx-board/os-hil-plan.json
```

After real equipment has produced retained observations, validate them:

```sh
python tools/hil/os_acceptance.py validate \
    --admission station/admission.json \
    --raw-evidence station/os-run.json \
    --report station/os-validation.json
```

Reports require unused destinations. Workload preparation, acquisition
admission and OS trace validation remain separate boundaries. A passing
physical record qualifies only its recorded OS cases, exact identities and
observed load; it does not establish universal timing, product safety or a
release. No physical ThreadX run is supplied by this software delivery.
