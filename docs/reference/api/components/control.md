---
id: reference.api.components.control
type: reference
audience: consumer
source_language: en
---

(reference.api.components.control)=

# Explicit motor and axis control

Select `Nexus::MotorControl` for the independent C11 current-control kernel,
`Nexus::AxisControl` for command admission and rotary axis control, and
`Nexus::DriveControl` for bounded reference and continuous-feedback exchange.
`Nexus::CurrentControl` converts qualified captures; `Nexus::ModulationControl`
jointly plans switching and sampling. Each caller owns its state, parameters,
execution context and time facts. These components
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

`nx_motor_voltage_step` produces a current-control voltage decision without
choosing PWM or sampling. A selected modulator then plans both and returns the
final quantized voltage for tracking. `nx_motor_step` retains its fixed
min/max common-mode modulation as a separate maintained entry point; it does not
prove that a chosen hardware sampling window is valid.

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

## Fast references and continuous feedback

One Task publishes complete finite references; one fast executor adopts at most
one record per step. A full reference queue rejects without overwriting or
refreshing the original receipt. The Drive parameter revision identifies its
stopped Motor profile, independently of an Axis parameter revision. An inhibit
word bypasses reference congestion, but hardware protection and withdrawal
remain the provider's responsibility.

Feedback decimation retains real measurement intervals and consecutive fast
coverage. Its queue latches overflow or a coverage gap; consuming old records
does not clear that fault. Droppable telemetry requires a separate object.
`accepted_iq_a` is an adopted reference associated with a generation, not a
measured current. Match that generation to its original Axis decision before
tracking a downstream limit; delayed feedback cannot track a newer decision.

The optional common-bus calculation applies separate finite motoring and
regeneration budgets. It does not assume credit from opposing axes or provide
power protection. The product supplies qualified power requests, applies the
resulting limits and completes the mechanical stopping policy.

```{doxygenfile} components/control/drive/include/nexus/control/drive.h
:project: nexus
```

## Current provenance and reconstruction

Current conversion matches the selected calibration identity and revision,
rejects saturated codes and bounds aperture skew. Three independent phase
measurements can use their sum as a consistency check; reconstructing a third
phase from two measurements cannot establish that independent evidence.
DC-link reconstruction requires two distinct observable active-vector phases
from the same accepted switching plan. These checks neither prove zero current
nor commission or persist calibration.

Explicit timing uses actual aperture positions; bounded timing retains a
qualified group reference and positive skew bound without reading fabricated
per-phase apertures. Scheduled provider trigger coordinates remain scheduled
facts. A two-trigger DC capture spans both observations and the last conversion,
independently of any separately qualified DC-bus voltage source.

```{doxygenfile} components/control/current/include/nexus/control/current.h
:project: nexus
```

## Joint pulse and capture planning

The modulation result contains full-carrier pulse widths, pulse starts,
selected capture windows and the final ideal quantized alpha/beta voltage.
Analog recovery, sample-and-hold, ADC occupation and CPU submission budgets have
separate meanings. A missing observable window returns an error without
publishing a partial plan.

The selected provider also supplies a capture slot. Trigger, result occupation
and rearm must remain inside it; a mathematically observable half-carrier is not
usable when the hardware profile opens only the other half.

The pure calculation supports selected edge/center alignment and shunt
topologies. A provider must explicitly map the whole result or reject it. In
particular, pulse starts and a single-shunt phase shift cannot be discarded by
a duty-only mapper. A software plan does not qualify a corresponding MCU mode,
PCB, actual aperture, voltage or WCET.

The selected F4 DC-link mapper uses unshifted edge-aligned pulses. Its initial
plan must contain two independent active-vector observations; neutral equal
duties do not qualify. DC descriptors reside in the complete declared session
context and are copied after validation, never retained from callback stack
storage. A degenerate zero-voltage request without an observable window returns
`UNSUPPORTED`; unsupported fixed shifts are not silently mapped or replaced by
unrequested excitation.

```{doxygenfile} components/control/modulation/include/nexus/control/modulation.h
:project: nexus
```
