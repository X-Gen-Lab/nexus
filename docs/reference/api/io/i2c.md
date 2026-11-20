---
id: reference.api.io.i2c
type: reference
audience: consumer
source_language: en
---

(reference.api.io.i2c)=

# I2C messages, target exchanges and bus recovery

The endpoint owns its selected address and provider limits. Message
construction, request preparation and hardware admission are separate
operations; accepted descriptors and payloads remain borrowed until
acquire-observed SETTLED.

I2C master descriptors use `nx_i2c_message_write()` for immutable TX and
`nx_i2c_message_read()` for writable RX, without adding a second payload
pointer. Construction performs no admission, allocation or payload access;
prepare and provider admission still validate spans, limits and ownership.
Rebuild a descriptor to change direction instead of changing `read` alone; the
selected union member must match its direction. Accepted descriptors and
payloads remain borrowed until acquire-observed SETTLED.

I2C request preparation validates message grammar and storage; selected
providers apply explicit `nx_i2c_limits_t` admission budgets. The maintained
bounded profile remains eight messages of at most 256 bytes each, with a
2048-byte total. A legal descriptor is not permission to exceed that profile.
There is no separate long-transfer engine, implicit splitting or mode fallback.

I2C target exchanges use `i2c_target.h` and borrow complete bounded requests.
WRITE_READ supplies a precomputed response across one repeated START. The [F407
target contract](../../../../soc/stm32/stm32f4/drivers/i2c/README-target.md) defines own addresses,
optional PEC/ARA, IRQ latency and full drain. Product command tables, ARP and
Host Notify remain external.

## Public declarations

### i2c.h

```{doxygenfile} io/include/nexus/io/i2c.h
:project: nexus
```

### i2c_bus_clear.h

```{doxygenfile} io/include/nexus/io/i2c_bus_clear.h
:project: nexus
```

### i2c_target.h

```{doxygenfile} io/include/nexus/io/i2c_target.h
:project: nexus
```
