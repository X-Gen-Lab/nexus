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
artifact-boundary consumers request `to_dict()`. Typed source epoch 5 and the
schema-1 JSON output projection are separate contracts. The
[source compatibility policy](../../design/source-compatibility.md) owns their
version declarations; neither introduces firmware metadata or runtime dispatch.

Before running the resolver, prepare the
[fixed source and recursive dependencies](../../guides/source-checkout.md).

## Complete maintained input

```{literalinclude} ../../../tools/configure/assemblies/qiming-baremetal-uart.toml
:language: toml
:caption: Complete F407ZG UART assembly, parsed by the production resolver.
```

| Selection | Meaning and rejection boundary |
| --- | --- |
| `schema = 2` | Authored TOML generation; unknown keys reject |
| `board` | Maintained immediate child of `boards/`; exactly one Board selector |
| `board_package` | External package path relative to the assembly; alternative to `board` |
| `backend` | Explicit `native`, `baremetal`, `freertos` or supported `threadx` subset |
| `clock` | Maintained exact profile matching Board and SoC facts |
| `optimization` | Maintained optimization/debug/LTO profile; no arbitrary compiler override |
| `[memory]` | Byte budgets plus optional executable-image prefix/suffix reservations |
| `layout` | Optional schema-1 physical Flash-owner JSON path relative to the assembly; exactly one image |
| `[factory]` | Optional capability metadata; only `capabilities` is accepted |
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

Every generated bundle contains `nexus_factory_strong.h`. Application code can
include it and use nominal references such as `NX_UART_REF_UART0` with
`nx_factory_uart_ref`; no `[factory]` selection enables that header.
`factory.strong` is not a supported field and rejects as unknown. The optional
`capabilities = true` selection emits immutable mode metadata separately from
the interface faces. Factory getters return existing static objects and do not
start hardware.

## Logical GPIO signals

On an authorized single-pin `gpio` input or output, the optional boolean
`active_high` selects an immutable logical signal in addition to the existing
port face. Omit it to retain only the physical-level port. Unknown values and
multi-bit masks reject; no polarity is inferred from the Board's initial level.

The following fragment belongs in an independently complete assembly whose
Board authorizes the output binding `rs485-de`; it declares no new wiring.

<!-- nexus-toml-fragment: GPIO signal within an independently complete Board-authorized assembly -->
```toml
[gpio.direction]
binding = "rs485-de"
mode = "output"
active_high = true
```

Include `nexus_factory_strong.h` and obtain the binding with
`nx_factory_gpio_signal_ref(NX_GPIO_REF_DIRECTION)`. Check for NULL before use;
undeclared signals and invalid IDs return NULL. The getter neither initializes
hardware nor changes the physical safe level. A signal reuses its authorized
port and shared ops, with no mutable state, per-pin vtable or new owner. The
RS485 adapter accepts this binding as `direction`; software wire protocols keep
their physical-level and open-drain contracts.

## I2C endpoint address width

An `i2c_device` has `address_bits = 7` by default, with an address from 8 to
119. Reviewed STM32F407 polling, IRQ and DMA message providers also accept
`address_bits = 10`, with an address from 0 to 1023. Width is an exact integer;
unknown widths and booleans reject. A controller may have both widths, and
duplicate endpoint checks include the width. Native does not acquire 10-bit
support from the schema. Controller metadata uses `address_bits = 0` when the
width belongs to each endpoint; the endpoint selected contract reports 7 or 10.

A 10-bit read includes its own repeated START before the read header. ES0182
Rev19 section 2.10.4 therefore restricts the maintained F407 10-bit profile to
`max_hz <= 88000` in Standard mode or explicit Fast mode with
`max_hz > 100000`. The default 100 kHz profile must be changed explicitly;
42 MHz PCLK is not an erratum exemption. Firmware also checks the actual CCR
before borrowing storage or starting hardware. The authored ceiling and ARM
link checks do not qualify physical rise times, target devices or bus timing.

## ADC raw resolution and scan timing

For the maintained F407 ADC1 modes, `resolution_bits` selects 6, 8, 10 or 12
bits. Omitting it produces an explicit 12-bit decision in the immutable IR and
resolved audit output. Booleans, strings and other integer values reject. A
provider must declare support for the chosen resolution; the configuration
field alone does not grant another device that capability.

The following fragment belongs in an assembly whose Board explicitly authorizes
the `analog` ADC binding. It is not a complete assembly or a PCB wiring claim.

<!-- nexus-toml-fragment: ADC selection within an independently complete Board-authorized assembly -->
```toml
[adc.analog]
binding = "analog"
mode = "low-rate-scan"
channels = [0, 1]
sample_times = [0, 0]
reference_mv = 3300
resolution_bits = 8
```

Polling, finite trigger DMA and continuous trigger DMA share the selected raw
format. According to RM0090 Rev22 §13.7, one rank costs its selected acquisition
cycles plus 12, 10, 8 or 6 conversion cycles. Rate admission and continuous
sequence metadata use the complete scan cost. A timer divider must also be exact;
lowering the resolution never silently changes the requested trigger rate.

