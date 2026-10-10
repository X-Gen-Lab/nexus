# Maintained platform workflows

`ci.yml` gates main/develop PRs, maintained branch pushes, nightly and manual runs.
Required-job aggregation rejects failed/cancelled results and unjustified skips.
Workflow edits require build, tool, analyzer and documentation validation.

| Workflow | Actual responsibility |
|---|---|
| `build-matrix.yml` | Three Native configurations each execute the complete CTest plan once and validate every GoogleTest/GoogleMock case; six exact Board/backend ARM assemblies; common components; software-only peripheral fixtures. |
| `quality-checks.yml` | Same staged style policy as local hooks; actual compile-database clang-tidy/cppcheck; missing tools or empty scope fail. |
| `enterprise-tools.yml` | Configuration, artifact tamper, budget, HIL admission, real source SDK relocation and command-failure boundaries. |
| `performance.yml` | Three-board O2/Os/O3 ±LTO minimum-image resource comparisons; GC and budget checks; no inferred cycles. |
| `release.yml` | Manual clean source SDK packaging and strict consumers after platform/tool checks; artifact retention, no tag/release/promotion. |
| `security.yml` | Resolved Python dependency audit, actual Native CodeQL contexts and secret scanning. |
| `docs-build.yml` | Public-only Doxygen and warning-blocking Sphinx; existing main-only Pages deployment. |

One authored TOML assembly generates one atomic resolved configuration, typed
factory IDs, readonly polymorphic faces and fixed typed bindings
and physical memory input. CMake owns the code dependency graph. SDK and SoC
implementation headers compile privately; startup/system objects enter firmware
once. Only the three used Git dependencies are initialized, including the pinned
FreeRTOS recursive dependencies. ARM uses the checksum-locked GNU distribution.

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

Physical HIL is currently unexecuted. Named team assignments, GitHub protection,
product deployments, signing and release promotion are external decisions;
workflow files do not create those permissions or qualifications.
