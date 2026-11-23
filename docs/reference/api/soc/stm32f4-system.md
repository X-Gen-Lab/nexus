---
id: reference.api.soc.stm32f4-system
type: reference
audience: consumer
source_language: en
---

(reference.api.soc.stm32f4-system)=

# STM32F4 system integration

These headers are exact STM32F4 system composition contracts rather than
portable I/O faces. Follow each header's part, privilege, clock, memory and
retained-resource requirements; public visibility does not extend it to another
device.

## Selected F407 entropy owner

Select the chip's one internal RNG through the authored assembly:

<!-- nexus-toml-fragment: RNG selection within a complete assembly. -->
```toml
[entropy.random]
binding = "rng"
mode = "rng-words"
```

The Board supplies the pinless `rng` binding. Configuration requires the
selected nominal PLL48 domain to be exactly 48 MHz, including an explicit `pll48
= true` reservation when SYSCLK uses a direct input. The generated
`nx_factory_entropy(NX_ENTROPY_ID_RANDOM)` returns an existing readonly
{doc}`entropy face </reference/api/io/system>` without starting hardware. No
separate CMake constructor, public mutable owner or second clock configuration
is used.

Readonly admission rejects foreign enabled RNG or IRQ state before System clock
effects. Platform prepares the same private RNG owner after System startup,
withdraws it before clock shutdown or suspend, and prepares it again in the new
epoch on resume. The existing C implementation checks the actual RCC/Flash plan
and retains its acquired gate and enable effects through cleanup. Join every
entropy, TLS and network consumer before serialized privileged Platform suspend
or stop. Failed withdrawal retains that owner and cleanup responsibility; retry
the lifecycle operation without resetting its state. A cold or stopped face
remains valid and rejects data calls with STATE. Clock failure or an epoch
change stops data publication while cleanup remains available.

The [System contract](../../../../soc/stm32/stm32f4/system/README.md) defines
clock authority and recovery. RNG health checks and software models establish
neither physical min-entropy nor HIL qualification.

The chip-specific entry is `nexus/soc/stm32f407_handoff.h`, owned by the [F4
System
contract](../../../../soc/stm32/stm32f4/system/README.md#f407-掩码状态下的镜像交接). Link
`Nexus::STM32F407Handoff` explicitly in the selected F407 baremetal firmware.

## Optional FreeRTOS STOP reference

`nexus_add_stm32f4_freertos_stop(target)` explicitly links the one-shot OBJECT
adapter for the reviewed F407ZG `lowpower`/`timebase` kernel and existing RTC
owner. The ordinary shallow-sleep path remains the default. The product
returns its stable caller-owned state from `nx_stm32_freertos_stop_context()`;
there is no runtime registry, second tick source or hidden worker.

Before arming, the product suspends Platform, joins old Core/I/O borrowers and
closes the exact ClockDomain. Only actual scheduler-suspended Idle may perform
the masked tick/STOP handoff. The reviewed RTC uses 32768 Hz LSE, PRER127/255,
WUT div16/reload255 (nominal 125 ms), EXTI22 events and no calibration. A
retained RTC still has its original unique owner; this adapter does not add a
second register writer.

`max_elapsed_us` is an externally established 140000..1000000 µs upper bound
including wake, restore and quantization; `drift_ppm` is a relative RTC/Core
frequency bound, at most 10000 ppm. The two nominal 256 Hz RTC snapshots cannot
detect arbitrary whole-day gaps or prove that LSE kept running: those liveness
and duration bounds are product preconditions. The receipt reports elapsed
lower/upper bounds and uncertainty, not physical precision.

The kernel advances only conservative complete ticks, strictly before its next
unblock boundary, and restores the same SysTick with a complete first tick.
Discarded partial phase remains explicit. Quantization contributes about
7.813 ms, plus declared drift, observation windows and less than one kernel
tick of phase lag; this is not 1 ms continuous time across STOP. Old Core
epoch deadlines and admission generations remain invalid after resume. The
product retains accumulated uncertainty across repeated explicit attempts.

Each qualified Idle attempt consumes the authorization once, including failure.
Inspect the receipt before rearming or restoring users. After STOP invalidates
old Core time, lost RTC timing proof, duration outside the declared bound or
failed tick restoration retains masks/tick responsibility and fail-stops.
Pre-STOP rejection may restore the original tick and report an error without
inventing deep-sleep elapsed time. No path automatically restarts output.
The [System contract](../../../../soc/stm32/stm32f4/system/README.md) owns the
complete ordering and failure requirements. The linked reference does not run
the scheduler or STOP on hardware; power, drift and wake latency remain
`not_executed`.

## Public declarations

### stm32f407_handoff.h

```{doxygenfile} soc/stm32/stm32f4/include/nexus/soc/stm32f407_handoff.h
:project: nexus
```

### stm32f407_supervision.h

```{doxygenfile} soc/stm32/stm32f4/include/nexus/soc/stm32f407_supervision.h
:project: nexus
```

### stm32f407_wwdg_ewi.h

```{doxygenfile} soc/stm32/stm32f4/include/nexus/soc/stm32f407_wwdg_ewi.h
:project: nexus
```

### stm32f4_power.h

```{doxygenfile} soc/stm32/stm32f4/include/nexus/soc/stm32f4_power.h
:project: nexus
```

### stm32f4_freertos_stop.h

```{doxygenfile} soc/stm32/stm32f4/include/nexus/soc/stm32f4_freertos_stop.h
:project: nexus
```
