---
id: design.h743_high_clock
type: explanation
audience: maintainer
source_language: en
---

(design-h743-high-clock)=

# H743 V-only high-clock profile

Use `hsi64-pll384-cache-off` when a cold STM32H743ZIT6 reference needs distinct
CPU, bus and peripheral clock domains. This page explains the current fixed
profile, its joint owner and failure boundaries. It covers V revision `0x2003`
only, with Cache off. Physical HIL remains `not_executed`; software execution
does not measure supply, junction temperature, clock accuracy or communication.

## One assembly selects one clock owner

![FIG-05: current generated Platform lifecycle; arrows describe explicit startup, reverse cleanup and retained responsibilities](../assets/diagrams/platform-lifecycle.svg)

FIG-05 describes the common Platform lifecycle. For this exact reference, its
SoC startup expands to clock owner, System timebase and selected consumers, in
that order. The reverse path must withdraw consumers and System before the clock
owner. The diagram is not a frequency-transition sequence or physical proof;
the exact clock stages below specialize its lifecycle responsibilities.

One TOML `clock` selection resolves through reviewed
[SoC facts](../../soc/stm32h743/soc.json) into immutable clock-domain IR.
The generated bundle selects the private implementation and CMake selects its
translation unit. The typed factory still returns existing static faces without
MMIO, allocation, clock setup or discovery. Runtime calls enter the selected
provider directly through its shared read-only operations table.

The [private clock contract](../../soc/stm32h743/private/stm32h743_clock.h) owns
admission and clock responsibility. [System](../../soc/stm32h743/system.c) owns
TIM2 and its vector. The generated
[lifecycle glue](../../tools/configure/providers/stm32h743/emission.py) orders
them and preserves startup and cleanup diagnostics. Their effect masks are
different namespaces; the platform projects retained responsibility without
treating the numeric masks as interchangeable.

## Admit an exact cold configuration

Automatic high-profile admission requires DEV_ID `0x450`, REV_ID `0x2003` (V)
and the factory-default HSI user trim. ES0392 Rev15 Table 2 maps `0x1003` to
both Y and W, and `0x2001` to X. The reviewed DS12110 Rev11 electrical chapter
explicitly covers V; the Y/W ambiguity and lack of an explicitly reviewed X
chapter prevent admitting those IDs. They and unknown revisions return
`UNSUPPORTED` without acquiring effects. There is no automatic fallback or
user-overridable revision allowlist.

Startup requires the real reset-owned call sequence before SysTick, a scheduler,
I/O or another clock owner has run. Matching register values cannot prove that
history. The diagnostic deliberately avoids reading SysTick CTRL because its
COUNTFLAG is read-clear; zero LOAD/VAL are not proof that ENABLE is clear.
Bootloader adoption and runtime frequency scaling require separate contracts.

Cold checks include ready LDO/VOS3, inactive Flash pulses and programming modes,
reset PLL configuration, quiet consumers and known-clean disabled caches.
Readable disabled cache bits do not prove that foreign dirty lines are absent.
Supply, temperature, package and PCB cannot be qualified by IDCODE or readback.

## Keep clock domains and uncertainty separate

The following are reviewed design bounds, not frequency measurements. CPU and
PLL-derived domains include the conservative PLL-ready deviation in addition to
the HSI envelope. Unmultiplied HSI kernels do not inherit that PLL multiplier.

| Domain | Nominal frequency | Reviewed design interval | Consumer responsibility |
| --- | --- | --- | --- |
| CPU | 384 MHz | 364.56..397.8 MHz | PLL1P, M=10/N=120/P=2 |
| HCLK | 192 MHz | 182.28..198.9 MHz | CPU /2 |
| APB1..4 | 96 MHz | 91.14..99.45 MHz | HCLK /2 |
| TIM2/TIM3 | 192 MHz | 182.28..198.9 MHz | Doubled APB timer kernel, not CPU frequency |
| USART3/I2C1 | 64 MHz | 62..65 MHz | Explicit HSI kernel selectors |
| SPI4 | 96 MHz | 91.14..99.45 MHz | APB2; divider admission uses its upper bound |
| ADC12 input | 64 MHz | 62..65 MHz | per_ck from HSI; input selection is not ADC conversion setup |
| Selected ADC1 conversion | 16 MHz | 15.5..16.25 MHz | Provider selects async /4 after acquiring ADC12 |

