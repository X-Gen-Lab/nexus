# HIL preparation and admission

`prepare.py` turns a generated `resolved.json` and one maintained fixture into
an unbound work plan. It validates the exact part and smoke-console pins and
retains selected controller modes. It records every physical case as
`not_executed`; it cannot establish station readiness or hardware qualification.

```sh
python tools/hil/prepare.py \
    --fixture tools/hil/fixtures/gd32f470_liangshan.json \
    --resolved build/gd32-liangshan-baremetal/generated/resolved.json \
    --report build/gd32-liangshan-baremetal/hil-preparation.json
```

Copy `fixtures/station.template.json` into station-owned storage when equipment
is connected. Observe the part, PCB revision, chip UID, probe serial, stable
serial path, wiring, ground and supply. Content-hash the actual flashing tool
and probe configuration files. Review numeric measurement budgets against the
intended workload; null fixture budgets require that review.

`admit.py` checks the observed station, exact linked ELF, configuration and
recomputed resource budget before equipment operation. It also validates raw
physical records against that admission. Software models, a copied report,
an empty execution, a changed ELF and a stale record cannot qualify hardware.
The four smoke tests qualify only their recorded scope. Pending DMA, IRQ,
stream, ADC, cancellation, timing and power-loss cases require their own
physical measurements before those capabilities can be qualified.

The platform supplies tooling and generic measurement cases. Products own
electrical limits, fixture wiring, erase regions, real workloads and release
decisions. Unknown PCB routes remain unknown until reviewed. These commands do not
operate probes, powers boards, flashes firmware or opens serial devices.


`measure.py plan` generates seven generic acquisition cases bound to an exact
fixture and resolved build. It requires no station or equipment and leaves all
budgets null. A plan always remains `prepared_unbound` / `not_executed`.

```sh
python tools/hil/measure.py plan \
    --fixture tools/hil/fixtures/gd32f470_liangshan.json \
    --resolved build/gd32-liangshan-baremetal/generated/resolved.json \
    --report build/gd32-liangshan-baremetal/hil-measurement-plan.json
```

After equipment is available, the station owner reviews its actual workload and
numeric budgets, then produces a fresh admission with `admit.py --station ...
--fixture ... --elf ... --resolved ... --resources ... --report admission.json`.
Each budget requires `maximum` and `unit`; optional `minimum` defaults to zero.
For throughput or endurance, a maximum alone cannot establish sufficient work.
Use a reviewed lower bound, for example `throughput: {minimum: 1000, maximum:
1000000, unit: bytes_per_s}` or `duration: {minimum: 3600, maximum: 7200, unit: s}`.
These numbers illustrate the schema; products establish their own acceptable
limits. Boolean, negative, nonfinite, reversed and oversized bounds are rejected.

The equipment adapter retains its observed measurements as separate JSON trace
files and a run record, then invokes the validator:

```sh
python tools/hil/measure.py validate \
    --admission station/admission.json \
    --raw-evidence station/measurements.json \
    --report station/measurement-validation.json
```

A measurement run is a strict schema-1 `physical_hil_measurements` object with
`started_utc`, `finished_utc` (UTC offsets required), `station_id`, `board`, `part`,
`chip_uid`, `elf_sha256`, `resolved_sha256`, `admission_sha256`, `context` and
`traces`. The admission digest is the canonical JSON digest supplied by
`tools.evidence.identity.canonical_digest`. `context` and each trace use the same
file identity object: `{path, sha256, size}`. Every identity names retained,
nonempty regular bytes; changing those bytes invalidates the record.

The context artifact is a strict schema-1 `hil_acquisition_context` object:

| Field | Required observation |
|---|---|
| `admission_sha256` | Canonical digest of the unchanged admission |
| `workload` | File identity of the actual workload and its parameters |
| `cpu_clock_hz` | Observed positive CPU clock, including clock changes |
| `load` | Load and interference description; identify any untested conditions |
| `executor` | Owner/service executor, priorities and scheduling conditions |
| `irq_priorities` | Named IRQ logical priorities, integer 0..255; nonempty for IRQ latency |
| `measurements` | Exactly the metrics selected by the reviewed station budgets |

Each `measurements` entry requires `method`, `instrument` (file identity of the
actual acquisition configuration/calibration), positive `resolution` in the
metric's unit, and positive `observation_window_s`. Additional bounds below are
mandatory where applicable. Context is hashed and independently bound to every
trace, so a trace cannot be reused with changed workload/acquisition conditions.

| Metric | Unit | Maintained methods | Additional context |
|---|---|---|---|
| `cycles` | `cycles` | `dwt_snapshot` | `counter_bits: 32`, positive `maximum_interval_cycles < 2^32`; enable/calibrate DWT and exclude ambiguous wrap |
| `irq_latency` | `us` | `logic_analyzer`, `hardware_timer` | Actual IRQ priorities, clock/load and capture resolution |
| `service_interval` | `us` | `monotonic_timestamp` | Keep the required owner/service executor running; retain scheduling conditions |
| `stack_used_peak` | `bytes` | `freertos_high_water`, `stack_watermark` | Positive integer `stack_capacity_bytes`; convert RTOS words to bytes before recording |
| `throughput` | `bytes_per_s` | `wire_byte_counter` | Count actual wire bytes over the recorded window; products define payload/overhead treatment |
| `loss` | `count` | `receiver_event_counter` | Positive integer `expected_events`; retain event sequence and missing-event accounting |
| `duration` | `s` | `monotonic_timestamp` | Retain the actual elapsed workload interval and monotonic-clock calibration |

Each trace is a strict schema-1 `physical_hil_trace` object with `metric`, `unit`,
`method`, `admission_sha256`, `context_sha256` and `samples`. Samples are objects
`{sequence, value}`: sequence starts at zero and is contiguous; values are finite,
nonnegative and bounded by unsigned 64-bit range. Cycles, bytes of stack use and
lost-event counts require integers and must also stay within their context bound.
A trace contains 1..100000 samples; a run contains 1..7 unique maintained metrics,
exactly matching the admission's budgets. The validator derives minimum, maximum,
mean and count from the retained samples; it never trusts supplied statistics.
Every sample must satisfy the corresponding lower and upper budget.

Validation rebuilds admission from unchanged station, fixture, ELF, resolved
configuration and resource evidence. Records older than 24 hours, future records,
missing/duplicate metrics, mismatched identities and acquisition methods fail.
The CLI refuses to overwrite input records, retained traces, context, workload,
instrument or admitted tool/configuration artifacts. A successful result is
`qualified_for_recorded_measurements` only: observed samples do not prove WCET,
unobserved loading, electrical safety or product release. Neither preparation nor
admission operates equipment. The platform does not supply an instrument adapter
or claim physical measurements until a real station executes and retains them.

Host-only measurement regressions run through the commit TDD gate. They mock the
external admission service and retain real synthetic file hashes and parser
inputs; the admission/resource suite separately links a real ARM ELF. Synthetic
records test tooling and never establish qualification for the three boards.
