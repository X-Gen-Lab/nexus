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

1. Prepare the [fixed source and recursive dependencies](source-checkout.md).
   Define the new behavior, supported modes, public ownership obligations and
   rejection boundaries before changing its production implementation.
2. Add and execute a meaningful RED GoogleTest/GoogleMock contract against the
   current production path. Retain the actual command, nonzero execution and
   expected failing assertion. Test MMIO/CPU/dependency ports through mocks;
   Python resolver behavior uses stdlib unittest. A missing tool or compile-only
   error is not the expected behavioral RED result.
3. Add exact `soc.json`/`routes.json` facts and documented provenance. Feed the
   shared CPU resolver; do not duplicate CPU flags or infer optional features.
   Place MCU implementations under `soc/<vendor>/<family>/` and declare that
   canonical `source_directory` in the maintained family descriptor. Public
   family identifiers stay independent of the directory. CPU implementation
   sources belong under their ISA, such as `arch/arm/cortex_m/`; reviewed
   cross-vendor register algorithms remain in `soc/common/`.
4. Add the family to the explicit maintained provider map in `tools/configure/`.
   Define supported modes, numerical limits, constructors, rollback and IRQ wiring.
   Return an explicit frozen `Construction` record, including the private state
   type and failed-init cleanup responsibility; do not require family-name
   inference in common generation. Unsupported modes reject; there is no runtime
   registration or plugin discovery.
5. Add explicit CMake targets and private SDK dependencies. Update declared
   resolver reconfiguration inputs and the Source SDK required-file closure.
6. Implement the smallest correct production change. Define an immutable public
   face with shared readonly ops and independent context.
   Reuse public Nexus types; keep vendor structures and concrete storage private.
   Review official HAL/LL reuse before duplicating a vendor state machine. Apply
   [the official-driver boundary](../design/official-stm32-provider.md) to time,
   callbacks, cancellation, private dependencies and mode-specific comparison.
7. Execute GREEN against the real provider. Verify rejection
   without residual effects, accepted borrow, cancel/deadline, failed drain,
   quarantine, stop/restart and instance isolation where the mode needs them.
8. Refactor while keeping contracts passing. Link exact ARM fixtures and verify
   actual resources, handler retention, vector bytes and unselected-driver pruning. Validate an
   independent relocated SDK consumer and record physical work as not executed.

For a component, pass explicit bus, GPIO, time and optional OS ports. Do not create
workers, probe devices, invent application roles or supply a maximum object pool.
Flash drivers expose physical geometry; persistence and update policy remain external.

The read-only [work-package view](../../tools/maintenance/README.md) connects
exact maintained modes to resolver fixtures, selected ops sources and reviewed
lifecycle properties. Its entries are navigation, not executed tests or new
support declarations. Run actual applicable gates separately.

## Review performance and ownership

Measure the selected empty image and the same fixture after enabling the new mode.
Report Flash LOAD, Flash footprint, each RAM domain and reserved task/IO storage
separately. Hot IRQ work should directly access its fixed provider; an interface
lookup is a cold action. Keep bounded admission and stream metadata operations
independent of queue length where their contracts require it.

A passing host register model proves its modeled software scope. New CPU, Cache,
FPU/MVE, security or low-power behavior needs its own actual execution boundary;
[platform support](../reference/platforms/support.md) does not infer it from a name.