The empty profile configures ADC12's input selector but does not initialize an
ADC or its divider. TIM2 uses PSC191 for a nominal 1 MHz counter. A software
millisecond remains an HSI-derived nominal interval; PSC383 would halve the
counter rate and is rejected by the production-model tests. TIM3 likewise uses
its actual 192 MHz kernel when admitting an integer tick divisor.

### Where the HSI envelope comes from

[DS12110 Rev11](https://www.st.com/resource/en/datasheet/stm32h743vi.pdf)
Table 134, page 232, gives V-cut factory HSI frequency 63.7..64.3 MHz at
VDD 3.3 V and junction 30 °C. It separately specifies voltage drift
−0.12..+0.03% over VDD 1.62..3.6 V and temperature drift −2..+1% over
−40 °C..TJmax. Factory frequency limits use production testing; the full
temperature drift uses characterization. These assurance levels remain distinct.
The narrower −20..105 °C row is not the full-temperature envelope. Suffix 6
junction limits must not be confused with ambient limits or thermal dissipation.

Conservative multiplicative endpoints are
`63.7 × 0.98 × 0.9988 = 62.3510888 MHz` and
`64.3 × 1.01 × 1.0003 = 64.9624829 MHz`.
The additive lower estimate `63.7 − 64.3 × (0.02 + 0.0012)` is 62.33684 MHz.
The selected 62..65 MHz interval covers these compositions; it is a reviewed
engineering envelope, not a literal datasheet interval. RM0433 Rev9's PLL-ready
deviation adds the conservative factor 0.98..1.02, yielding
`62 × 6 × 0.98 = 364.56 MHz` through `65 × 6 × 1.02 = 397.8 MHz`.
The design stays below the reviewed VOS1 CPU/HCLK/APB limits of 400/200/100 MHz.
Different source assumptions require a new reviewed profile.

### Local UART arithmetic does not qualify a link

At requested 115200 baud, oversampling 16 and prescaler 1, the selected HSI64
kernel produces BRR556. Nominal baud is about 115107.914; the 62..65 MHz
source envelope produces about 111510.791..116906.475 baud, or
−3.202438..+1.481315% relative to the request. The lower endpoint already
exceeds a hypothetical 2% local budget. Nominal divider accuracy alone cannot
establish acceptable communication over the whole source envelope.

The platform checks local divider representability and exposes the selected
source bounds. It does not invent a zero-error peer, require product peer-budget
fields or silently choose another source. The external product must evaluate
the complete transmitter, receiver, quantization, line and wakeup budget in
RM0433 Rev9 section 48.5.8, then obtain physical evidence for its link.

## Establish protection before speed and retain failed cleanup

The selected joint owner stores one four-byte effect record. It owns the
reversible SYSCFG gate, voltage request, shared Flash timing, bus prescalers,
PLL configuration/source and USART/I2C/ADC kernel selectors. It does not own
consumer requests, buffers, worker tasks or a scheduler.

1. Validate cold prerequisites, acquire the reversible gate and reject foreign
   overdrive. Prove VOS1 readiness and Flash LATENCY2/WRHIGHFREQ2 readback.
2. Program the fixed bus dividers and wait 32 ordered RCC observations before
   programming PLL1. RM0433 section 8.7.8 allows HPRE propagation up to 16
   slowest-APB periods; this profile has APB=HCLK/2. Readable HPRE alone is
   not proof that the effective divider has settled.
3. Establish PLL1, observe readiness, request its source and prove actual SWS
   before publishing the nominal CPU clock. Acquire and verify the kernel
   selectors, then start System and selected consumers.
4. Stop admission, drain and join consumers, and withdraw System. A failed
   consumer or System stop retains the clock owner and its protections.
5. Restore only owned selectors; each may already be at its baseline or owned
   target. A foreign selector rejects without clobbering its register. Prove
   actual HSI selection before disabling PLL or reducing bus, Flash and voltage
   protections. Clear an effect only after its restoration is observed.

A startup error preserves its primary result while ordered cleanup reports its
own failure and retained responsibility. A failed restore keeps the same owner
alive for retry; timeout does not authorize releasing unproved effects. This is
a single fixed cold profile, not a general-purpose clock tree manager.

High-profile Flash pulses require the observed V-cut, VOS1, HCLK, 2WS/WR2 and
Cache-off combination. The high owner replaces the fixed-HSI optional Flash
timing owner; both must not manage the same timing field. A pulse never retunes
ACR. Original voltage, unswapped-bank, region, alignment and fresh-erase
requirements still apply. All-FF readback does not prove virgin ECC cells.
Busy-bank instruction fetch may stall; an unabortable pulse must settle before
return, and failure to prove final withdrawal retains the calling frame through
the existing terminal fault path. There is no tight physical deadline promise.

I2C timing requires explicit kernel and electrical bounds; PCLK is not its
selected 64 MHz kernel. SPI divider selection uses 99.45 MHz, while its stored
nominal source remains 96 MHz. For a 1.5 MHz ceiling it selects /128, rather
than /64 from nominal arithmetic. Wire-tail drain and CS release remain required
even after the caller's deadline. ADC's 16-bit storage does not establish
measured effective resolution or calibrated voltage.

## Selection cost, executed scope and next exits

Only the high profile emits the joint owner and selects `clock.c`. The four
existing fixed-HSI/cache profiles retain their own lifecycle and do not call
this owner. The four-byte record is a source-level selected-state fact, not a
measured whole-image RAM delta; alignment, retained code and linked ELF costs
must be observed separately. No allocation or extra hot-path forwarding layer
is introduced by clock selection.

| Executed development scope | Actual result | Boundary |
| --- | --- | --- |
| [Clock tests](../../tests/google/h743_clock_test.cpp) | 38 GoogleTest cases passed | V-only rejection, ordering, partial effects and retry in external register models |
| [High System tests](../../tests/google/h743_high_system_test.cpp) | 19 cases passed | Actual clock owner plus TIM2, IRQ, nominal time and failure cleanup |
| [Kernel tests](../../tests/google/h743_high_kernel_test.cpp) | 30 cases passed | Distinct domains, divider bounds, Flash admission and mixed-selector rollback |
| [Generated factory tests](../../tests/google/h743_high_factory_test.cpp) | 10 cases passed | Normal TOML/IR/generated lifecycle, cold lookup, consumer calls and stop-failure retry |
| Three normal ARM configurations | Configure and build returned zero | Exact empty, UART and peripheral fixtures below; code generation and linking, not hardware |

The ARM presets are `fixture-io-evolution-h743-high-empty`,
`fixture-io-evolution-h743-high-uart` and
`fixture-io-evolution-h743-high-peripherals`. Their maintained inputs are
[empty](../../cmake/package/verification/io_evolution/h743-high-empty.toml),
[UART](../../cmake/package/verification/io_evolution/h743-high-uart.toml) and
[peripherals](../../cmake/package/verification/io_evolution/h743-high-peripherals.toml).
These records describe this development iteration; they do not establish a
clean software candidate, offline reproducibility or physical qualification.
The [I/O ledger](io-evolution-execution.csv) remains the status authority.

High Cache-on, continuous H743 DMA, SPI DMA, additional silicon cuts, bootloader
handoff and running-clock transitions have separate implementation and
acceptance exits. Existing finite DMA modes do not imply these extensions.
Physical clock, voltage, temperature, UART, SPI, I2C, ADC and Flash tests remain
`not_executed` until connected equipment actually measures the selected image.
