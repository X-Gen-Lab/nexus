---
id: reference.api.io.storage
type: reference
audience: consumer
source_language: en
---

(reference.api.io.storage)=

# Block media, Flash and memory access

Block sessions, SDIO functions, Flash maintenance and memory-window access are
distinct capabilities. Products own partitions, media policy, recovery and
permanent startup reservations.

Media activation, function selection, memory-window leases and permanent startup
backing are distinct operations. A block session does not turn an SDIO function
into sector media. A CPU-window lease grants only its declared bank access;
startup backing remains held while its static objects exist. See [physical
storage](../../contracts/io/f4-flash-storage.md) and
[FSMC](../../contracts/io/f4-external-memory.md) before selecting those interfaces.

## Public declarations

### block.h

```{doxygenfile} io/include/nexus/io/block.h
:project: nexus
```

### block_async.h

```{doxygenfile} io/include/nexus/io/block_async.h
:project: nexus
```

### external_bus.h

```{doxygenfile} io/include/nexus/io/external_bus.h
:project: nexus
```

### parallel16.h

```{doxygenfile} io/include/nexus/io/parallel16.h
:project: nexus
```

### memory_window.h

```{doxygenfile} io/include/nexus/io/memory_window.h
:project: nexus
```

### flash.h

```{doxygenfile} io/include/nexus/io/flash.h
:project: nexus
```

### flash_async.h

```{doxygenfile} io/include/nexus/io/flash_async.h
:project: nexus
```

### flash_maintenance.h

```{doxygenfile} io/include/nexus/io/flash_maintenance.h
:project: nexus
```

### nand.h

```{doxygenfile} io/include/nexus/io/nand.h
:project: nexus
```

### sdio_io.h

```{doxygenfile} io/include/nexus/io/sdio_io.h
:project: nexus
```
