---
id: docs-guides-migration
type: how-to
audience: consumer
source_language: en
---

(docs-guides-migration)=
# Migrate legacy configuration and interface access

Use this guide to migrate an older HAL/OSAL/Kconfig consumer to the current Source
SDK generation. Keep a fixed source revision while migrating. The platform retains
typed factory plus C polymorphic interfaces; it changes instance generation and
ownership rather than replacing interfaces with a runtime service registry.
The [compatibility contract](../design/source-compatibility.md) owns
version and interface epoch behavior.

Obtain the [fixed source and recursive dependencies](source-checkout.md) first;
keep the old consumer in its own checkout while validating the new one.

| Legacy pattern | Current migration action |
| --- | --- |
| Kconfig or cache macros selecting instances | Write one complete schema-2 TOML assembly |
| Macro-driven object construction during lookup | Use generated static objects; factory lookup only returns identity |
| HAL/OSAL implementation pointers in products | Include public Nexus APIs and generated `nexus_factory_strong.h` |
| First SPI/I2C child used as bus | Obtain controller and endpoint separately with their typed getters |
| Global maximum pools or default task | Provide exact selected buffers, slots and application workers |
| Cancel/timeout followed by immediate reuse | Keep borrow until acquire-observed SETTLED and observer drain |
| Platform-owned product partition or policy | Move reservations, roles and recovery to the external product |

## Move one complete assembly

Inspect this maintained F407ZG assembly; it is complete, not a fragment:

```{literalinclude} ../../tools/configure/assemblies/qiming-baremetal-uart.toml
:language: toml
:caption: Current schema-2 F407ZG UART input.
```

Run from the repository root with Python 3.11+ and CMake 3.31+ on `PATH`, after
preparing the selected dependencies. CMake is required by the compatibility
check's real package-version tests. Execute the actual parser and generation path:

```sh
python tools/configure/configure.py check \
    --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
python tools/configure/configure.py generate \
    --assembly tools/configure/assemblies/qiming-baremetal-uart.toml \
    --output /tmp/nexus-migration-config
python tools/maintenance/compatibility.py --expected-factory-epoch 3 \
    --generated /tmp/nexus-migration-config/nexus_config.h
```

Use a new output path you own, and stop if any command returns nonzero. Successful
resolution reports the selected part, Board, backend and configuration digest;
the compatibility check must accept epoch 3. Inspect `resolved.json`,
`nexus_config.h`, `nexus_factory.h`, `nexus_factory_strong.h` and
`nexus_bindings.h`. The version macros checked by `--generated` are in
`nexus_config.h`. These commands validate and generate configuration; they do not
compile firmware or execute hardware. Retain needed diagnostics before removing
only your generated output.
Never author the resolved output or reuse stale generated files after a failure.
Legacy schema-1 JSON is no longer a platform input. Convert it to a complete
schema-2 TOML assembly; generated `resolved.json` remains audit output only.
A failed TOML does not fall back to JSON, Kconfig, old headers or cache values.
Frozen historical evidence remains verifiable under its original format; this
read-only retention does not reopen old configuration or generation paths.

## Move access and lifecycle together

Use compact typed IDs from the current assembly and check getter NULL results.
Getter names and ID constants are generated from authored instance names; IDs are
not persistent IDs or a binary ABI. Every Platform assembly emits
`nexus_factory_strong.h`; new calls should use its class-specific `NX_*_REF_*`
constants and `nx_factory_*_ref()` getters. No configuration field enables strong
references. Optional capability records are selected separately with
`factory.capabilities`. Start hardware through `nx_platform_start()` and inspect
primary, cleanup and remaining-effects results.
After successful startup, ordinary I/O uses the returned face directly.

Convert synchronous and asynchronous calls according to their actual mode.
For accepted requests, migrate storage lifetime, service/wait progress, cancellation,
settlement and shutdown together. [The async tutorial](../tutorials/async-io.md)
executes these failure paths; replacing a function spelling alone is insufficient.

Build an independent relocated [SDK consumer](../tutorials/source-sdk.md), then
link the exact MCU configuration with the locked compiler. Retain evidence for
the migrated source and configuration. Old HAL/OSAL documentation remains historical;
a redirect does not make an old API contract equivalent to a new one.

## Rebuild for source interface epoch 3

