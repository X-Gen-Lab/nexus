---
id: explanation.rtc-lptim
type: explanation
audience: maintainer
source_language: en
---

(explanation-rtc-lptim)=

# RTC and LPTIM: separate time domains and retained requests

Use this explanation to integrate the Native raw-calendar RTC and finite-counter LPTIM through static typed factory lookup and explicit Platform lifecycle. One ordinary TOML assembly selects independent instances; public calls enter the matching provider directly. This Native host-model scope does not provide an MCU RTC/LPTIM driver, a product clock service or hardware low-power continuity.

The public declarations in [rtc.h](../../io/include/nexus/io/rtc.h), [lptim.h](../../io/include/nexus/io/lptim.h) and the host-only [model controls](../../io/include/nexus/io/native/rtc_lptim_model.h) remain authoritative. This page explains ownership and use boundaries; it does not duplicate their complete return-value reference.

## Current implementation and planned integration

The current implementation has common C wrappers and a Native provider. The wrappers validate a face and dispatch through its immutable operations table to its context. Native instances share two readonly operations tables, one per device class, and retain independent state. There is no heap, registry, hidden worker, request queue or mandatory OS adapter.

Construction is an internal integration boundary. Generated Platform preparation constructs fresh owner storage once, copies the selected Core clock face without invoking its time reader, and opens admission. Later starts of the constructed owner use explicit private reopen. External products use public and generated headers, not private provider constructors. The RTC initially stays unset until an explicit public set.

Explicit private reopen and ordinary typed configuration assembly are implemented for the selected Native host models. Private reopen changes the owner incarnation after actual withdrawal and external joins instead of reconstructing storage. H743 and U575 RTC/LPTIM adaptation remains a future target with separate hardware contracts and evidence.

![Current Native RTC/LPTIM assembly, ownership and final publication; dashed hardware branch is a future target](../assets/diagrams/rtc-lptim-lifecycle.svg)

FIG-21 distinguishes current Native build-time assembly from runtime calls and source progression. Edge labels state which relationship applies. The dashed hardware branch is a target; it does not establish a physical driver, IRQ, STOP/VBAT continuity or HIL result. A factory returns an existing face; it never performs lifecycle work.

## Three domains, three meanings

Core deadlines are absolute monotonic microseconds read through the copied `nx_clock_t` face. The original face descriptor may be copied, but its context remains borrowed: assembly must preserve the entire context lifetime, disjoint storage, finite monotonic readings and non-reentrant access. A finite deadline expires when the observed Core time is greater than or equal to it. `NX_DEADLINE_NEVER` avoids a deadline read; it does not make the calendar or counter advance.

RTC uses raw year `00..99`, month/day and 24-hour time without UTC, a century, timezone or leap seconds. Leap years satisfy raw year modulo 4 equals zero. Weekday is an independent `1..7` field. Natural `99 -> 00` rollover advances the calendar without creating a new setting domain; an explicit successful set advances the nonwrapping generation.

An RTC alarm matches one future day-of-month/hour/minute/second pattern. It has no month/year field or wildcard mask. A pattern already matching the current second is rejected with `STATE`; the provider does not silently interpret it as a later recurrence. The product owns UTC conversion, date selection, synchronization, recurrence and authorization policy.

LPTIM accepts one finite shot of `1..65536` ticks. Its positive `tick_hz` is a fixed declared unit rate, not measured oscillator accuracy. The 16-bit sample is modulo 65536: a complete 65536-tick shot can sample as zero. Counter zero alone proves neither completion nor low-power continuity. Open idle returns zero and `running=false`; closed public read/sample returns `STATE` without changing output.

RTC and LPTIM domains contain the owner address and a checked `uint64_t` generation. They are separate types and are not Core deadline tokens. RTC set, private reopen and completed LPTIM withdrawal invalidate older domains. Tokens compare identity without dereferencing it; they establish neither authorization nor cross-boot identity. Retire every token and external observer before ending or reusing owner storage.

Generation never wraps silently. A Native RTC set at `UINT64_MAX` returns `EXHAUSTED`; a Native LPTIM submission at that limit refuses before acceptance so a previously accepted shot can always perform its reserved final generation increment. Exhaustion is not permission to reset a stopped owner or abandon a loan.

## Who owns storage and execution

