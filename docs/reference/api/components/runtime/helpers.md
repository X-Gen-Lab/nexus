---
id: reference.api.components.runtime.helpers
type: reference
audience: consumer
source_language: en
---

(reference.api.components.runtime.helpers)=

# Explicit-event runtime helpers

These optional C11 helpers run in the caller's existing serialized execution
path. They do not create threads, OS timers, global ticks or another event
queue. Keep immutable tables and contexts live through the final call, and
provide independent state for each instance.

| Target | Public header under `nexus/components/` | Selected behavior |
| --- | --- | --- |
| `Nexus::FSM` | `fsm.h` | Flat synchronous run-to-completion; ordered guards and explicit internal/external transitions |
| `Nexus::Periodic` | `periodic.h` | Fixed-phase release calculation with skip or caller-bounded catch-up |
| `Nexus::Timers` | `timers.h` | Exact caller slots, O(capacity) scan and stable slot/epoch tickets |
| `Nexus::Debounce` | `debounce.h` | Boolean stability with independent rising/falling intervals |
| `Nexus::EventFanout` | `event_fanout.h` | Ordered synchronous sinks with per-emission delivery accounting |

FSM callbacks must finish within the caller's budget. Events are borrowed only
during dispatch; a callback cannot retain their payload implicitly. The flat
profile does not implement hierarchical states, history or orthogonal regions;
that choice remains `not_selected`. See the [FSM contract](../../../../../components/runtime/fsm/README.md)
for transition ordering and reentry rejection.

Periodic, Timers and Debounce consume finite Core microsecond observations.
After a clock reset or domain change, join former users and explicitly reset or
reinitialize the helper. A new numerical deadline alone does not prove epoch
continuity. Periodic releases retain planned phase; they do not refresh the
deadline of an already queued I/O request. Timers preserve identity counters on
reset so a stale ticket cannot cancel a newly armed event.

EventFanout attempts the declared sinks and reports their individual outcome
counts and first error; its successful return does not mean every sink accepted
the message. Queue sinks use the existing queue's capacity and context rules.
Stream sinks retain the existing reserve/publish/acquire/release contract.
Copying a pointer in a message does not transfer the pointed-to storage.

Close upstream event admission before joining dispatchers and sinks. Then drain
accepted I/O through its existing owner and join observers before reclaiming
shared storage. [Owners](owners.md), [ClockDomain](clock-domain.md) and the
[I/O execution guide](../../../../guides/io-execution.md) provide those separate
lifecycle contracts. These helpers do not establish callback WCET, GPIO timing
or scheduler latency; physical timing remains `not_executed`.

## Public declarations

### fsm.h

```{doxygenfile} components/runtime/fsm/include/nexus/components/fsm.h
:project: nexus
```

### periodic.h

```{doxygenfile} components/runtime/periodic/include/nexus/components/periodic.h
:project: nexus
```

### timers.h

```{doxygenfile} components/runtime/timers/include/nexus/components/timers.h
:project: nexus
```

### debounce.h

```{doxygenfile} components/runtime/debounce/include/nexus/components/debounce.h
:project: nexus
```

### event_fanout.h

```{doxygenfile} components/runtime/event_fanout/include/nexus/components/event_fanout.h
:project: nexus
```
