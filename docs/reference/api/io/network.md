---
id: reference.api.io.network
type: reference
audience: consumer
source_language: en
---

(reference.api.io.network)=

# CAN, Ethernet frames and MDIO

These interfaces expose bounded bus frames, MAC observations and PHY register
access. They do not create a protocol stack or establish Board wiring, PHY
timing or physical network performance.

## Public declarations

### can.h

```{doxygenfile} io/include/nexus/io/can.h
:project: nexus
```

### can_fd.h

```{doxygenfile} io/include/nexus/io/can_fd.h
:project: nexus
```

### ethernet.h

```{doxygenfile} io/include/nexus/io/ethernet.h
:project: nexus
```

### ethernet_frames.h

```{doxygenfile} io/include/nexus/io/ethernet_frames.h
:project: nexus
```

### mdio.h

```{doxygenfile} io/include/nexus/io/mdio.h
:project: nexus
```
