---
id: docs-guides-provider-porting
type: how-to
audience: maintainer
source_language: en
---

(docs-guides-provider-porting)=
# Add a SoC, provider or reusable component

Use this guide to add a maintained implementation with explicit resources and
measurable costs. A new family or mode requires behavior, configuration and build
integration together. Similar STM32/GD32 peripheral names do not prove register
compatibility.

## Keep the layer boundary narrow

| Owner | Required responsibility |
| --- | --- |
| Arch | Exact CPU primitives, feature facts and ABI; no PCB or device registry |
| SoC | Full part, memory, clocks, IRQ/DMA routes, reset/startup and private provider |
| Board | Source-reviewed wiring, oscillators, safe levels and actual unknowns |
| I/O provider | Public operation contract, private state, admission and final drain |
| Component | Caller-owned storage, bounded algorithm and explicit narrow ports |
| External product | Roles, tasks, queue depth, recovery, Flash reservations and release |

Read the [architecture](../explanation/platform-architecture.md),
[integration contract](../design/integration-contracts.md) and
[provider assembly contract](../design/provider-assemblies.md).

## Implement in a reviewable sequence

1. Add exact `soc.json`/`routes.json` facts and documented provenance. Feed the
   shared CPU resolver; do not duplicate CPU flags or infer optional features.
2. Add the family to the explicit maintained provider map in `tools/configure/`.
   Define supported modes, numerical limits, constructors, rollback and IRQ wiring.
   Unsupported modes reject; there is no runtime registration or plugin discovery.
3. Add explicit CMake targets and private SDK dependencies. Update declared
   resolver reconfiguration inputs and the Source SDK required-file closure.
4. Define an immutable public face with shared readonly ops and independent context.
   Reuse public Nexus types; keep vendor structures and concrete storage private.
5. Write and execute RED GoogleTest/GoogleMock contracts before production behavior.
   Mock MMIO/CPU/dependency ports, then execute the real provider. Verify rejection
   without residual effects, accepted borrow, cancel/deadline, failed drain,
   quarantine, stop/restart and instance isolation where the mode needs them.
6. Execute GREEN, then refactor. Link exact ARM fixtures and verify actual resources,
   handler retention, vector bytes and unselected-driver pruning. Validate an
   independent relocated SDK consumer and record physical work as not executed.

For a component, pass explicit bus, GPIO, time and optional OS ports. Do not create
workers, probe devices, invent application roles or supply a maximum object pool.
Flash drivers expose physical geometry; persistence and update policy remain external.

## Review performance and ownership

Measure the selected empty image and the same fixture after enabling the new mode.
Report Flash LOAD, Flash footprint, each RAM domain and reserved task/IO storage
separately. Hot IRQ work should directly access its fixed provider; an interface
lookup is a cold action. Keep bounded admission and stream metadata operations
independent of queue length where their contracts require it.

A passing host register model proves its modeled software scope. New CPU, Cache,
FPU/MVE, security or low-power behavior needs its own actual execution boundary;
[platform support](../reference/platforms/support.md) does not infer it from a name.
