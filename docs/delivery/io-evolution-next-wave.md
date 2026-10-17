---
id: delivery.io-evolution-next-wave
type: status
audience: maintainer
source_language: en
---

(io-evolution-next-wave-delivery)=

# Selected I/O and clock extension delivery

This page records development observations for the selected H743, U575 and
G474 software extensions and the checks actually executed for them. The current working source has not reached
its final freeze. Focused model results below do not qualify a later commit,
an ARM image, an electrical connection or a software candidate.

The [result index](io-evolution-next-wave-results.json) identifies the exact
fixtures, raw command receipts and fresh XML. The [I/O architecture contract](../design/io-evolution.md)
and its FIG-16 explain static assembly and direct runtime dispatch; the
[request lifecycle diagram](../assets/diagrams/request-lifecycle.svg) defines
borrow and withdrawal transitions. These existing diagrams do not establish
qualification of the new selections.

## Selected implementation

| Slice | Exact selection | Ownership and present boundary |
| --- | --- | --- |
| STM32H743 | `hsi64-pll384-cache-off`, reviewed V revision, bare-metal reference | Explicit cold clock plan, nominal CPU 384 MHz/HCLK 192 MHz/APB 96 MHz with authored HSI bounds; peripheral kernel and timer domains remain distinct. This is a selected reference, not DVFS or a measured clock. |
| STM32U575 | USART1 `dma-tx`, TZEN=0, MSIS 4 MHz, 8N1 | One selected state and const ops table; GPDMA1/channel0/request25, finite 1–65535-byte TX wholly inside nonsecure SRAM1 `[0x20000000,0x20030000)`. IRQ byte RX remains available; RX DMA, linked lists and a TrustZone broker remain outside this mode. |
| STM32G474 | `can_fd` / `iso-fd-normal` or `iso-fd-loopback`, FDCAN1 | One fixed 212-word message-RAM slice and three physical TX elements; nominal 500 kbit/s and data 2 Mbit/s at HSE24. Normal and internal-loopback model tests do not qualify a transceiver, terminated bus or peer. |
| STM32G474 | `pwm_adc` / `finite-regular-dma` | One whole TIM1/ADC1/ADC12/DMA1-channel1/DMAMUX/COMP1 reference owner; 1 MHz counter, 100-tick period, 50-tick trigger and 170 kernel-tick deadtime. Finite halfword samples and the PWM frame remain borrowed through proven withdrawal. |

The authored fixtures choose exact SoC facts and Board routes. Applications,
control policies, network protocols, worker tasks, private PCB data and recovery
policy remain in external repositories. The platform does not infer electrical
thresholds or acceptable actuator behavior from a software fixture.

Typed factory lookup returns existing immutable faces without hardware access,
allocation or a hidden worker. Optional state and private official headers are
selected by the assembly. A source selection alone is not a measured Flash/RAM
or latency result; final ARM costs require the matching ELF/BIN/map and checks.

## Completion and failure

U575 DMA transfer completion does not imply USART wire completion. The selected
mode retains DMAT across requests and cancellation, in accordance with the
reviewed U5 erratum. Final settlement requires fresh channel withdrawal/reset
proof, independent USART TC and reference drain. Failed drain retains the
request in `QUARANTINED`; a timeout does not revoke its source-buffer loan.

G474 CAN FD separates pending TX add/request bits from fresh per-element sent
or cancellation outcomes. Receive queue capacity does not hide the latched
protocol state. G474 PWM/ADC joins timer, ADC, DMA and IRQ references and latches
late faults before publishing its final request state. Products must continue
service and preserve storage whenever withdrawal fails. See the public API and
[finite PWM/ADC contract](../design/pwm-adc.md) for the selected call contexts and
physical reference assumptions.

## Executed generated-factory scopes

These five programs resolve the maintained TOML and run generated C together
with actual providers on the host. GoogleTest/GoogleMock replaces only external
CPU, MMIO, NVIC and explicit bus/event leaves. It does not replace provider
admission, drain, factory lookup or platform rollback algorithms.

