---
id: development.implementation-map
type: reference
audience: maintainer
source_language: en
---

(development-implementation-map)=
# Find the implementation authority

Use this map to locate an existing mechanism before changing it. It is a source
navigation, not another build graph or support database. Source links bind the
site's exact document revision; offline consumers can inspect the same paths in
the matching checkout or Source SDK.

| Task | Maintained entry | Contract and validation |
| --- | --- | --- |
| Authored assembly and fields | [configure](../../tools/configure/README.md) | [Assembly reference](../reference/configuration/assembly.md); actual resolver rejection tests |
| CPU facts and Runtime | [Arch](../../arch/README.md) | [Arch contract](../design/arch-contracts.md); CPU runtime compiler/model matrix |
| Request and deadlines | [Core](../../core/README.md) | [Ownership](../explanation/ownership.md); actual Core/epoch contracts |
| Public face and dispatch | [I/O](../../io/README.md) | [Factory](../explanation/factory.md); public headers and generated fixture tests |
| Chip/IRQ/DMA implementation | [Provider assemblies](../design/provider-assemblies.md) | [Provider guide](../guides/provider-porting.md); production-register models and exact ARM ELF |
| PCB data | [Board guide](../guides/board-porting.md) | Reviewed Board JSON and exact physical unknowns |
| Wait, task and kernel adapters | [OS](../../os/README.md) | [OS contract](../design/os-contracts.md); actual kernel and boundary tests |
| Reusable algorithms | [Components](../../components/README.md) | Public narrow ports; component and owner behavior tests |
| Consumer/export boundary | [Source SDK](../../cmake/package/README.md) | [SDK tutorial](../tutorials/source-sdk.md); independent relocated C/C++ consumers |
| Build targets and presets | [CMake](../../cmake/README.md) | [Build contract](../design/build-system.md); actual configure/build/resource gates |
| New production behavior | [Testing](../../tools/testing/README.md) | [TDD workflow](workflow.md); retained actual RED/GREEN and full gate |
| Qualification and hardware | [HIL](../../tools/hil/README.md) | [Qualification](../reference/platforms/qualification.md); distinct software and physical records |
| Document content and delivery | [Document standard](../design/documentation-standards.md) | [Maintenance](documentation.md); strict source/build/package/published-site checks |

Read the relevant contract and adjacent README before changing behavior. Select
only maintained modes, keep private provider/vendor types out of consumers, and
update declarations, diagrams, examples and translations together when their
meaning changes. Preserve original evidence bytes and scope instead of renaming
historical results to current HEAD.
