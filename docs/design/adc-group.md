---
id: design.adc-group
type: explanation
audience: maintainer
source_language: en
---

(adc-group)=

# Finite ADC sequences with explicit integer frames

Use `nx_adc_group_port_t` to acquire complete ordered ADC frames through one
physical owner. The current reference selects STM32G474RET6, HSE24/PLL170 and
baremetal, with regular normal DMA or one injected frame. It keeps integer
layout, trigger provenance, silicon expectations and storage ownership explicit.
Control algorithms, calibrated engineering units and product sampling policy
remain external. Physical HIL is `not_executed`.

## Select one static owner

![FIG-16: current assembly, static factory lookup and direct provider calls for one selected ADC sequence owner](../assets/diagrams/io-evolution.svg)

**FIG-16 describes shared assembly and ownership mechanisms, not an ADC timing
measurement.** Here the generated face is `nx_adc_group_port_t`; its private
context owns the selected ADC1 sequence and optional DMA/TIM6 resources. The
application owns the request and complete raw frame array. Generation, lookup
and runtime call arrows retain their distinct meanings. Factory lookup performs
no conversion, calibration, resource acquisition or task scheduling.

The four complete maintained inputs select
[regular software](../../cmake/package/verification/io_evolution/g474-adc-group-regular-software.toml),
[regular TIM6](../../cmake/package/verification/io_evolution/g474-adc-group-regular-timer.toml),
[injected software](../../cmake/package/verification/io_evolution/g474-adc-group-injected-software.toml)
or [injected TIM6](../../cmake/package/verification/io_evolution/g474-adc-group-injected-timer.toml).
Their software-only Board supplies PA0/PA1 analog route and voltage assumptions;
it does not establish a fitted analog circuit, source impedance or connector
access. The resolver rejects conflicting ADC/common, DMA, timer, vector and pad
claims before publishing one atomic bundle.

| Owner | Responsibility |
| --- | --- |
| Authored assembly and typed IR | Select logical rank order, sample codes, named silicon layout, trigger and exact resource claims |
| Platform Construction | Admit actual silicon before effects; acquire and restore pins, kernel mux, gates and system time |
| Private provider | Independently recheck cold silicon; program actual SQR/JSQR/SMPR, own source/ADC/DMA/vector drain and retain partial effects |
| Application executor | Serialize privileged, unmasked task calls, preserve monotonic deadlines and explicitly advance service |
| Application storage | Keep accepted metadata immutable and exclude all other access to the complete raw array until acquired SETTLED |

The factory returns the existing `NX_ADC_GROUP_ID_SAMPLES` face through
`nx_factory_adc_group`. The two-pointer face shares one const five-operation
table. On a 32-bit target these structural costs are 8 B per face and 20 B per
shared table; private state, request and caller arrays are separate. Ordinary
single ADC, continuous-block ADC and PWM/ADC state and tables do not grow.
There is no allocator, maximum pool, hidden worker or runtime registry.

## Exact hardware profiles and layout

| Selection | Actual execution and finite boundary |
| --- | --- |
| Regular, software | Real ADC `CONT1` scan sequence with finite normal DMA; software service does not emulate each conversion or frame |
| Regular, TIM6 | Rising TIM6 TRGO drives each complete regular scan; finite normal DMA ends on a whole physical-rank boundary |
| Injected, software | One software JADSTART with JQDIS1 and fresh JEOS; the task copies the whole group only after source/vector drain |
| Injected, TIM6 | One owned TIM6 OPM update drives one injected group; no second context or trigger is admitted before complete withdrawal |

All selections use ADC12 SYSCLK/8 = 21.25 MHz and HCLK 170 MHz, single-ended
unsigned12 normal sampling. Regular has at most 16 physical ranks; injected has
at most four. Regular owns DMA1 channel1, DMAMUX1 channel0/request5 and vectors 11
and 18. Injected owns vector 18 and performs no DMA-channel admission or access.
Software selections do not lease TIM6. TIM6 selections additionally own the
whole timer at a 1 MHz counter tick. All vectors use logical priority 5 and no
kernel calls.

The maintained inputs select channels `[2, 1]`, sample codes `[0, 0]` and nominal
3300 mV. Channel 1 is PA0 and channel 2 is PA1. The provider also admits the reviewed
internal channels 16/17/18 for temperature, VBAT and VREF respectively. One
physical channel has one SMPR field, so repeated logical occurrences cannot
select different sample codes.

`nx_adc_group_info` reports the actual immutable channel order, sampling-code
lookup, physical rank count and integer format. Storage is little-endian,
two-byte unsigned12 with zero LSB offset. A normal layout has sample stride 2;
the Z layout has stride 4 because each logical rank occupies two physical slots.
The scan stride is logical channel count multiplied by sample stride. Consumers
use `nx_adc_format_decode` after SETTLED and ignore discarded padding. The raw
array is not a floating-point voltage array.