DMA still transfers halfwords, and each sample remains a right-aligned native
`uint16_t` raw count. Block capacities and lengths remain byte counts and must
contain whole scans.
No packed byte format or extra buffer is introduced for 6- or 8-bit conversions.
These software facts do not establish analog accuracy, effective resolution,
sample aperture timestamps or physical throughput.

## Physical Flash and the selected image

`flash` in the resolved output describes the complete physical device, including
its exact erase and programming geometry. `layout` selects the physical erase
owner that contains the executable image. Without an authored `layout`, that
owner is the complete physical Flash. Products still own writable regions and
recovery; this selection grants no Flash access control or protection.

A `layout` JSON file has `schema_version = 1` and `regions`. Each region has an
`id`, integer `origin`, positive `size` and `kind` (`image` or `data`). Exactly one
region is an image. IDs must be unique; all regions must be nonoverlapping,
within physical Flash, and start/end on its exact erase boundaries. This is an
auxiliary layout input; schema-1 JSON assemblies remain rejected.

### Executable prefix and suffix

The optional `[memory]` fields reserve bytes inside the selected physical owner.
Both fields are unsigned 32-bit byte counts with a default of zero. Booleans,
floats, negative values and values above `4294967295` reject. Zero-valued fields
are omitted from the resolved projection. They preserve the existing default
layout, linked addresses and budgets; source changes still change source and
configuration identities.

| Field | Meaning |
| --- | --- |
| `image_prefix_bytes` | Reserved bytes before the executable vector table |
| `image_suffix_bytes` | Reserved bytes after the executable load window |

`ConfigurationIR` derives the executable origin and size from its immutable
`FlashRegionIR` owner and `MemoryBudgetIR` reservations. `PhysicalPartIR` remains
the sole physical geometry authority; the JSON `layout` projection retains that
geometry for audit. No independent executable address or CMake address override
is authored.

```text
owner       = [layout.origin, layout.origin + layout.size)
image_start = layout.origin + image_prefix_bytes
image_end   = layout.origin + layout.size - image_suffix_bytes
image_size  = image_end - image_start
```

The physical owner remains erase-aligned and nonoverlapping. Its executable
window need not start on an erase boundary. The prefix plus suffix must be less
than the owner size and leave a complete reset vector. The derived window must
stay inside that owner without address overflow. An explicit
`flash_load_limit_bytes` cannot exceed the executable size; the default Flash
budget uses that size. Reserved prefix/suffix bytes do not count as actual ELF
LOAD bytes and do not change the physical Flash API.

The reviewed offset and nonzero-reservation profiles are STM32F407VET6 and
STM32F407ZGT6. They require complete physical erase geometry and
a 512-byte-aligned executable vector base, including when the physical owner
starts at the reset address. Other SoCs and external provider packages gain no
prefix/suffix support from these fields. The maintained profile owns that exact
alignment and its actual vendor vector symbol; authored or artifact metadata
cannot grant another startup that capability.

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

For an offset image or nonzero reservations, generated linker checks require
`.isr_vector` at the derived executable origin, correct alignment, a nonempty
table no larger than 512 bytes, and the actual vendor symbol at the section
base: `g_pfnVectors` for F407 or `__gVectors` for GD470. The reviewed startup sets
VTOR from that linked symbol with the required barriers. Default zero-reservation
reset-address linking keeps its existing semantics.

ELF LOAD bounds, Flash capacity and footprint use the derived executable window.
The resource checker independently validates the physical owner, reservations,
vector words, reset entry and all actual Flash LOAD ranges against that window.
ELF-to-BIN checks reconstruct bytes from the executable origin. They add no
prefix or suffix bytes. Physical erase/program facts and RAM budgets remain
unchanged; use `flash`, not `layout.origin`, to interpret erase geometry.

The complete [public image-window reference](../../../cmake/package/verification/image_window/README.md)
selects a 128 KiB F407 sector owner at `[0x08020000, 0x08040000)`. Its 512-byte
prefix and 1,024-byte suffix derive `[0x08020200, 0x0803fc00)`, or 129,536 bytes,
for vectors and executable LOADs. The reference links the actual reviewed
startup through the installed SDK; its declared 2,048-byte MSP is not a measured
stack peak.

```{literalinclude} ../../../cmake/package/verification/image_window/assembly.toml
:language: toml
:caption: Complete F407 assembly with explicit byte reservations; layout.json is part of the linked public reference.
```

A prefix reservation does not create an image header. A suffix reservation does
not create a trailer, activation marker or rollback record. Products supply any
metadata format, keys, Flash permissions and recovery policy. The platform does
not build a bootloader or authenticate, install, activate or boot this image.

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
python tools/configure/configure.py list-bindings     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
python tools/configure/configure.py check     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
python tools/configure/configure.py explain     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
python tools/configure/configure.py generate     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml     --output /tmp/nexus-native-config
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
