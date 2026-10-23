---
id: docs-design-u575-stop2-copy
type: reference
audience: integrator
source_language: en
---

# U575 finite SRAM4 copy across Stop2

This page describes the implemented STM32U575ZIT6Q TZEN0 baremetal finite-copy
contract. Focused software models, independent source review, a selected public Cortex-M33
reset-image link and one relocated installed development-SDK consumer have passed.
The complete clean v7 verification plan, current CI/offline qualification and
physical validation remain separate; these development checks establish no
physical result. The complete
[IO-32](io-evolution-execution.csv) and [SOC-35](soc-refinement-execution.csv)
exits remain open; physical HIL is `not_executed`.

The optional `Nexus::U575LpdmaCopy` receiver uses only LPDMA1 channel 0 and the
16 KiB nonsecure SRAM4 window at `0x28000000..0x28003fff`. The same concrete
`Nexus::U575Stop2Copy` owner controls RTC AlarmA, WUT and the full sleep/recovery
sequence. The WUT-only `Nexus::U575Stop2` is an alternative target; selecting both
is rejected by CMake. This is a finite memory-to-memory copy, with no peripheral
route, general LPBAM plan, RTOS worker, queue or generated factory.

(fig-27-u575-stop2-copy)=
![FIG-27: current selected SRAM4 copy and compound Stop2 release; labeled arrows distinguish private calls, hardware triggers and final-access proofs; selected ARM and installed-SDK development checks passed; full qualification and physical validation remain open](../assets/diagrams/u575-stop2-copy.svg)

FIG-27 is separate from the WUT-only FIG-24. Solid arrows name calls, hardware
events or ordered cleanup; dashed arrows name external premises or open physical
exits. An idle DMA channel alone cannot release this compound loan.

## Prepare the exact memory and clock owners

The maintained [assembly](../../tools/configure/assemblies/u575-lpdma-copy-stop2.toml)
selects SRAM4 explicitly. Link only `Nexus::U575Stop2Copy` for this mode; it brings
the copy and retained-time dependencies. Its reset-only startup must begin with
the DMA gates off, verify the existing Range4 supply and SRAM4 power/retention
state, enable SRAM4, initialize
its DATA/BSS, then enable and read back LPDMA1EN. It preserves NOINIT bytes; their
presence proves neither valid data nor a constructed descriptor. Runtime owners
must not rescue lost SRAM4 by changing its power-down fields.

