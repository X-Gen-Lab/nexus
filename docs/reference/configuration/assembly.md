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
cache variables, schema-1 JSON or old generated headers. Legacy JSON assemblies
are rejected as inputs, not retained as an alternate authoring interface.

The parser creates immutable `AuthoredAssembly` intent. Production consumers use
`resolve_ir()` and named immutable `ConfigurationIR` decisions; only audit and
artifact-boundary consumers request `to_dict()`. Typed source epoch 2 and the
schema-1 JSON output projection are separate contracts. The
[source compatibility policy](../../design/source-compatibility.md) owns their
version declarations; neither introduces firmware metadata or runtime dispatch.

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
| `layout` | Optional schema-1 Flash-region JSON path relative to the assembly; exactly one image |
| `[factory]` | Explicit optional strong IDs and capability metadata |
| Kind instance table | Stable authored name, reviewed binding, mode and bounded options |
| SPI/I2C device table | Separate endpoint identity tied to an existing controller |
| `[abi]` | Assert reviewed CPU architecture/FPU/float ABI; cannot invent chip features |

Resource checks cover actual pin/AF, controller, IRQ ceiling, shared timer/timebase,
physical DMA controller/stream, clock, exact memory domain, mode and endpoint identity.
A syntactically valid selector can still reject because that provider does not
implement the route. The resolver never silently changes pins, modes or clock.

See the [configuration and ownership guide](../../guides/developer-workflow.md)
for Board-first queries, validated profiles, human summaries and effective diffs.
`explain` defaults to human-readable output; automation must select `--json`.

## Physical Flash and the selected image

`flash` in the resolved output describes the complete physical device, including
its exact erase and programming geometry. `layout` selects the linked image
window. Without an authored `layout`, that image uses the full physical Flash.
A smaller image does not redefine the physical Flash API or create a Flash access
control policy. Products still own erase-aligned writable regions and recovery.

A `layout` JSON file has `schema_version = 1` and `regions`. Each region has an
`id`, integer `origin`, positive `size` and `kind` (`image` or `data`). Exactly one
region is an image. IDs must be unique; all regions must be nonoverlapping,
within physical Flash, and start/end on its exact erase boundaries. This is an
auxiliary layout input; schema-1 JSON assemblies remain rejected.

The reviewed offset-image profiles are STM32F407VET6, STM32F407ZGT6 and
GD32F470ZGT6. Other SoCs still require a reset-address image. An admitted offset
must additionally satisfy the source-owned 512-byte vector alignment. The
selected profile owns that alignment and the actual vendor vector symbol;
authored or artifact JSON cannot grant another chip offset support.

For example, this complete layout file selects the final 384 KiB of Sky
STM32F407VET6 Flash. Select it with the assembly's `layout` path. The decimal
origin is `0x08020000`; the omitted first 128 KiB is not another selected image
and is not protected from writes by this layout.

```json
{
  "schema_version": 1,
  "regions": [
    {
      "id": "application",
      "origin": 134348800,
      "size": 393216,
      "kind": "image"
    }
  ]
}
```

For an offset image, generated linker checks require `.isr_vector` at the image
origin, correct alignment, a nonempty table no larger than 512 bytes, and the
actual vendor symbol at the section base: `g_pfnVectors` for F407 or `__gVectors`
for GD470. The selected startup sets VTOR from that linked symbol with the
required barriers. Default reset-address linker output keeps its existing path.

ELF LOAD bounds, Flash capacity and footprint use the selected image window.
ELF-to-BIN checks reconstruct bytes from that image origin rather than prefixing
the binary to physical Flash base. Physical erase/program facts and RAM budgets
remain unchanged; use `flash`, not `layout.origin`, to interpret erase geometry.

An offset executable still needs a product-owned boot path that reaches its
vendor reset entry with valid initial machine state. Linking, vector checks and
BIN identity do not execute that path. This mechanism does not provide image
authentication, activation, A/B rollback, Flash permissions or a general running
image handoff. Baremetal or FreeRTOS links prove their build scope only. In
particular, the existing F407 handoff retains PRIMASK=1 and startup does not
unmask it; no FreeRTOS handoff qualification follows from an offset link.

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
