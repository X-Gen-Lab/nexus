---
id: reference.api.io.usb
type: reference
audience: consumer
source_language: en
---

(reference.api.io.usb)=

# USB packets, activation and explicit roles

Device packets, Host pipes and explicitly managed OTG roles have distinct
identities. The selected controller, PHY, power and endpoint profile must be
admitted before a ready face can be used.

USB Device and Host use distinct packet faces. An explicitly managed OTG role
owns the selected core/PHY and exposes the ready face for its current
generation. The [USB ownership contract](../../contracts/io/usb-lifecycle.md) defines
PHY speeds, PIO or HS AHB DMA, power handover and optional ST Core bridges.

## Public declarations

### usb_activation.h

```{doxygenfile} io/include/nexus/io/usb_activation.h
:project: nexus
```

### usb_device.h

```{doxygenfile} io/include/nexus/io/usb_device.h
:project: nexus
```

### usb_host.h

```{doxygenfile} io/include/nexus/io/usb_host.h
:project: nexus
```

### usb_otg.h

```{doxygenfile} io/include/nexus/io/usb_otg.h
:project: nexus
```

### usb_profile.h

```{doxygenfile} io/include/nexus/io/usb_profile.h
:project: nexus
```
