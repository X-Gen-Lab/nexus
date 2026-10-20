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
decisions. Unknown PCB routes remain unknown until reviewed. Preparation,
admission and validation do not operate probes, power boards, flash firmware or
open serial devices. Explicit acquisition execution is a separate station action.

The three maintained first-release PCB fixtures still need observed PCB revisions
and equipment. The G474 HSE24, NUCLEO-H743ZI2 and NUCLEO-U575ZI-Q fixtures supply
only software-reference part and console facts. Tooling pins their exact
Board/part pairs and requires `software_reference`; deleting scope, substituting
a part or relabelling a Board cannot promote them. Station admission rejects all
three references, including direct validation of an edited fixture. The public
raw validator rebuilds admission from its actual artifacts before qualification;
rehashing edited metadata cannot bypass reference scope or station identity.
A future station needs a separately reviewed physical fixture and a corresponding
tooling change. All budgets remain null and all physical cases `not_executed`.

The H743 reference is HSI64 with caches off, PB0 GPIO and USART3 PD8/PD9 IRQ bytes.
The U575 reference is TZEN0/SWAP_BANK0, MSIS4MHz, PC7 GPIO and USART1 PA9/PA10 IRQ
bytes. Review actual PCB revision, supply/regulator fitting and solder bridges;
source defaults do not observe these facts. U575 preboot option state must be
observed without modifying option bytes. DMA, cache, TrustZone, dynamic clocks,
deep STOP and running-state handoff are outside these reference fixtures.

Five additional exact-mode contracts prepare H743 SPI4 cache DMA, H743
continuous RX with cache off/on and IRQ TX, independently selected H743 finite
DMA TX plus continuous DMA RX, U575 finite USART1 hardware TX chains, and U575
LPTIM2 RUN one-shots. Duplex uses its own nine-scenario protocol and both lane
identities; it inherits no earlier RX-only result. Its RX sampling window does
not claim continuous reception for the entire maximum-length TX. Model-only
cache-clean cancellation stays outside physical HIL obligations. The independent
raw parsers are integrated into
the existing SoC acquisition/validation path. Additional software-reference
Board/part pairs remain rejected by physical station admission. SPI-only and
pinless timer plans explicitly leave their control channel unbound. See
[selected-mode HIL contracts](../../docs/design/selected-mode-hil.md) for exact
stimuli, raw fields, boundary obligations and scope. All new plans remain
`prepared_unbound` / `not_executed`; parser tests do not qualify hardware.



The opt-in UART FIFO workload uses public example code and a separate internal
[reference/instrumented fixture](../../tests/contracts/uart_fifo_acquisition/README.md).
`uart_fifo.py` parses post-join device exports, complete raw wire bytes and
four-column IRQ CSV through the normal platform tool entry point. It does not
operate equipment or admit a station. The CSV yields unverified arithmetic
candidates only: both origin labels retain
`irq_capture.measurement_status=not_executed`, with no `samples.irq_latency`.
Caller rate/window parameters are retained, not observed calibration. Real IRQ
assertion/dispatch association, bidirectional overlap, complete capture/loss and
clock facts still require the existing full SOC-55 contract. Software-reference
Boards remain unadmittable; physical HIL stays `not_executed`.


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

A measurement run is a strict schema-2 `physical_hil_measurements` object with
`started_utc`, `finished_utc` (UTC offsets required), `station_id`, `board`, `part`,
`chip_uid`, `elf_sha256`, `resolved_sha256`, `admission_sha256`, `context` and
`traces`. The admission digest is the canonical JSON digest supplied by
`tools.evidence.identity.canonical_digest`. `context` and each trace use the same
file identity object: `{path, sha256, size}`. Every identity names retained,
nonempty regular bytes; changing those bytes invalidates the record.

The context artifact is a strict schema-2 `hil_acquisition_context` object:

