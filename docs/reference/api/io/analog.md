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

## Select independent sampling or one coupled owner

Ordinary ADC/DAC, groups and observers remain independent interfaces. A control
application needing one coherent PWM/ADC schedule selects a separate coupled
face; the ordinary PWM/ADC method tables and states do not grow to hold it.

| Face / target | Selected behavior | Borrow and execution |
| --- | --- | --- |
| `pwm_adc.h` / `Nexus::IOPWMADC` | A finite window with one fixed complete duty frame and triggered raw samples | Task submission/service; request, duty and samples borrowed until acquire-observed SETTLED |
| `pwm_adc_loop.h` / `Nexus::IOPWMADCLoop` | A finite-lifetime continuous session; each complete scan invokes bounded compute and stages one complete duty group | Task prepare/start/service/stop; compute runs only in the selected ADC IRQ |
| `power_stage.h` / `Nexus::IOPowerStage` | An independent complementary-output drive session with sequential raw currents/bus and encoder observation | Finite startup deadline; nonblocking long-lived borrow until explicit stop/fault and acquire-observed SETTLED |

The current F407ZGT6 software reference selects TIM8 CH1..3, BKIN and ADC1_IN0.
`finite-regular-dma` uses TIM8 TRGO, ADC regular conversion and DMA2 Stream4;
`injected-control` uses TIM8 CH4 and injected JEOC without DMA. These alternative
modes share the same controller resources and cannot be selected together.
Neither establishes power-board wiring, complementary outputs, FOC, a measured
aperture timestamp or an exact physical PWM pulse count. See the
[coupled provider contract](../../../../soc/stm32/stm32f4/drivers/pwm_adc/README.md)
for the exact route, clocks and timing inequalities.

The IRQ session has explicit trigger, conversion, IRQ-delay, compute and commit
budgets. Its callback must return within budget; software cannot preempt it.
It cannot call task-only I/O, OS, Flash, file or network methods, nor retain
callback-local scan/output views. Session, buffers, callback and context remain
exclusively borrowed through SETTLED, even after output inhibition. A separately
owned bounded queue/mailbox can transfer targets under its own synchronization
contract; tasks must not mutate live borrowed algorithm state field by field.

Late IRQ, lost-cycle evidence, invalid duty, break or failed full-group commit
closes output admission. Stop/service must still drain sources and join IRQs;
failed withdrawal retains storage, and cold close/reinitialization establishes
the next session. Finite deadline and completed-cycle counters are software
contracts, not measured worst-case control latency or physical qualification.

## Keep the selected electric-drive profile explicit

`injected-power-stage` directly owns its selected TIM8, ADC shared domain and
TIM4 route; it does not forward through another live `pwm_adc_loop` owner. The
F407ZG software reference uses edge-aligned complementary PWM and four sequential
ADC1 injected ranks: raw phase A/B/C followed by raw bus voltage. It does not
establish simultaneous multi-ADC sampling or a real power-board connection.

`sample_at` is a scheduled trigger reference with declared `scan_span_us`, not a
measured aperture. `angle_at` and `observed_at` are software observations, and
`planned_apply_at` is the intended next update. The caller's explicit bridge
owns calibration, skew/angle interpretation, quantization and final limitation
feedback to the [motor kernel](../components/control.md). See the
[selected provider contract](../../../../soc/stm32/stm32f4/drivers/pwm_adc/README.md#电驱完整软件参考injected-power-stage)
for the complete sampling-window inequalities and exact pins.

The session's finite `startup_deadline` bounds startup. Its Core request uses
`NX_DEADLINE_NEVER` for the nonblocking running lifetime; external command
authority and cleanup attempts remain separate. Output inhibition never ends
the session/context borrow. Stop/service must withdraw sources and join actual
publishers before settlement, including after time or clock faults.

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

### pwm_adc_loop.h

```{doxygenfile} io/include/nexus/io/pwm_adc_loop.h
:project: nexus
```

### power_stage.h

```{doxygenfile} io/include/nexus/io/power_stage.h
:project: nexus
```
