---
id: reference.api.extension-components
type: reference
audience: consumer
source_language: en
---

# Extension and lifecycle APIs

These public headers describe framed data, explicit owners, retained clocks and trusted mediation. Their declarations do not establish a board connection, execution mode, kernel gateway or physical qualification. Construction, memory ownership and final-access requirements remain part of each contract.

(stm32f407-entropy-construction)=
(stm32f407__entropy_8h)=

## Selected F407 entropy owner

Select the chip's one internal RNG through the authored assembly:

<!-- nexus-toml-fragment: Entropy selection only; the enclosing assembly supplies Board, backend, clock and memory. -->
```toml
[entropy.random]
binding = "rng"
mode = "rng-words"
```


The Board supplies the pinless `rng` binding. Configuration requires the selected
nominal PLL48 domain to be exactly 48 MHz, including an explicit `pll48 = true`
reservation when SYSCLK uses a direct input. The generated
`nx_factory_entropy(NX_ENTROPY_ID_RANDOM)` returns an existing readonly
{ref}`entropy face <entropy_8h>` without starting hardware. No separate CMake
constructor, public mutable owner or second clock configuration is used.

Readonly admission rejects foreign enabled RNG or IRQ state before System clock
effects. Platform prepares the same private RNG owner after System startup, withdraws it
before clock shutdown or suspend, and prepares it again in the new epoch on
resume. The existing C implementation checks the actual RCC/Flash plan and
retains its acquired gate and enable effects through cleanup. Join every entropy,
TLS and network consumer before serialized privileged Platform suspend or stop.
Failed withdrawal retains that owner and cleanup responsibility; retry the
lifecycle operation without resetting its state. A cold or stopped face remains
valid and rejects data calls with STATE. Clock failure or an epoch change stops
data publication while cleanup remains available.

The [System contract](../../../soc/stm32/stm32f4/README-system-power.md) defines
clock authority and recovery. RNG health checks and software models establish
neither physical min-entropy nor HIL qualification.


(usb__role_8h)=

## usb_role.h

```{doxygenfile} usb_role.h
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