At runtime, the external gate owner keeps SRAM4EN and LPDMA1EN enabled and the
hardware configuration stable through stop and all joins. All four LPDMA channels
and vectors must initially be quiet; no other channel may start. The selected
profile requires TZEN0, unswapped Flash, effective MSIS4 and MSIK4, equal buses, true
Range4 (`VOS=00`, `VOSRDY=1`, `BOOSTEN=0`) and caches off. The existing
[Stop2 contract](u575-retained-time.md#select-and-use-the-stop2-owner) supplies
the remaining masked-MSP, stopped-RUN, SRAM-retention and external exclusions.

Source, destination and the whole 24 B, four-byte-aligned generated LLI workspace
must be disjoint spans inside that exact SRAM4 window. CPU owner storage and the
request may remain in retained SRAM1, but all control storage must be separate
from the borrowed spans. Both blocks use incrementing byte transfers; length is
at least 2 B and every extent must fit the window. The provider generates all six
LLI words; callers supply no raw DMA instructions.

Use the [retained-time API](../../components/soc/stm32/stm32u575_retained_time/include/nexus/components/stm32u575_retained_time.h),
[copy API](../../components/soc/stm32/stm32u575_lpdma_copy/include/nexus/components/stm32u575_lpdma_copy.h)
and [Stop2 copy API](../../components/soc/stm32/stm32u575_stop2/include/nexus/components/stm32u575_stop2_copy.h)
as the declaration authority. One privileged Thread serializes lifecycle and
control calls; no ISR or concurrent producer uses these objects. Copied faces
retain the same owner identity and require their own external observer joins.

## Admit, trigger and recover one copy

1. Initialize the retained clock with
   `nx_stm32u575_retained_time_initialize_alarm_a_copy` and
   `wake_owner=nx_stm32u575_stop2_identity()`. Initialize the Stop2 owner and
   `nx_stm32u575_lpdma_copy_initialize` with that same live clock and identity.
   Initial RTC AlarmA, WUT and IRQ state must be quiet. The ordinary read-only
   clock initializer does not authorize this AlarmA mode.
2. Initialize the request base, obtain a successful
   `nx_stm32u575_retained_time_sample`, then call
   `nx_stm32u575_lpdma_copy_request_prepare` with the separate snapshot, SRAM4
   spans, workspace and an absolute deadline in this retained domain or NEVER.
   Preparation copies metadata without borrowing or touching the buffers or
   workspace. Rejection leaves request bytes unchanged. End old Core TIM2
   deadlines before Platform stop; no deadline is converted between domains.
3. Enroll every RUN producer and observer in the exact Stop2 ClockDomain. Close
   admission, stop Platform/System, join actual producers, IRQ/DMA users and
   deadline consumers, then release their leases after real drain. Call
   `nx_stm32u575_stop2_copy_enter` with this closed, empty barrier, the concrete
   copy face, prepared request, `escape_ticks` in 4096..65536 RTC/16 ticks
   (nominal 2..32 s) and a positive per-phase `poll_limit`.
4. Keep privileged MSP Thread, CONTROL=0, PRIMASK=1 and BASEPRI=FAULTMASK=0 through
   entry and every retained recovery attempt. No scheduler may have started.
   The Stop2 owner privately validates readiness, arms the receiver and validates
   sleep before enabling the RTC producer. Rejection retains no new loan;
   `accepted=true` borrows the request, both buffers and workspace even if a
   later setup step fails.

AlarmA uses all calendar mask bits, ALRMASSR=0, ALRAIE=0 and ALRAFCLR=0. Its next
whole-second event drives rising trigger 14 for the first `floor(length/2)`
bytes. The generated second LLI copies the remaining bytes without a trigger;
TCEM selects completion of the last LLI and the final link is zero. The owner
preserves ALRAF and DMA pending while the producer is enabled. Cleanup clears
them only after producer withdrawal. WUT supplies an
independent escape wake. Channel-0 IRQ114 or RTC IRQ2 may wake masked WFI without
executing an interrupt handler. A pending wake may return immediately;
`wait_executed` proves WFI ran, never physical Stop2 residency. WUT escape without
an AlarmA event latches TIMEOUT and drains the finite receiver.

The returned `accepted`, `dma_wake` and `transition` describe admission and the
sleep boundary. They are not the final copy result. Read result and transferred
count only after acquire-observing the request's SETTLED state. A deadline or
`nx_stm32u575_lpdma_copy_cancel` requests withdrawal without revoking any borrow;
late cancellation after observed DMA completion rejects STATE. Public
`service` performs bounded DMA progress/drain and `stop` closes copy admission;
neither can independently publish SETTLED.

After wake or setup failure, verify the selected RUN access profile before any
RTC or DMA MMIO. An unknown profile permits CPU deep-bit/vector withdrawal only
and retains the compound loan and peripheral effects. Keep RUN admission closed and
all storage alive until an external reviewed owner restores the required profile.
`nx_stm32u575_stop2_recover` retries the same compound cleanup without another WFI.
Observed external LPDMA1 reset during an active loan permanently quarantines the
loan. Deasserting reset or observing quiet reset values cannot prove the prior
master's final access; only an actual external system reset ends that responsibility.

With a valid profile, withdraw and observe AlarmA/WUT producers before DMA drain.
The receiver proves idle, using suspend/SUSPF when enabled, then writes the
full-word channel RESET value and observes the complete CCR equal to zero before
clearing channel address/link references, flags and NVIC pending state. Checking
EN/SUSP alone cannot detect an ignored RESET write when the channel is already
disabled; the full-word write also clears the ordinary interrupt-enable fields. It restores only
its claimed channel-0 privilege and LPDMA/SRAM4 SMEN/AMEN bits, preserving RTC and
unrelated bits; it owns no whole-controller reset or engine-enable gate. Only
after all RTC, DMA, IRQ, power, gate and clock restoration is complete does Stop2
detach its face/request references and call the private final release. SETTLED
is then the provider's final request access. Any failed proof retains the whole
loan, including when the DMA itself is already idle.
The retained copy marker remains live until final release succeeds, including
when hardware effects are already zero; it rejects ordinary entry and another
copy. Recovery retries final release too.

Hardware isolation may finish with a tainted retained clock and publish an error;
it cannot produce a fresh successful elapsed-time result. Join all clock users
before explicit clock stop/reopen; a new generation invalidates every old binding
and deadline. Reopen RUN admission and restart Platform only after no effects
remain, creating a fresh Core TIM2 epoch. Retained samples resolve 3906.25 µs;
WUT uses separate 2048 Hz ticks. `max_gap_seconds` must cover the full interval
plus externally bounded arm, wake and drain latency. `poll_limit` bounds
observations, not elapsed time; WFI has no CPU duration bound if hardware wake
fails.

## Scope and evidence

The public headers and
[copy implementation](../../soc/stm32/stm32u575/drivers/lpdma_copy.c) define the source
contract; the [private collaboration](../../components/soc/stm32/stm32u575_lpdma_copy/private/stm32u575_lpdma_copy_internal.h)
connects only the concrete Stop2 owner. Meaningful RED preceded the production
changes. Focused Native execution passed 30 receiver cases, 33 retained-clock
cases, 17 coupled cases using all three real C owners and Core, and 30 WUT-only
compatibility cases (21 unit plus 9 real-clock cases). Independent source review
is complete. Four direct-source Cortex-M33 images passed development checks:
the public copy reset image and three default/retained/WUT omission images.
One relocated, nonpublishable installed-SDK consumer also compiled and linked
against the reviewed source. The direct-source copy image uses 15,816 B Flash;
the installed consumer uses 15,840 B, with the 24 B difference from source-path
strings and alignment. Each reserves 2,384 B SRAM1 including the 2,048 B MSP,
64 B SRAM4 and no heap. The development receipts bind these checks to their exact sources. This is not execution of the full
clean v7 plan of 50 images or current whole-candidate CI/offline qualification.
No earlier software candidate or physical result is inherited by this copy.

This slice does not implement peripheral LPBAM, general autonomous scheduling,
SRAM4-only operation, DVFS, RTOS Tick compensation, TrustZone sleep, Standby or
VBAT continuity. Physical copy correctness, Stop2 residency, retention, current,
oscillator accuracy and wake latency remain unexecuted. Software-model and ARM
link results must bind their exact source; the original IO-32/SOC-35 exits remain
open and unchanged.
