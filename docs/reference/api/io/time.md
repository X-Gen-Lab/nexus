---
id: reference.api.io.time
type: reference
audience: consumer
source_language: en
---

(reference.api.io.time)=

# RTC, low-power counters and time sources

Calendar operations, finite low-power counters and monotonic deadline clocks
have separate identities. Read the selected provider contract for continuity,
backup-domain access and final source withdrawal.

## Public declarations

### lptim.h

```{doxygenfile} io/include/nexus/io/lptim.h
:project: nexus
```

### rtc.h

```{doxygenfile} io/include/nexus/io/rtc.h
:project: nexus
```

### rtc_aux.h

```{doxygenfile} io/include/nexus/io/rtc_aux.h
:project: nexus
```