| Field | Required observation |
|---|---|
| `admission_sha256` | Canonical digest of the unchanged admission |
| `workload` | File identity of the actual workload and its parameters |
| `cpu_clock_hz` | Positive declared CPU rate; cycle measurements check it against the admitted frequency envelope |
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
| `cycles` | `cycles` | `dwt_snapshot` | `counter_bits: 32`, nonzero `cycle_domain`/`cycle_epoch`, and the independent `reference` contract below |
| `irq_latency` | `us` | `logic_analyzer`, `hardware_timer` | Actual IRQ priorities, clock/load and capture resolution |
| `service_interval` | `us` | `monotonic_timestamp` | Keep the required owner/service executor running; retain scheduling conditions |
| `stack_used_peak` | `bytes` | `freertos_high_water`, `stack_watermark` | Positive integer `stack_capacity_bytes`; convert RTOS words to bytes before recording |
| `throughput` | `bytes_per_s` | `wire_byte_counter` | Count actual wire bytes over the recorded window; products define payload/overhead treatment |
| `loss` | `count` | `receiver_event_counter` | Positive integer `expected_events`; retain event sequence and missing-event accounting |
| `duration` | `s` | `monotonic_timestamp` | Retain the actual elapsed workload interval and monotonic-clock calibration |

Each trace is a strict schema-2 `physical_hil_trace` object with `metric`, `unit`,
`method`, `admission_sha256`, `context_sha256` and `samples`. Non-cycle samples
are objects `{sequence, value}`: sequence starts at zero and is contiguous;
values are finite, nonnegative and bounded by unsigned 64-bit range. Stack use
and lost-event counts require integers and stay within their context bound.
A trace contains 1..100000 samples; a run contains 1..7 unique maintained metrics,
exactly matching the admission's budgets. The validator derives minimum, maximum,
mean and count from the retained samples; it never trusts supplied statistics.
Every sample must satisfy the corresponding lower and upper budget.

Cycle samples contain `{sequence, begin, end}` and reject a supplied `value`.
Each endpoint contains exactly `counter`, `cycle_domain`, `cycle_epoch`,
`cpu_clock_hz`, `demcr`, `dwt_control` and `reference`. Counter/register values
are unsigned 32-bit integers. Both endpoints must show TRCENA and CYCCNTENA,
no NOCYCCNT, and the context's unchanged counter domain, epoch and declared rate.
The declared rate is not an exact measurement of an oscillator's frequency.
The actual counter owner must advance its epoch on reset, withdrawal or clock
change. The raw cycle probe does not provide that owner or manufacture an epoch.

The cycle context's `reference` contains `domain`, `epoch`,
`ticks_per_second`, `method: "logic_analyzer"` and `instrument` (file identity).
This instrument artifact must be a retained schema-1
`physical_hil_reference_clock` record with the following fields:

| Reference-clock field | Required fact |
|---|---|
| `admission_sha256` | The current admission digest |
| `role` / `clock_source` | `reference_capture` / `external_instrument_oscillator` |
| `instrument_id` | The actual external capture equipment identity |
| `domain`, `epoch`, `ticks_per_second` | The context's nonzero reference identity and rate; counter and reference domains differ |
| `cycle_domain`, `cycle_epoch` | The CPU counter owner and epoch covered by the frequency calibration |
| `cpu_clock_lower_hz`, `cpu_clock_upper_hz` | Positive inclusive CPU-frequency bounds for that entire counter epoch and capture interval; lower <= declared CPU rate <= upper, each at most `2^32-1` |
| `first_tick`, `last_tick` | Nonempty inclusive capture window in extended unsigned 64-bit ticks |
| `uncertainty_ticks` | Calibrated minimum required endpoint uncertainty bound, in reference ticks |

Every endpoint's `reference` contains `ticks`, `domain`, `epoch`,
`ticks_per_second`, `uncertainty_ticks` and `uncertainty_known: true`.
Its identity/rate must match the admitted reference. Its complete error interval
must lie inside the capture window, and its error cannot understate the retained
clock calibration. The error must include capture pairing, sampling skew,
quantization, reference-clock accuracy and calibration over the observed
interval. The separate CPU-frequency envelope must include CPU oscillator
tolerance, calibration error and drift for the recorded conditions. Missing,
reversed or foreign-epoch frequency bounds are inadmissible. A nominal HSI/MSIS
or PLL setting cannot supply a measured exact CPU-frequency bound.
An MCU timestamp without live status/epoch, an arbitrary domain number or a
host arrival timestamp cannot substitute for these acquisition facts.

