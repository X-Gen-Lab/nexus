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

See [the observation and recovery contract](../../docs/design/io-observation-recovery.md)
for the Core observer, stream budget, measured optional storage and external
application escalation policy.
