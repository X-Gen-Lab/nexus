# Bounded Settings/KV

`Nexus::Settings` stages one bounded key/value snapshot over an already open
`nx_storage_t`. Select `settings` in the assembly component list; CMake owns the
dependency on Storage. The public [header](include/nexus/components/settings.h)
defines the operation and failure contracts.

The caller supplies metadata, a disjoint image buffer, its capacity, a nonzero
entry limit and a product-defined `uint32_t` schema. Keys are product-assigned
`uint32_t` identifiers; values are copied byte strings, including empty values.
There is no heap, worker, global key registry or retained pointer into input
values. All operations are task-only and serialized with the underlying store.

1. Open the underlying Storage, then open Settings with unused or closed metadata.
   A missing blob stages an empty image without committing it. An unsupported
   schema/format or corrupt load is an error, never an empty writable database.
2. Get copies a value, or reports the required size when querying or undersized.
   Set and remove change only staging. Rejected mutations preserve entries and
   dirty state; assigning an identical value does not add a write obligation.
3. Commit explicitly saves the complete snapshot through Storage. Clean staging
   causes no write. A mutating I/O failure closes Settings; reopen Storage and
   then Settings to determine whether the old or new commit survived. Do not
   retry by treating the failed operation as a safe rollback.
4. Close discards uncommitted staging without I/O and leaves Storage open. End the
   underlying storage workspace lifetime only under its own shutdown contract.

Products own defaults, schema migration, authorization, durability frequency,
Flash reservations and recovery decisions. The format is not authenticated;
Storage CRC and host NOR power-cut models do not establish signed updates or
physical Flash power-cut qualification. Current execution and pending exits stay
in the [delivery ledger](../../docs/status/ledgers/platform-delivery-completion.md).
