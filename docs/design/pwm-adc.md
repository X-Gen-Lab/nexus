---
id: design.pwm-adc
type: explanation
audience: maintainer
source_language: en
---

(pwm-adc)=

# Finite PWM and ADC windows with one hardware owner

Use this contract to select and execute a finite, timer-triggered acquisition
window through `nx_pwm_adc_t`. The current implementation is the exact
STM32G474RET6 `finite-regular-dma` software reference described below. It keeps
one PWM duty frame constant while DMA fills a finite raw sample array, then
withdraws the sources before releasing the request. Product control algorithms,
power-stage policy and inspected PCB wiring remain external. HIL is
`not_executed`.

## Select one coupled interface

![FIG-16: current build-time assembly and runtime ownership; the coupled window uses one selected face and private owner](../assets/diagrams/io-evolution.svg)

**FIG-16 describes current shared mechanisms, not a measured PWM/ADC timing
diagram.** In this selection, its generated face is `nx_pwm_adc_t`, its provider
is the joint TIM1/ADC1 owner, and its application-owned storage is the request,
duty array and sample array. The arrows retain their generation, lookup and
call meanings. Factory lookup never primes ADC, starts PWM or schedules work.

The normal input is
[the maintained TOML fixture](../../tests/contracts/g474_control_assembly/pwm-adc.toml).
Its software-only Board package supplies silicon pin and voltage assumptions;
it does not establish a fitted analog circuit. The strict resolver selects a
single Construction, a compact `NX_PWM_ADC_ID_WINDOW` and
`nx_factory_pwm_adc`. It rejects conflicting TIM1, ADC/common, comparator, DMA,
vector and pad claims before publishing an atomic bundle. Ordinary PWM-group
and ADC faces do not independently expose this selection's hardware.

| Layer | Owned responsibility |
| --- | --- |
| Authored assembly and typed IR | Select the exact mode, immutable facts, trigger identity and resource claims |
| Platform Construction | Acquire and restore pads, clock/reset and comparator leases; establish comparator settling before the private constructor |
| Coupled provider | Own TIM1, ADC1/common, DMA1 channel1, DMAMUX channel0 and vectors11/24 as one retained lifecycle |
| Application executor | Serialize task calls, maintain absolute deadlines, advance service and join external observers before reuse |
| Application storage | Keep the admitted request and complete duty array immutable, and exclude all other access to the sample array until acquired SETTLED |

The public interface has two pointers and five shared operations. The optional
provider adds private selected state; ordinary PWM and ADC state and tables do
not acquire coupled-session fields. There is no allocator, hidden worker,
runtime registry or independently startable subordinate face.
On a 32-bit target, the face uses 8 B and its shared five-pointer table uses
20 B. Private context, request and caller arrays are separate costs; these
structural sizes are not measured cycle, latency or stack bounds.

## Exact first software reference

| Fact | Selected value and boundary |
| --- | --- |
| Target and backend | STM32G474RET6, baremetal, HSE24/PLL170 fixed system profile |
| PWM | TIM1 main CH1/2/3 on PA8/PA9/PA10 AF6; whole-timer ownership; no complementary output |
| Counter | 170 MHz kernel, PSC169, 1 MHz tick, 100-tick period and 50-tick trigger phase |
| Deadtime | 170 kernel-clock ticks at 170 MHz; PSC does not scale this separate unit |
| Trigger | Internal CH4 PWM2 to TIM1 TRGO2; ADC regular EXTSEL10 rising edge |
| Comparator | COMP1 PA1/PA4 analog inputs, enabled noninverted INMSEL6 and TIM1 break; no measured threshold or power-stage safety qualification |
| ADC | PA0/ADC1_IN1, single-ended unsigned12, BULB, sample code0; SYSCLK/128 = 1,328,125 Hz |
| DMA | DMA1 channel1, DMAMUX1 channel0/request5; halfword, MINC, normal DMA; ADC limited requests with DMACFG0 |
| Storage | Entire aligned sample span inside a selected writable domain within 0x20000000..0x20018000; CCM and its alias are excluded |
| Count and format | 1..65535 requested samples, additionally bounded by the actual whole memory span; two-byte little-endian storage and stride, one regular channel |
| Vectors | DMA1 channel1 IRQ11 and TIM1 break/TIM15 IRQ24, logical priority5; ADC IRQ18 remains disabled |

The generated trigger identity is nonzero provenance within the assembly. It is
not an address, sample-aperture timestamp, calibrated voltage, duty epoch or
exact PWM pulse count. `nx_pwm_adc_state` reports the actual integer format and
separate counter/deadtime units.

The fixed 100-tick period is deliberate. At the selected ADC clock, it keeps
the normal BULB interval within DS12288's 2.5..640.5-cycle envelope. A 1 ms
period from a different PWM-group example is unsupported here. The register
model does not establish actual analog timing, trigger latency, clock accuracy
or board loading.

## Prepare, admit and explicitly advance

1. Retrieve the immutable generated face and explicitly start Platform. Startup
   prepares the cold owner; it starts neither a request nor PWM output.
2. Initialize the request once, then prepare a separate sample array and
   complete three-channel duty frame with a finite absolute monotonic deadline.
   Preparation copies frame metadata, borrows nothing and touches no samples.
3. Submit from the unique privileged, unmasked task executor. A rejected call
   leaves no new effects or retained references. `NX_SUCCESS` means ACCEPTED,
   including an arm error that may already have settled through its real drain.
