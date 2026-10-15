# Build system execution record

This record covers the [48-item ledger](../design/build-system-execution.csv).
Its requirements and original exit conditions remain unchanged. The qualified
code source is `cc6a065c002041234a212a964be863a69198f9b4`,
tree `de0e687f659a22badb55259b2e03646fc7b748d1`. Later
changes to these documents record this code source and do not claim a new code
execution at their HEAD. The [result index](build-system-results.json) binds
actual receipts, archive navigation and byte identities. The fixed primary
execution index is
`/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json`,
SHA-256 `b42e1857f6802cbabe35d10f515fd9033ee3bb0bbb40368a50222927c5a45e47`.

The implemented build path remains one authored TOML assembly, immutable typed
IR and static typed factory, with one CMake target graph executed by Ninja.
Configuration owns atomic publication and selected hardware facts; CMake owns
software dependencies and scoped compile/link policy. Products continue to own
application sources, composition, scheduling and deployment policy.

## Final-source software gates

The exact staged code tree passed ordinary commit hooks, including the complete
TDD and commit-message gates. The index and behavioral source stayed unchanged
through execution. The subsequent canonical GitHub commit contains the same tree.
Final qualification separately executes the clean committed source with fresh
unfiltered discovery and reports. No hook bypass, test filter, empty report or
skipped-only execution establishes acceptance.

| Scope | Actual final-source result | Receipt |
| --- | --- | --- |
| Complete Native CTest | 200 entries | `/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json` |
| GoogleTest/GoogleMock host contracts | 177 executables / 3252 cases | `/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json` |
| Tool/evidence contracts | 677 cases | `/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json` |
| Configuration/IR/transaction contracts | 480 cases | `/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json` |
| CI runner/checker contracts | 369 cases | `/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json` |
| Workflow orchestration boundaries | 24 cases | `/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json` |
| Both complete Source SDK boundary modules | 62 cases, 280 child commands | `/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json` |

All final-source suites in the table have zero failures, errors and skips. Expected
nonzero child commands test rejection and retained failure evidence; they are not
suite failures. Counts overlap and must not be added into a platform test total.
GoogleTest and GoogleMock remain host-only. Production firmware remains C11 and
contains no C++ test framework or its runtime.

The real small CMake projects exercise protected inputs, output publication,
identical reconfigure/no-op builds, derived artifact repair, configuration and
resource dependencies, external LTO and C/C++ policy propagation, invalid helper
arguments, mixed assembly rejection, private includes and process cancellation.
Source SDK fixtures deliberately mutate packages and remain nonpublishable. The
candidate path separately generates a clean committed SDK and verifies its whole
manifest before consumption.

The selected SDK result is attempt 2 in `sdk-boundaries-attempt-2`, with its
matching explicit receipt. It executes all 62 cases in 673.078 seconds and
retains 280 actual child commands and 13 consumer artifact directories.
Attempt 1 remains failed in the original `sdk-boundaries` namespace: missing
CMake on its PATH caused six errors and six skips, with zero test failures.
No report or failed namespace was renamed or replaced to select attempt 2.
The SDK suite runs serially with a one-job environment default; original
consumer build argv explicitly request four or two jobs where specified.
The offline candidate compiler uses its separately recorded bound; the SDK
execution is not described as all compiler commands running with one job.

## Actual GitHub execution and CI corrections

