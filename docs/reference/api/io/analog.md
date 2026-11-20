---
id: reference.api.io.analog
type: reference
audience: consumer
source_language: en
---

(reference.api.io.analog)=

# ADC, DAC and synchronized sampling

Select the numeric format, conversion or output geometry, trigger and borrowing
contract before starting a sample operation. The provider retains responsibility
for actual DMA, trigger and peripheral withdrawal.

## Public declarations

### dac.h

```{doxygenfile} io/include/nexus/io/dac.h
:project: nexus
```

### dac_tx.h

```{doxygenfile} io/include/nexus/io/dac_tx.h
:project: nexus
```

### adc.h

```{doxygenfile} io/include/nexus/io/adc.h
:project: nexus
```

### adc_format.h

```{doxygenfile} io/include/nexus/io/adc_format.h
:project: nexus
```

### adc_group.h

```{doxygenfile} io/include/nexus/io/adc_group.h
:project: nexus
```

### adc_observer.h

```{doxygenfile} io/include/nexus/io/adc_observer.h
:project: nexus
```

### pwm_adc.h

```{doxygenfile} io/include/nexus/io/pwm_adc.h
:project: nexus
```
