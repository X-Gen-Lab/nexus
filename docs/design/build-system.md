# Build system maintenance contract

Nexus uses one authored TOML assembly, a validated immutable configuration IR,
static instances and typed factory, and one CMake software target graph. Ninja
executes the declared graph. Products own application sources and composition.

The [48-item execution ledger](build-system-execution.csv) preserves the requested
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
graph. ABI, optimization, diagnostics and firmware link policy have explicit
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

The following support matrix records the current maintenance environment and
declared locks. It is not a qualification of the changed platform source. Final
whole-source gates and clean-source candidates require their own fresh evidence.

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
| GoogleTest/GoogleMock | 1.16.0, pinned commit and archive SHA-256 | Host-only framework; final full-source execution separately recorded |

Ninja single-configuration builds are maintained. Ninja Multi-Config is
explicitly rejected; other generators have no implied acceptance. C11 remains
the firmware language. Native test targets enable C++17 without linking that
test runtime into firmware.

The Linux x86_64 CP312 offline tool closure contains 14 hash-locked wheels.
A fresh CP312 virtual environment completed a real `--no-index --require-hashes`
installation. CP311 wheels were downloaded and checked against their lock;
CP311 installation was not executed. The minimum Python version and the CP311
wheel inventory do not substitute for that missing installation result.

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
unchanged. Its paths identify implementation and actual development evidence.
`implemented_pending_final_gate` records an outstanding aggregate gate;
`implemented_fixture_verified_full_plan_pending` records implemented orchestration
whose complete maintained software plan has not yet run. An owner suite or a
small consumer fixture is not whole-platform qualification.

Automatic developer provenance binds the unique command-attempt receipt and
checks the expected command and working directory. The concurrent association
regression passes. BLD-43 and BLD-48 retain `implemented_pending_final_gate`
until the aggregate clean-source checks execute. A shared latest-report name
cannot establish that association.

BLD-18 retains complete SDK byte checks at each consumption boundary after
measurement; it introduces no parent marker or persistent verification bypass.
BLD-31 measures declared and actually compiled units separately and retains
common static libraries without speculative source splitting. Their
`decision_verified` state records these measured decisions. BLD-42 explicitly
uses `declared_recipe_verified_reconstruction_unverified`. BLD-40 covers actual
CP312 offline installation, while BLD-41 covers actual toolchain/OCI transport.
BLD-44 covers verified durable archive navigation, package restoration and
survival of original-name removal; off-site storage or a published release is
not established by local hard-link adoption.

The current matrix observations are retained in
`/workspace/nexus-build-maintenance-20261012/documentation/host-tool-support-observations.json`.
The ledger's configuration, target, workflow, delivery and transport reports
retain their raw executions, failure attempts and source scopes. Physical HIL
remains `not_executed` throughout this build-system maintenance work.
