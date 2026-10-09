Architecture Design
===================

Use the maintained :doc:`../user_guide/architecture` and the repository's
``docs/strategy/architecture-decisions.md`` when changing public contracts.
The architecture is organized around ownership, capability and failure states,
with reusable platform code separated from external product composition.

Dependency Direction
--------------------

Arch owns CPU primitives. HAL core/facades depend on narrow Nexus contracts;
OSAL selects one backend. SoC controllers depend privately on vendor SDK and
provider contracts. Board resources describe reviewed wiring. Platform targets
assemble SoC/Board objects and startup. Runtime owns only serial HAL/OSAL
bootstrap/release. Component cores use narrow ports; adapters and external
applications explicitly connect those ports.

A public compile interface must not propagate vendor SDK headers/macros, mutable
controller state, product partitions or business workers. Library link dependency
and public header dependency are reviewed separately. Raw SDK bring-up is an
explicit opt-in and brings its own resource-settlement responsibility.

Changes and Verification
------------------------

Breaking refactors are permitted when callers, providers, tests and docs change
together. Validate observable behavior: wrong/stale owner, bounded pool
exhaustion, busy/error retry, cancel/deadline races, failure rollback and final
buffer return. Directory movement needs actual firmware link checks for retained
startup/registration/strong IRQ; an archive or renamed target is insufficient.

Each build has one effective configuration and one Board/backend. Fixed SDK
revisions and exact source/Board/layout/toolchain/artifact identities make
independent consumption repeatable. External examples update Gitlink and lock
in one commit and run their own applications against that exact source pair.
Current evidence lives outside the source identity it verifies, avoiding a
self-referential source SHA.

Host models, real FreeRTOS POSIX execution, ARM compilation/linkage and physical
qualification are distinct gates. Hardware is currently deferred. No Native
result, target declaration or workflow definition qualifies MCU IRQ/DMA,
electrical behavior, power loss, timing or enterprise/LTS support.
