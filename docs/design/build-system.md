# Build system maintenance contract

Nexus uses one authored TOML assembly, a validated immutable configuration IR,
static instances and typed factory, and one CMake software target graph. Ninja
executes the declared graph. Products own application sources and composition.

See the [verification plan for additional MCU families](#verification-plan-for-additional-mcu-families)
for proposed changes to commit checks, MCU onboarding and release verification.
The [build architecture review](build-architecture.md) traces the firmware target and gives the CMake refactoring sequence.

The [48-item execution ledger](https://github.com/X-Gen-Lab/nexus/blob/83a27dfbee4e8fbf4d35875b1e9db85af7516eec/docs/design/build-system-execution.csv) preserves the requested
exit conditions. Implementation, executed software verification, environment
distribution and physical acceptance have distinct evidence. Historical source
qualification does not qualify changed build tools.

Configuration publication is a single-writer transaction. It protects every
authored and fact input before changing owned output. A rejected or interrupted
attempt cannot expose the previous configuration as current. Successful identical
generation preserves file content and timestamps so ordinary reconfiguration
does not force unrelated compilation. A configuration digest is not a compiler
or generator-source identity.

CMake owns source lists and target dependencies. The resolver emits selected
facts and its configuration-code dependencies, never a parallel software build
graph. The selected `package.json`, its declared facts and binding files, and
the generic configuration-code closure are explicit generation inputs. The
package also declares Source SDK export ownership. Board and SoC contain no
Python plugins; production uses the generic DataProvider rather than family
constructor imports. Package enumeration does not select C source units.

Physical Platform targets connect selected modes to their necessary public I/O
and private provider targets. `Nexus::IOUART` supplies byte UART, shared format
validation and necessary wake support; `Nexus::IOUARTWord`,
`Nexus::IOUARTStream` and `Nexus::IOUARTRS485` supply explicit extensions. An
application using RS485 links its target even when its adapter is assembled
outside TOML. Native and CPU Runtime retain caller-defined provider composition.
Headers and compile database entries do not prove that a target was built;
inspect the actual command closure, objects and linked sections.

SPI public helpers require wake support, while selected SoC DMA modes connect
the DMA target explicitly. `Nexus::IOADCStream` separates ADC stream-profile
code and dependencies from the base ADC domain; selected ADC stream modes connect
that target. ADC-group block modes connect the shared stream/DMA targets as needed.
Block and USB keep shared DMA validation dependencies
where their actual paths require them. Splitting follows function references and
working consumer builds, not filenames alone.

The generated schema-2 `resolved.json` contains effective selections and their
input identities, while complete candidate facts remain in the resolver and
query path. This reduces output duplication without introducing a second
configuration authority or source graph. Generation size, actual compilation
work and final Flash/RAM costs are separate measurements.

The selected provider supplies its actual private include directories through
the existing `NEXUS_SYSTEM_PRIVATE_INCLUDES` connection. Root CMake consumes that
declaration with PRIVATE scope; it no longer owns a fixed list of F4 IP paths.
External provider bindings therefore do not automatically inherit F4 private
headers. Mode/IP selection and supplier sources remain in the one CMake graph;
this connection adds no registry or second build framework. Private compile templates
validate required specialization bindings and preserve wrapper-owned macro scope;
CMake still selects and checks each actual translation unit.
ABI, optimization, diagnostics and firmware link policy have explicit
target boundaries. The firmware target guarantees its declared delivery outputs
and resource check. External application warnings are a consumer policy.

Developer presets and native CMake/CTest remain the normal build interface.
Focused tests are development feedback. Commit gates and formal qualifications
execute their complete declared suites with fresh reports. All runners preserve
actual command exits, bounded concurrency, deadlines and process cleanup.

Preparation may retrieve dependencies. Formal reproducibility uses two fresh
network-disabled builds with the same verified source SDK and recorded environment
and toolchain bytes. Compiler caches improve development only; they cannot prove
fresh-build reproducibility. Durable evidence stores retain original records,
member identities and failed attempts without changing their historical status.

Behavioral fixes use actual RED then GREEN execution. Python tools use unittest;
C/C++ contracts use the pinned GoogleTest/GoogleMock host framework. CMake graph
and policy defects require real small consumer builds and artifact observations.
Firmware stays C11 and contains no host test runtime. Normal commit hooks and
independent review apply to this maintenance work.

The following support matrix records observed host tools and declared locks.
The final clean-source software gates and eight exact offline candidates are
recorded in the [execution record](https://github.com/X-Gen-Lab/nexus/blob/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/delivery/build-system-verification.md).
Host observations retain their own scope; source qualification uses the recorded
code commit and never follows a later documentation HEAD automatically.

| Host or execution path | Maintained boundary | Current evidence |
| --- | --- | --- |
| Linux x86_64 development host | Configuration, Ninja, Native/ARM consumers, offline tools and evidence transport | Actual local execution; Linux 6.18.44 and glibc 2.41 observed |
| Linux descendant cancellation | Bounded `/proc` inspection, group/descendant freeze, kill and terminal-state observation | Actual nested-process and hostile signal-handler regressions |
| Linux/amd64 OCI artifact | Sealed manifest, config and layer bytes; local offline load | Actual export, independent archive restore and Docker load/inspect |
| Windows development paths | Bootstrap selects `.venv/Scripts`; CMake and tool paths remain explicit | Integration not executed in this maintenance run |
| macOS development paths | Python/CMake tooling entry points; host-specific compiler setup | Integration not executed in this maintenance run |
| Other hosts or cross-host ARM distributions | Require a separately reviewed tool lock and execution matrix | Not established by Linux archive identities |

Linux descendant cleanup depends on Linux process observation. Generic process
termination paths on other hosts do not establish the same nested-process
acceptance. Windows and macOS path handling is an implementation boundary, not
an integrated test result.

| Tool | Declared requirement or lock | Actual local observation |
| --- | --- | --- |
| Python | Minimum 3.11; local lock 3.12.14 | CPython 3.12.14 |
| CMake | Minimum 3.31; tool lock 3.31.6 | 3.31.6 |
| Ninja | Tool lock 1.13.2; single configuration | `1.13.2.git.kitware.jobserver-pipe-1` |
| Native compiler | GNU 14.2.0 in the environment lock | GCC/G++ 14.2.0, Debian 14.2.0-19 |
| ARM compiler | GNU Arm 14.3.rel1, Linux x86_64 archive lock | GCC 14.3.1, build dated 20250623 |
| clang-format | 14.0.6 | 14.0.6 |
| pre-commit | 4.3.0 | 4.3.0 |
| Optional ccache | 4.14.1, Linux x86_64 static distribution | Actual binary and cache hit/miss validation |
| Git | Source and submodule identity tooling | Host 2.52.0; recorded OCI artifact 2.47.3 |
| GoogleTest/GoogleMock | 1.16.0, pinned Git submodule `ext/googletest` and matching lock commit | Host-only framework; final full-source execution separately recorded |

Ninja single-configuration builds are maintained. Ninja Multi-Config is
explicitly rejected; other generators have no implied acceptance. C11 remains
the firmware language. Native test targets enable C++17 without linking that
test runtime into firmware.

The Linux x86_64 CP312 offline tool closure contains 15 hash-locked wheels,
including the Setuptools build backend required by isolated local pre-commit
preparation. A fresh CP312 virtual environment completed an actual
`--no-index --require-hashes` installation and real isolated hook initialization.
The complete isolated tool domains retain their actual recorded source scopes;
the final ordinary commit gate separately executes the current source. CP311
wheel bytes were checked against their 15-wheel lock; CP311 installation was
not executed. The minimum Python version and wheel inventory do not substitute
for that missing installation result.

The recorded OCI artifact uses Python 3.12.15 and Git 2.47.3, distinct from the
current host versions. Its manifest is
`e936b188b9e30cf37ce109a5407019c91fe2dbc62fb10afceb3f2abbfc823ac8` and
its image config is
`a1ba03c16d424aef88dad6e5b2ead2892734a0bf909dedc23d8855fa4861fbe0`.
The actual ARM toolchain tree contains 7,190 files and 1,106,755,364 bytes with
tree SHA-256
`44d2fe14e401730074302ad87bdef9762a6cfe12de4bebc6853d181fd51c74fd`.
Toolchain and OCI transport executions retain their own tool-source identities;
their results do not transfer source qualification to later tool changes.

Fixed base/frontend digests, Debian snapshot selection, five APT package
versions and hash-required Python inputs are declared in the recipe lock.
Reconstruction of the observed OCI bytes has not executed successfully:
`recipe_reproducible` remains `false`. Verified artifact reuse and declared
recipe inputs therefore remain separate acceptance states.

The execution ledger keeps all 48 requested requirements and exit conditions
unchanged. Its updated paths bind the implementation, original RED/GREEN
records and the final-source software gates in the
[execution record](https://github.com/X-Gen-Lab/nexus/blob/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/delivery/build-system-verification.md) and
[result index](https://github.com/X-Gen-Lab/nexus/blob/83a27dfbee4e8fbf4d35875b1e9db85af7516eec/docs/delivery/build-system-results.json). Historical measurements
retain their source and exact selections instead of inheriting a later HEAD.

Ordinary configure/build commands keep process logs and actual exits without
creating a complete provenance archive. Explicit `--record-provenance` binds an image to
its command-attempt receipt, source and configuration for device operations.
Device admission still verifies that identity. Candidate-plan executions below
are historical records; their retired orchestration is not a current acceptance
mechanism. A shared latest report name cannot substitute for an exact attempt.

BLD-18 retains full SDK byte checks at each consumption boundary after
measurement, with no parent marker or persistent verification bypass. BLD-31
retains common static libraries after measuring declared versus actually
compiled units and exact selected-image costs. Their `decision_verified` states
record measured choices rather than speculative source splitting.

BLD-38 records actual fixed-checker candidate plans and genuine failed-checker
execution. The original orchestration exit condition remains unchanged. The
maintained 72-stage software plan did not run through one complete plan
invocation; individually executed full gates are recorded separately. BLD-39
validates dependency-upgrade evidence contracts, without claiming an actual
vendor dependency upgrade.

BLD-40 covers actual CP312 offline installation and isolated hook preparation;
CP311 installation remains unexecuted. BLD-41 covers actual whole-toolchain and
OCI transport and verified reuse of the same bytes. BLD-42 remains
`declared_recipe_verified_reconstruction_unverified`, with
`recipe_reproducible=false`. BLD-44 covers actual new-source archived proof and
independent archive-only verification. Local archive retention and a durable
hard-link name do not establish off-site storage, publication or a release.

The host observations retain their recorded source scopes in
`/workspace/nexus-build-maintenance-20261012/documentation/host-tool-support-observations.json`.
Current locked closure and final-source receipts are indexed in the delivery
result index. The ledger's configuration, target, workflow, delivery and transport
reports retain their raw executions, failure attempts and source scopes. Physical HIL
remains `not_executed` throughout this build-system maintenance work.

## Verification plan for additional MCU families

This proposal starts at platform revision `a06bb94`. The active production
package is STM32F4, with eight exact parts and four public boards. Each task
below is open. Current commit and delivery rules apply until the corresponding
policy, implementation and checks land together.

### Baseline and objectives

The recorded local gate took 30 min 04 s, including 28 min 45 s for four tool
domains and about 18 s for Native GoogleTest discovery, execution and results.
A separate diagnostic executed all 347 tool modules and 3,796 cases in 9 min
02 s with four workers. It consumed 2,096 CPU seconds. F4 peripheral tests,
production build graphs and models, and SDK consumers used 513, 393 and 366
CPU seconds respectively. The original gate also shared the machine with
other jobs. Use end-to-end wall time to evaluate developer latency and CPU
time to identify repeated work.

Proposed warm targets are p95 at or below 180 s for a driver implementation
commit and 300 s for a single board change including its ARM link. Full SDK
verification is independently budgeted. The recorded 51 consumers took
22 min 38 s with two workers. Adding an unrelated family should add no family
tool modules, ARM targets or SDK consumers to an ordinary existing board
commit. Full Native behavior tests still run on behavioral commits; measure
their growth separately as families gain host models.

### How a changed input selects work

Use the complete Git change, including both sides of renames, with the existing
tool planner. Discover packages from manifests, boards from their declared
family and exact part, and assemblies from their authored board selection.
Invert the resolver's input lists for maintained assemblies. Translate local
paths to package-relative identities. Use CMake targets, compiler header
dependencies and the SDK consumer plan for software edges. Selection runs on
the host; firmware continues to use static C instances and typed factories.

| Changed input | Smallest useful check | Expand selection when |
| --- | --- | --- |
| Private driver source or header | Full Native behavior plus an exact-part ARM link for affected hardware paths | A shared implementation or header reaches several packages or distinct compiler contexts |
| Board pin, clock or connection | Resolve every maintained assembly using that board, check resources and link the affected configurations represented by distinct hardware or backend paths | A shared clock, board dependency or ABI changes |
| Part, route, IRQ, DMA or errata fact | Check exact parts and controllers that consume the fact, including an illegal route or conflict and a representative ARM link | A shared fact file has no verified per-record dependencies; check its family |
| Public I/O contract or common generator | Check all consuming implementations, a generated factory and an external application build | ABI, schema or common output semantics can affect multiple families |
| SDK fixture or export policy | Run that named consumer on its relevant backends; export, relocate and consume a minimal SDK when packaging changes | Dependency locks, toolchain, export ownership or public ABI affect a wider closure |
| Workflow or tool rule | Run the changed complete tool module and exercise its real entry point | Shared selection policy or an unknown dependency requires broader coverage |

Take the union of consumers in the base and proposed trees. A new board has no
prior dependency record. Resolve its assemblies from the proposed tree. For a
deleted board, use the previous tree to find references and require their
removal or migration. A new manifest or conflicting package identity triggers
catalogue validation. Missing depfiles, stale inputs or unclassified changes
expand to the owning family or tool domain. A shared change with unknown
owners goes to full verification. Record the selected assemblies, targets,
consumers and reason for every expansion in the existing reports.

### Onboarding a board, part, IP or family

| Contribution | Package and board work | Evidence required before declaring support |
| --- | --- | --- |
| New PCB with an existing part | Add board facts and reference assemblies with the crystal, pins, external devices and PCB revision | Resolve the board, reject an invalid connection, link a reference image and record physical status |
| New exact part using reviewed F4 IP | Extend the selected family facts, package/pad map, clock and memory limits, revision/errata and reviewed routes. Bind existing C drivers where their contracts apply | Check an allowed and an absent peripheral or pad, compile with the exact device header and startup vector, and verify the ELF memory map |
| New IP revision or controller architecture | Add a package-private C adapter or specialization and declarative binding. Verify reset, IRQ, DMA, stop, error and ownership semantics. Extend the common schema only for a new hardware concept | Run shared I/O contracts and IP-specific failures, inspect the selected source graph, link an exact target and compare Flash and RAM for the corresponding assembly |
| New family from the same or another vendor | Add `soc/<vendor>/<family>/package.json` with facts, bindings, startup/system sources, CMake target and SDK export ownership. Lock the required official dependencies. Add an exact-part reference board or an explicitly marked software fixture | Verify the toolchain ABI, vector and linker map, clock/reset, memory and DMA reachability, one real I/O operation in a software model where available, an ARM link for Cortex-M and an external SDK consumer |

Reuse an IP implementation across vendors only after checking the IP revision,
register semantics, clock/reset, interrupts, DMA and errata for both parts.
For example, a DMA channel or request multiplexer needs its own representation
when an F4 stream description cannot express the hardware. Keep board and SoC
packages declarative with C hardware behavior. Onboarding a board or a part
using existing concepts should require zero model-name branches in common
Python, CMake or CI. Runtime configuration adds no discovery table or heap.
Host, linked, SDK-consumed and physically tested support each keep their own
status, with the exact mode and board revision.

### Implementation tasks and exit conditions

1. **B01. Establish the baseline.** Build/release owns the timing sample. Keep
   the 347-module inventory, per-module CPU and wall time, selection reasons
   and the 50-commit replay with the source identity. Replay found 37 commits
   selecting both heavy domains. Use this corpus as a comparison input and
   include small driver, board and SDK edits in the live benchmark.
2. **B02. Select by dependency.** After B01, update the existing planner in
   `tools/testing/run_tool_tests.py` and its Git change inputs. Implement
   board-to-assembly and private-source/header-to-target selection first.
   Exercise a shared IP, a public header, an added manifest, a rename and a
   missing depfile. Require the same result after relocation; print the fallback
   reason for every ambiguous case.
3. **B03. Use incremental CMake.** Build tooling may work alongside B02.
   Reuse a warm Ninja tree for the same source directory, preset, toolchain
   and cache options across ordinary C edits. Cold or changed identities reconfigure explicitly. Test a C-only
   edit, a changed/deleted fact, a changed preset and an added input. The
   affected build must regenerate or reject invalid input; a C-only edit
   should not resolve unrelated assemblies.
4. **B04. Replace commit-time global scans.** After B02, use the affected
   board and family assemblies for commit checks. Keep one full capability
   matrix check in integration. Change one Qiming pin, then one shared F4 route
   and then remove a selected board. Verify the expected expansion, conflict
   diagnostics, selected ARM images and absence of unrelated board links.
5. **B05. Eliminate repeated resolution.** Configuration owners share
   immutable normal fixtures for EXTI and catalogue checks, generate each SPI
   factory once per input and avoid rereading an existing byte snapshot.
   Keep final input integrity checks. Exercise a same-timestamp content change,
   a pin conflict and interrupted publication. Reuse a parsed package only
   within a batch and only for matching content, root and package scope.
   Enumerate cheap rules for pads, clocks and IRQ/DMA routes. Run the full
   generation and link path for each distinct binding, ABI or IP behavior,
   including controllers competing for a shared resource. A wrong route or
   missing owner must still fail.
6. **B06. Reduce tool fixture cost.** Test owners use a single SDK file
   selection before the isolated import, an exact compilation database for
   CMSIS and small real child processes for worker limits and cancellation.
   Generate one baseline simulated HIL record and isolate each mutation.
   Retain real timeout/cleanup, module isolation and compile-flag assertions.
   Delete a weaker test after its stronger replacement is selected for the
   same change and detects the original fault.
7. **B07. Select SDK consumers.** After B02, extend
   `cmake/package/verify_consumers.py` to select names from its existing plan.
   Unknown names fail. Test a TMP117 edit against its Native and F407 consumers
   and verify both consumer chains use the relocated SDK. In full mode keep every
   declared consumer and the post-consumption integrity check. Tag selected
   reports with their actual scope.
8. **B08. Remove per-family assumptions.** After B02 and B07, derive active
   vendor payload, startup/header selection and reference ARM jobs from package
   ownership and reviewed assemblies. Audit existing F4-specific packaging
   and workflow lists before switching their consumers. Compare F4 payloads
   and ARM job membership before and after. An unselected vendor tree must
   stay out of preparation and the exported SDK.
9. **B09. Prove extension cost.** After B04 and B08, run two public F407 boards
   and the F411, F446 and F479 software references under
   `tests/contracts/stm32f4_family`. Use `tests/support/external_provider`
   for host package isolation and name collisions. This is the F4 rollout
   checkpoint. For each future production family, repeat the exact-part ARM
   and relocated SDK checks with its own dependencies before admitting that
   family. Track common files changed, family exceptions, selected jobs and
   firmware size. An F4-only commit must select the same hardware targets
   and link the same resources before and after an unrelated package is added.
10. **B10. Introduce bounded parallel execution.** After B05 and B06, run
    complete independent modules with one host-wide worker budget. Serialize
    mutations of each build directory and keep per-run outputs. Compare the
    discovered and executed module/case sets with the serial run and inject
    a worker failure and cancellation. Publish a full result only after all
    required work and cleanup succeed.
11. **B11. Roll out the policy.** After B03, B04, B07 and the F4 checkpoint of
    B09, compare proposed selection with full integration on the same revision.
    Enable parallel execution separately after B10 passes.
    Inject an invalid board pin, a missing shared-header dependency, a wrong
    DMA route, a stale SDK export and a wrong startup vector. Each applicable
    fast path must detect the fault or expand to a check that does. Update
    `AGENTS.md`, the developer workflow, CI job conditions and result checks
    together. Revert the affected selection rule to its broader scope if a
    required failure is missed.

Start with F4 hardening. Evaluate a distinct STM32 family and then a second
vendor using the same onboarding criteria when their exact parts, packages and
references are selected. Record those ports separately from the F4 work; a
package isolation fixture provides host evidence only.

For the latency decision, time every hook from entry to exit on the same host
and toolchain. Collect at least 30 warm observations per change class; publish
sample count, median, nearest-rank p95, maximum, fallback frequency and CPU
budget. Include fallbacks in their original class. Report clean builds
separately. Compare retained negative tests, ARM links and relocated consumers
at the same revision before adopting the narrower policy. A reproducible
failure, incomplete selection or missed performance target keeps the task open.
