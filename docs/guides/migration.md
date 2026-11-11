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
| HAL/OSAL implementation pointers in products | Include public Nexus APIs and generated `nexus_factory.h` |
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

From the repository root, execute the actual parser and generation path:

```sh
python tools/configure/configure.py check     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
python tools/configure/configure.py generate     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml     --output /tmp/nexus-migration-config
```

Use a new output path you own. Check `resolved.json` and generated headers; never
author the resolved output or reuse stale generated files after a failure.
Legacy schema-1 JSON is no longer a platform input. Convert it to a complete
schema-2 TOML assembly; generated `resolved.json` remains audit output only.
A failed TOML does not fall back to JSON, Kconfig, old headers or cache values.
Frozen historical evidence remains verifiable under its original format; this
read-only retention does not reopen old configuration or generation paths.

## Move access and lifecycle together

Use compact typed IDs from the current assembly and check getter NULL results.
Getter names and ID constants are generated from authored instance names; IDs are
not persistent IDs or a binary ABI. Optional nominal strong IDs and capability
records require their explicit configuration. Start hardware through
`nx_platform_start()` and inspect both primary and cleanup results.
After successful startup, ordinary I/O uses the returned face directly.

Convert synchronous and asynchronous calls according to their actual mode.
For accepted requests, migrate storage lifetime, service/wait progress, cancellation,
settlement and shutdown together. [The async tutorial](../tutorials/async-io.md)
executes these failure paths; replacing a function spelling alone is insufficient.

Build an independent relocated [SDK consumer](../tutorials/source-sdk.md), then
link the exact MCU configuration with the locked compiler. Retain evidence for
the migrated source and configuration. Old HAL/OSAL documentation remains historical;
a redirect does not make an old API contract equivalent to a new one.

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
