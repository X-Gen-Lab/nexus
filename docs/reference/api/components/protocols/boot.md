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
The selected overwrite-only profile supports permanent requests; the control API
does not authenticate a candidate, reset hardware or provide rollback. Software
models and linked-image budgets do not establish physical Flash, power-cut or
HIL qualification.

## Public declarations

### mcuboot_control.h

```{doxygenfile} components/protocols/mcuboot/include/nexus/components/mcuboot_control.h
:project: nexus
```
