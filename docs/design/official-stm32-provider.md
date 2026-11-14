---
id: design.official_stm32_provider
type: explanation
audience: maintainer
source_language: en
---

(design-official-stm32-provider)=
# Official STM32F4 drivers and Nexus contracts

Production integration is `soc/stm32/stm32f4/`. Select reviewed exact device and
mode facts from [current support](../delivery/support.md), then reuse the locked
F4 HAL/LL and CMSIS Device sources. Vendor types and Handles stay private.
Internal-CRC software references for added parts do not qualify every peripheral,
Board route or physical behavior.

## Responsibility boundary

| Owner | Responsibility |
| --- | --- |
| Official HAL/LL | Selected hardware operations and peripheral state handling |
| F4 C implementation | Resource admission, static binding, deadlines, borrows, callbacks, drain and lifecycle |
| F4 facts | Exact part, device macro, memory, startup, IRQ/DMA and reviewed routes |
| Board | Actual wiring, oscillators, initial levels and electrical conditions |
| Public I/O | Stable Nexus types, checked dispatch and hardware-independent contracts |
| External product | Tasks, execution progress, private regions and recovery policy |

One live hardware behavior domain has one transport owner. HAL and a private
state machine must not independently drive the same transfer, DMA or interrupt
sources. A hybrid mode needs explicit ownership boundaries, not two competing
executors. Direct access requires a specific SDK gap, erratum, atomic ordering
contract or demonstrated resource budget.

## Build-time provider contract

One schema-2 TOML selects a complete assembly. Reviewed JSON facts resolve into
immutable named IR; the generator creates exact storage, configuration, interface
faces, typed factory identities and fixed connections. CMake owns the actual
source and link graph. Do not duplicate source lists in a manifest.

Each image chooses one exact CMSIS device macro and matching startup. Compile
only selected HAL modules and required dependencies, with a consistent private
configuration and callback ABI. Do not enable every module or modify vendor
files. All other inventoried vendor sources remain retained and unselected.

Official source admission is shared only within one CMake configure. The first
official target verifies the complete selected HAL/CMSIS source and SDK identity,
then retains a unique pending input snapshot bound to its SHA256. After all root
deferred work, the same complete verification must match the original content,
filesystem and Git identity. Rewrites with identical bytes also reject the
configure. Intermediate targets still check their own target, device, ABI and
private source/include contracts; they do not inherit another target's setup.

The snapshot is build-local input metadata, not a successful test, hardware
qualification or persistent cache. Final verification consumes a recognized
pending snapshot on success or failure; unrelated malformed files are not
deleted. CMake removes its own pending path after every final subprocess result.
Each new configure starts a fresh admission. Exact existing input files and
missing Git metadata candidates drive reconfiguration; Git lock directories
never become timestamp dependencies.

Default host generation remains centralized Python; Board and SoC trees contain
no Python plugins. The generator emits static configuration and fixed typed
connections. Core's C platform lifecycle engine owns startup, stop, rollback and
diagnostics; F4's `platform_coordinator.c` owns System and shared-resource
sequencing with the concrete owners' original receipts. The centralized host
configuration implementation is not linked into firmware.
Generated factories, C models, linked ARM images and physical behavior retain
their separate validation boundaries.

## Selected F407 HAL UART transport

The current finite `dma-tx` selection combines official HAL TX with a disjoint
LL IRQ byte/event receiver under one private static owner. Handles, DMA state
and callbacks stay private. Other UART modes retain their explicit engines;
this selection does not establish whole-library or all-mode HAL replacement.

DMA completion is distinct from UART wire TC. A callback, successful abort call
or timeout does not authorize reuse of a request or buffer. Detach actual DMA,
IRQ and callback references, prove required peripheral drain, then publish
SETTLED as the provider's final request access. Failed withdrawal retains the
same storage and recovery responsibility.

Do not unconditionally call `HAL_Init()`: default SysTick setup and NVIC grouping
must respect Platform and RTOS ownership. Use the maintained time domain and
validate any kernel ISR ceiling before entering a kernel API.

## F415/F417 basic crypto transports

`STM32F415ZGT6` and `STM32F417ZGT6` select `cipher/CRYP` and `hash/HASH` through
the existing static factory. Each controller independently selects
`irq-blocks` or `dma-blocks`. Selecting one engine's DMA mode does not add the
other engine's DMA implementation or storage. This contract does not enable
F479 enhanced crypto, authenticated encryption, key storage, padding or protocol
policy. SHA1, MD5, DES and TDES availability is a hardware integration fact,
not a recommendation for new security protocols.

