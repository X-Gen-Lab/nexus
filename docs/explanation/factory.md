---
id: docs-explanation-factory
type: explanation
audience: consumer
source_language: en
---

(docs-explanation-factory)=
# Typed factory, static instances and polymorphic faces

The current platform keeps typed factory lookup and C polymorphism. Configuration
generates the exact instances; factory lookup returns them without constructing,
initializing, locking or discovering anything. Hardware startup and request execution
are separate explicit actions.

![Current factory IDs, shared readonly ops and independent instance contexts](../assets/diagrams/factory-instances.svg)

FIG-04 shows the current identity structure. A controller and a child endpoint have
different interfaces and IDs even when their implementation shares a physical bus.

## What generation owns

One schema-2 TOML selects instance names, bindings, modes and exact capacities.
The strict resolver produces typed IR; emitters publish readonly faces, shared ops
references, private independent contexts, fixed IRQ wiring and compact getters.
Native assemblies receive independent model state too; a test singleton is not
the production instance-generation mechanism.

`nexus_factory.h` is generated for each Platform assembly, so its IDs are not a universal
header or persistent numbering scheme. Default IDs are compact `uint16_t` aliases
with named constants. COUNT, invalid and unselected entries return NULL.
A chip capability record does not grant Board routing or physical qualification.

## Choose strong references for new products

Every Platform assembly emits `nexus_factory_strong.h`. New products should
include it and use the class-specific `NX_*_REF_*` constants with
`nx_factory_*_ref()` getters. These wrappers return the same static faces and
add no global storage, hardware access or configuration key.

Default scalar IDs remain assembly-local `uint16_t` aliases; C does not reject
mixing two such aliases. Nominal structure references let the compiler reject
that class mismatch. They do not authenticate an external value or make an
instance active. Optional capability records are separately selected by
`factory.capabilities`.

The factory header includes complete public APIs only for selected interface
classes and preserves forward declarations for unselected getters. Include a
specific public I/O header explicitly when code needs that API or its complete
face layout; an empty getter still returns NULL.

## What a call does

| Action | Work performed |
| --- | --- |
| `nx_factory_uart(id)` | Select and return an existing readonly UART face |
| `nx_platform_start()` | Execute selected setup and retain cleanup outcomes |
| Public I/O operation | Validate face, then dispatch `ops(context, ...)` |
| IRQ entry | Access the fixed concrete provider and its owned hardware |
| Explicit service/wait | Advance or observe the selected lifecycle |
| Stop | Close/drain according to contract; cannot revoke outstanding storage |

Products may retain a face for the provider-storage lifetime. Stopping hardware
does not revoke a pointer or synchronize callers. The application must quiesce
callers and publishers before reclaiming external adapter storage. No getter
changes mode or silently acquires execution ownership.

## Verify the generated design

This complete maintained assembly selects independent Native instances:

```{literalinclude} ../../tests/contracts/native_assembly/multi.toml
:language: toml
:caption: Maintained generated factory fixture; its software Board is not a physical PCB.
```

The [async tutorial](../tutorials/async-io.md) runs the complete generated factory
binary. It verifies cold lookup, distinct contexts with shared ops, bus/endpoint
identity, invalid IDs, stop/restart and retained quarantine. The authority for
these interfaces is the [integration contract](../design/integration-contracts.md)
and generated/public headers; this page explains the design rather than duplicating
function declarations.
