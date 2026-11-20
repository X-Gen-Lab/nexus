---
id: reference.api.io.execution
type: reference
audience: consumer
source_language: en
---

(reference.api.io.execution)=

# Borrowing, streams, progress and observations

Accepted requests and buffers remain borrowed until acquire-observed SETTLED.
Cancellation, deadline expiry and DMA completion do not revoke a borrow.
Applications own progress, waiting and joined shutdown; diagnostics report
observations rather than creating a worker.

A valid request snapshot has a nonzero epoch and finite `observed_at`; its
`deadline` may be `NX_DEADLINE_NEVER` to mean no deadline. SERVICE facts require
finite `last_service_at` and `observed_at` in the same monotonic domain, and a
finite positive `maximum_service_gap_us`. Unknown groups stay unknown.

Selected readonly facts use stable public domain types. Optional factory lookups
return concrete records; manually supplied providers can supply the same types.
Read these constraints when binding a component, then retain the selected
provider's normal admission checks. A record does not establish readiness,
resource ownership, measured timing or physical qualification. Unknown bounds
never mean unlimited support. Generic capabilities use `nx_io_capability_t` and
`NX_IO_CAP_*`; enhanced SPI endpoints use transaction traits, while I2C uses
explicit `nx_i2c_limits_t` provider profiles.

## Public declarations

### cycle_probe.h

```{doxygenfile} io/include/nexus/io/cycle_probe.h
:project: nexus
```

### diagnostic.h

```{doxygenfile} io/include/nexus/io/diagnostic.h
:project: nexus
```

### dma.h

```{doxygenfile} io/include/nexus/io/dma.h
:project: nexus
```

### frame_stream.h

```{doxygenfile} io/include/nexus/io/frame_stream.h
:project: nexus
```

### fwd.h

```{doxygenfile} io/include/nexus/io/fwd.h
:project: nexus
```

### observation.h

```{doxygenfile} io/include/nexus/io/observation.h
:project: nexus
```

### stream.h

```{doxygenfile} io/include/nexus/io/stream.h
:project: nexus
```

### stream_budget.h

```{doxygenfile} io/include/nexus/io/stream_budget.h
:project: nexus
```

### stream_profile.h

```{doxygenfile} io/include/nexus/io/stream_profile.h
:project: nexus
```

### wake.h

```{doxygenfile} io/include/nexus/io/wake.h
:project: nexus
```

### traits.h

```{doxygenfile} io/include/nexus/io/traits.h
:project: nexus
```