The DMA selection adds separate statically allocated channel state and readonly
memory-region connections to the same controller owner. CRYP uses DMA2 stream6
channel2 for input and stream5 channel2 for output. HASH uses stream7 channel2
for input. Platform reserves these streams and vectors; their NVIC entries stay
disabled. The sole serialized task executor services DMA callbacks. The existing
CRYP or HASH_RNG peripheral entry only records its selected engine's facts and
completion hints. There is no worker, IRQ scan, runtime registry or maximum pool.

CRYP DMA accepts aligned complete algorithm blocks and rejects exact in-place
input/output: independent DMA channels do not prove a safe read-before-write
ordering. Key and IV remain standard bytes with the existing format constraints.
Query the selected algorithm's alignment, length multiple and maximum rather
than inferring requirements from CTR or AES alone.

The locked HASH DMA HAL reads complete words. DMA messages are positive,
four-byte aligned and have a byte length divisible by four, at most 262140 B.
HMAC keys follow the same positive whole-word requirements; plain hashes have
no key. `input_length_multiple` and `key_length_multiple` report these rules;
the IRQ transport reports one for both. Digest storage remains byte-aligned:
the provider reads a proven ready digest through the existing CPU byte copy,
without a hidden tail pad, full-message copy or output bounce buffer.

Both transports validate complete descriptor, key, IV, input and output spans
before acceptance. DMA domains come from the selected physical memory facts;
main SRAM is readable/writable, Flash is readable, and CCM is excluded. Memory
TC is not cipher-output completion, HASH digest readiness or request SETTLED.
HMAC retains the caller's key through both official key phases and the message
phase. Before terminal publication, the owner joins the actual channels and
peripheral, withdraws callback/pointer references and clears internal secret
context. Failed withdrawal retains the original request and spans as
QUARANTINED; cancel, timeout and a successful HAL abort call never authorize
their reuse. Caller key/IV storage is not erased by the provider.

The `stm32f4_crypto` references include an F415 configuration with both DMA
transports and an F417 configuration with CRYP DMA and HASH IRQ. They exercise
factory composition and source selection. Model execution and ARM linking
retain their own source identities; no PCB, throughput, cryptographic
certification or physical HIL qualification follows from these references.

## F407 DAC DMA restart boundary

The selected F407 DAC DMA path handles ES0182 Rev19 section 2.6.1 across
requests. After request acceptance, restart first proves that the old DMA,
IRQ and addresses are detached. It installs the declared idle code, uses the
exclusively owned TIM6/7 hardware trigger to observe UDR, then withdraws DMAEN,
converter output and clock before reconfiguring DAC, DMA and trigger state.
The same owner retains every partial effect if cleanup or clock restoration
fails; a timeout never releases the payload early.

Only UDR induced during the explicitly controlled recovery phase belongs to
recovery rather than payload progress or an independent operational error. It
does not by itself replace an expired TIMEOUT with IO. Normal operational
underrun still requires explicit stop recovery; failed W1C clearing and real
DMA errors remain IO with borrows retained. This is not a rule to ignore every
UDR observation.