For a nondecreasing reference delta `d`, summed error `u`, admitted CPU rates
`c_min`/`c_max` and reference rate `r`, the validator computes cycle bounds:
`lower = max(0, floor(max(0, d-u)*c_min/r)-1)` and
`upper = ceil((d+u)*c_max/r)+1`. The extra cycle covers counter sampling phase.
All calculations use integers and reject overflowing reference bounds.
`upper` must be strictly below `2^32`. Only then can the endpoint residue
`(end.counter-begin.counter) mod 2^32` be admitted; it must also lie in that
reference interval. Numerical wrap is allowed; an ambiguous full period,
epoch/rate change, unavailable counter or unknown error is rejected.
The retired `maximum_interval_cycles` declaration is not accepted as evidence.

Schema-1 measurement runs, contexts and traces remain historical inputs for
their original source/helpers; the current parser rejects them. The measurement
plan advertises `measurement_schema_version: 2` while remaining unbound.
This version change does not change the separate SoC, OS or selected-mode raw
protocols. A structured identity file establishes metadata consistency, not the
physical truth of its clock owner, continuity or independence. A real station
must capture and review those facts before any physical measurement claim.

Validation rebuilds admission from unchanged station, fixture, ELF, resolved
configuration and resource evidence. Records older than 24 hours, future records,
missing/duplicate metrics, mismatched identities and acquisition methods fail.
The CLI refuses to overwrite input records, retained traces, context, workload,
instrument or admitted tool/configuration artifacts. A successful result is
`qualified_for_recorded_measurements` only: observed samples do not prove WCET,
unobserved loading, electrical safety or product release. Neither preparation nor
admission operates equipment. A real station must execute its reviewed adapters
and retain actual instrument bytes; software fixtures never establish physical
measurements.

Host-only measurement regressions run through the commit TDD gate. They mock the
external admission service and retain real synthetic file hashes and parser
inputs; the admission/resource suite separately links a real ARM ELF. Synthetic
records test tooling and never establish qualification for the three boards.

## Reviewed acquisition adapters

`workloads.py plan` supplies thirty explicit acquisition protocols for station
binding; clock/reset/GPIO/watchdog; UART/SPI/DMA; I2C; ADC/PWM/EXTI; Flash power
loss; concurrent load; M4F context; time/deadlines; all task/MSP peaks; lifecycle
endurance; tickless timer-port wake; and fail-stop/external safe outputs. Separate
protocols cover RX DMA/IDLE/continuous loans, UART flow and direction adapters,
continuous ADC, classic CAN, raw RMII Ethernet, USB FS device, timer capture and
advanced PWM break. The mode checks use the maintained configuration mode traits;
selecting a UART alone does not install RTS/CTS, DE wiring or a transceiver.
`templates/workload.json` is an incomplete authoring template, not a reviewed
station. Budgets, instrument identities, wiring and safety steps must come from
the actual station owner. Missing selected controllers/modes stay
`requires_selection`; unsupported CPU/backend cases stay `unsupported`.

The four typed I/O protocols select exact resolved faces: `uart_word_wire`
requires an IRQ 9N1 word port and a native nine-bit peer that preserves bit 8;
`spi_dma_whole_cs_segments` requires a same-controller enhanced DMA endpoint;
`i2c_v2_long_timing` requires a selected long endpoint and the authored
G474/H743/U575 analog-filter-on, digital-filter-zero timing projection;
`pwm_group_comp_break` requires the generated exclusive TIM1/COMP1 route.
Generic SPI, I2C or advanced PWM selection cannot substitute for these faces.
Each protocol retains waveform, drain and instrument obligations with null
limits until reviewed. They remain `not_executed`. A word-only assembly cannot
use the eight-bit smoke adapter: supply a separately selected and reviewed 8N1
control console before full workload or station admission. The offline mode
selector alone establishes neither that console nor physical readiness.