The current development snapshot keeps semantic version `0.2.1` and uses
factory/source interface epoch `3`. Pin its exact commit, regenerate the bundle
and rebuild every caller, public face/ops, generated factory, provider and
component with one configuration and toolchain. Current interface admission
rejects epoch-1 and epoch-2 SDKs. Select a new build directory so old CMake caches,
generated headers and objects cannot enter the new application. Keep the old
build separate until the migrated consumer passes its required checks. No stable
binary ABI is supplied. A development-fixture opt-in does not relax interface
checks. Existing tags are unchanged.

The version fields remain independent: authored TOML schema `2`, typed IR source
epoch `3`, factory/source interface epoch `3`, resolved JSON projection `1` and
installed SDK manifest schema `2`. A matching semantic version or manifest schema
does not establish matching interface generation.

The epoch-2 ADC result migration remains required when upgrading older callers.
For ADC groups, direct `request.frames` access becomes `request.output.frames`.
Prefer `nx_adc_group_prepare()` for raw requests: it installs the explicit RAW
`results_kind`. Use `nx_adc_group_prepare_results()` for a paired regular/injected
result descriptor. Inspect the selected discriminant before reading the union;
a raw pointer must never be reinterpreted as a paired descriptor. No compatibility
shadow field or runtime layout inference is provided. Request, descriptor and
both spans retain one lifetime through acquire-observed SETTLED.

Use public domain traits from `nexus/io/traits.h` instead of generated
`nx_factory_*_traits_t` typedefs. Generic capabilities use `nx_io_capability_t`
and `NX_IO_CAP_*`; enhanced SPI endpoint traits use
`nx_spi_transaction_traits_t`, while I2C provider profiles use `nx_i2c_limits_t`.
Factory lookup still returns selected immutable facts; it does not initialize
hardware or prove readiness. SDIO function requests now contain `base`, payload
and function-local address fields directly. Replace
`request.transfer.base` and other nested accesses with the direct fields, remove
`first_block = 0`, and never cast an SDIO request to a block request. Follow the
public encoder modes, error enums and checksum semantics rather than retaining
old numeric encodings.

Migrate payload direction explicitly. Construct I2C writes with
`nx_i2c_message_write()` and reads with `nx_i2c_message_read()`; replace old
message `.data` access with the matching `buffer.tx` or `buffer.rx`. Prepare block
writes with `nx_block_write_prepare()` and reads with `nx_block_read_prepare()`.
Do not change only the direction flag to reinterpret the other union member.
Preparation does not admit or borrow a request. The selected provider's bounds
still apply; the removed `i2c_long` entry is not an alternative transfer engine.

Review consumers of EXTI sources, private UART configuration, logical CAN filter
tags, USB Host pipe IDs and ADC/DAC units. Their values cannot be recovered by
preserving old register encodings or structure sizes. The
[epoch-3 field migration table](../design/source-compatibility.md#epoch-3-的领域接口迁移)
owns these mappings, including encoder, NAND and Flash maintenance semantics.
Use the corresponding public header for complete operation and borrow contracts.

Discard old generated output and rebuild from the pinned source. The
[compatibility contract](../design/source-compatibility.md) defines epoch and
manifest admission; a matching semantic version alone does not permit mixing
source generations or binary objects. For an installed SDK, follow the
[existing-product upgrade commands](../tutorials/source-sdk.md#upgrade-an-existing-product)
with its verified prefix and exact source revision. Run the migrated product's
Native contracts and selected MCU build before accepting it. A generation check
or successful link does not establish physical qualification.

## Update private source integrations

Production ownership is one `soc/stm32/stm32f4/` package. Exact parts and modes
follow the [current support catalogue](../delivery/support.md); obsolete family
names are rejected rather than mapped to a replacement. Existing F407 assemblies
select `stm32f4` with an exact part. Public C interfaces remain the consumption
boundary; added internal-CRC references do not change product wiring.

Board and SoC trees contain no Python plugins. Current generation is centralized
under `tools/configure/stm32f4/`; firmware hardware algorithms and lifecycle
remain handwritten C. Native fixtures live under
`tests/support/native/` and require explicit test support.

Private F4 IP belongs to `soc/stm32/stm32f4/drivers/ip/`; there is no root MCU
`drivers/` directory. Update private includes, configuration inputs, CMake source
selection and SDK payload together. The [source map](../explanation/source-map.md)
and [provider guide](provider-porting.md) describe current ownership. Original
cross-family layouts remain traceable at the [fixed prior source](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18).

Resolve the actual assembly, exercise complete provider contracts and build a
relocated consumer. Source movement establishes neither new modes nor physical
qualification; no previous candidate transfers to this revision.