Recovery may internally convert the declared idle code before waveform timing
starts and briefly disable converter output. These conversions are not payload
progress. No uninterrupted analog hold, glitch-free transition or continuous
waveform is promised between requests. The C model and ARM image do not
establish a silicon-workaround or analog-load qualification. See the
[near-source DAC contract](../../soc/stm32/stm32f4/README.md#dac-输出与触发所有权)
and public `dac_tx.h` requirements; this is a selected recovery sequence, not a
second HAL owner or a general register-script mechanism.

## F479 LTDC scanout and DMA2D memory operations

The selected software reference is STM32F479NIH6. LTDC uses the reviewed AF14
RGB565 or RGB888 wire route; framebuffer memory independently uses RGB565 or
ARGB8888. DMA2D has no external pins or DMA1/2 stream. These selections do not
qualify a panel, PCB, external framebuffer, bus bandwidth or physical output.
FMC presence alone never adds an external region to either engine's DMA domains.

| Owner | Exact responsibility |
| --- | --- |
| System | Live bus/time domain and the one PLLSAI plan; LTDC consumes its R output and optional SAI shares Q |
| Platform | GPIO gates, original pad snapshots and final pad restoration |
| Selected SDRAM bank | Original stable lease and System epoch retained through attached graphics controllers' complete cold withdrawal |
| LTDC | One HAL/controller, two IRQs, selected AF14 pad leases, its gate, two active frame loans and one global pending reload |
| DMA2D | One HAL/controller, its gate/IRQ and one operation with independently checked source/destination spans |
| Application | Pixel storage, panel setup, execution progress, wake adapter and recovery policy |

The shared source-ownership diagram also applies to these two implementations.
Solid edges label runtime interface calls; dashed edges label selected source,
SDK and static assembly relationships. It adds no runtime forwarding layer and
makes no software or physical qualification claim. The table above states the
specific runtime resource ownership for this selected graphics slice.

External graphics memory requires the explicit `graphics-owned` SDRAM endpoint
and each selected engine's `memory_window` reference. The
[SDRAM contract](f407-fsmc.md#显式图形存储模式) defines the bank lease,
per-master grants and cold-stop ordering. Unreferenced engines retain their
internal-memory domains; ordinary CPU windows grant no graphics access. These
are cooperative provider checks, not an MPU or a BusMatrix access firewall.

![FIG-31: F4 source ownership and interface boundaries](../assets/diagrams/driver-source-ownership.svg)

Pure `admit` runs before System/GPIO startup. It validates static extents and
refuses foreign selected gates or vectors without effects or retained references.
`prepare` then acquires each engine's own gate. LTDC programs AF14 through the
locked official GPIO HAL and independently checks the resulting fields. Its
snapshot storage cannot overlap configuration, layer/region facts, System state
or caller pixels. Platform keeps the single original snapshot; the controller
withdraws its leased pads to analog after scanout/IRQ retirement, and Platform
restores their original values later. Partial failures retain the exact receipt
and configuration; they do not authorize reclamation or a second engine owner.

`display` exposes one controller face. `query` and `snapshot` select a physical
layer; `stop` covers the whole controller. A frame becoming visible does not end
its borrow. A replacement can settle the old frame only after its vertical
blanking reload took effect and the relevant active configuration and memory
access retirement were proved. The shared shadow bank permits one global
pending replacement. Cancelling one layer preserves its peer; stopping the
controller joins both layers, all publishers and both IRQs before final release.

LTDC reload/line IRQs capture actual blanking and active-address evidence for the
current stage generation and retirement mask. A reload hint alone is not proof.
Task `service` checks current geometry/domain and joins references before
publishing SETTLED, including when Task execution occurs after the short blanking
window. Old-stage or old-domain evidence is unusable. No hidden worker or
mandatory high-rate Task polling is introduced. Clock loss, unproved retirement
or refused withdrawal retains the affected loans in quarantine. GCR/reset readback
or a CPU barrier alone never proves external memory withdrawal.

DMA2D supports native RGB565/ARGB8888 fill, copy, conversion and straight-alpha
blend through the official HAL. Strides use the corresponding source/output
pixel width. RGB565-to8888 expands channel bits and adds opaque alpha;
8888-to565 truncates channel low bits. Blend alpha is explicitly multiplied or
replaced. RGB at an ARGB result with zero alpha is unspecified; blend-to565 needs
a parameter-proven positive input alpha. The provider does not scan pixels to
admit an operation, allocate scratch pixels or implement a CPU fallback.

Every complete source/destination capacity must satisfy reachability and alias
checks, including request/control/immutable owner metadata. TC is an intent hint;
actual START withdrawal, sources, IRQ, publishers and all address references join
before SETTLED. Raw hardware errors are collected before W1C even if no ISR ran.
An error cannot preserve an old SUCCESS or a valid-output byte count. An already
selected TIMEOUT/CANCELLED remains the first terminal cause; a failed withdrawal
still quarantines and retains all storage. Partial output is not rolled back.

The authored timing uses pixel/line counts and explicit polarities; the generator
only computes static HAL accumulated values. Pixel frequency comes from the
single System PLLSAI R plan and must satisfy the selected DS11118 83 MHz nominal
limit. This check is not physical oscillator, load or panel qualification.
CLUT/extra packed formats, watermark/suspend modes and additional exact parts
remain outside this selected graphics slice.

The maintained combined SRAM and
[`graphics_sdram` consumers](../../tests/contracts/stm32f4_graphics_sdram/CMakeLists.txt)
use the fixture-local `sustained_sequence.c` with two static framebuffers.
One explicit caller drives DMA2D fill, observes image SETTLED, submits display,
and waits for the old display request's SETTLED before rendering into those
bytes again. Four submissions form a finite replacement sequence; each display
request explicitly uses NEVER until replacement or stop. The successful path
then joins both engines, shuts down Platform and runs a second prepared sequence. The
image-only reference retains its separate single-shot consumer. No fixture
helper is a platform service, queue, allocator or hidden worker.

| Observed fact | Storage permission |
| --- | --- |
| Image request SETTLED successfully | Completed pixels can be submitted to display |
| Display accepted, pending, visible or a reload hint | Full frame remains borrowed; rendering into it is forbidden |
| Old display request acquire-observed SETTLED | Its buffer can become the next image destination |
| One frame retired while graphics bank remains attached | Only that frame is reusable; the bank lease still belongs to its cold engine owners |
| Error or failed stop without SETTLED | Retain original requests, both buffers and provider recovery responsibilities |

The explicit `graphics-owned` bank lease grants the selected engines access to
their regions; it never grants simultaneous writes over another engine's
borrowed frame. The SDRAM consumer uses two admitted hardware addresses without
acquiring a public CPU window loan or accessing pixel bytes with the CPU.
Provider snapshots explain a latched failure; neither `visible` nor a snapshot
grants reuse. A failed image is never submitted as a valid display frame.

Three distinguishing composed regressions execute this same consumer sequence
with the actual LTDC C/HAL model and a controlled external image port. They
verify actual retirement before reuse and restart, image failure, and underrun
with retained frames. The real DMA2D program independently verifies image
kernel, final memory access and complete withdrawal; the composed tests do not
replace it or duplicate its pixel model. Fixture controls remain alive through
every failed test assertion. ARM/SDK compile and link validate the exact
selected public composition; physical execution remains `not_executed`.

That software reference selects HCLK 16 MHz, 8 MHz 16-bit SDRAM and a 64×32
layer within a 320×240 timing at approximately 53.76 Hz. Active pixel reads alone
require approximately 0.220 MB/s RGB565 or 0.440 MB/s ARGB8888; DMA2D adds its own
traffic. The SDRAM bus has a theoretical 16 MB/s payload ceiling before refresh,
row commands, turnaround and arbitration. Read burst is disabled, and ES0321
§2.3.4 requires the shared write FIFO to remain enabled (`WFDIS=0`). Neither the
ceiling nor the active-pixel demand proves burst latency or measured throughput.
No fitted memory, panel or electrical timing is qualified; an 800×480×60 RGB565
full frame already requires 46.08 MB/s active reads and is outside this profile.

## Replace a mode and verify it

Start from a complete selected mode and its tests. Preserve RX duties, instance
isolation, cancellation, deadlines, partial-start cleanup, late IRQ and failed
drain. Switch the complete behavior domain to its chosen implementation before
removing the old path; a successful TX fixture alone is insufficient.

Compare actual ELF/map and hot paths under the same toolchain and configuration.
Directory consolidation itself does not reduce dispatch or guarantee smaller
code. See [current support](../delivery/support.md),
[ownership](../explanation/ownership.md) and the
[provider guide](../guides/provider-porting.md). Physical HIL is not executed.

## Cortex-M4F short-ISR constraint

F407 ES0182 Rev19 section2.1.2 explicitly lists Arm776924 for every listed cut.
The exact chip errata, emitted instructions and selected FPU context remain
integration obligations; another CPU name does not grant the same workaround.

The failure requires an enabled FPU and lazy context saving, an interrupted
`VDIV` or `VSQRT` whose destination is s0–s15, a handler without floating-point
instructions, and exception return within fourteen cycles of the interrupted
instruction. The manufacturers' sufficient instruction-count workaround is
**more than two executed instructions in every ISR, excluding exception
return**. External stack-memory wait states are an alternative source condition,
not a premise supplied by these internal-SRAM platform profiles.

F407 startup enables CP10/CP11. Its selected FreeRTOS CM4F
port explicitly enables ASPEN/LSPEN. An integer-only current workload therefore
does not make the platform FPU-disabled or exclude floating-point application
code. Do not silently clear LSPEN in startup: the kernel re-enables it and its
context/stack ABI requires a separately reviewed decision.

Review the actual installed vector table and reachable instruction prefixes in
each emitted configuration. A thin `ldr` plus tail branch needs its callee
examined; unreachable `nop` alignment and literal-pool words are not executed
work. A default nonreturning fault loop is distinct from an empty returning
handler. Apply the same rule to external handlers, spurious/disabled-state fast
paths, aliases and compiler/LTO changes. Source line counts and nominal clock
frequency cannot prove this condition.

Selected-image software review belongs with its actual ELF, resolved input,
compiler commands and disassembly. It proves only those installed paths, not
every declared mode or a connected device result. A new or replacement external
ISR that can return within the excluded short prefix remains an open integration
condition until its emitted path or another exact workaround is reviewed.
