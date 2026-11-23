---
id: reference.api.components.protocols.boot
type: reference
audience: consumer
source_language: en
---

(reference.api.components.protocols.boot)=

# MCUboot control and Flash composition

The stateless control API and concrete Flash composition have separate
responsibilities. Products own authentication policy, image regions, reset and
recovery decisions.

## MCUboot control and explicit Flash composition

`Nexus::MCUBootControl` exposes the stateless public control API below. It uses
only Nexus types. `Nexus::MCUBootPort` is the separate, concrete Flash
composition boundary; `Nexus::MCUBootBootStorage` belongs to a separately linked
boot image. These targets are outside the default platform assembly.

Use the [canonical MCUboot
contract](../../../../../components/protocols/mcuboot/README.md) for helper
selection, the boot/application ownership diagram, finite operation and
Flash-region lifetime, partial persistent writes and product recovery duties.
The overwrite-only profile supports permanent requests and has no rollback.
The separate [scratch-swap profile](../../../../../components/protocols/mcuboot/swap/README.md)
adds TEST, explicit confirmation and unconfirmed revert using real upstream
bootutil. Neither control API authenticates a candidate or resets hardware;
the separately linked boot image owns verification and exchange. The product
owns the decision to accept, execute and confirm an image.

## Select one profile per final image

| Profile | Application targets | Boot target | Persistent geometry |
| --- | --- | --- | --- |
| Overwrite-only | `Nexus::MCUBootControl`, `Nexus::MCUBootPort` | `Nexus::MCUBootBootStorage` | Explicit primary/secondary; permanent request only |
| Scratch swap | `Nexus::MCUBootSwapControl`, `Nexus::MCUBootSwapPort` | `Nexus::MCUBootSwapBootStorage` | Explicit primary/secondary/scratch; TEST/PERMANENT, confirm and revert |

Use `nexus_add_mcuboot_swap_control()` for application control and
`nexus_add_mcuboot_swap_boot()` for the independent boot profile. CMake rejects
mixing overwrite and swap in one final consumer; separate executables may select
different profiles in the same build graph. Public types and operation scopes
remain separate so an overwrite consumer does not acquire scratch storage.

Every Flash operation uses the caller's stable explicit region/clock/deadline
scope through the entire call. A failed request or confirmation may already
have persistent effects; do not retry blindly, extend the old deadline or infer
rollback from an error. A TEST request is not automatic confirmation. Confirm
only after the product validates the actual running image and its health.
On F407, single-bank program/erase can stall instruction fetch and IRQ service;
the operation deadline cannot preempt a physical programming pulse.

The reviewed F407ZG swap reference reserves 128 KiB boot, two 384 KiB image
slots and 128 KiB scratch in 1 MiB internal Flash. Its shared layout file owns
linker and region geometry; do not copy those addresses to another part or
existing product layout. Exact model interruption and ARM-linked image budgets
do not establish physical Flash, endurance, power-cut or HIL qualification.

## Public declarations

### mcuboot_control.h

```{doxygenfile} components/protocols/mcuboot/include/nexus/components/mcuboot_control.h
:project: nexus
```

### mcuboot_swap_control.h

```{doxygenfile} components/protocols/mcuboot/include/nexus/components/mcuboot_swap_control.h
:project: nexus
```
