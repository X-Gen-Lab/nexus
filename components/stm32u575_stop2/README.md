# Exact U575 retained-RTC Stop2 owner

`Nexus::U575Stop2` is an optional STM32U575ZIT6Q, TZEN0, baremetal component.
It owns RTC WUT, direct RTC interrupt 2, the Stop2 power selection, RTC sleep and
autonomous gates, and the deep wait boundary. It uses the independent
`Nexus::U575RetainedTime` calendar clock. It never reads Core TIM2, starts an OS,
or restores old TIM2 deadlines. Applications and product policy remain external.

Initialize a retained-time owner with `wake_owner` equal to the stable value
returned by `nx_stm32u575_stop2_identity()`, then initialize this owner with that
clock storage and an explicit RUN consumer capacity. The clock storage outlives
both owners and every copied clock face. Trusted static assembly binds these
identities; this is not hardware authentication or access control. No factory
RTC/AlarmA controller may coexist. No other calendar, WUT or RTC IRQ writer is
permitted. The caller serializes both owners.

Every platform and product producer, active device, IRQ, DMA, observer and Core
deadline consumer must enroll in this owner's explicit ClockDomain. Close RUN
admission, stop and join them through their real lifecycle APIs, then release
each lease only after its actual drain. Pass the exact closed barrier when no
consumers remain. Closing an empty unrelated domain does not authorize entry.
The independent RTC clock stays live outside the stopped RUN domain. No automatic
platform enrollment is claimed. The caller keeps admission closed through a
failed transition and recovery; a ClockDomain barrier proves only enrolled owners.

Enter in privileged MSP Thread mode with CONTROL=0, PRIMASK=1 and
BASEPRI=FAULTMASK=0. System/Platform must already be stopped with no residual
effects, and no scheduler may have started. Entry verifies TZEN0, unswapped 2 MiB
Flash, effective MSIS4 and equal buses; all other high-speed clocks are off except
the optional MSIK clock; kernel muxes are reset HCLK choices. It excludes active
caches, MPU, SysTick, debugger, other NVIC and EXTI wake sources, peripheral/DMA
gates and SRAM power-down selections. All SRAM is retained, including caller
stack, owner state and retained-clock state. Reading SysTick CTRL consumes its
COUNTFLAG. Raw zero registers cannot establish watchdog inactivity, absence of
NMI/HardFault/debug agents, peripheral analog state, other bus masters or external
configuration writers: those remain explicit external assembly obligations.

A boot owner supplies the retained LSE32768 calendar and RTC/PWR access gates,
valid provenance and DBP=1. This component never initializes or changes calendar,
LSE, backup reset, option bytes or security attribution. It unlocks WPR only
around owned WUT writes and always writes the lock key before returning. It
requires all SRAM power-down bits clear and Flash prefetch disabled, applies
RTC SMEN/AMEN for wake delivery and uses direct RTC IRQ2 without an EXTI19 path. The pinned ARM CMSIS
`Ref_cmInstr.txt` explicitly permits WFI wake when a PRIMASK-masked interrupt
becomes pending; the owner never unmasks it or invokes an RTC handler.

The caller requests 2..65536 RTC/16 ticks at nominal 2048 Hz. The owner programs
WUTR=ticks-1 and WUTOCLR=0 after observing WUTWF. The first WUTF assertion is
between WUTR and WUTR+2 counter ticks after enabling. This is a requested wake
interval, not a measured elapsed time or a bounded physical response guarantee.
The clock's 256 Hz resolution is 3906.25us, so short requests can yield zero elapsed
microseconds. Its configured maximum sample gap must cover the requested sleep,
arm/drain execution and actual wake latency; exceeding that gap invalidates the
clock epoch rather than fabricating continuity. Hardware timing and wake latency
remain unqualified until measured on the selected exact device.

After WFI, the owner clears SLEEPDEEP and verifies MSIS4/Range4 wake state
before any RTC read or write. An invalid RUN profile permits only core deep-bit
clear and CPU vector withdrawal; it keeps WUT and related effects owned until an
external reviewed owner restores MSIS4. Recovery also verifies this profile before
RTC access. With a verified profile the owner restores
the incoming MSIK enable state and drains WUTE/WUTIE, WUTWF, WUTF and NVIC state
before restoring WUTR, power and added sleep/autonomous gates. It captures before
and after RTC timestamps only in the same live generation. WFI execution cannot
prove physical Stop2 residency; a pending wake can return immediately. An
unexpected wake is reported and drained. Every poll phase has the caller's
positive finite observation budget; no iteration count is advertised as time.

Failed withdrawal keeps the static hardware owner and residual-effects flag.
Do not restart Platform, reopen RUN admission, reclaim a loan or stop the
retained clock. Retry `nx_stm32u575_stop2_recover` at the same masked boundary;
it never executes WFI. Recovery can finish hardware isolation even when RTC
sampling is invalid. A tainted clock requires its own joined stop/reopen; it does
not authorize reuse of old deadlines. Only after all effects are zero may the
caller reopen RUN admission and restart Platform, creating a new TIM2 epoch.

The hardware sequence follows RM0456 Rev7 sections 10.7.8/Table 104,
11.8.1/11.8.4/11.8.44/11.8.45 and 63.3.10/63.3.11/63.6.4/63.6.6/63.6.7/63.6.23.
ES0499 Rev12 requires rejecting active PLL2/PLL3/HSI48/SHSI (2.2.5), using HCLK
for inactive ADC/DAC/ADF kernels (2.2.8), disabled caches (2.2.11), no debug Stop
(2.2.19), all SRAM retained (2.2.22), and disabled Flash prefetch plus exclusion
of other Flash masters (2.2.26). Retained-time admission handles LSE drive and
calendar bypass errata. The selected U575 header has only EXTI bank1; the
multi-device RM's bank2 note does not create nonexistent exact-part registers.

This is a bounded software increment toward SOC-35. It does not close original
SOC-35/IO-30 exits or implement DVFS, RTOS Tick compensation, LPDMA/LPBAM,
SRAM4-only operation, general TrustZone sleep, Standby or physical qualification.
