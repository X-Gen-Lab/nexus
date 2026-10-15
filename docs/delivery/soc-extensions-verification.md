# SoC extension verification

This record continues the [earlier SoC baseline](soc-refinement-verification.md).
The [55-item ledger](../design/soc-refinement-execution.csv) retains its original
exit conditions. Implemented software, executed software evidence and physical
acceptance are separate states. Hardware is not connected; every physical result
remains `not_executed`.

The qualified software source is commit
`98270512ea2ff7eefe8ff79720018e582be3a949`, tree
`8d3e69d3179d7b0d2382c5fd074c970dfc9348d9`. All formal runs use the clean
canonical checkout and verify unchanged source inputs. Later documentation
commits record this source; they do not claim a new software execution.

## Executed software verification

| Scope | Complete observed execution |
|---|---|
| Native | 199 CTest entries; 177 GoogleTest/GoogleMock programs, 3252 cases; no failures or skips |
| Configuration and tools | 458 configuration, 585 tooling and 351 CI-tool tests; complete discovery and execution |
| ARM platform matrix | 121 linked images, including all 105 provider assemblies; six expected rejection cases |
| Cortex-M runtime | 104 runtime variants and 4359 actual ARM objects |
| Cache and dual-world reference | 12 cache compile profiles; actual U575 paired images and 15 mutation checks |
| Arch/startup tools | 133 tests across seven complete modules; includes the 15 paired-image mutations above |
| Source SDK | 59 boundary tests, 258 actual child commands, 13 ARM consumers and their maps; negative commands retain their expected nonzero exits |
| Static analysis | Cppcheck 2.13 and clang-tidy 14 each checked all 358 owned translation units |
| External examples | Code `3de8523811b00da8bd887abf53e61eedbaf5161e`, pinned to this SDK: 8 CTests, 43 Google cases, 44 tool tests and 20 ARM images; ten unbound HIL plans |

These scopes overlap and must not be added into a unique-test total. The source
SDK tests intentionally create a nonpublishable development fixture to exercise
failure boundaries. The separately packaged clean SDK requires its own actual
verification before candidate sealing.