4. After acceptance, preserve request and duty storage and exclude application
   reads/writes of sample storage. IRQs latch bounded facts; the same task calls
   service to advance completion or withdrawal. No implicit task supplies this
   progress.
5. Acquire-observe `NX_REQUEST_SETTLED` before consuming result and samples.
   Join adapter/observer references separately before preparing the storage for
   another explicit request. Ordinary successful completion never rearms itself.

Preparation rejects overlapping, misaligned or wrapping request, frame, duty and
sample spans without changing the initialized request. The provider additionally
rejects aliases with its retained state, facts, memory-domain descriptors and
known MMIO controls before admission. Known control descriptors are rejected
before their fields are read.

Each accepted window loads a coherent frame while TIM1 is stopped and reloads
preloads behind a quiet trigger route. It enables ADC and discards two software
conversions before enabling normal DMA and the external regular trigger. Neither
prime enters user DMA storage. The finite prime sequence uses at most a 100 us
deadline and a secondary poll bound. Each later explicit window repeats ADC
enable and both primes; idle never leaves the BULB sampler enabled.

The full-window deadline is an observation rule. Without a trusted completion
timestamp, TC first observed at or after that deadline reports TIMEOUT. A TC
already observed before the deadline may complete its required withdrawal
later. This does not establish a physical execution-time bound. Timeout and
cancellation retain the same loan until real source drain succeeds.

## Prove withdrawal before publishing a result

![FIG-06: current request ownership states; cancellation and quarantine retain the window loan until actual withdrawal](../assets/diagrams/request-lifecycle.svg)

**FIG-06 supplies the shared request-state contract.** ACCEPTED and REJECTED are
admission results, not additional request states. In this provider, the source
proof behind DRAINING/QUARANTINED and SETTLED includes each step below.

1. Withdraw master output, channels and timer interrupt sources. Prove CEN0
   before selecting the quiet enable-based trigger route; a refused CEN clear
   must not create a new rising TRGO2 edge.
2. Request ADC stop, observe ADSTART/ADSTP withdrawal, disable ADC and observe
   ADEN/ADDIS withdrawal. A service call uses bounded progress polls outside a
   long interrupt-masked wait.
3. Disable DMA and prove its state, detach both hardware addresses and count,
   and detach the DMAMUX route. A remaining-count snapshot does not establish
   a valid prefix after cancellation or error.
4. Preserve every captured TE/OVR/break fact before acknowledging its source.
   Clear only observed owned DMA flags, then recheck faults. Late flags during
   drain must not turn into successful acquisition.
5. Disable and join both vectors, including pending and active publishers.
   Under the final short metadata guard, sample faults again, detach the
   active request and release-publish SETTLED as the final provider access.

DMA TC alone proves none of the output, trigger or ADC withdrawal. Limited DMA
prevents repeated user-buffer overwrites while the terminal IRQ is late, but
does not stop TIM1. COMP break withdraws MOE; it does not stop CEN, TRGO2, ADC or
DMA. These independent facts must not substitute for one another.

An ordinary successful result reports exactly the requested sample count. Any
error or cancellation reports zero guaranteed valid samples. The first error,
cancellation or timeout cause remains authoritative while failed withdrawal
retries retain ownership. DMA completion does not override a physical fault.
An incomplete stop keeps the request QUARANTINED and the private recovery state
live; a later successful retry can settle it. A latched fault requires cold
recovery and never automatically restarts output.

Public stop closes further admission and attempts source drain. Its success
does not release pads, clocks or private storage. Platform close additionally
joins both vectors, closes the subordinate hardware state, restores the original
logical priorities, and only then restores comparator, pins and clocks.
A failed close preserves every unfinished outer lease for the same owner to
retry. Stopped identities must not be treated as revoked pointers.

## Sources and software evidence

The authoritative public contracts are
[the coupled request API](../../io/include/nexus/io/pwm_adc.h) and
[the core request lifecycle](../../core/include/nexus/core/request.h).
[The private provider](../../soc/stm32g474/drivers/pwm_adc.c) reuses the selected
official ADC/DMAMUX primitives and subordinate cold helpers; it does not replace
the existing TIM6 stream's timer pointer or expose two hardware owners.

The reviewed hardware references are RM0440 Rev9, DS12288 Rev6 and ES0430 Rev9.
The original official driver identities, paired CMSIS and licenses remain in
[the exact source lock](../../dependencies/stm32g4-hal.lock.json). ADC priming,
limited DMA and specific flag acknowledgement address the selected errata
conditions; they are not a claim that every silicon limitation or physical
circuit has been qualified.

[The external register/event tests](../../tests/google/g474_pwm_adc_test.cpp)
exercise real provider code, rejected aliases, priming, late DMA completion,
conservative deadlines, independent refused withdrawal, final fault snapshots
and late IRQ reuse. [The generated-factory tests](../../tests/google/g474_pwm_adc_factory_test.cpp)
execute the normal TOML bundle, cold clock/pin/comparator construction, two-prime
acquisition and retained cleanup/priority restoration. GoogleMock observes
external effects; it does not replace provider methods.

These actual host executions establish their recorded software scope. The
normal `fixture-io-evolution-g474-pwm-adc` ARM selection also links with its
resource budget and retention checks: one selected face, one concrete table
and five methods. That software link does not establish a clean release
candidate, reproducibility or physical HIL; each requires its own exact artifact
and source evidence. No inspected PCB, analog accuracy, waveform
measurement, interrupt latency or continuous/simultaneous/injected acquisition
is established by this finite software reference.
