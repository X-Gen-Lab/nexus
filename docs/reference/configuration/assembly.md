---
id: docs-reference-configuration-assembly
type: reference
audience: consumer
source_language: en
---

(docs-reference-configuration-assembly)=
# Assembly input and rejected selections

The authored configuration authority is one schema-2 TOML assembly. SoC and Board
facts remain maintained JSON; `resolved.json` is output, not an authored alternative.
CMake owns software dependencies. Failed TOML resolution never falls back to Kconfig,
cache variables, schema-1 JSON or old generated headers.

Before running the resolver, prepare the
[fixed source and recursive dependencies](../../guides/source-checkout.md).

## Complete maintained input

```{literalinclude} ../../../tools/configure/assemblies/native-uart.toml
:language: toml
:caption: Complete Native UART assembly, parsed by the production resolver.
```

| Selection | Meaning and rejection boundary |
| --- | --- |
| `schema = 2` | Authored TOML generation; unknown keys reject |
| `board` | Maintained immediate child of `boards/`; exactly one Board selector |
| `board_package` | External package path relative to the assembly; alternative to `board` |
| `backend` | Explicit `native`, `baremetal`, `freertos` or supported `threadx` subset |
| `clock` | Maintained exact profile matching Board and SoC facts |
| `optimization` | Maintained optimization/debug/LTO profile; no arbitrary compiler override |
| `[memory]` | Byte budgets for stack, libc heap, static RAM and Flash LOAD |
| `[factory]` | Explicit optional strong IDs and capability metadata |
| Kind instance table | Stable authored name, reviewed binding, mode and bounded options |
| SPI/I2C device table | Separate endpoint identity tied to an existing controller |
| `[abi]` | Assert reviewed CPU architecture/FPU/float ABI; cannot invent chip features |

Resource checks cover actual pin/AF, controller, IRQ ceiling, shared timer/timebase,
physical DMA controller/stream, clock, exact memory domain, mode and endpoint identity.
A syntactically valid selector can still reject because that provider does not
implement the route. The resolver never silently changes pins, modes or clock.

## Inspect and generate

Run from the repository root, using a new output you own:

```sh
python tools/configure/configure.py list-bindings     --assembly tools/configure/assemblies/native-uart.toml
python tools/configure/configure.py check     --assembly tools/configure/assemblies/native-uart.toml
python tools/configure/configure.py explain     --assembly tools/configure/assemblies/native-uart.toml
python tools/configure/configure.py generate     --assembly tools/configure/assemblies/native-uart.toml     --output /tmp/nexus-native-config
```

`check` and `explain` do not generate output. `generate` publishes one atomic owned
bundle; failure invalidates its owned output without replacing unrelated data.
Inspect `resolved.json`, input identities, generated `nexus_config.h`, bindings,
factory, linker memory and budgets. Changing declared inputs requires a fresh
resolution; separate Board/backend combinations use separate build roots.

CPU-only Runtime has a separate authored runtime TOML using the shared CPU resolver.
It emits Core/Arch/optional OS policy, no SoC/Board/startup/factory. Do not use it to
claim a full device platform. The [build contract](../../design/build-system.md),
[integration contract](../../design/integration-contracts.md) and production field
model/schema own detailed accepted fields; editor completion does not establish
hardware capability.
