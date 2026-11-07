---
id: reference.api.extension-components
type: reference
audience: consumer
source_language: en
---

# Extension and lifecycle APIs

These public headers describe framed data, explicit owners, retained clocks and trusted mediation. Their declarations do not establish a board connection, execution mode, kernel gateway or physical qualification. Construction, memory ownership and final-access requirements remain part of each contract.

(stm32f407-entropy-construction)=

## Explicit F407 entropy construction

For an F407 assembly, explicitly call `nexus_add_stm32f407_entropy(target)` and
link that target at the product's composition site. It owns the unique static
RNG source and refuses a second constructor. The getter
`nx_stm32f407_entropy_instance()` returns an existing readonly face; it performs
no startup, register access or factory registration.

After successful Platform startup, call
`nx_stm32f407_entropy_instance_start()` with exact validated Board/profile
`hse_hz` and `hclk_hz` facts. The implementation checks the live PLL48/HCLK
relationship and acquires only its RNG clock gate; it neither configures the
shared PLL nor starts Platform. The reviewed source choices are 8 MHz HSE or
16 MHz HSI. The product must preserve those clock dependencies through stop.

Run start/stop from one privileged, unmasked, serialized task context. Join every
entropy consumer before `nx_stm32f407_entropy_instance_stop()`, and require that
stop to succeed before withdrawing Platform clocks. Failed withdrawal retains
the same owner and cleanup responsibility; retry stop without resetting or
rebinding its state. A stopped face remains valid and rejects data calls with
STATE. This explicit constructor is separate from TOML typed-factory selection;
neither it nor software RNG health checks qualify physical min-entropy or HIL.

(stm32f407__entropy_8h)=

### stm32f407_entropy.h

```{doxygenfile} soc/stm32/stm32f4/include/nexus/soc/stm32f407_entropy.h
:project: nexus
```

(frame__stream_8h)=

## frame_stream.h

```{doxygenfile} frame_stream.h
:project: nexus
```

(spi__dma__owner_8h)=

## spi_dma_owner.h

```{doxygenfile} spi_dma_owner.h
:project: nexus
```

(mapped__storage_8h)=

## mapped_storage.h

```{doxygenfile} mapped_storage.h
:project: nexus
```

(mapped__storage__owner_8h)=

## mapped_storage_owner.h

```{doxygenfile} mapped_storage_owner.h
:project: nexus
```

(native__mapped__storage_8h)=

## native_mapped_storage.h

```{doxygenfile} native_mapped_storage.h
:project: nexus
```

(amp__ipc_8h)=

## amp_ipc.h

```{doxygenfile} amp_ipc.h
:project: nexus
```

(amp__ipc__storage_8h)=

## amp_ipc_storage.h

```{doxygenfile} amp_ipc_storage.h
:project: nexus
```

(stm32f407__handoff_8h)=

## stm32f407_handoff.h

```{doxygenfile} stm32f407_handoff.h
:project: nexus
```

(user__io__async_8h)=

## user_io_async.h

```{doxygenfile} user_io_async.h
:project: nexus
```

(user__io__async__storage_8h)=

## user_io_async_storage.h

```{doxygenfile} user_io_async_storage.h
:project: nexus
```

(i3c__controller_8h)=

## i3c_controller.h

```{doxygenfile} i3c_controller.h
:project: nexus
```

(i3c__controller__storage_8h)=

## i3c_controller_storage.h

```{doxygenfile} i3c_controller_storage.h
:project: nexus
```

(f407__i2s2__slave__tx_8h)=

## f407_i2s2_slave_tx.h

```{doxygenfile} f407_i2s2_slave_tx.h
:project: nexus
```

(mcuboot-control-contract)=

## MCUboot control and explicit Flash composition

`Nexus::MCUBootControl` exposes the stateless public control API below. It uses
only Nexus types. `Nexus::MCUBootPort` is the separate, concrete Flash
composition boundary; `Nexus::MCUBootBootStorage` belongs to a separately linked
boot image. These targets are outside the default platform assembly.

Use the [canonical MCUboot contract](../../../components/mcuboot/README.md) for
helper selection, the boot/application ownership diagram, finite operation and
Flash-region lifetime, partial persistent writes and product recovery duties.
The selected overwrite-only profile supports permanent requests; the control
API does not authenticate a candidate, reset hardware or provide rollback.
Software models and linked-image budgets do not establish physical Flash,
power-cut or HIL qualification.

(mcuboot__control_8h)=

### mcuboot_control.h

```{doxygenfile} mcuboot_control.h
:project: nexus
```
