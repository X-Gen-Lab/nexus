# Maintained platform workflows

`ci.yml` gates main/develop PRs, maintained branch pushes, nightly and manual runs.
Required-job aggregation rejects failed/cancelled results and unjustified skips.
Workflow edits require build, tool, analyzer and documentation validation.

| Workflow | Actual responsibility |
|---|---|
| `build-matrix.yml` | Four Native configurations (Debug, Release, ASan/UBSan, TSan) each execute the complete CTest plan and validate every GoogleTest/GoogleMock case; six exact Board/backend ARM assemblies; CPU runtime links; kernel profiles and measured static OS costs; optional MPU and dual-image TrustZone links; software-only peripheral fixtures. |
| `quality-checks.yml` | Same staged style policy as local hooks; actual compile-database clang-tidy/cppcheck; missing tools or empty scope fail. |
| `enterprise-tools.yml` | Configuration, artifact tamper, budget, HIL admission, real source SDK relocation and command-failure boundaries. |
| `performance.yml` | Three-board O2/Os/O3 ±LTO minimum-image resource comparisons; GC and budget checks; no inferred cycles. |
| `release.yml` | Exact-SHA source SDK packaging and strict consumers; optional policy-versioned software preview publication from the trusted main workflow requires fresh complete main CI, existing matching tag, verified assets and actual provenance. Product promotion remains separate. |
| `security.yml` | Resolved Python dependency audit, actual Native CodeQL contexts and secret scanning. |
| `docs-build.yml` | Public-only Doxygen, strict bilingual Sphinx, locked documentation tools and verified prepared publication bytes. |
| `docs-publish.yml` | Verify trusted prepared documentation bytes before Pages deployment; publication has its own source and receipts. |

One authored TOML assembly generates one atomic resolved configuration, typed
factory IDs, readonly polymorphic faces and fixed typed bindings
and physical memory input. CMake owns the code dependency graph. SDK and SoC
implementation headers compile privately; startup/system objects enter firmware
once. Dependency preparation initializes the declared selected closure, including
the pinned recursive FreeRTOS dependencies and required CMSIS/ThreadX inputs.
Source SDK packaging requires its complete declared closure. ARM uses the
checksum-locked GNU distribution.

Native test execution deletes stale JUnit and verifies the actual fresh nonempty
CTest report. ARM compilation and software-register fixtures have distinct scopes
from real hardware. Advanced route fixtures explicitly declare unknown PCB
revisions and cannot be used to claim three-board electrical qualification.

`scripts/ci/native_contracts.py` enumerates the complete CTest plan and every
registered GoogleTest case, then executes CTest once. Its clean environment
requests separate case-level XML from each Google binary, including disabled
cases. Both the CTest identities and Google case identities must exactly match
discovery; missing, extra, stale, skipped or failed reports reject execution.
The commit hook keeps its independent full `tdd_gate.py` execution unchanged.
TSan instruments owned Native providers, adapters and tests. The pinned POSIX
kernel library retains its separate vendor compile policy, so its internal memory
accesses are not a complete kernel race audit. A host-only cooperative Idle hook
allows tick signals to progress under the sanitizer; it changes no MCU Idle hook.
Tool reporting/configuration suites no longer run a second time after every
Native CTest execution. The independently callable tool workflow retains its
complete suites, including on manual invocation. It has one automatic caller
in `ci.yml`, rather than a duplicate push/PR trigger. Maintained branch pushes
and PR checks remain enabled in `ci.yml`.

Filters, shards, empty reports and skipped runs cannot substitute for the
contract set. GoogleTest/GoogleMock
are checksum-pinned host test dependencies; firmware and the source SDK contain
no framework runtime or test archive. TDD records describe executed failing and
passing boundaries; hooks alone cannot prove the order of every local edit.

`tools/hil/prepare.py` creates an unbound station work plan for each resolved
assembly. It checks exact Board/part and UART routes, carries selected modes and
explicitly lists pending IRQ wake, DMA, RX block, ADC trigger/block and controller
isolation measurements. Null fixture budgets require review before physical
admission. Preparation never operates equipment or establishes physical status.

Formal reproducibility uses a declared complete source SDK, content-hashed ARM
input, observed sealed OCI filesystem, two independent empty output directories
and network-disabled execution. `tools/evidence/candidate.py` additionally checks
actual source/config/ELF/BIN/map and all six qualification scopes. Mutable runner
checks and workflow definitions do not establish those claims by themselves.

Physical HIL is currently unexecuted. Codex owns platform maintenance through
the checked agent-managed policy; named people are not required. GitHub
protection, product deployments, signing and release promotion retain their
separate actual states; workflow files do not establish remote permissions or
physical qualifications.

Nexus 0.1.0 is the first public Source SDK developer preview, dated 2026-10-15
(Asia/Shanghai). Its source commit, package hashes, pinned dependencies, licenses
and executed tool tuple belong in the release asset manifests and receipts.
The release process must publish and re-download those exact bytes; a successful
Actions artifact upload alone does not establish durable release delivery.
Historical `cc6a065c` candidates retain their original source and cannot qualify
a new source or version. Their eight archive paths currently point through a
broken relocated `/tmp` directory; retain that availability limitation until
the original bytes are recovered. The [release guide](../../docs/status/release-0.1.0.md)
records scope and verification without rewriting historical result indices.

SDK preparation and strict consumers run in `RUNNER_TEMP`; verified copies enter
owned `build/delivery` staging for observed job identities. Publication requires
all 29 exact CI jobs to succeed on the final main commit within 24 hours. It
checks the existing main and tag again before making the release public.

A failed upload or verification leaves the release as a draft. A retry refuses
an existing release. Inspect its tag, source, uploaded bytes and failure logs;
remove only the incomplete draft after preserving that evidence, then retry the
same reviewed commit with fresh CI. Never delete or overwrite a public release
or move its tag as routine recovery. Source-only preview publication does not
establish physical acceptance or signed firmware qualification.

Every ordinary artifact upload now retains an observed job identity and its
tool-version command logs for 30 days. Documentation uploads include the same
record in the published tree. Actual checkout SHA/tree, recursive gitlinks,
locks, resolved configuration and input closure, executable bytes and uploaded
file digests are checked together. A metadata success cannot replace successful
test execution. Failed or cancelled jobs retain their actual partial outputs.
The source SDK workflow requests provenance for this digest-bearing manifest,
then preserves the actual returned attestation bundle in a separate upload.
Attestation status comes from an actual successful workflow and its returned
bundle. This configuration alone does not establish an attestation, a signed
product release or physical qualification.


`dependency-maintenance.yml` defines a Monday 03:00 UTC read-only observation
job and manual dispatch. It retains actual locked revisions, compatibility
checks and bounded public upstream refs under an observed source identity.
It never changes a dependency lock or opens an unverified upgrade. A candidate
upgrade needs license, model, ARM, SDK and reproducibility evidence for that
exact source. Scheduled execution begins only when this workflow is present
on the default branch; the definition does not claim an executed weekly run.

The [release maintenance guide](../../tools/release/README.md) defines the sole
version policy, source and builder identities, complete raw consumer transport,
and the separate roles of historical delivery and evidence branches.
