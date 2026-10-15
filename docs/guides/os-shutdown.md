---
id: docs-guides-os-shutdown
type: how-to
audience: consumer
source_language: en
---

(docs-guides-os-shutdown)=
# Wait correctly and shut down an explicit owner

Use this guide for applications selecting an optional Native, FreeRTOS or ThreadX
adapter. The application owns tasks, stacks, notifications, queues and execution
progress. Baremetal has no scheduler. Platform start never starts a kernel.
The [OS contract](../design/os-contracts.md) remains authoritative.

## Avoid lost completion and unbounded waits

Use the request's latched predicate as truth. Check the completion predicate, arm the
notification sequence, recheck, wait with the same absolute monotonic deadline,
then recheck again. Wakes are hints and may be coalesced or unrelated. On timeout,
perform the contract's final predicate observation. Do not restart a relative
budget after each hint and do not block the only executor that must make progress.

![Predicate, wake and explicit shutdown responsibilities](../assets/diagrams/wait-wake-shutdown.svg)

FIG-08 shows the current wait/wake responsibilities. A provider IRQ sink must
respect the selected kernel syscall ceiling; an unqualified priority cannot call
an ISR kernel API. ThreadX has its own reviewed M4/Native subset and masking policy,
not the FreeRTOS port or tickless/isolation capability set.

## Shut down in dependency order

1. Close client admission and reject new work without borrowing it.
2. Keep the resource owner/executor running. Request cancellation using stable
   slots and matching epochs; cancellation does not release accepted storage.
3. Drain transfers and block loans. Acquire-observe SETTLED and read results.
   A failed drain retains QUARANTINED storage and explicit recovery responsibility.
4. Join producers and detach/join control observers before slot or request reuse.
5. Stop and join the owner. Quiesce hardware IRQ publishers before destroying
   their notification, queue, guard, stack or TCB storage.
6. Stop platform providers only when their selected lifetime and kernel clock/Tick
   ownership permit it. A running scheduler is not stopped by ordinary I/O teardown.

Task return has a backend-specific implementation:

| Backend | After the application entry returns | External owner must prove |
| --- | --- | --- |
| Native | The pthread entry returns and the host thread terminates | Successful `nx_native_task_join()` |
| FreeRTOS | The wrapper publishes its completion hint and suspends until the external joiner deletes it | Successful `nx_freertos_task_join()` and external kernel detachment |
| ThreadX | The wrapper publishes its return predicate and completion hint, then follows the kernel completion path | Successful `nx_threadx_task_join()`; the hint alone does not prove ready-list detachment |

Returning from an entry function or observing a completion hint never authorizes
stack/TCB reclamation. Keep wrapper, stack and kernel-control storage until the
one external lifecycle owner successfully joins it. Raw queues and closable
queues have different lifecycle contracts; close is not destruction. Keep all
storage on the failure path and retry cleanup only through its real owner.

## Execute maintained software fixtures

First prepare the [fixed source and recursive dependencies](source-checkout.md).
From that repository root, with host build tools available:

```sh
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4     --target nexus_os_wait_test nexus_uart_owner_mock_test
build/native-debug/bin/nexus_os_wait_test
build/native-debug/bin/nexus_uart_owner_mock_test
```

Both binaries must execute nonempty passing cases. This targeted development scope
does not replace the complete TDD gate. The external UART owner application in
[nexus-examples](https://github.com/X-Gen-Lab/nexus-examples) supplies product workers;
its pin/revision must be recorded before claiming consumer execution.

Physical scheduler context, IRQ ceiling, FP/MVE retention, stack peaks and wake
latency require real station evidence. Do not inherit POSIX behavior as MCU timing.
