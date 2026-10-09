API Contract Design
===================

Public APIs use C11 Nexus types and explicit target dependencies. Keep vendor SDK
includes/macros, mutable controller state and product policy private. The user
authorizes breaking refactors when public callers/providers/tests/docs are
updated together; do not preserve a wrong design with successful placeholders.

State Observable Behavior
-------------------------

For each operation state task/ISR/mask context, ownership and storage lifetime,
no-wait/blocking bound, original timeout budget, capability, cancellation,
callback/reentrancy and success/failure state. Status must distinguish unsupported,
wrong/stale owner, invalid context, busy, exhaustion, timeout and backend failure.
Initialization/cleanup errors must preserve retryable resources instead of
forgetting an owner or returning borrowed memory.

Opaque consumer descriptor and owner/generation value references prevent public
implementation mutation. Provider constructor/registration binds identity/state
without hardware initialization; lifecycle open performs real initialization.
Metadata locks stay short and never invoke vendor/user callbacks or wait.
Typed provider dispatch requires task/startup with incoming masks unset.

Composition and Boundaries
--------------------------

Controller, child, region and operation are distinct resources. Finite slots may
reuse storage while lifetime identities cannot redirect stale handles. Cancel
acceptance is not terminal settlement; recovery must prove hardware stop and
UNINITIALIZED before returning leases. Completion queues reserve finite callback
capacity, retain FULL retries and use an explicit caller pump, not a hidden worker.

Arch/SoC/Board/platform own separate responsibilities. Component core uses narrow
ports; adapter binds explicit device/backend. Runtime owns HAL/OSAL infrastructure
only. External products own main/workers/partitions/trust/install/health/manufacturing.

Verify Meaningful Contracts
---------------------------

Use actual production paths and fault cases: busy/error retry, stale generation,
pool exhaustion, deadline/cancel races, zero ticket and final buffer return.
Validate independent minimal consumers, exact config failures and real firmware
startup/IRQ/registry retention. Host model, ARM link and physical HIL are distinct
records. Old count/path or declaration does not qualify a new API/source.

The authoritative APIs are public headers plus source-bound evidence; avoid
copying invented legacy factory/free functions into documentation. Current
examples are fixed external consumers. Architecture/contract/security/persistence
changes record ADR and exact remaining qualification.
