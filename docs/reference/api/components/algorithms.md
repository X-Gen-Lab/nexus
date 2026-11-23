---
id: reference.api.components.algorithms
type: reference
audience: consumer
source_language: en
---

(reference.api.components.algorithms)=

# Explicit-state numeric algorithms

Select `Nexus::Algorithms` for the portable C11 scalar profile. The caller
provides every mutable history, coefficient array and workspace, and serializes
updates with reset and teardown. Algorithms perform no device I/O, clock read,
allocation or scheduling. The [leaf contract](../../../../components/algorithms/README.md)
owns the selected conventions; public declarations below own each operation.

| Need | Header under `nexus/algorithms/` | Selection boundary |
| --- | --- | --- |
| Saturating Q1.15/Q1.31 arithmetic | `numeric.h` | Explicit scaling; nearest rounding with ties away from zero |
| Checksums and byte representation | `crc.h`, `endian.h` | Declared CRC model and wire byte order; CRC is not authentication |
| Streaming statistics | `statistics.h` | Caller chooses population/sample interpretation and sample quality |
| Small filters | `filter.h` | EMA, moving mean and bounded odd-window median |
| Portable FIR/SOS | `dsp.h` | Current-sample-first FIR coefficients; conventional SOS denominator signs |
| Calibration tables | `interpolation.h` | One-dimensional linear interpolation with an explicit boundary policy |
| Scalar calibration | `calibration.h` | Explicit offset and scale; calibration identity and activation remain external |
| Scalar control and PI/PID | `control.h`, `pid.h` | Explicit elapsed seconds, limits and independently owned controller state |
| Modulo feedback and motion targets | `motion.h` | Bounded count displacement and preplanned rest-to-rest trajectories |
| Calibrated vectors and relative attitude | `attitude.h` | Body-to-world Hamilton quaternions and an explicit six-axis proportional estimator |

The product supplies physical units, sampling continuity, coefficient design and
the response to missed samples. PID gains use continuous-time Ki/Kd; do not
apply the sample interval twice. Anti-windup and explicit actual-output feedback
do not establish stability of a physical plant. Numeric rejection is not an
actuator shutdown command. Preserve IEEE nonfinite checks when compiling this
profile; fast-math assumptions cannot replace its input contract.

Portable SOS computes into separate scratch and commits history only after a
successful step. Scratch may change on failure. Exact input/output separation,
startup history and reset conditions remain those of the selected header; a
generic reset must not race a live caller.

Motion tracking requires a product-proven displacement bound between samples.
It cannot reconstruct missed full counter wraps. The first sample establishes
the raw reference without claiming mechanical zero or valid velocity. After a
time discontinuity, reinitialize the tracker explicitly. Calibration and target
parameters must come from one consistently published version.

Trajectory planning runs in the caller's planning context and may use double
precision and square root. Evaluation uses binary32 and explicit elapsed
seconds. Reaching the commanded endpoint does not prove measured position,
output withdrawal or physical stopping. The selected hardware provider defines
which feedback and output methods can run in an interrupt.

The six-axis attitude estimator consumes calibrated angular rate in rad/s,
specific force in m/s² and an explicit continuous `dt_s`. Mounting rotation is
separate from per-axis calibration. An acceleration norm outside the selected
window skips gravity correction and reports that fact. Without an external
heading reference, yaw remains relative and drifts; sustained acceleration can
also bias tilt. These bounded binary32 operations use transcendental functions;
the caller must budget their actual execution/FPU context. They do not establish
sample pairing, an aperture timestamp or physical estimator accuracy.

## Conditional CMSIS-DSP backend

Explicit `nexus_add_cmsis_dsp()` selection creates the locked upstream
`Nexus::CMSISDSP` backend and the Nexus-facing `Nexus::AlgorithmsCMSISDSP`
adapter. Ordinary Algorithms does not select that dependency. The reviewed
[f32 profile](../../../../components/algorithms/cmsis_dsp/README.md) includes
dot, scalar FIR, DF1 biquad and fixed RFFT1024, with caller-provided history and
FFT workspace. Vendor headers remain private.

CMSIS FIR coefficient order and biquad feedback signs differ from the portable
profile. Do not reuse those arrays without conversion. CMSIS block processing
does not inherit portable SOS history rollback. RFFT output is packed as
`[DC, Nyquist, Re(1), Im(1), ...]`, with no forward normalization; the caller
owns sample rate, window and spectral units. MCU cycle budgets and physical
sampling/control qualification remain `not_executed` unless their exact target
and hardware are separately exercised.

## Conditional CMSIS-NN backend

Explicit `nexus_add_cmsis_nn()` creates `Nexus::AlgorithmsCMSISNN` over the locked
CMSIS-NN signed-int8 fully-connected kernels. Ordinary Algorithms does not
select it. The [selected profile](../../../../components/algorithms/cmsis_nn/README.md)
is one batch, 1..1024 inputs and 1..256 outputs, output-major weights, optional
accumulator-unit bias, positive Q31 multiplier and shift -30..0. Shapes and a
conservative accumulator bound are checked before output access.

The Native/M4 profile requires no heap, retained state or workspace. Input,
weights, bias and configuration remain immutable during the synchronous call;
the entire advertised output span is disjoint from those inputs. Work is
bounded by the selected shape, not preempted by a deadline. The product owns
feature extraction, scaling, scheduling and model validity. A fixed numerical
graph is not a trained classifier or an actuator command policy; see the
[control and smart-device guide](../../../guides/control-and-smart-devices.md).

## Public declarations

### numeric.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/numeric.h
:project: nexus
```

### crc.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/crc.h
:project: nexus
```

### endian.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/endian.h
:project: nexus
```

### statistics.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/statistics.h
:project: nexus
```

### filter.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/filter.h
:project: nexus
```

### interpolation.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/interpolation.h
:project: nexus
```

### control.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/control.h
:project: nexus
```

### calibration.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/calibration.h
:project: nexus
```

### motion.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/motion.h
:project: nexus
```

### attitude.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/attitude.h
:project: nexus
```

### pid.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/pid.h
:project: nexus
```

### dsp.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/dsp.h
:project: nexus
```

### cmsis_dsp.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/cmsis_dsp.h
:project: nexus
```

### cmsis_nn.h

```{doxygenfile} components/algorithms/include/nexus/algorithms/cmsis_nn.h
:project: nexus
```
