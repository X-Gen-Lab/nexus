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

Inspect this maintained Native assembly; it is complete, not a fragment:

```{literalinclude} ../../tools/configure/assemblies/native-uart.toml
:language: toml
:caption: Current schema-2 Native UART input.
```

From the repository root, execute the actual parser and generation path:

```sh
python tools/configure/configure.py check     --assembly tools/configure/assemblies/native-uart.toml
python tools/configure/configure.py generate     --assembly tools/configure/assemblies/native-uart.toml     --output /tmp/nexus-migration-config
```

Use a new output path you own. Check `resolved.json` and generated headers; never
author the resolved output or reuse stale generated files after a failure.
Legacy schema-1 JSON exists only for explicit migration fixtures. A failed TOML
does not fall back to JSON, Kconfig, old headers or cache values.

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