```sh
python tools/hil/workloads.py plan \
    --fixture tools/hil/fixtures/gd32f470_liangshan.json \
    --resolved build/gd32-liangshan-baremetal/generated/resolved.json \
    --report station/workload-plan.json
```

Supplying `--workload station/reviewed-workload.json` checks strict bindings,
stimulation, raw observations, instrument bytes, safety actions and numeric
limits. Endurance duration and power-cut cases need reviewed positive work
lower bounds. RX bytes, sample/packet/frame/edge counts and flow/break cases also
need positive integer lower bounds. The template keeps all such limits null.
A valid protocol remains `prepared_unbound` / `not_executed`:
describing measurements does not execute them or qualify equipment.

Acquisition recipe validation, measurement validation and OS acceptance all
revalidate the content-pinned workload against its admitted fixture and canonical
resolved configuration. A matching file hash does not admit missing stimulation,
wrong controller bindings, absent instrument bytes or unreviewed limits. This
check runs before acquisition equipment operations. It validates authored
obligations, not the truth of an instrument description or physical observation.

ThreadX has its own seven task, IRQ-wake, context, stack, deadline, lifecycle and
fail-stop observation contracts. Its maintained M4 preparation covers the three
first-release Boards; Native remains host software only. These independent
protocols do not inherit FreeRTOS low-power acceptance contracts. See the
[ThreadX acquisition contract](threadx.md) for exact backend, task, IRQ and raw
record requirements. Plans remain `prepared_unbound` / `not_executed`; no real
ThreadX station result is supplied. Likewise, the new peripheral protocols
describe station-owned stimuli and retained raw bytes;
they do not add automatic CAN peers, USB hosts, Ethernet links or waveform
instrument drivers. The existing validator qualifies only its recorded smoke,
metric and OS scope, not every declared protocol.

Fail-stop protocols require the observer record, incoming masks, retained loans
and terminal progress. Products bind their own safe-output port and observed
electrical behavior. Neither an observer callback nor a terminal kernel ceiling
establishes DMA quiescence, successful output shutdown or a hardware safety claim.

`acquire.py plan` rebuilds an existing admission and verifies a station-owned
recipe without opening equipment. It is `prepared_bound` / `not_executed`;
`prepare.py`, `measure.py plan` and `workloads.py plan` remain unbound until the
station, wiring, artifact and reviewed limits are supplied.

```sh
python tools/hil/acquire.py plan --recipe station/acquisition.json \
    --admission station/admission.json --output station/acquisition-plan.json
```

The strict schema-1 `hil_acquisition_recipe` supplies `admission_sha256`, a
`workload` file identity, an absolute station-owned `station_lock`, and exactly
six operations: `identify`, `flash`, `verify_flash`, `reset`, `serial`, `cleanup`.
Each operation supplies a content-pinned executable `tool`, nonempty pinned
`inputs`, an `arguments` array, integer `timeout_s` (1..3600) and
`maximum_output_bytes` (1..16777216). Probe operations must bind the admitted
tool. Review the actual adapter scripts and complete tool/configuration closure;
input hashes do not sandbox arbitrary station scripts.

Arguments use no shell. Whole arguments `{record}`, `{session}`, `{admission}`,
`{elf}` and `{output}` are replaced with the fresh observation path, random public
session token, unchanged admission, admitted ELF and attempt directory. Every
adapter must receive the first three. Adapters read real probe or instrument
facts and write a fresh `hil_adapter_observation` object with `schema_version`,
`kind`, `operation`, `session` and nonempty `observations`. The identify adapter
must report actual `part`, `chip_uid` and `probe_serial`; mismatches stop before
flashing. No chip-ID register address or GD32 target is guessed by the runner.

Only a connected, reviewed station may use the explicit operating command:

```sh
python tools/hil/acquire.py execute --recipe station/acquisition.json \
    --admission station/admission.json --output station/fresh-attempt
```