Regular requests contain 1 through `floor(65535 / physical_ranks)` whole frames,
further bounded by their actual DMA-visible memory span. Injected requests
contain exactly one frame. The four reference bundles select writable SRAM1/2
domains; this selection does not authorize CCM aliases. The exact byte length
must equal `frame_count * scan_stride`, including all padding, with halfword
alignment. A partial rank, EOC, JEOC or DMA prefix never establishes a valid
frame result.

The trigger identity is zero for software and a nonzero assembly provenance for
TIM6. It is not a counter, aperture timestamp, physical wire identity or measured
phase relationship. `period_us` describes the selected timer period; it does
not describe an instantaneous ADC aperture.

## Observe silicon and execute its actual algorithm

The named `g474-adc-group-x`, `g474-adc-group-y` and `g474-adc-group-z` profiles
expect revisions 0x2003, 0x2002 and 0x2001 respectively. A profile is an immutable
expectation, not observed hardware. Platform admission and the independent
private constructor use the existing `nx_stm32_identity_read` source and shared
core silicon policy against actual IDCODE and Flash-size registers. Wrong
group/density, unavailable density, unknown cuts and mismatched expected cuts
reject. An ordinary restart observes again rather than caching admission.
All four canonical inputs select the X layout. Y/Z selections require their
matching observed cut and separate qualification receipts; an X factory run
does not qualify those configurations.

[The reviewed mode record](../../soc/stm32/stm32g474/reviews/adc-group.json) is the
single source of cut rules and required workaround masks. The provider does not
maintain another revision allowlist or reader. The group and density values
cannot identify an exact package or orderable part by themselves; the selected
SoC and Board must still authorize their exact routes.

The Z profile repeats every physical channel rank twice and keeps the first
slot of each pair. It therefore permits at most eight logical regular ranks or
two logical injected ranks. The explicit raw stride preserves the extra slots;
there is no hidden packing buffer. Every profile discards a real priming scan
and disables/re-enables ADC around each valid acquisition. These are actual
register algorithms, not evidence inferred from workaround metadata.

The fixed ratio HCLK/ADC = 8 avoids ES0430 section 2.7.5's condition above 10.
JQDIS1, one ADC group, no dual conversion, no automatic injected conversion and
no preemption exclude additional dynamic-context cases. These mode constraints
do not qualify unselected continuous injected queues or different clock ratios.

## Sampling time and real priming

The eight sample codes represent 2.5, 6.5, 12.5, 24.5, 47.5, 92.5, 247.5 and
640.5 ADC cycles. Conversion adds 12.5 cycles per physical rank. The reported
halfcycle lookup and expanded scan budget include discarded ranks. Internal
sources require at least code 5 for VREF, code 6 for temperature and code 7 for
VBAT at the selected ADC clock. Cold initialization enables only the selected
internal sources, waits 122 us if temperature is selected or 22 us otherwise,
performs calibration and waits at least four ADC cycles afterwards. These
unmasked waits do not establish analog accuracy or calibrated voltage.

Calibration has no independent fresh-completion flag. A short guard saves and
restores the incoming interrupt mask around the ADCAL command, barrier and
immediate busy observation; the completion wait runs unmasked. Initialization
requires observed ADCAL1 followed by hardware ADCAL0. An initial zero is
ambiguous: the command may have been refused, or calibration may already have
finished before the read. It conservatively reports IO and retains partial
effects for joined cleanup and a cold retry. An unchanged CALFACT or the nominal
116-cycle duration cannot prove fresh completion; no factor sentinel is written.

Before valid samples of each accepted acquisition, the provider executes one
complete discarded software scan. Regular priming uses the accepted raw output
span as temporary DMA storage; injected priming uses the JDR bank. The caller cannot read either
as valid samples. Watchdog and periodic trigger sources are disabled during
priming. Idle leaves ADC disabled, and a late task must really prime again;
updating a timestamp cannot substitute for a conversion.

For a hardware trigger, the complete expanded scan and asynchronous-trigger
margin must fit the exact period. The necessary cold freshness budget is
`period_us + ceil(scan_halfcycles * 4 / 170) < 1000`; slow sample codes therefore
reduce the admitted maximum period. Configuration and the private constructor
use the same physical units and reject impossible budgets without rounding.
Runtime freshness checks still cover actual task/arm delays. A stale arm retains
the loan, legally withdraws the in-progress hardware and converts another
discarded scan before publishing valid frames.

The optional analog watchdog applies raw lower/upper thresholds to the selected
group. It is disabled during discarded priming and latches a terminal IO fault
in the valid window. It does not supply a product trip policy, comparator break
route, safe output or guaranteed reaction time.

## Admit, advance and consume

1. Retrieve the static typed face and explicitly start Platform. Startup
   calibrates the cold owner but starts no acquisition and no hidden task.
2. Initialize the request, obtain the current immutable info and prepare a
   separate complete raw array with a finite absolute monotonic deadline.
   Preparation borrows nothing, touches no sample bytes and validates no chip
   or DMA domain on its own.