[GitHub CI run 38256744920](https://github.com/X-Gen-Lab/nexus/actions/runs/38256744920)
completed successfully for this exact source: 29 successful jobs and one normal
branch-conditioned documentation deployment skip. Debug, Release, ASan/UBSan
and TSan each executed all 199 CTest entries and 3252 Google cases. The artifact
catalog is an API observation; it is not a claim that every remote archive was
downloaded and independently checked.

Eight unsigned software candidates passed sealing and the platform's complete
packed verification. Each has all six required qualification scopes bound to
its actual run-a ELF and configuration. Sixteen independent builds used 32
network-disabled container commands; all eight ELF/BIN pairs are byte-identical.
Both maps and original raw logs are retained. The
[source-bound result index](soc-extensions-results.json) records the precise
candidate identities, independent review and durable archive references.

The final closure check found 102 report references to cleaned SDK fixture paths.
Their 1,871,287 bytes were restored only from already retained, hash-identical
artifacts. Original reports and qualifications were not rewritten; the first
failed closure check remains recorded. All eight complete closures subsequently
passed the original validator. The archived Native executables and test sources
also remain available for audit. Native binary hashes are explicitly bound at
archive time, rather than presented as invented execution-time signatures.

## Exact candidate image costs

These are the recorded GNU Arm 14.3.rel1 reference images, measured from the
linked ELF and configuration. RAM below includes the 2048-byte MSP reservation;
all eight images reserve zero libc heap. These values describe the selected
reference firmware, not a product workload, runtime stack high-water, interrupt
latency or WCET. The result index retains loaded Flash bytes, address footprint,
per-domain RAM and the original budget reports separately.

| Candidate | Flash address footprint (bytes) | Reserved RAM including MSP (bytes) |
|---|---:|---:|
| h743-baremetal-empty | 2440 | 2104 |
| h743-baremetal-uart | 8708 | 2360 |
| h743-freertos-empty | 4828 | 3032 |
| h743-freertos-uart | 11100 | 3284 |
| u575-baremetal-empty | 2124 | 2112 |
| u575-baremetal-uart | 8364 | 2368 |
| u575-freertos-empty | 4548 | 3040 |
| u575-freertos-uart | 10804 | 3292 |

## Implemented scope

| Layer | Added behavior | Exact boundary |
|---|---|---|
| Arch | Caller-owned Cortex-M7 cache transition control | Cold privileged entry, verified cache geometry, explicit ownership and bounded cleanup; other cores reject this particular control operation |
| Arch | Cortex-M33 NonSecure range permission observation | Real CMSE check; observation does not pin storage, copy data or authorize arbitrary peripherals |
| G474 | Static EXTI event providers | Two selected software-fixture lines, bounded rings, pending-bit isolation, overflow order and retained cleanup; PCB routing and electrical behavior remain unqualified |
| H743 | Fixed HSI32/HSI64 startup with cache selection | Four cold profiles; independent divider, timer and cache ownership; this is not runtime frequency switching |
| H743 | USART3 finite TX using DMA1/DMAMUX | AXI SRAM only; explicit buffer borrow, wire completion, cancellation and quarantine; RX remains IRQ byte mode |
| U575 | Exact Secure/NonSecure software reference | Fixed Flash/RAM attribution, restricted NonSecure MPU, actual CMSE gateway, synchronous bounded broker and real paired images |
| SDK | Required private implementation closure | Deleting a required source/header and recalculating the manifest still fails validation |
| Automation | Complete dependency initialization and compiler context | SDK dependencies come from declared locks; static analysis must preserve the actual translation unit's defines and forced includes |

The public device architecture remains static typed factory plus immutable
polymorphic interfaces. Factory lookup does not allocate, start hardware or take
a lock. New modes extend selected private providers and configuration facts;
they do not add a runtime registry, implicit worker or mandatory OS layer.

H743 DMA borrows request and payload storage until acquire-observed SETTLED.
Cache-enabled transfers require the caller to own complete aligned cache lines;
the provider does not round maintenance outward into neighboring allocations or
allocate a bounce buffer. Cancellation and timeout do not revoke the borrow.
Failure to withdraw hardware references retains storage in quarantine.

The U575 reference admits a fixed TZEN=1 boot profile and an immutable
unprivileged NonSecure thread without NonSecure interrupts, an RTOS or concurrent
DMA. It clears and verifies future NonSecure RAM before changing attribution,
checks final protection state, and enters through the actual compiler CMSE
sequence. Its synchronous gateway copies bounded input and checks output before
publication. This does not implement general asynchronous cross-domain loans,
arbitrary userspace tasks, product authentication or secure update policy. See
the [complete contract](../design/u575-trustzone-contract.md).

## Evidence rules

Normal commit hooks execute the complete GoogleTest/GoogleMock, configuration
and tool suites for behavioral changes. Formal qualification additionally binds
a clean canonical commit and tree, actual compiler inputs, raw command output,
configuration and linked artifacts. Failed attempts remain failed evidence;
changing the source requires new source-bound execution.

The verification work exposed missing generated-header input in the H743 ARM
startup test, a fixed virtual-environment path in CPU configuration tests, an
uncontrolled scheduling assumption in a serial-noise test, and lost forced-include
context in static analysis. Sanitizer jobs also exposed register-mask width
conversions in host models and signal-dependent termination observers. Their
regressions retain the actual compiler warnings and production nonreturning
loops; test observers must work without a signal-delivery point inside those
loops. U575 production clock and MPU masks also required explicit 32-bit
operands before bit operations for GCC 13 on the LP64 sanitizer host. The
independent comparison found identical allocated ARM sections for the tested
old/new translation units after normalizing `__FILE__`; this is a scoped code
and storage comparison, not measured cycles or a promise for every compiler.
Regression evidence and final execution status belong with the delivery index.
No suppression or skipped test can substitute for closing these findings.

Offline candidates cover the eight maintained public H743/U575 combinations of
empty/IRQ-UART assemblies and baremetal/FreeRTOS. Each requires two fresh,
network-disabled builds and comparison of actual ELF/BIN bytes, with both maps
and raw logs retained. The H743 cache/DMA test fixtures and U575 paired-image
reference have their own ARM evidence; they are outside those eight single-image
candidate configurations. An unsigned software candidate is not a product
release or physical qualification.

## Durable evidence

The local archive is
`/workspace/nexus-soc-delivery-98270512/platform-evidence.tar.gz`: 1,269,027,807
bytes, SHA-256
`3aae3a6b96701ce1a042cb4e43078d00d59ba5460ebc4816fa68e2fe4a8fe41c`.
Its embedded `delivery-manifest.json` and external manifest describe 216,230
entries, including 173,376 regular files. Original raw paths and record bytes
remain unchanged. Resolve the recorded final execution root beneath `platform/`;
additional roots and nested Native/source archives have explicit member maps.
The result index binds the archive, manifests and verification receipts. An
independent streaming review checked every outer member, all eight candidate
payload closures, the three nested Native/source archives and the exact source
identities from the retained archive bytes; no full unpack or original-root
fallback was used.

All eight candidates also passed actual verification after copying to separate
fresh directories while access to original evidence, SDK, OCI and ARM tool roots
was denied. Original-path read attempts were zero. This verifies their packed
evidence closure; rebuilding still requires provision of the recorded external
OCI image and GNU Arm toolchain, whose binaries are not embedded. The local
archive is not a published GitHub Release asset or a signed product release.

## Remaining acceptance

SOC-28 through SOC-32 and SOC-36 are exact partial references. Their original
hardware, peripheral and isolation exits remain open. SOC-33 bootloader/running
state handoff, SOC-34 dynamic frequency changes and SOC-35 deep STOP with
continuous time are not implemented. SOC-37 through SOC-55 provide station,
admission, capture and result-validation tooling; physical execution remains
pending equipment.

Fixed clock profiles do not establish DVFS. Ordinary WFI or tickless idle does
not establish deep STOP. Host models and linked ARM images do not establish
electrical behavior, DMA/cache coherence on silicon, TrustZone isolation,
interrupt latency, measured power or WCET. Applications, PCB-specific policies
and recovery decisions remain in external projects.