The final-source push run
[38294880491](https://github.com/X-Gen-Lab/nexus/actions/runs/38294880491) completed with `success`:
29 successful jobs, zero failed jobs, and one expected
main-only `Documentation / Deploy to Pages` skip. All job pages, actual executed
job logs and the artifact metadata catalog are retained. The receipt binds the
run, jobs and referenced workflow to the exact canonical code source.

The four Native debug/release/ASan/TSan profiles each execute their complete
reported CTest and GoogleTest scopes. ARM, Cortex-M, OS, provider, installed SDK,
quality and security scopes retain their actual individual results. All 27
artifact ZIP bytes were downloaded, digest-checked and CRC-checked.
Twenty-six Nexus job identities and 25 required selected-dependency closures
were verified; the Pages ZIP retains GitHub artifact byte identity without a
Nexus job identity. The sealed CI index covers 131 files / 104,572,091 bytes.

The earlier `acc70f30` run
[38280227545](https://github.com/X-Gen-Lab/nexus/actions/runs/38280227545)
failed and remains failed evidence. Its successful Native jobs do not qualify the
failed aggregate run or this later source. The fixes make scoped preparation
receipts match actual selected dependency closures, separate canonical-budget
byte tampering from measured authored-budget overruns, bind expected ThreadX
link failure to the exact Ninja ELF edge, and execute all four Python tool
domains through the maintained complete runner. Each fix retains real RED/GREEN
execution and independent review. The subsequent `3e7c0d50` run also failed:
three Tool Contract cases still addressed the removed GD32 helper. Its full raw
196-case execution remains failed evidence; `cc6a065c` restores the checked
contract entry point and retains the helper semantics. The final passing run
above must establish this correction separately.

## Offline software candidates and provenance

Eight exact H743/U575 reference profiles pass two fresh network-disabled builds
per profile: H743 HSI64 with Cache off and U575 MSIS4 with TZEN=0, each with
empty/UART selection and baremetal/FreeRTOS backend. The matrix records sixteen
fresh builds and 32 actual network-none container
commands. It does not qualify H743 Cache/DMA or U575 dual-domain firmware.

The actual selected-image resource observations are below. Flash LOAD counts
physical initialization bytes; footprint includes the occupied address span.
Reserved RAM includes the 2,048-byte MSP once and zero heap, rather than measured
stack high water. These are separate selections, not MCU cycle or latency results.

| Profile | Flash LOAD bytes | Flash footprint bytes | Reserved RAM bytes |
| --- | ---: | ---: | ---: |
| h743-baremetal-empty | 2437 | 2440 | 2104 |
| h743-baremetal-uart | 8708 | 8708 | 2360 |
| h743-freertos-empty | 4828 | 4828 | 3032 |
| h743-freertos-uart | 11100 | 11100 | 3284 |
| u575-baremetal-empty | 2121 | 2124 | 2112 |
| u575-baremetal-uart | 8364 | 8364 | 2368 |
| u575-freertos-empty | 4548 | 4548 | 3040 |
| u575-freertos-uart | 10804 | 10804 | 3292 |

The immutable Native/tool/SDK proof remains under `cc6a065c`; the new candidate
execution uses the separate `cc6a065c00` namespace. The same clean committed
Source SDK bytes are fully reverified and copied into independent fresh build
roots. No failed matrix output or compiled object is reused.

The same source, clean Source SDK, authored configuration, recursive dependency
pins, compiler/toolchain and OCI identities bind the executed commands and actual
ELF/BIN/map/resource bytes. Each candidate passes the owned verifier and an
actual fixed-checker `dev candidate` plan. Each complete archive is streamed and
verified before only its owned live profile scratch is retired. Original live
artifact paths then remain historical locators; the result index supplies the
archive and member-relative navigation used for current verification.

Two earlier matrices remain failed with zero qualified profiles: the first
exhausted storage during reproduction, and the second passed two fresh builds
but could not seal after temporary SDK reference files had been cleaned up.
Independent review restored 102 exact original references from precaptured
identities and verified source and original SDK proof records unchanged. The
failed scratch was fully archived before retirement. These are storage operations;
they do not add source behavior, a new SDK test execution or platform support.
The new matrix uses the separate `cc6a065c00` namespace and qualifies all eight
profiles from its own fresh builds and actual executed candidate plans.

Independent archive-only review passed for all eight archives after every
original live profile was absent. It verified 168,468 members / 150,508 files and
3,356,224,498 uncompressed file bytes, executed the owned candidate verifier on
each packed candidate, and independently reconstructed and fully verified each
packed Source SDK. Private temporary copies were removed afterward. The same
source and matrix remained unchanged. The main receipt is
`/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json`,
SHA-256 `1c127cb2a348b62e07809cfa7ce7b784f57c45cac7cca3f58922c5e973817c12`.
Local archive retention does not establish off-site replication or publication.
Candidates remain unsigned
software candidates, with no product-release or hardware acceptance claim.

The maintained `tools/dev/plans/software.json` declares 72 reviewed stages.
The individual software gates and eight actual candidate plans are recorded
separately. **The complete 72-stage plan did not execute end-to-end through one
qualification-plan invocation.** Its plan contracts and genuine failed-checker
fixtures establish orchestration behavior; no separate execution is invented.

## Offline tools and environment boundaries

The final Linux x86_64 CP312 lock contains 15 hash-locked wheels. A fresh CP312
virtual environment executes a no-index, hash-required installation. Actual
`pre-commit install --install-hooks` primes the Python hook with PEP517 build
isolation enabled, and the isolated interpreter executes the complete tool
domains. Setuptools is included for local PEP517 backend preparation; the hook
has its exact PyYAML, pre-commit and clang-format runtime closure. Vendored nested
wheel metadata remains covered by the whole-wheel digest, while top-level
canonical distribution identity and duplicate metadata are checked strictly.

CP311 wheel bytes are verified against the declared lock. CP311 installation is
`not_executed`. The original 14-wheel runtime bundles remain historical artifacts
and are not relabelled as the new hook closure. Linux is the executed host scope;
Windows/macOS integration and non-Linux descendant cancellation remain untested.

The recorded OCI and full ARM toolchain archives were actually transported,
restored and byte-verified at their own recorded source scopes. The new candidate
builds consume those same artifact identities. The OCI's Python/Git observations
remain distinct from the development host. This proves artifact reuse, not
recipe reconstruction.

The recipe declares fixed base/frontend digests, Debian snapshot and five exact
APT packages, plus the hash of the final CP312 requirements. Emitted Dockerfile
and ephemeral proxy-CA contracts are verified. Reconstruction of the observed
OCI image bytes has not executed successfully; `recipe_reproducible` stays
`false`.

## Reading the 48-item ledger

`implemented_software_validated` records a completed mechanism with final-source
software evidence. More specific states retain measured decisions, actual tool
installation/transport, verified archive restoration and truthful scope limits.
They do not turn every requirement into a physical or product exit condition.

BLD-18 retains full SDK byte checks at each consuming boundary after measurement;
no persistent verification bypass is introduced. BLD-31 retains measured compiler
unit and selected linked-image costs without a speculative source split. Their
`decision_verified` states are intentional results. Prior timing/cache/LTO
measurements retain their original source and selected-profile identities.

BLD-38 records actual candidate plans while explicitly retaining the absence of
one complete 72-stage software-plan invocation. BLD-39 verifies the dependency
upgrade validator's evidence contracts; a real vendor dependency upgrade was not
performed. BLD-40 records actual CP312 installation and hook isolation, and
preserves the missing CP311 installation. BLD-42 retains declared-recipe versus
successful reconstruction as separate states. BLD-44 records actual preserved
current archives and archive-only verification, without claiming off-site storage.

Earlier normal-hook failures, the isolated-environment installation failure and
the failed GitHub run remain immutable failed attempts. Later passing attempts
use new receipts and never overwrite their historical outcomes. No physical
station is connected: every HIL result remains `not_executed`. This iteration
makes no new MCU cycle/WCET, electrical, power-loss, cache/DMA coherence,
TrustZone isolation, external-example, certification or product-release claim.
