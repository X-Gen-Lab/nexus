Core Concepts
=============

Ownership and Capability
------------------------

Discovery/query reads metadata without starting hardware. Typed open uses explicit
owner/generation; close/recover errors preserve retryable ownership. Parent,
child, region, operation ticket and buffer lease have distinct lifetimes. A
capability is supplied only by an actual provider operation. Timeout/cancel
acceptance does not return unsettled buffers.

Layer Boundaries
----------------

Arch owns CPU-local masks/barriers/context. SoC owns controllers, clock/IRQ/time,
identity/geometry and private SDK. Board owns reviewed wiring/HSE/safe levels.
Platform owns startup/lifecycle/object assembly. HAL/OSAL define narrow contracts.
Runtime owns serial infrastructure; components use explicit ports/adapters.
External applications own main, workers, partitions, budgets and product policy.
``soc/native`` is a virtual host model, not physical silicon.

Configuration and Runtime
-------------------------

One build root owns one effective configuration and one Board/backend. Presets
select compiler/mode; one fragment selects implemented software/resources; one
external Board and optional layout produce hashed outputs. Default image owns
all physical Flash with no storage reservation. No root ``.config`` fallback or
implicit business main is used.

Runtime READY means HAL/OSAL ownership, not health. Baremetal owns a loop;
FreeRTOS starts checked application workers/scheduler explicitly. Idle/pre-scheduler
MCU teardown is bounded; running/suspended kernel remains BUSY. Actual cleanup
failure preserves PARTIAL and admission fencing until settled retry succeeds.

Evidence
--------

Host models, real FreeRTOS POSIX execution, ARM compilation/linkage and physical
HIL are separate scopes. Old test counts and workflow definitions are not new
source validation. A clean delivery binds exact source-pair/config/dependencies,
Board/layout/toolchain and same-artifact hashes. Hardware is currently deferred;
no enterprise/LTS or physical qualification is implied by software checks.