One serialized, non-reentrant task executor owns all calls on an instance, including reads, model controls and lifecycle. The face, operations table, private owner and borrowed clock context outlive every accepted request and any separate observer/control joins. No implicit cross-task lock or ISR entry contract is provided. Products that need cross-task cancellation must add explicit stable-slot/epoch control and joining outside this provider.

The caller owns the complete request. Initialize its Core base before preparing it; prepare only unborrowed `READY` or consumed `SETTLED` storage after all joins. Preparation copies metadata and takes no loan. The request and input/output spans must be aligned, nonwrapping and disjoint from known protected objects. These checks are not a universal invalid-pointer or memory-access oracle; assembly remains responsible for opaque context extents and live mapped storage.

Each instance accepts at most one request. Submission validates lifecycle, idle state, the current domain and the class-specific range before reading a finite deadline. `SUCCESS` means ACCEPTED; rejection retains no request or source effects. A rejected request stays caller-owned. A missing optional operations-table method returns `UNSUPPORTED`; malformed public inputs may return `INVALID` before provider dispatch.

After ACCEPTED the caller keeps every request byte live and immutable until acquire-observed `SETTLED`, then consumes the result and joins any separate controls or observers before reuse. Cancellation, expiry, stop, a failed withdrawal or a sampled completion flag never revoke this loan.

## Source progression, terminal cause and final access

Host model advancement changes source state only. RTC advancement is bounded to `0..86400` seconds per call with at most one date carry; an initialized calendar may advance while admission is closed. LPTIM advancement is bounded to `0..65536` ticks and saturates at the shot limit; an unarmed source accumulates nothing. These hooks neither read Core time nor touch or settle requests. Setting or clearing the withdrawal hold also does not settle a request.

Explicit public service observes the predicate and advances withdrawal. Cancel and stop can call withdrawal directly, so they may settle immediately when no hold remains; service is not the only final publisher. On first service observation an expired finite deadline wins over a pending event. An earlier cancel or stop may instead latch `CANCELLED`. Once latched, the first terminal cause stays fixed while withdrawal waits.

If withdrawal is held, the request remains borrowed and `QUARANTINED`. Cancel can return `SUCCESS` because cancellation was accepted while service/stop returns `BUSY`. Removing a model hold only makes a later real withdrawal possible; the executor must still call an appropriate public service/cancel/stop operation. Keep the owner and request alive while that work remains.

Successful withdrawal first disables the source, clears pending state and detaches the owner reference. LPTIM also records final tick progress and advances its generation before publication. Only then does the provider release-publish `SETTLED` as its last request access. RTC `transferred` is 1 for success and 0 otherwise. LPTIM `transferred` is the final observed tick count, including ticks accrued while withdrawal was held; it is not frozen when cancel/timeout first latches.

Stop closes admission immediately, even when withdrawal returns `BUSY`. Closed reads and submissions remain rejected, while service/cancel can still drain the retained loan. A repeated clean stop is idempotent. Successful stop does not end owner storage lifetime or waive external joins. Reopening is a separate explicit private lifecycle operation, not a side effect of lookup, model advancement or a repeated constructor.

## Explicit private reopen preserves identity and responsibility

A constructor is valid once per owner storage lifetime. The internal `nx_native_rtc_start` and `nx_native_lptim_start` operations reopen that same stable owner; they are not public application SDK APIs. They require a valid constructed, closed owner with no active request, armed source, pending event or terminal responsibility. The caller must also prove that external controls and observers have joined; inspecting owner fields cannot establish those external joins.

After full-span validation, retained request/source responsibility takes `BUSY` priority over unconstructed/already-open `STATE`. A closed joined owner at `UINT64_MAX` returns `NX_ERROR_EXHAUSTED`. Every rejection preserves all owner bytes, source facts, fault state, clock and generation. Start is not idempotent: a clean already-open owner returns `STATE`. A held closed loan remains borrowed and `QUARANTINED`; neither start nor repeated construction can revoke it.

On success private start checks generation capacity, increments the generation, then opens admission. It reads no Core time, advances no source, reconstructs nothing and adds no state field or owner pool. It preserves the RTC set/unset state and calendar, LPTIM fixed `tick_hz`, copied clock binding and external withdrawal hold. A hold flag on an otherwise idle joined owner is an external fault, not an active loan; reopening preserves it rather than silently clearing it.

