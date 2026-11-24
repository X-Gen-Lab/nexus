---
id: reference.api.components.control
type: reference
audience: consumer
source_language: en
---

(reference.api.components.control)=

# Explicit motor and axis control

Select `Nexus::MotorControl` for the independent C11 current-control kernel and
`Nexus::AxisControl` for command admission and rotary axis control. Each caller
owns its state, parameters, execution context and time facts. These components
do not allocate, schedule work, read a clock or initialize a peripheral.

Current, voltage, mechanical angle and velocity use explicitly named SI units.
Motor pole pairs, encoder scaling and mechanical transmission are different
facts. Keep parameter publication separate from live IRQ computation; ordinary
structure copies do not provide cross-context synchronization.

An axis decision adopts a software reference. A motor result is an output plan.
Neither establishes hardware adoption, physical position or a safe electrical
state. The selected provider owns actual PWM/ADC transitions and final request,
buffer and callback withdrawal. Failed withdrawal retains that ownership even
after outputs have been inhibited.

The [control component boundary](../../../../components/control/README.md) and
[control composition guide](../../../guides/control-and-smart-devices.md) explain
how external products provide scheduling, hardware bindings and recovery.

## Preserve explicit control and stopping policy

Axis commands retain the original local receipt, complete identity and finite
authority. Repeated/old sequences do not renew it; accepting a command does not
adopt it until a later valid step. Feedback carries its mechanical side and
observation interval. Position trajectory completion and measured target settling
remain separate results.

`NX_AXIS_INHIBIT_AFTER_DWELL` ends the selected software decision after observed
zero-speed dwell. `NX_AXIS_HOLD_ZERO_VELOCITY` enters `NX_AXIS_HOLDING` and keeps
the zero-velocity PI running while refusing new motion. Holding is not proof of
load support, fixed position or mechanical safety. The product first completes
its mechanical handoff, then disables and separately withdraws actual hardware.
Invalid feedback cannot be repaired by retaining a previous holding output.

Motor computation uses calibrated A/V/rad/rad/s with a shared microsecond domain
and 64-bit epoch. It distinguishes the declared sample event, angle observation,
control observation and planned apply boundary. Backward angle projection and
output prediction have separate bounded spans. A sequential ADC bridge must
qualify its sampling skew instead of claiming simultaneous measurements.

The complete duty plan and `planned_voltage` are not a measured voltage receipt.
`nx_motor_track_voltage` feeds final quantization/window limitation back once for
the matching latest cycle. A hardware commit error must close kernel admission
and follow the real provider's withdrawal; resetting a C object cannot join IRQs.

## Motor computation

```{doxygenfile} components/control/motor/include/nexus/control/motor.h
:project: nexus
```

## Axis commands and execution

```{doxygenfile} components/control/axis/include/nexus/control/axis.h
:project: nexus
```
