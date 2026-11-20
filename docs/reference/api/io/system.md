---
id: reference.api.io.system
type: reference
audience: consumer
source_language: en
---

(reference.api.io.system)=

# Chip information, integrity and entropy faces

Read chip identity and bounded integrity or entropy results through the selected
public face. A UID is not a secret, a CRC unit is not an arbitrary hash, and
source words alone do not establish cryptographic entropy quality.

The entropy face supplies bounded, health-admitted source words.
`nx_entropy_fill` uses a finite absolute deadline and erases the complete
admitted output on a read failure. It does not allocate, create a worker or
establish entropy quality; it is not a DRBG. A source's existence does not
authorize full entropy credit for a cryptographic library. See the selected
{doc}`F407 owner </reference/api/soc/stm32f4-system>` for its typed factory,
Platform lifecycle and clock prerequisites. Other retained SDKs do not grant an
entropy provider.

## Public declarations

### chip.h

```{doxygenfile} io/include/nexus/io/chip.h
:project: nexus
```

### cipher.h

```{doxygenfile} io/include/nexus/io/cipher.h
:project: nexus
```

### crc.h

```{doxygenfile} io/include/nexus/io/crc.h
:project: nexus
```

### entropy.h

```{doxygenfile} io/include/nexus/io/entropy.h
:project: nexus
```

### hash.h

```{doxygenfile} io/include/nexus/io/hash.h
:project: nexus
```

### watchdog.h

```{doxygenfile} io/include/nexus/io/watchdog.h
:project: nexus
```