The runner creates an exclusive station lock and a new attempt directory. It
retains exact stdout/stderr bytes, actual argv/exit, timeout/output bounds,
observation identities and unchanged admission/recipe inputs. Nonzero exit,
missing or stale observation, identity change and output exhaustion stop later
operations; cleanup is still attempted. Cleanup failure keeps the station lock
as quarantine. Resolve it by inspecting equipment and the retained record, not
by automatically deleting the lock or starting a retry over old evidence.
Ownership is checked before and after every adapter. A lost/replaced station
lock or an operator interrupt stops all further equipment actions and retains
quarantine. Capture errors and cancellation kill and reap the adapter process
group before returning; raw partial streams remain available.

A complete successful acquisition is `captured` / `executed_unqualified`:
commands completed, but no physical test or measurement is automatically passed.
Feed actual smoke, measurement and OS records to `admit.py`, `measure.py` and
`os_acceptance.py` respectively. Host subprocess fixtures exercise failure and
audit paths; they do not operate the three boards or establish physical support.

The first-party `serial_echo.py` adapter opens only the admitted stable
`/dev/serial/by-id/...` identity. It verifies the character device across open,
uses `TIOCEXCL`, restores original terminal settings, and configures 115200 8N1.
Admission requires that exact byte frame without flow control; omitted legacy
frame metadata retains the defined 8N1 default. A word face, 7E2 or another
explicit frame is rejected before equipment access. Testing 9N1 requires a
reviewed nine-bit peer and waveform capture, with any eight-bit diagnostic
console kept separate from the tested wire. A USB 8N1 echo cannot qualify
nine-bit data or framing.
After a bounded quiet drain it sends a random public 32-byte challenge and
retains exact TX/RX/drain bytes plus host monotonic event timestamps. Partial
writes/reads share one finite capture deadline. Wrong/incomplete echo, excessive
output, device replacement and failed admission retain failure evidence and do
not write a successful observation.

To use it as the recipe's `serial` step, pin the Python executable and
`serial_echo.py` as adapter inputs and pass `--admission {admission}`,
`--session {session}`, `--record {record}`. Tool arguments containing placeholders
must be separate whole arguments. Review `--timeout-s` and `--quiet-s`; defaults
are host I/O bounds, not product performance budgets. The current
`platform_reference` firmware provides byte echo; the UART owner workload is a
different application and must not be selected as an echo fixture.

Host arrival/close timestamps never establish MCU cycles, IRQ latency or UART TC
timing. Waveform, clock, Flash power-loss and OS context collectors need their
actual station instruments and reviewed workload protocol. The real PTY tests
exercise POSIX software mechanics only; no first-party board has been operated.

## SoC-specific acceptance

`soc_acceptance.py` covers SOC-37–55 from the
[SoC execution ledger](../../docs/design/soc-refinement-execution.csv). Its nineteen
ledger cases and five separately selected mode cases specify hardware,
stimulation, raw observations, invariant assertions, metric units, safety actions
and physical exits. Planning does not operate
instruments, fill numeric limits or establish station readiness.

```sh
python tools/hil/soc_acceptance.py plan \
    --fixture tools/hil/fixtures/gd32f470_liangshan.json \
    --resolved build/gd32-liangshan-baremetal/generated/resolved.json \
    --output station/soc-plan.json
python tools/hil/soc_acceptance.py template --case SOC-43 \
    --fixture tools/hil/fixtures/gd32f470_liangshan.json \
    --resolved build/gd32-liangshan-baremetal/generated/resolved.json \
    --output station/soc-uart-template.json
```

A template contains a `protocol` object to copy into station-owned storage. Bind
actual selected controller IDs, required instrument file identities, reviewed
stimulation, observations, isolation/recovery descriptions and every metric's
`minimum`, `maximum` and `unit`. Null instruments/limits deliberately fail
validation. Discrete work counts require integer limits and positive work lower
bounds; invariant failure counts require both bounds zero. Scope follows the
resolved selected modes, not every mode named in a case description. Tickless
acceptance requires an explicit FreeRTOS tickless selection. G474's software
reference still cannot admit a physical station.

