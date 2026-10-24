# Explicit owner observations

`Nexus::OwnerObserver` is an optional caller-owned wrapper around one real
`nx_bus_owner_service` step. It preserves the actual service result and records
BUSY, errors, observed quarantine states and maximum observed service gap. It
never scans the slot population, adds a default owner field, creates a worker or
changes request settlement.

The unique executor supplies a monotonic timestamp and distinct output storage.
Hardware service executes outside the existing owner guard; a short guarded
metadata snapshot follows. Reversed observation time is a separate diagnostic
error and cannot prevent mandatory service or drain. Counts saturate. Quarantine
counts are observations, not distinct operation counts.

Owner, slots, callbacks and sidecar remain alive until all wrapper calls exit and
the observer detaches. Detach drops only the sidecar reference. A timeout,
quarantined operation or observer failure never permits request/buffer reuse.
Copied output contains a ticket and state, without borrowed payload pointers.

The same bounded snapshot reports `accepting`, `queued` and `slot_available`.
These describe admission and retained work independently of the service return
code: BUSY alone never proves a full queue. Consumers use these fields only when
`diagnostic_result` succeeds. They can copy the facts into
`nx_io_diagnostic_facts_t` without introducing an I/O dependency into the owner
component or retaining its live request pointers. The additional fields change
the optional public observation/sidecar layout, so consumers must recompile;
they add no fields to the default owner or provider. Optional diagnostic storage
must be measured for the selected ABI rather than assumed to cost zero RAM.

See [the observation and recovery contract](../../docs/design/io-observation-recovery.md)
for the Core observer, stream budget, measured optional storage and external
application escalation policy.
