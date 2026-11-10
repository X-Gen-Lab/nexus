# Nexus platform maintenance

Nexus owns reusable embedded mechanisms. Applications, product roles, protocol
register policy, workers, private PCB data, Flash reservations and recovery policy
belong in external repositories. Application examples live in the private
X-Gen-Lab/nexus-examples repository; access to Nexus does not grant access to it.
Public platform evidence and authenticated examples evidence keep separate
publication boundaries.

Codex owns all platform maintenance domains under the checked agent-managed
policy. Named people are not required. Preserve automated gates and independent
agent review for high-risk changes; never invent GitHub CODEOWNERS accounts or
claim remote branch protection from a local policy file.

Read [the design contracts](docs/design/README.md),
[delivery boundaries](docs/delivery/README.md) and the relevant layer README
before changing behavior. Historical contracts and results remain in immutable
[Git history](https://github.com/X-Gen-Lab/nexus/tree/83a27dfbee4e8fbf4d35875b1e9db85af7516eec).
They do not establish current support or qualification.

## Architecture and configuration

- The production SoC scope is STM32F4, under `soc/stm32/stm32f4`.
  Exact parts select reviewed device, memory, clock and route facts; a family
  directory or official HAL module does not establish part/mode support.
  Keep all existing `vendors/`, gitlinks, source locks and license records.
  Unselected inventory dependencies never enter default preparation, validation,
  compilation or SDK payloads. Never remove them during platform pruning.
- Board and SoC packages contain C11, necessary startup assembly and declarative
  facts, with no Python modules. Host configuration code belongs in `tools/`.
  F4 implementations and reviewed IP code belong in the F4 package; no extra
  top-level MCU driver registry or runtime forwarding layer is required.
  Native models belong in `tests/support/native` and require explicit test
  support; they are not a second production SoC.
- Keep the default C11 path statically assembled and allocation-free: Core,
  Arch, SoC, typed I/O, optional OS/owner adapters, optional common components.
- Maintain firmware hardware algorithms directly in C11 (with necessary Arch
  assembly): register access/readback, admission, waiting, peripheral lifecycle,
  rollback and recovery. Central host tools validate and generate static
  configuration, storage, faces, factory and fixed connections. Generated thin
  IRQ entries and explicit typed call ordering are permitted. Do not maintain
  hardware algorithms as generated C strings or introduce a runtime register
  script interpreter. Track remaining migration by complete selected paths,
  including preflight, platform resources, shared owners and resume.
- Prefer reviewed official HAL/LL/SPL for hardware operations and complete
  transports that satisfy the selected mode. Vendor types and Handles stay
  private. One live controller behavior domain has one hardware transport
  owner; Nexus bridges admission, borrowing, deadlines, callbacks and full
  withdrawal. Keep direct access only for an explicit SDK gap, erratum,
  atomic contract or demonstrated budget. Evaluate the complete mode before
  replacement; a TX-only fixture does not preserve a mode with RX duties.
- Public device APIs use Nexus types and immutable interface faces. Vendor SDKs
  and mutable provider state remain private. Instances share read-only typed
  operations tables; callbacks receive the instance context, never the table.
- A generated typed factory returns existing static objects by compact ID. It
  never allocates, initializes hardware, acquires locks or discovers devices.
  Do not introduce runtime name registries, maximum object pools, hidden workers,
  mandatory OS locks or forwarding-only runtime layers. Controller and endpoint
  identities remain separate; no endpoint substitutes for its controller.
- SoC facts and Board `board.json` are machine-maintained JSON. One authored TOML
  assembly resolves through a validated typed IR into one atomic bundle. CMake
  targets own software dependencies. Do not introduce Kconfig/cache fallback,
  competing configuration authorities or a second source build graph. Authored
  platform input is TOML schema 2 only; reject legacy JSON assemblies. Production
  resolution and emitters use immutable named records, without a mapping facade
  or duplicate decision values. JSON facts and keyed immutable indexes remain
  explicit leaf data; resolved JSON is a one-way audit/artifact projection.
  Frozen historical evidence retains its original bytes and read-only verifiers;
  that retention never reopens a legacy production configuration path.
- Reject unknown fields, unsupported modes, resource conflicts and stale inputs.
  Never repair configuration by silently selecting a different mode or Board.
- Distinguish a chip capability from a reviewed PCB route and physical HIL.
  Unknown PCB revisions and electrical facts stay explicit. Do not invent wiring.
- Startup does not start a scheduler. Applications own tasks, wait adapters,
  queue depths, stack/TCB storage and their shutdown sequence.

## Ownership and correctness

- REJECTED admission has no retained references or residual hardware effects.
  ACCEPTED transfers borrow request and buffers until acquire-observed SETTLED.
- SETTLED is the provider's final access. Detach hardware/IRQ/queue references
  before release-publication. Cancel, timeout or a failure to drain never revoke
  a borrow; retain QUARANTINED storage and expose recovery responsibility.
- Cross-task controls address stable slots and monotonically increasing epochs.
  Drain controls and join observers before reclaiming a slot or request.
- Use absolute monotonic deadlines in one domain. Save and restore incoming
  interrupt masks. Keep peripheral drain, wire completion and request completion
  distinct. Do not remove barriers or volatile reads needed by the contract.
- Physical Flash drivers expose geometry. Products explicitly supply erase-aligned
  regions. Persistence policy, authentication and migration remain external.
- IRQ notification is a hint. The latched request predicate remains authoritative.
  Validate the FreeRTOS syscall ceiling before any kernel ISR API.

## Tools, style and validation

Documentation changes follow the [content and diagram standards](docs/design/documentation-standards.md)
and [page templates](docs/design/documentation-templates.md). Architecture pages
include or reference diagrams that distinguish current implementation, target
design and exact reference configurations. Review arrow meaning, ownership,
failure paths and source identity; rendering does not establish correctness or
hardware qualification. Preserve one authoritative contract and existing style.

The offline `nexus-docs` hook checks complete staged documentation inputs and
production configuration examples without installing tools or using the network.
Prepare the hash-locked documentation tuple explicitly with
`python tools/documentation/install.py`; `dev.py docs test` executes the complete
documentation suite using the same nonzero, no-skip result admission as other
tool tests. Strict bilingual builds and whole-site translation checks use that tuple.
Every current page has complete English and Chinese prose through one canonical
source and reviewed gettext catalogs. Mixed-source contracts may use a reviewed
catalog for their declared source locale too. Keep symbols, configuration keys
and original evidence values intact. A stale, fuzzy or missing translation fails
current-site admission; historical archives retain their frozen language scope.

Install commit tools with `python scripts/setup/install_dev_tools.py`, then
`.venv/bin/python -m pip install -r dependencies/environment-tools.txt` (Windows:
use `.venv/Scripts/python.exe`). This installs pre-commit and commit-msg hooks;
no hook bypass or automatic staging is allowed. CI uses the same checks.

Use `python tools/dev/dev.py configure|build|test --preset <name>`; native CMake
arguments pass through. `doctor` checks tools and `check` runs repository gates.
Fresh JUnit, nonzero tests and actual return codes are required. Missing tools,
empty reports and skipped-only executions fail qualification.

TDD is mandatory for new production behavior and defect fixes: write the
regression/contract first, execute it and record the expected RED failure, then
implement the smallest correct change, execute GREEN and refactor while keeping
tests passing. New C API and provider tests use GoogleTest and GoogleMock on the
host; Python tools retain stdlib unittest. Mock external dependencies and hardware
ports, not the implementation under test. Verify rejection, cancellation, drain,
overflow and instance isolation where the changed contract requires them.

Focused harnesses retain the maintained target's compiler, language standard,
warning flags and defines from its actual `compile_commands.json` entry. A
differently configured harness is narrower evidence and does not establish
acceptance of that target.

Parallel TDD has an explicit handoff barrier: the test owner records the actual
expected RED execution and snapshots the tested source and binary before
releasing the implementation owner to change behavior. The implementation owner
waits for that receipt, not a proposed test or a promised run. Reconstructed
source is never contemporaneous evidence of an earlier RED execution. Disclose
any draft that predates its tests; retain later genuine RED/GREEN fixes without
rewriting the earlier chronology.

`python scripts/ci/tdd_gate.py --all --preset native-debug` builds and runs
all offline GoogleTest/GoogleMock contracts and the four complete tool domains.
The gate checks locked tools required by the selected domains (Native plus ARM
for tool fixtures), configures once and builds once before test
execution. It never reuses successful reports. The ordinary commit hook selects
complete affected tool domains from the complete staged index, including deleted
paths; it always runs the full Native GoogleTest plan for behavioral changes.
Broad build/configuration-policy changes select all four tool domains.
`--tools none` is available for an explicitly separate Native CI execution only;
the commit hook uses the ordinary changed-domain policy. Independent CI owns the
retained tool suites and all four Native runtime modes. Do not disable tests,
use runtime filters/shards, or bypass hooks to create acceptance evidence.

The hook rejects differing unstaged/untracked behavior before and after execution
and requires an unchanged Git index tree. Record actual RED/GREEN commands and
failure reasons in the PR. Automation verifies execution, not historical order.
C++17, GoogleTest and GoogleMock belong only in host tests; firmware stays C11.

Deliver one complete functional increment at a time: implementation, focused
regressions, current documentation and the ordinary complete-index gates. Report
the exact working behavior and remaining gaps; commit and test counts alone are
not completion. Retain original evidence, but track it once in a compact archive
or a recoverable artifact with an exact source and member manifest. Reference
that copy instead of committing repeated before-images, transport archives and
control bundles. Do not build new evidence frameworks for routine changes.

Apply the test-retention and execution rules in the
[engineering handbook](docs/design/engineering-handbook.md). Remove a test only
after identifying its obsolete contract or equivalent independent coverage;
preserve meaningful failure regressions. Move mode-independent Python and SDK
checks to the explicit tool domains instead of repeating them in each Native
preset. Keep all four Native C/Google modes and the ordinary complete-index
commit gate. Shared contracts run once per meaningful implementation class;
small compile-time variants retain their distinguishing IRQ, ABI and feature
contracts instead of repeating the entire behavior suite. Analyze every distinct compiler context; bound concurrency and
retain cancellation, diagnostics and fresh reports. Cache verified tool inputs,
never successful test reports.

Keep the existing `.clang-format`, `.editorconfig` and backslash Doxygen style:
80 columns, four spaces, attached braces and type-attached pointer stars. Sources
have file/brief/author/version/date/copyright; headers have file/brief/author and
public context, deadline, ownership and failure contracts. Implementation comments
explain invariants and hardware ordering. Do not duplicate declaration parameter
lists in source definitions. New or changed files pass whole-file checks. Frozen
historical debt may only shrink; never rebaseline new defects.

Before a parallel handoff, run `python scripts/ci/style_gate.py <changed-files>`
as well as the focused behavior checks. Clang-format alone does not check the
repository's comment contracts. Record the actual commands and results before
claiming that formatting and comments passed.

Run the maintained host/model and tool boundary suites. Changes to interrupt,
lifecycle, persistence or concurrency need meaningful failure-path regressions.
Compile common components for maintained F4 parts. Native models establish software
behavior, ARM ELF checks establish linking/resources, and physical station results
establish hardware behavior. Never inherit old test counts or hardware status.

Builds, tests, SDK checks and physical HIL have separate scopes. Preserve fresh
raw failures and actual source/configuration identities in build outputs and PRs.
Retired candidate, promotion, archive recovery and task-ledger engines are not
platform runtime/build requirements. Never restore them merely to retain an old
test count. No software result is a product release, safety certification or LTS
promise. Physical tests remain `not_executed` until equipment executes them.
