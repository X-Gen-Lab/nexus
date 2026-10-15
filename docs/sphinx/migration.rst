Migrating an external consumer
==============================

Previous HAL/OSAL/factory/registry and Kconfig APIs are removed. Use an isolated
migration branch and pin the new platform revision. Historical documents are
archived instead of used as current support evidence.

Replace device name lookup and generic references with generated typed aliases.
Move provider-specific construction to Board/assembly inputs. Consumers include
public ``nexus/io`` contracts and ``nexus_bindings.h`` rather than vendor headers.
Product instances, workers and peripheral policy stay outside the platform.

Replace hidden UART buffering with explicitly sized byte/event RX storage and
caller-owned TX requests. Preserve request and payload until settlement. Choose
a direct serialized owner or an explicit shared owner adapter. Keep absolute
deadlines in one clock domain and continue drain during stop.

Use per-object OS TCB/stack/queue storage and join before reclaiming it. Supply
explicit writable erase-aligned Flash regions and storage workspace; there are
no default product partitions. Product persistence migration requires its own
recovery policy and physical power-loss verification.

Rebuild relocated source SDK consumers, execute current Native/model tests and
inspect actual ARM ELF/resource reports. Re-run physical qualification for the
PCB, wiring and deployment. Old test counts and binary hashes do not qualify
the new architecture.
