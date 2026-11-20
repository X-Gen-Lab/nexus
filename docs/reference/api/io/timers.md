---
id: reference.api.io.timers
type: reference
audience: consumer
source_language: en
---

(reference.api.io.timers)=

# Timers, PWM, capture and encoders

These interfaces describe timed outputs, captured observations and encoder
counts. The selected provider defines controller/channel combinations, clock and
synchronization limits; a public declaration does not authorize every timer
mode.

## Public declarations

### advanced_pwm.h

```{doxygenfile} io/include/nexus/io/advanced_pwm.h
:project: nexus
```

### encoder.h

```{doxygenfile} io/include/nexus/io/encoder.h
:project: nexus
```

### pwm_group.h

```{doxygenfile} io/include/nexus/io/pwm_group.h
:project: nexus
```

### timer.h

```{doxygenfile} io/include/nexus/io/timer.h
:project: nexus
```

### timer_capture.h

```{doxygenfile} io/include/nexus/io/timer_capture.h
:project: nexus
```

### timer_pulse.h

```{doxygenfile} io/include/nexus/io/timer_pulse.h
:project: nexus
```

### timer_sequence.h

```{doxygenfile} io/include/nexus/io/timer_sequence.h
:project: nexus
```
