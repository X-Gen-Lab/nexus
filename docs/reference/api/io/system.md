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

## Sign an existing digest without exporting the key

`Nexus::IOP256Signer` exposes an existing selected signing-key face. The caller
supplies exactly 32 SHA256 digest bytes; the synchronous task-only method
returns raw P256 `R || S`, 64 bytes with each scalar big-endian. Public-key
output is 64-byte big-endian `X || Y`, without an `04` tag. No implicit hashing,
DER encoding, low-S normalization, certificate or authorization policy is added.

The face, operations and private context remain live through serialized calls
and joined provider stop. Inputs/outputs are disjoint from each other and
control storage. Every return releases MCU buffer access; failures leave the
complete public output unchanged. A finite provider-clock deadline does not
preempt device computation. Key identity, provisioning, physical recovery and
trust remain the product's responsibility. The selected
[ATECC608B adapter](../components/devices.md) verifies a specific already-locked
device/slot; its existence does not establish an opaque-key TLS integration.

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

### p256_signer.h

```{doxygenfile} io/include/nexus/io/p256_signer.h
:project: nexus
```

### watchdog.h

```{doxygenfile} io/include/nexus/io/watchdog.h
:project: nexus
```
