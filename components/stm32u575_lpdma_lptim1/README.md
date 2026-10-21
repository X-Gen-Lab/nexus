# Exact U575 finite LPTIM1 receiver

`Nexus::U575LpdmaLptim1` is an optional STM32U575ZIT6Q TZEN0 baremetal
reference. Link `Nexus::U575Stop2Lptim1` to select its same concrete Stop2
owner. The WUT, SRAM4-copy and LPTIM1 alternatives reject coexistence. Nothing
is selected by default. Firmware stays C11, static and allocation-free; vendor
types stay private.

The API accepts one SRAM4 tuple `{CCR1, RCR=0, ARR}` with `CCR1<ARR` and
`1023<=ARR<=65535`, plus a separate aligned 48-byte SRAM4 workspace. It exposes
no arbitrary peripheral address, caller DMA instruction or repeated tuple mode.
A second tuple would require a separate preceding-write-OK proof under bus
delay.

Initialize ordinary retained time with this Stop2 identity, initialize the
owner, then explicitly call receiver `start`. Start acquires cold LPTIM1
access, its LSE kernel selection and only a needed LSE-system gate. It
preserves existing RTC/LSE provenance. Any partial start effects remain owned
until public stop succeeds; applications require no private RCC writes. The
existing selected SRAM4/LPDMA1 startup and retained RTC boot contracts still
apply.

Close and drain the exact RUN barrier, stop Platform, then enter at the
existing privileged masked MSP Thread boundary. RUN LPTIM2 must already be
disabled and joined; it is never repurposed as the Stop2 producer. The
dedicated owner enables LPTIM1 only after exact profile checks. A post-ENABLE
retained-clock sample and bounded source progress prove the required
counter-cycle delay. Initial register writes require fresh write-OK flags
before the DMA program is armed.

LPTIM1 update request13 with BREQ1 gates the CCR1 word. Two generated software
continuation LLIs write noncontiguous RCR then ARR last; ARR clears UE. WUT is
an independent escape. No AlarmA trigger or GPIO route is used. A successful
request means twelve APB bytes and their fresh kernel write-OK acknowledgements
completed; it does not establish output playback or any physical Stop2
residence.

ACCEPTED borrows request, tuple and full workspace until acquire-observed
SETTLED. Cancel and deadline never revoke storage. Producer UEDE/DIEROK
withdrawal precedes DMA suspend/reset/idle and reference removal. An owned RCC
reset then disables LPTIM1 per ES0499; the implementation never clears
CR.ENABLE. All peripheral, IRQ, loan-gate and Stop2 clock restoration must
finish before final release. Public-start access effects remain reserved until
the joined public stop.

Unknown access profiles retain effects and prohibit peripheral access. Recovery
retries the same chain without another WFI. Observed external DMA/LPTIM reset
permanently quarantines the active loan; reset defaults are no final-access
proof. Owned reset withdrawal failures remain retryable after the DMA proof.

The public headers define ownership and failure behavior. This is a bounded
software reference toward IO-32/SOC-35, with full exits still open. Board
routing is unknown and all physical timing, waveform, retention, wake and power
HIL is `not_executed`.
