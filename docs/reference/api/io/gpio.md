---
id: reference.api.io.gpio
type: reference
audience: consumer
source_language: en
---

(reference.api.io.gpio)=

# GPIO, pin state and external events

Use the selected GPIO and EXTI faces for authorized pin access and copied event
observations. The selected Board owns wiring and electrical authorization; a
sampled level or IRQ timestamp retains its declared observation quality.

## Public declarations

### exti.h

```{doxygenfile} io/include/nexus/io/exti.h
:project: nexus
```

### gpio.h

```{doxygenfile} io/include/nexus/io/gpio.h
:project: nexus
```

### pin_state.h

```{doxygenfile} io/include/nexus/io/pin_state.h
:project: nexus
```
