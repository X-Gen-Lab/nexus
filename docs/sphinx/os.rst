Optional OS contracts
=====================

The common wait face expresses one waiter, legal publishers, absolute monotonic
deadlines and an authoritative completion predicate. A notification is a hint;
it neither transfers payload ownership nor advances a device executor. The
baremetal, Native and FreeRTOS adapters preserve their explicit storage and
backend-specific capabilities rather than pretending every kernel is identical.

Applications own scheduler startup, tasks, priorities, queues, stack budgets and
recovery. Returning tasks retain storage until the single lifecycle owner joins
them. Permanent tasks, direct notifications and closable queues are separate
optional APIs with explicit cost. Shutdown stops admission, keeps the owner
draining, joins producers and owner, quiesces publishers, then reclaims storage.
A running FreeRTOS scheduler still needs platform clocks and Tick after I/O stops.

OS profiles separate physical CPU/IRQ facts from runtime policy. Optional shallow
idle requires a continuous clock and explicit wake timer; trace uses caller-sized
storage. MPU and split-security modes have separate linker, port and ownership
contracts. Configuring an option cannot qualify missing hardware behavior.

Current source documentation and the exact OS01--OS55 ledger:

* `Execution and lifecycle contracts <https://github.com/X-Gen-Lab/nexus/blob/codex/next-generation-platform/docs/design/os-contracts.md>`_
* `Backend integration and active probe <https://github.com/X-Gen-Lab/nexus/blob/codex/next-generation-platform/docs/design/os-backend-integration.md>`_
* `OS delivery scope <https://github.com/X-Gen-Lab/nexus/blob/codex/next-generation-platform/docs/delivery/os.md>`_
* `OS execution ledger <https://github.com/X-Gen-Lab/nexus/blob/codex/next-generation-platform/docs/design/os-execution.csv>`_

The public API index is generated from the current OS include trees. Host Google
execution, real ARM linking and physical HIL establish different evidence. The
HIL tool only prepares plans or verifies retained station data; prepared plans
cannot qualify IRQ latency, FP/MVE context retention, stack peaks or safe outputs.
