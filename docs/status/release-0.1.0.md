---
id: status.release-0.1.0
type: status
audience: consumer
source_language: en
---

(status-release-0.1.0)=
# Nexus 0.1.0 Source SDK developer preview

Nexus 0.1.0 is the first public platform Source SDK developer preview, dated
2026-10-15 (Asia/Shanghai). Use this page to check its software scope, required
inputs and release evidence before integrating a consumer. Every MCU physical
HIL result remains `not_executed`.

## Source and compatibility

The release asset inventory and SDK manifest identify the actual released
commit/tree and bytes. Documentation packages independently record their
`documentation_source`, `code_source` and historical `qualified_code`; publication
does not transfer old software qualification to a new source. See
[software verification](software-verification.md) for those identity rules.

`dependencies/platform-compatibility.json` is the sole authored semantic-version
policy. CMake, generated headers and installed package metadata derive `0.1.0`
from it. Consumers request `find_package(Nexus 0.1.0 EXACT CONFIG REQUIRED)`.
Source interface epoch `1`, authored TOML schema `2` and exact source-only SDK
matching remain separate, unchanged contracts. The SDK promises no stable binary
ABI; compile the platform, generated bindings and consumer under the same
configuration, compiler and ABI choices.

Prepublication source snapshots used `1.0.0`. They keep their historical source
identities and do not become 0.1.0 packages. The 2026-01-12 Phase 1 changelog entry
is an early development milestone; no corresponding public platform GitHub
tag/release was observed in the 2026-10-15 release inventory.

## Maintained software scope

| Platform | Explicit backend scope | Reference boundary |
| --- | --- | --- |
| Native | Native, ThreadX | Linux/POSIX models and selected real host kernel paths |
| F407ZG Qiming, F407VE Sky, F470ZG Liangshan | Baremetal, FreeRTOS, ThreadX | Exact three initial Board assemblies and their maintained modes |
| G474 MB1367 source reference | Baremetal, FreeRTOS | Fixed HSE24/170 MHz, TIM2, GPIO, IRQ LPUART and selected EXTI fixtures |
| H743 NUCLEO source reference | Baremetal, FreeRTOS | Fixed HSI32/64 and cache profiles; GPIO, TIM2, IRQ UART and finite TX DMA |
| U575 single-world NUCLEO source reference | Baremetal, FreeRTOS | Preconfigured TZEN=0/SWAP_BANK=0, MSIS4, GPIO, TIM2 and IRQ UART |
| Separate U575 paired-world Runtime reference | No OS | Fixed TZEN=1 attribution and bounded synchronous copied GPIO messages |

The [exact support record](../delivery/support.md) and
[derived capability matrix](../delivery/capabilities.md) remain authoritative for
part numbers, routes, controller modes and unknown Board facts. H743 RX DMA,
runtime frequency switching, deep STOP with continuous time and general U575
dual-domain factory/NS RTOS/asynchronous loans are outside this preview's
implemented references. G474's M4 does not authorize a ThreadX Board backend.

CPU-only Runtime covers M0/M0+/M3/M4/M7/M23/M33/M55/M85 software profiles without
creating SoC, Board, startup, routes, linker layouts or factories. Baremetal and
FreeRTOS profile coverage does not expand the Native/M4 ThreadX subset. See the
[CPU support contract](../delivery/cortex-m.md).

## Required package and tool inputs

Use a clean committed source SDK with its full pinned recursive dependency,
reviewed import and license/notice closure. Its `.nexus-source-sdk.json` records
the source commit/tree, dependency identities, complete file hashes and exported
CMake package hashes. A dirty `--development-fixture` is nonpublishable.

The maintained software delivery path uses Linux x86_64. Read the release's actual
tool-version and command receipts for the executed Python, CMake, Ninja, Native
compiler, GNU Arm distribution and any OCI identity. Declared minimum versions
and lock files alone do not prove installation, consumer execution or offline
reproduction. CP311 offline installation and Windows/macOS integrated execution
are not established by the older Linux/CP312 evidence.

From a checkout, use the [source setup guide](../guides/source-checkout.md).
For an installed prefix, follow the [relocated SDK tutorial](../tutorials/source-sdk.md)
and the package's maintained verification commands. Consumer configuration must
reject a changed manifest, wrong source pin, different exact version or invalid
assembly rather than silently selecting another Board or mode.

## Verify the released bytes and actual results

1. Read the release asset inventory, preserve the published archive size and
   SHA-256, and verify the downloaded archive before unpacking it into a new
   directory. Retain the immutable source and asset locations with those hashes.
2. Run the SDK's whole-manifest verifier and actual independent relocated
   consumers against that exact prefix. Inspect their recorded argv, exits and
   raw logs. Native execution and ARM link/resource checks establish separate
   scopes; missing tools, empty reports and skipped-only results do not pass.
3. Match the source, configuration, dependencies, executed tools and artifact
   hashes to each qualification claim. If reference firmware is supplied, inspect
   its exact ELF/BIN/map and resource records. Claim offline reproducibility only
   for two actual fresh network-disabled builds of the same bound inputs.
4. Read the actual provenance bundle and its subject identity when supplied.
   File digests detect changed bytes; an unsigned checksum is not publisher
   authentication or a product-release signature.
5. Keep physical acceptance separate. Software models, ARM linking, SDK consumers
   and documentation publication do not observe Board wiring, power, silicon
   revision, clock accuracy, IRQ/DMA timing, cache coherence or TrustZone isolation.

These are verification requirements, not invented execution receipts. The
release's actual result files determine which software scopes executed. Existing
failed attempts and historical source IDs remain unchanged.

## Historical candidate availability and remaining acceptance

The formal build-system baseline remains `cc6a065c`; earlier SoC baselines are
separate. Its eight unsigned H743/U575 candidates select only HSI64/cache off or
MSIS4/TZEN=0, empty/IRQ-UART and baremetal/FreeRTOS. They do not qualify this new
release source or the broader cache/DMA/paired-world fixtures.

On 2026-10-15, all eight candidate archives were unavailable at the paths in the
[original result index](../delivery/build-system-results.json). Their
`candidate-archives` directory was a symlink into the absent relocated
`/tmp/nexus-docs-polish-20261014/relocated-historical-cache/candidate-archives`.
Historical archive-verification receipts remain evidence of their original
execution; they do not make the archive bytes currently available. The original
indices have not been rewritten. Recover and reverify byte-identical copies
before offering those historical archives as downloadable release assets.

The baseline's complete 72-stage single invocation, OCI recipe reconstruction,
CP311 offline installation, Windows/macOS integration and external-example
upgrade remain unexecuted in that record. A clearly bounded source preview can
retain those limits. Source/interface changes still need their applicable fresh
software checks; old test counts cannot substitute for them.

Every MCU physical test remains `not_executed`. Board electrical behavior,
measured timing/power, Flash power-loss recovery, DMA/cache coherence and physical
security isolation need real admitted station records. Applications, private PCB
facts, safety outputs, recovery policy, certification and product acceptance
remain external responsibilities. This developer preview establishes no product
safety certification, signed product qualification or LTS commitment. See
[physical status](hardware.md) and the
[software/physical qualification contract](../reference/platforms/qualification.md).