After normal `admit.py` admission and reviewed `workloads.py` authoring, a
station-owned serial/instrument operation receives the absolute SoC protocol
path as a whole argument. Pin that protocol and every instrument/calibration
identity in its recipe `inputs`. Other operations keep the existing ordered
`identify`, `flash`, `verify_flash`, `reset`, `serial`, `cleanup` contract.

```sh
python tools/hil/soc_acceptance.py capture \
    --protocol station/soc-uart-protocol.json \
    --recipe station/acquisition.json --admission station/admission.json \
    --output station/fresh-soc-attempt
python tools/hil/soc_acceptance.py validate \
    --admission station/admission.json --raw station/soc-record.json \
    --output station/soc-validation.json
```

Capture reuses the existing exclusive station lock, content-pinned no-shell argv,
timeout/output bounds and cleanup/quarantine. It produces `soc-capture.json`;
`captured` means `executed_unqualified`. Commit checks never call equipment.
A station needs its real waveform, bus-peer and power-interruption adapters;
this tooling does not manufacture their physical observations.

The serial adapter's `hil_adapter_observation.observations.soc` must contain a
strict schema-1 `soc_hil_observation`: `case_id`, `protocol_sha256`, `context`,
`observations` and `traces`. The three latter fields retain file identities of
bytes actually produced during that operation. A context/trace assembled later
without that captured manifest is rejected. Validation also replays observed
part/UID/probe identity and the exact reviewed argv/instrument closure.

The context is schema-1 `soc_hil_context` with `case_id`, `session`,
`protocol_sha256`, `admission_sha256`, observed positive `cpu_clock_hz`, `load`,
`executor`, `mode_scope`, `time_resolution_s` and `methods`. `mode_scope` exactly
maps protocol controller IDs to `{kind, mode}`. Every metric's method records a
reviewed `instrument` role, acquisition `method`, positive `resolution` in that
metric's unit and positive `window_s`. Wall-clock resolution is at most 1 ms;
windows and duration cannot exceed the actual serial acquisition plus that
resolution. IRQ latency additionally needs observed `irq_priorities`; stack peak
needs `stack_capacity_bytes`, and samples cannot exceed it.

Each schema-1 `soc_hil_trace` records `case_id`, `metric`, `unit`, `session`,
`protocol_sha256`, `context_sha256`, canonical `observations_sha256` and 1–100000
contiguous `{sequence, value}` samples. Finite samples must satisfy every reviewed
limit. No reported pass flag or aggregate substitutes for these bytes.

The schema-1 `physical_soc_hil` record has `case_id`, `started_utc`,
`finished_utc`, `station_id`, `board`, `part`, `chip_uid`, `admission_sha256`,
`elf_sha256`, `resolved_sha256`, and file identities `protocol`, `capture`,
`context`, `observations`, `traces`. Observation/trace maps exactly match the case
catalog and captured serial manifest. Times must fit the actual captured interval
and be at most 24 hours old. Failed, skipped, overlapping, unstarted or quarantined
operations, changed raw streams, wrong sessions and stale inputs fail.

SOC-52 also requires `flash_region = {layout, start, size, recovery_policy}`.
`layout` pins a station/product-authored schema-1 `product_flash_test_layout`
with `board`, `part`, `test_region = {start, size}` and nonempty `protected_regions`
entries `{id, start, size}`. The region must match that external layout, exact
physical erase boundaries and density. It must avoid protected ranges and every
actual firmware LOAD; all firmware LOAD bytes must be protected. The product
owns recovery and safe power-cut policy. No default partition is reserved.

Successful validation is `qualified_for_recorded_soc_case`, limited to that case,
selected modes, admitted equipment and recorded conditions. It does not prove
WCET, unobserved loading, electrical safety or product release. All current
physical SoC cases remain `not_executed`; synthetic parser/subprocess tests prove
tooling behavior only.
