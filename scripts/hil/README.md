# Physical HIL adapter contract

Run on a controlled lab station, with a reviewed manifest and matching firmware:

```sh
python scripts/hil/run_hil.py --manifest /lab/stm32-revision-a.json \
  --leases /lab/leases --report /lab/evidence/hil.json
```

This defaults to validation/dry-run: it executes no adapter and does not acquire
equipment. Append `--execute` only on the reviewed physical station to execute
the manifest. A dry-run reports `hardware_verified: false` and is ineligible for
physical HIL. The new [Board readiness workflow](../../docs/implementation/hil-readiness.md)
also binds effective Board/layout, the ARM static report, ELF and BIN identity.

Missing hardware, empty commands, unknown board/revision/probe, flash readback mismatch,
zero/missing/skipped tests, exceeded budgets, adapter failure and cleanup failure return
nonzero. A crashed lease stays reserved for supervised recovery. Do not delete a lease
until the probe, processes and hardware outputs are settled; elapsed time is not ownership.
Failed teardown quarantines the lease rather than exposing unsettled equipment to a new job.

The JSON manifest has schema version 1 and these required fields:

| Field | Contract |
|---|---|
| `kind`, `lab_id` | `physical_hil` and a registered lab identifier |
| `board` | `id`, `profile`, `revision`, `probe_serial`; optional `chip_part`/`chip_uid`, all observed or physically signed off by the adapter |
| `firmware` | Nonempty regular `path`, expected lowercase `sha256` |
| `config_sha256` | Exact build-local effective configuration digest |
| `adapter_files` | Nonempty list of adapter `{path, sha256, size}` identities |
| `commands` | Nonempty argv arrays for `identify`, `flash`, `verify_flash`, `reset`, `serial`, `cleanup` |
| `operation_timeout_s`, `total_timeout_s` | Positive operation deadline ≤300 s; total ≤1800 s |
| `required_tests` | Nonempty unique test IDs |
| `budgets` | Reviewed `{metric: {maximum, unit}}`; may be empty for smoke HIL, enterprise promotion requires budgets |

The runner leases both board ID and physical `probe_serial`; a second manifest cannot
race the same probe merely by using a different logical board name. The lease root must
be shared by all station jobs that can reach those resources.
The Board readiness wrapper additionally provides `serial_lease_id`, based on
the actual canonical tty, so different logical stations cannot race one UART.

Commands use no shell. Substitutions are `{firmware}`, `{id}`, `{profile}`, `{revision}`
and `{probe_serial}`. Paths can contain spaces because each argument remains one element.
Pin and record the wrapper, OpenOCD/pyOCD, probe firmware and vendor tools in lab policy;
the manifest adapter hashes do not by themselves attest equipment integrity.

Adapter responses on stdout:

- `identify`: `{schema_version: 1, board: {...}}`, based on actual probe/device identity.
- `flash`, `reset`, `cleanup`: successful exit; failure must return nonzero.
- `verify_flash`: `{schema_version: 1, board: {...}, firmware_sha256: "..."}`,
  computed from actual flash readback across the released image extent.
- `serial`: one JSON object with `schema_version: 1`, `board`, `firmware_sha256`,
  `config_sha256`, `tests: [{id, status: "pass", detail?: "..."}]`,
  `metrics: {name: {value, unit}}`. A serial adapter must measure/extract actual
  firmware test results and associate them with the verified readback, never
  echo manifest expectations as observations.

The runner records the validated serial transcript, its digest, operations and measured
budgets. It stops the entire POSIX adapter process group on timeout, rejects orphaned
background processes and invokes teardown even after identification or flash failure.
Station adapters must bound their own output/filesystem use and place outputs in a safe
state; process termination cannot establish electrical safety on its own.

`--model` requires `kind: orchestrator_model` and reports
`eligible_physical_hil: false` and `hardware_verified: false`. Injected adapter
runners require model mode. The unit suite uses that mode with subprocess fixtures.
Its passing results establish orchestration behavior, not physical timing/DMA or support.
The repository contains no fabricated lab manifest, board identity or passing HIL record.
The test-execution deadline reserves an independent teardown attempt of at most 5 seconds;
failed teardown keeps both board and probe quarantined.

HIL runners should execute trusted, reviewed revisions in a restricted hardware network.
Do not flash code from untrusted pull requests or expose signing/provisioning credentials
to those runners. Required product tests, fault injection and budgets belong to reviewed
release policy, so a report cannot weaken its own promotion criteria.
