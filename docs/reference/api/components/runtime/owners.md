---
id: reference.api.components.runtime.owners
type: reference
audience: consumer
source_language: en
---

(reference.api.components.runtime.owners)=

# Serialized owners and final-access observers

Owners serialize selected I/O operations using caller-owned bounded storage.
Final request access, owner idleness, observer withdrawal and external executor
joins are separate shutdown conditions.

## Public declarations

### bus_owner.h

```{doxygenfile} components/runtime/bus_owner/include/nexus/components/bus_owner.h
:project: nexus
```

### owner_observer.h

```{doxygenfile} components/runtime/owner_observer/include/nexus/components/owner_observer.h
:project: nexus
```

### spi_async_owner.h

```{doxygenfile} components/runtime/bus_owner/include/nexus/components/spi_async_owner.h
:project: nexus
```

### spi_owner.h

```{doxygenfile} components/runtime/bus_owner/include/nexus/components/spi_owner.h
:project: nexus
```

### uart_owner.h

```{doxygenfile} components/runtime/bus_owner/include/nexus/components/uart_owner.h
:project: nexus
```

### spi_dma_owner.h

```{doxygenfile} components/runtime/bus_owner/include/nexus/components/spi_dma_owner.h
:project: nexus
```