| Program | Executed cases | Failures/errors/skips | What the result establishes |
| --- | ---: | --- | --- |
| `nexus_h743_high_factory_test` | 10 | 0/0/0 | Selected high-profile cold assembly and software rollback behavior |
| `nexus_g474_fdcan_factory_test` | 5 | 0/0/0 | Selected internal-loopback factory, domain and modeled hardware behavior |
| `nexus_g474_fdcan_normal_factory_test` | 5 | 0/0/0 | Selected normal-mode factory and modeled controller behavior |
| `nexus_u575_uart_gpdma_factory_test` | 8 | 0/0/0 | Exact generated channel/vector binding, byte RX, finite TX and lifecycle behavior |
| `nexus_g474_pwm_adc_factory_test` | 11 | 0/0/0 | Coupled generated ownership and finite-window software behavior |

Counts belong to their named scopes and are not a complete-project test count.
Later provider changes require a matching rerun; the result index keeps each
original receipt. Earlier RED attempts, fixture failures and corrected runs
remain preserved rather than overwritten.

## Recorded ARM development builds

Six normal preset configurations and builds completed with actual exit 0.
The following values come from their resource reports and matching ELF files;
Flash LOAD, Flash footprint and reserved RAM are separate byte quantities.
Reserved RAM includes linker reservations and is not a measured stack high-water
mark. These are recorded development builds, not final-source cost comparisons.

| Preset suffix | Flash LOAD (B) | Flash footprint (B) | Reserved RAM (B) | Provider retention |
| --- | ---: | ---: | ---: | --- |
| `h743-high-empty` | 5403 | 5404 | 2104 | Not generated for the empty standard firmware target |
| `h743-high-uart` | 12016 | 12016 | 2336 | Passed |
| `h743-high-peripherals` | 23660 | 23660 | 2628 | Passed |
| `u575-uart-gpdma` | 8964 | 8964 | 2312 | Passed |
| `g474-can-fd` | 10184 | 10184 | 2788 | Passed |
| `g474-pwm-adc` | 13620 | 13620 | 2264 | Passed |

Each suffix belongs to `fixture-io-evolution-SUFFIX`. The empty H743 result uses
`nexus_firmware`, not a leftover `io_evolution_fixture` from an earlier failed
strong-retention attempt. The other five use `io_evolution_fixture` and have
actual passing provider-retention reports. The CAN FD build here is the
loopback selection; no normal-bus ARM build is claimed by this table. No cycles,
latency, electrical accuracy or dynamic high-water value was measured.

## Build and dependency delivery

Private official inputs add exactly two U5 LL headers (`dma`, `usart`) and three
G4 FDCAN-definition headers, including their transitive definitions. Pins,
licenses and CMSIS pairing remain checked. The selected closure does not link
HAL handles, a vendor tick, vendor IRQ callbacks or a second transfer owner.

The source SDK now requires the new public/private I/O modules, coupled
configuration code and explicit family payload. `verify_consumers --parallel N`
accepts a strict positive job count for every consumer build. Omitting it
preserves the previous 4-job base/2-job modern defaults. Its Python regression
checks all 13 consumer roots and 16 image targets with external subprocess test
doubles; it does not constitute execution of a new real SDK consumer package.

## Source and remaining qualification

Historical GitHub run [39028022087](https://github.com/X-Gen-Lab/nexus/actions/runs/39028022087)
reports 30 completed jobs with scalar `conclusion="success"` for clean source
`2963c34aa399c43bea05022eb8846ad2370f0f76`. This is the previous committed source.
It does not validate the current working extensions. Job-level documentation
success alone also does not establish a newly inspected whole documentation
package or prove that every documentation step ran.

Before final delivery, bind the frozen source to complete normal hooks,
focused final model reruns, exact ARM resource/retention checks, fresh source
SDK and independent consumers, and current API/English/Chinese documentation
checks. Report each actual result separately. Offline reproduction and a
software candidate require their own executed evidence. Physical HIL remains
`not_executed`; IO-01–42 ledger status is maintained separately and is not
advanced by this page.