The final permitted reopen can move `UINT64_MAX - 1` to `UINT64_MAX`; after a later clean stop another reopen is exhausted. Old externally held `READY` domains remain stale and reject submission unchanged. No wrap, reset-to-one or implicit fresh construction repairs exhaustion. Model source progression and result consumption remain separate from this lifecycle transition.

## Typed assembly uses the existing configuration path

The maintained pair fixture below is a complete Native assembly. Ordinary `[rtc.name]` and `[lptim.name]` sections resolve through the existing immutable `ControllerIR -> provider -> factory` path. `calendar-alarm` has no date or timezone option; `counter16-oneshot` requires an explicit positive `uint32_t` `tick_hz`. Unknown policy/IRQ fields and duplicate controller claims are rejected rather than repaired.

```{literalinclude} ../../cmake/package/verification/io_evolution/native-time-pair.toml
:language: toml
```

Native host-model facts expose RTC0/RTC1 and LPTIM0/LPTIM1 without pins or IRQ routes. Two software RTC instances exercise isolation; they do not assert two physical RTC peripherals. The generator emits only selected static state, readonly faces and pure typed lookup. The selected shared deadline adapter reads `nx_time_now_us()` without advancing any RTC/LPTIM source. An empty assembly emits no selected RTC/LPTIM state or that adapter.

## Public and generated entry points

Use `nexus_factory.h` to obtain `nx_factory_rtc(id)` or `nx_factory_lptim(id)`. IDs belong to the selected assembly and class; invalid/unselected IDs return NULL. Faces and shared operations tables exist before startup, but RTC read and LPTIM sample reject closed/unconstructed use. Lookup neither constructs an owner nor reads a clock.

The external lifecycle owner calls `nx_platform_start()` and checks `primary`, `cleanup` and `remaining_effects`. It then explicitly sets the RTC if needed, copies a current domain, prepares caller-owned requests and submits through public ports. It drives the required model source and public service separately. Startup creates no scheduler, and Core-time advancement does not advance a calendar or finite shot.

If `nx_platform_stop()` returns `BUSY`, retain every borrowed request and its owner, drive real withdrawal, consume final results and join external users before retrying stop. Complete Platform shutdown before another start; it does not silently reconstruct or reopen an unfinished stop. The [pair consumer](../../cmake/package/verification/io_evolution/consumers/native-time-pair.c) and [empty consumer](../../cmake/package/verification/io_evolution/consumers/native-time-empty.c) are maintained public/generated host verification programs, not hardware application examples.

## Costs and verification boundary

The public face contains two pointers; per-instance state contains the copied clock face, domain and class-specific calendar/counter state. One request is retained at a time. RTC model work is bounded without a per-second loop; LPTIM uses bounded saturating arithmetic. This structure avoids dynamic allocation and queue capacity costs, but it does not establish a byte budget, stack bound, WCET, oscillator accuracy or MCU power result. Measure those against the actual selected build and hardware.

The Native behavior and explicit private reopen described here build on separately recorded manual evidence. The phase3 manifest does not record a rerun of those manual programs.

Phase3 development evidence binds the frozen dirty-source tuple identified by source-and-execution manifest SHA-256 `85e01921e1bc53b81f30bff232bf3477aec3ac6d228a04ebebc3891dd21d729f`. It records configuration tests, generated pair/empty programs and focused installed development SDK consumers. It does not inherit main-branch qualification or establish a clean publishable SDK, complete SDK matrix, repeatability, whole-repository CI or physical results.

No H743/U575 RTC or LPTIM provider, ARM RTC/LPTIM execution, backup-domain reset/ownership, oscillator selection/calibration, STOP-mode wake, VBAT persistence, hardware retention or physical HIL result is established here. Future adaptation must prove its exact SoC/Board/clock/IRQ/security and retained-storage contracts; a chip datasheet capability or this public API alone does not provide that proof.

Related mechanisms: [request ownership](ownership.md), [typed factory](factory.md), [Native provider source](../../soc/native/drivers/rtc_lptim.c) and the [I/O evolution contract](../design/io-evolution.md). Product schedules, protocol state, persistence policy and private board wiring stay in external repositories.
