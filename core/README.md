# Core contracts

C11 caller-owned request admission, stable control identities, completion state
and monotonic absolute deadlines. No devices, OS scheduler, allocation or global
registry. Public interfaces are in `include/nexus/core`.

REJECTED retains no references. An accepted chain borrows request and buffers
until acquire-observed SETTLED. SETTLED is its last request access. Cancel/timeouts
never revoke a borrow; failed drain keeps QUARANTINED storage alive. Producers
wait on latched state; a single executor services the operation. Queue adapters
own their outer request and use explicit inner provider metadata where needed.

`tests/contracts/core_contract_test.c` executes publication, admission, wrapping
time arithmetic and terminal-state invariants. Host results do not prove MCU
interrupt ordering or worst-case execution time.
