---
id: reference.api.components.protocols.usb
type: reference
audience: consumer
source_language: en
---

(reference.api.components.protocols.usb)=

# USB control and CDC composition

These helpers compose explicit control requests, descriptors, CDC traffic and
role changes over selected USB faces. They do not discover hardware, authorize
VBUS wiring or create a scheduler.

## Public declarations

### usb_acm.h

```{doxygenfile} components/protocols/usb_control/include/nexus/components/usb_acm.h
:project: nexus
```

### usb_control.h

```{doxygenfile} components/protocols/usb_control/include/nexus/components/usb_control.h
:project: nexus
```

### usb_descriptors.h

```{doxygenfile} components/protocols/usb_control/include/nexus/components/usb_descriptors.h
:project: nexus
```

### usb_role.h

```{doxygenfile} components/protocols/usb_control/include/nexus/components/usb_role.h
:project: nexus
```
