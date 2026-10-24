---
id: docs-explanation-ownership
type: explanation
audience: consumer
source_language: en
---

(docs-explanation-ownership)=
# Borrow, completion and observer lifetime

The caller owns request and buffer storage. Accepted asynchronous I/O borrows it;
rejection does not. Cancellation, a deadline or a failed stop cannot revoke that
borrow. This separation prevents IRQ/DMA or an executor from accessing recycled
memory. The [lifecycle contract](../design/next-generation-platform.md)
and public operation declarations define exact context and result behavior.

| Event | Storage consequence |
| --- | --- |
| Rejected admission | No retained request/buffer references or residual hardware effects |
| Accepted admission | Keep request and buffers valid until acquire-observed SETTLED |
| Cancel or deadline | Request termination; continue required progress and retain borrow |
| DMA completion | Memory engine stage complete; wire/publisher references may remain |
| Failed drain / QUARANTINED | Keep storage and explicit recovery responsibility |
| SETTLED publication | Provider's final access has ended after necessary reference drain |
| Observer/control reuse | Also detach/drain matching epochs and join observers |

![Current borrowed request state and settlement proof obligations](../assets/diagrams/request-lifecycle.svg)

FIG-06 does not treat REJECTED/ACCEPTED as request-state enum members. Core helpers
cannot observe a peripheral's physical drain; the provider must establish that
proof before publishing settlement.

## Execution contexts and shutdown ownership

| Operation group | Execution owner | Required lifetime and progress |
| --- | --- | --- |
| Direct request admission and service | One serialized executor in the public operation's permitted context | Keep admitted request and buffers through acquire-observed SETTLED; explicit service advances deadline and drain |
| Fixed provider IRQ | The selected vector and its declared IRQ policy | Record bounded hardware facts; retain every reference until the provider proves withdrawal |
| Cross-task producers or cancellation | An explicit owner/adapter using stable slots and epochs | Drain controls and join observers before recycling operation or adapter storage |
| Waiting | The caller with an explicit wait/wake port | Check readiness, arm, recheck, wait and recheck the authoritative predicate; a hint alone does not settle a request or service hardware |
| Platform start and stop | One external lifecycle owner | Quiesce callers and publishers, keep cleanup progressing and retain remaining effects after BUSY or failure |

Atomic publication of request state does not serialize accesses to other
request fields or payload buffers, or permit competing admission owners. Use
the public operation contract and an explicit shared owner when multiple tasks
access a controller. Platform stop does not revoke a static face or
automatically stop product tasks.

## Three different completion facts

![Current UART and SPI memory, wire and final-publication order](../assets/diagrams/dma-completion.svg)

FIG-07 separates memory completion, wire completion and final publication. UART
TC is not NDTR zero; SPI idle and CS interval are not a DMA interrupt. Disable
and detach the actual IRQ/DMA/queue references before release-publication of
SETTLED. The caller's acquire observation then exposes the final result/count.
A timeout result can be final only after this lifecycle, not at deadline expiry.

## Cross-task control and continuous streams

Use stable slots plus monotonically increasing epochs for remote control. Do not
send a raw pointer to storage another task can recycle. Detach control/observer
references and join observers before releasing the slot and preparing the next
request. A generation counter detects stale identity but does not itself join a task.

Continuous receive and ADC use explicit caller-owned block loans. A consumer keeps
a block immutable until matching release. The producer needs a replacement target
before publishing an old hardware bank. No replacement means visible stop/backpressure;
it must never overwrite a consumer. Stop waits for filling, ready and borrowed
blocks, not just active DMA. [The stream contract](../design/io-observation-recovery.md)
records what loss counters and software timestamps do and do not prove.

![Current continuous block producer, replacement and loan drain](../assets/diagrams/continuous-stream.svg)

FIG-15 includes observable loss and stop/drain responsibilities. Capacity and
service-latency budgets are explicit product inputs; arbitrary IRQ delay or
consumer speed cannot be promised lossless. The [tutorial](../tutorials/async-io.md)
executes rejection and quarantine; [OS shutdown](../guides/os-shutdown.md) explains
wait and join responsibilities.