3. Submit through the unique privileged, unmasked task executor. Rejection
   retains no pointers or new effects. `NX_SUCCESS` means ACCEPTED, including
   an arm failure that may already have settled through actual withdrawal.
4. Preserve the descriptor and array. IRQs record bounded owned facts; the
   same task explicitly advances service through priming, acquisition and drain.
   No worker supplies progress when the application stops calling service.
5. Acquire-observe `NX_REQUEST_SETTLED`, inspect result and decode valid frames.
   Join any external observers separately before preparing the same storage
   for another request. An ordinary success never automatically rearms itself.

The provider rejects wrapping, overlapping, control-aliasing and incomplete
spans before admission; known control descriptors reject before their fields
are read. Without a trusted completion timestamp, completion first observed at
or after the deadline reports TIMEOUT. Completion already observed in time may
finish required withdrawal later. This observation rule is not a physical
execution-time or service-latency guarantee.

## Withdraw before releasing the loan

![FIG-06: current request lifecycle; cancellation and quarantine retain ADC frame storage until actual source and reference withdrawal](../assets/diagrams/request-lifecycle.svg)

**FIG-06 supplies the shared request-state contract.** ACCEPTED and REJECTED
are admission outcomes, not extra request states. In this provider, SETTLED
requires the following physical and reference proofs.

1. For TIM6, prove CEN0 before selecting the quiet enable-based MMS route.
   Cold/rearm UG occurs behind that quiet route with fresh UIF/acknowledgement;
   stop does not issue UG or create another update trigger.
2. Request only the matching regular or injected stop, observe its START/STOP
   withdrawal, then legally disable ADC and observe ADEN/ADDIS0. JEXTEN changes
   follow JADSTART0; ADDIS never substitutes for group drain.
3. For regular DMA, prove EN0, detach addresses and remaining count, and detach
   DMAMUX. TC and CNDTR0 alone do not stop a CONT1 scan or a hardware trigger.
4. Preserve captured TE/OVR/watchdog faults before W1C, acknowledge only owned
   observed flags and recheck during drain and final publication.
5. Disable/join selected vectors and active publishers. The final metadata
   guard detaches the request and release-publishes SETTLED as the final provider
   access. Injected JDR values are copied only after whole fresh JEOS and drain.

Success reports exactly the requested frame count. Errors, cancellation and
timeout report zero guaranteed valid frames, even if raw storage contains a
partial or complete-looking prefix. The first terminal cause remains latched.
A refused counter, ADC, DMA or vector withdrawal retains QUARANTINED storage
and recovery responsibility; service can retry the same real drain. A retired
predecessor's repeated stop cannot withdraw a successor's controller.

Public stop closes new admission and drains the accepted request. Its success
does not release pins, internal-source enables or clock leases. Final Platform
close restores logical priorities and common configuration only after complete
request/source/vector withdrawal, then releases outer resources. Failed close
retains unfinished leases for retry. Fault recovery is an explicit joined cold
close and restart, never an automatic replay.

## Sources and software evidence

The authoritative API is [the public group contract](../../io/include/nexus/io/adc_group.h).
[The private provider](../../soc/stm32/stm32g474/drivers/adc_group.c) implements the
actual sequencer, timer, watchdog and DMA transitions. The selected official
ADC/DMAMUX definitions remain private and locked by
[the STM32G4 dependency record](../../dependencies/stm32g4-hal.lock.json).
RM0440 Rev9, DS12288 Rev6 and ES0430 Rev9 establish the reviewed hardware facts;
the mode record binds their source identities and algorithm scope.

[Public GoogleTest/GoogleMock contracts](../../tests/google/adc_group_test.cpp),
[external register/event tests](../../tests/google/g474_adc_group_test.cpp) and
[normal generated-factory tests](../../tests/google/g474_adc_group_factory_test.cpp)
exercise real common/provider code. They cover whole-rank boundaries, integer
decoding, discarded priming, actual continuous regular scans, OPM, aliases,
deadline/late faults, refused withdrawal, instance handoff and retained cleanup.
Independent admission cases use actual IDCODE/Flash-size storage and the same
generated policy, without replacing the reader or comparison function.

Current development validation passed eight public API cases, 55 real-provider
cases and four normal generated-factory programs with 16 cases each: 127 cases
with no failures or skips. All four canonical X configurations also passed
normal ARM configure/build, resource budgets and retention checks for one face,
one table and its five method slots. These are model and firmware-link results,
not executed analog measurements, Y/Z normal-configuration qualification or a
clean reproducible software candidate.

Actual model results and ARM ELF/resource checks belong to their exact source,
configuration and execution receipts. Linking does not execute ADC or prove
analog waveforms. A clean reproducible software candidate, installed-SDK
composition and physical station acceptance have separate evidence. No model,
diagram or policy mask establishes PCB connectivity, measured accuracy,
aperture timing, physical HIL or a safety-qualified control system.
