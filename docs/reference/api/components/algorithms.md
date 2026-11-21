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
| Scalar control and PI/PID | `control.h`, `pid.h` | Explicit elapsed seconds, limits and independently owned controller state |

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
