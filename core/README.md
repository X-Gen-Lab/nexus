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

## Admission, shutdown and reuse

Initialize fresh, unpublished storage once. `nx_request_prepare` resets only a
READY request or a consumed SETTLED incarnation. It cannot discover outstanding
adapter loans, control commands or observers; the owning adapter must join them.
One serialized executor admits and advances the request. The request state is a
release/acquire predicate, independent of whether a notification was delivered.

The complete shutdown order is:

1. Close admission and stop new controls or observation bindings.
2. Let the unique executor cancel and drain hardware, IRQ, DMA and deferred
   references. A failed drain retains the request and payload in QUARANTINED;
   neither a timeout nor detaching an observer authorizes reclamation.
3. Before release-publishing SETTLED, detach every provider reference and capture
   any independently lived wake target. Settlement is the provider's final
   request/payload access. A pending notification must not retain the request.
4. Acquire-observe SETTLED and consume its result. Separately acknowledge queued
   controls and join all task, diagnostic and completion observers, including
   already executing calls. Detach their sidecars under the adapter's guard.
5. Release the stable slot. Only then prepare/rebind the request, reclaim its
   storage, or stop dependencies once every other consumer has joined.

The adapter synchronizes bind, lookup and release. Epochs identify incarnations
inside one stable slot's lifetime; they are not globally unique wire IDs.
`UINT64_MAX` is the last usable epoch. Its next bind rejects without changing the
slot or output. Retire that exhausted slot; never zero it while any old command
can address its storage. Replacing storage or restarting the owner requires
joining all controls/observers and withdrawing every old reference first.

The maintained UART/SPI example in
[`io_owner_diagnostic_pipeline_test.cpp`](../tests/google/io_owner_diagnostic_pipeline_test.cpp)
uses real Native providers and owner adapters: failed drain retains the hardware,
recovery settles, observers detach, and only then does provider stop succeed.
`core_lifecycle_test.cpp` additionally checks the full legal-transition matrix,
prepare rejection and the last usable epoch. The publication regression in the
C contract target observes results across real host threads without a wake hint.
None of these tests authorizes concurrent admissions or unguarded slot access.

## Clock domains and bounded waits

A `nx_clock32_t` has one execution owner, a stable nonzero frequency and an
uninterrupted counter. Its maximum observation gap is strictly less than
`2^32 / frequency_hz` seconds: at 1 MHz this is 4294.967296 seconds. Equal
snapshots after a full wrap are indistinguishable from no progress. Account for
idle, masked work, Flash maintenance, sleep and worst-case owner latency; this
helper cannot detect a missed wrap. A successful observation commits the tick
extension and output together. Invalid arguments, alias or exhaustion change
neither. Output storage must be distinct from the complete extension state.

The selected physical clock has its own reviewed overflow/service contract;
the generic 32-bit helper does not establish that provider's readiness. Stop,
restart, clock replacement or frequency change creates a new domain. Join all
deadline consumers before withdrawing the old clock; even an equal numerical
timestamp cannot make old deadlines valid. This is a lifecycle rule, without a
mandatory generation field in each request. A terminal clock failure retains
loans and follows the selected provider's fail-stop contract.

For a required finite wait, use `nx_deadline_finite_after(now, duration, &end)`
before admission and handle failure. It rejects the nonfinite `now` sentinel and
any sum reaching the sentinel without rewriting `end`; zero duration is valid.
`nx_deadline_after` keeps its explicit saturation-to-infinite-wait semantics.
Neither helper reads a clock or proves readiness; a zero clock value is not a
startup acknowledgement.

## Optional observation and minimal fault capture

Request observation is a copied diagnostic view, not another execution owner.
Its gaps and transition counts describe sampled observations, not IRQ latency,
WCET or every intervening state. The stable slot outlives the observer; the caller
serializes sampling against request reuse and slot release. The helper rejects
output overlap with known control objects. Only the caller knows full request,
payload and external context spans and must keep them disjoint and alive.

The weak `nx_fault_observe` remains stateless. A product can link one bounded C11
override writing an explicitly owned, debugger-visible record. The following
minimal shape deliberately stores no clock, task name or dynamic pointer:

```c
#include "nexus/core/fault.h"

static volatile struct {
    nx_fault_domain_t domain;
    nx_result_t reason;
    unsigned line;
} terminal_snapshot;

void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                      const char* file, unsigned line) {
    (void)file;
    terminal_snapshot.domain = domain;
    terminal_snapshot.reason = reason;
    terminal_snapshot.line = line;
}
```

This is best-effort forensic storage inspected after terminal halt, with no
coherent concurrent-reader or nested-fault guarantee. `volatile` is not a lock or
a release/acquire protocol. The owning product selects valid writable memory and
retention requirements. The hook cannot repair a corrupt stack, claim safe
outputs, release loans or rely on a clock, kernel or transport. It never changes
incoming masks. If it returns, the reporting terminal path still parks. The
existing FreeRTOS fail-stop tests exercise this actual hook boundary, retained
requests and preservation of PRIMASK/BASEPRI/FAULTMASK. No snapshot, queue, worker
or record is added to the default Core image.

## Stack and explicitly placed memory

For `nx_stack_observe`, the caller first acquires an external quiescent lease:
the stack owner is stopped/joined and cannot execute or receive an exception on
the sampled stack while the read is in progress. Supply the original painted
low-address region, excluding active frames, current SP and independently sized
exception/context reservation. Boot startup establishes MSP paint provenance;
the helper does not infer it. Never paint a live stack or mask interrupts around
an unbounded scan. Cost is bounded by the declared byte count; reservations or
writes equal to the paint hide use, so the result is not a proven stack peak.
`tests/google/stack_observation_test.cpp` checks bounds, read-only behavior and
alias rejection; OS stack-snapshot helpers own the separate task-context rules.

Memory-placement macros apply to one static variable definition per declaration,
only when the authored assembly selects that region:

| Suffix | Reset initialization | Caller obligation |
|---|---|---|
| DATA | Copy initializer from Flash | Budget both load and run regions |
| BSS | Zero storage | Use no nonzero initializer |
| NOINIT | Neither copy nor clear | Initialize or validate before every read |

Select the matching ADDSRAM/TCM/CCM/SRAM1/SRAM4 macro; there is no implicit fallback
region. Startup/linker tests and the linked ELF establish copy/zero spans,
alignment, garbage collection and budgets. NOINIT alone promises no reset or
power-loss retention. Treat TCM/CCM placement as CPU-only unless the exact SoC
contract authorizes the selected bus master's access. Placement alone never
grants DMA reachability, cache coherence, alignment or borrowing permission.
The default main stack and ordinary storage layout remain unchanged.

## Exact silicon admission

Optional cold `nx_silicon_preflight` reads the selected source once, after the
caller establishes legal privilege, MMIO and security state, before acquiring
I/O resources. It never enables clocks or repairs hardware. Keep the policy,
strictly ordered exact revision rules and their review-source SHA256 together;
the digest identifies review input and is not authentication or a claim that
workarounds were applied. The build supplies its reviewed applied-workaround
mask. Numerical revision zero is valid when explicitly observed.

`allow_unreviewed` cannot bypass a blocked revision, missing observation,
group/density mismatch or missing workaround. A completed rejection copies its
diagnostic reason without authorizing startup; source failure leaves output
unchanged. A device group and density do not establish package, PCB or physical
qualification. `silicon_admission_test.cpp` tests these distinctions against the
real preflight implementation and an explicit mocked identity source.
