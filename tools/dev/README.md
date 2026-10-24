# Development commands and execution boundaries

`dev.py` forwards configure/build/test to CMake and CTest. CMake targets remain
the only software dependency graph. Native arguments and actual failures pass
through. The default build parallelism is two; set `NEXUS_BUILD_JOBS` or pass
CMake's explicit `--parallel` to select a different finite budget.

Application creation uses versioned templates from the separate
`X-Gen-Lab/nexus-examples` checkout. It does not copy a platform tree or download
dependencies. Select its exact checked-out commit; branches and mutable names
are rejected. The output must be a new directory outside that checkout.

```sh
python tools/dev/dev.py create --templates ../nexus-examples \
  --revision <exact-40-character-examples-commit> \
  --template native --output ../my-controller
```

The maintained template IDs are `native`, `qiming`, `sky` and `liangshan`.
The command verifies the selected commit, manifest, tracked regular files and
their hashes before exclusive creation. It resolves the copied assembly in its
final location and rolls back its new directory if validation or copying fails.
Referenced inputs must belong to the new project or selected platform. Each file
is bounded to 4 MiB and the project to 16 MiB before reading the next payload.
An existing destination is never overwritten. `.nexus-template.json` records
the examples identity, its **expected** platform lock and the actual resolver
and input identities used for this invocation. That lock does not imply the
current checkout equals the expected platform. Follow the template README to
build with the selected source SDK, or explicitly select a development
`NEXUS_SOURCE`. Creation is configuration validation, not a successful build.

Daily operations consume one explicit equipment JSON file. `flash` and `debug`
also require an actual completed build's `quality/build-provenance.json`.
`monitor` needs only the Board identifier, serial selection and station lock;
an optional build provenance associates known image bytes with the recording.
They reuse the bounded process runner; serial monitoring also reuses HIL's
stable by-id device opening, exclusive access and terminal restoration.
They do not run the formal HIL admission or qualification workflow.

```sh
# Inspect the command and exact target without opening equipment.
python tools/dev/dev.py flash --target /station/development-target.json \
  --provenance build/board/quality/build-provenance.json
# Execute only after reviewing that equipment selection.
python tools/dev/dev.py flash --target /station/development-target.json \
  --provenance build/board/quality/build-provenance.json \
  --execute --output /station/runs/flash-001
python tools/dev/dev.py debug --target /station/development-target.json \
  --provenance build/board/quality/build-provenance.json \
  --timeout 600 --port 3333 --execute --output /station/runs/debug-001
python tools/dev/dev.py monitor --target /station/development-target.json \
  --timeout 30 --execute --output /station/runs/monitor-001
```

The equipment file is a local operator selection, not a second Board authority:

```json
{
  "schema": 1,
  "kind": "nexus_development_target",
  "board": "<resolved-board-id>",
  "part": "<resolved-part-id>",
  "station_lock": "/station/board-01.lock",
  "probe": {
    "serial": "<explicit-probe-serial>",
    "transport": "swd",
    "tool": {"path": "/tools/openocd", "size": 1, "sha256": "<sha256>"},
    "interface_config": {
      "path": "/station/interface.cfg", "size": 1, "sha256": "<sha256>"
    },
    "target_config": {
      "path": "/station/target.cfg", "size": 1, "sha256": "<sha256>"
    }
  },
  "serial": {"path": "/dev/serial/by-id/<actual-device>", "baudrate": 115200}
}
```

Replace every placeholder and size with the actual reviewed tool, configuration
and equipment identity. For SWD-only use, omit `serial`; for serial-only use,
omit `probe` and optional `part`. Commands validate only their required equipment.
When a build is supplied, Board and any supplied part must match its resolved
configuration; the image must
be an unchanged little-endian ARM ELF from that build. `--elf` selects one when
multiple ELF artifacts exist. Probe serial is mandatory, including at a station
with only one attached probe; no first-device fallback exists. Target/interface
configuration remains explicit and must support the selected OpenOCD adapter's
serial selector. Without provenance, monitor explicitly records
`image_linkage = unassociated`; a Board label never proves the running firmware.
Software identifiers cannot prove the physical PCB identity.

Execution defaults to 300 seconds and accepts 1..3600. Debug binds only
`127.0.0.1`, disables Telnet/Tcl ports and prints the GDB connection command;
connect GDB from another terminal. Monitor is Linux/POSIX, read-only 115200 8N1,
bounded to 16 MiB, and uses a `/dev/serial/by-id/` name. Every actual operation
requires a new output directory and acquires the shared station lock. Successful
commands, including monitor reaching its requested duration, release their owned
lock. Monitor treats EOF as a disconnect failure. Failure, cancellation or changed
input/lock
identity retains quarantine; inspect the board and process receipt before an
operator explicitly clears that lock. Exit codes and raw attempt logs survive.
An operation reports `not_executed` until launched and then
`executed_unqualified`; a zero host exit never establishes physical acceptance.

Observe feedback cost from existing independent attempt receipts:

```sh
python tools/dev/dev.py timings build/quality/developer-command.attempts \
  --json /new/output/development-costs.json
```

Inputs can be receipt files, latest `.command.json` views or directories. The
tool rechecks completed stdout/stderr bytes and hashes, deduplicates attempts
and retains failures and unfinished attempts. Phase grouping is a conservative
view of the original command; each stage/argv remains in JSON. Cumulative
attempt seconds can overlap. Wall time is the union of observed wall-clock
intervals, excluding idle gaps and assuming one stable host clock. Unobserved
stages, unfinished duration, CPU, peak RSS and cache counters remain unknown.
The JSON output must be new. This is a cost summary, never a cached success
report, MCU timing measurement or qualification record.

For a small edit, use the relevant `unittest` module or `dev.py test -R <case>`
first, then the ordinary complete gate at delivery. Test metadata now resolves
the selected configure path and filter from one preset snapshot, avoiding a
second parse and inconsistent observations. Changed inputs must still be
validated for every cache hit; `cache.py` binds compiler namespaces to compiler,
flags, dependencies and environment, and rejects compiler caching in formal
independent rebuilds. Keep finite `NEXUS_BUILD_JOBS` and command deadlines.
Do not cache successful JUnit or skip complete domains after a focused pass.

Test retention is semantic: identical test bodies are only review candidates.
For example, F1 and F4 official-source lock regressions use different SDK
verifiers, source closures, commits and CMSIS pairings and must remain. Remove a
test only when its contract is obsolete or equivalent independent coverage is
identified. A review with no justified deletion does not claim reduced coverage
or shorter whole-gate time. Keep mode-independent tools in their owned domain;
all four Native modes, failure/cancellation regressions and fresh full-domain
reports remain required.

```sh
python tools/dev/dev.py doctor --target arm
python tools/dev/dev.py configure --preset stm32-nucleo-h743-baremetal-uart
python tools/dev/dev.py build --preset stm32-nucleo-h743-baremetal-uart
python tools/dev/dev.py test --preset native-debug -R Request
python tools/dev/dev.py check --preset native-debug
python tools/dev/dev.py configure --preset native-step-debug
```

Documentation has its own explicit Linux x86_64 CPython 3.12 tool closure:

```sh
python tools/documentation/install.py --prefix build/docs-tools --cache build/docs-cache
build/docs-tools/venv/bin/python tools/dev/dev.py doctor --target docs
build/docs-tools/venv/bin/python tools/dev/dev.py docs check
build/docs-tools/venv/bin/python tools/dev/dev.py docs test
build/docs-tools/venv/bin/python tools/dev/dev.py docs build --language all
build/docs-tools/venv/bin/python tools/dev/dev.py docs preview
build/docs-tools/venv/bin/python tools/dev/dev.py docs package --output ../nexus-docs-package
```

Preparation is explicit; checks and builds never install tools implicitly. The
offline commit hook checks the same source, configuration and authority rules
against the complete staged inputs. The dedicated documentation CI executes the
complete documentation tests in its locked environment and builds fresh XML and
both language views. The existing Google contract gate retains its four complete
tool domains. Documentation packaging requires clean source and an output outside
the source tree. Publication consumes verified bytes on the dedicated delivery
branch; neither a documentation build nor deployment establishes physical HIL.

`native-debug` retains size optimization and debug symbols. `native-step-debug`
uses `Og` for interactive stepping. `fixture-stm32-g474-*` names are software
fixtures; they do not certify their routes or hardware. Public H743/U575 presets
retain the fixed clock/cache/security settings in their authored TOML files.

`test` is a development execution. Filters and preset filters are recorded as
`focused-development`; an unfiltered preset remains `preset-development`.
Neither is software qualification. `check` and the commit hook execute the
maintained complete contract gate. The gate strips GoogleTest filters/shards,
discovers every case, executes the complete plan and verifies fresh reports.
The shared `run_tool_tests.py` runner executes four complete domains: `testing`,
`configure`, `ci` and `workflows`. The commit gate, enterprise CI and software
plan use this same discovery and reject empty, incomplete or skipped execution.
Each domain writes a fresh report only after its actual full suite passes.

Successful `configure` and `build` automatically capture
`<binaryDir>/quality/configure-provenance.json` or `build-provenance.json` from
this invocation's independent attempt receipt. A later concurrent command cannot
replace this association; expected argv and working directory are checked.
Explicit configure `-B` overrides are honored.
Records bind source, compiler/configuration/lock bytes, raw execution and observed
outputs. The compile database is labeled as a plan; an incremental build does not
claim every object was rebuilt. Identity capture is metadata, never qualification.
Capture failure returns an error and retains a separate failure report under
`build/quality/developer-provenance-failures/` alongside the actual command receipt.

`doctor --target tools|native|arm|candidate` rejects missing tools required by
the selected target. `tools` is the default informational compiler diagnosis.
Candidate diagnosis checks availability, not the complete toolchain tree,
source SDK, independent builds or software qualification.

Dependency preparation and qualification use explicit maintained checkers:

```sh
python tools/dev/dev.py prepare --scope arm --family stm32h743
python tools/dev/dev.py prepare --preset stm32-nucleo-h743-baremetal-uart
python tools/dev/dev.py qualify --plan /path/plan.json --output /new/output
python tools/dev/dev.py candidate --plan /path/plan.json --output /new/output
```

Preparation checks first and requires `--allow-network` for initialization that
needs network access. Candidate plans must end with the owned candidate verifier.
These commands delegate to `dependencies.py` and `qualification.py`; they do not
establish a second build graph. Cache commands delegate to `cache.py`.

The manual `formal-software.yml` workflow runs the complete maintained software
plan on a fresh `main` checkout. The requested commit must equal both the source
and workflow SHA:

```sh
gh workflow run formal-software.yml --repo X-Gen-Lab/nexus --ref main \
  -f expected_commit=<exact-main-SHA>
```

It reuses the existing tools and plan runner with two build jobs. An explicit
five-hour outer deadline leaves time within the six-hour job to preserve
failure/cancellation diagnostics; each stage retains its original deadline.
The complete temporary output and `build/` tree are archived with the existing
byte verifier and retained for 30 days. Failed archiving uploads unqualified raw
trees instead; cancellation uploads are best effort. Download and verify the
archives and actual job result before admitting evidence. An executed plan
remains separate from SDK/offline candidate qualification and physical HIL.

Preset preparation records the assembly/Board bytes and every selected recursive
Gitlink. Native models require all registered runtime and host-test roots,
recursively.
ARM preparation requires CMSIS, the chosen STM32 family when applicable, and the
selected scheduler closure. SDK and candidate preparation use only registered
runtime sources; the host-only `ext/googletest` submodule is not required.
CI's ARM identity binds this receipt to the explicit matrix preset and every
uploaded resolved configuration. It rechecks current pins, nested sources and
dirty files before and after capture, and uploads the original receipt.
`dependencies_complete` continues to describe the whole repository; a verified
subset is recorded separately as `selected_dependencies_complete`. Uninitialized
unselected sources are observed rather than declared verified. Default Native/SDK
identity still requires complete repository dependencies.

`presets.py` supplies one checked metadata reader for developer/TDD/Native CI
paths. It handles literal includes, schema-7 `$penv` include paths, same-kind
inheritance with CMake's earlier-parent precedence, and implicit user-to-project
includes. Configure/build/test preset names resolve to their actual configure
name. Common path/environment macros and boolean/equality/list/composite
conditions are supported.
Build/test generator macros and configure environments follow the selected
configure preset; `inheritConfigureEnvironment = false` is respected.
Cycles, duplicates, illegal file inheritance and unsupported macros/conditions
fail explicitly. Regex conditions and field
`${fileDir}` are deliberately rejected by this reader; CMake still validates
and executes the complete preset schema. A real CMake regression compares
included/inherited/user preset cache values with the metadata result.

`process.py` applies finite deadlines. Set `NEXUS_COMMAND_TIMEOUT` in seconds to
select an explicit budget. On maintained Linux, timeout/cancellation withdraws
the actual owned descendant tree, including nested runner sessions. Linux first
stops the owned group/tree, confirms a stable stopped set within a finite budget,
then kills and observes terminal states. Cleanup never executes a child TERM
handler that could fork and exit. Identities include kernel start times. POSIX systems
without Linux `/proc` use process-group withdrawal, and Windows uses `taskkill`.
Linux is the maintained integration-test host for nested-session cleanup.
Repeated cancellation cannot interrupt withdrawal or receipt publication.
Timeout/cancellation/launch failure return 124/130/127. Outstanding uninterruptible
kernel processes remain explicit identities with `withdrawal_complete = false`;
they cannot establish success. Receipts also retain the
actual child return code, command, working directory, timing and raw output hashes.
Complete withdrawal also requires a stable inspected frozen tree; merely stopping
the identities discovered before the inspection deadline is insufficient.
Each logged invocation records its start before launching a child. An interrupted
nested runner retains a `running` receipt; it cannot qualify as completed.
Each invocation creates a new `<stem>.attempts/<identity>/` directory with
stdout, stderr and a receipt. The conventional log is a latest-result view;
retries retain the original failed attempt. Developer logs use
`build/quality/developer-command.*`; contract runners retain stage-specific logs.

`change_paths.py` is the single commit/CI classification policy. Gitlinks,
dependency locks, tool changes and unknown non-document inputs require behavior
execution. Ordinary Markdown/RST and `docs/` changes are documentation; explicit
coding-policy documents also require behavior checks. Git diffs include additions,
deletions and both rename sides. CI runs this same policy on the actual Git diff.

`python scripts/setup/install_dev_tools.py` explicitly prepares the pinned
GitHub Actions linter in `.venv/nexus-actionlint`, with its MIT notice and exact
archive/binary identities from `dependencies/actionlint.lock.json`. Supported
hosts are Linux x86_64, macOS x86_64/arm64 and Windows x86_64. The existing
content-addressed download cache is rechecked on every hit. An existing owned
prefix is verified without a download; an unknown or damaged prefix is rejected
and preserved for explicit repair. `--skip-hooks` still prepares the host tools.
The original two development Python pins remain in `development-tools.txt`.

The `nexus-workflow-lint` hook and commit-style CI invoke
`python scripts/ci/lint_workflows.py --staged`. Prepare dependencies before
committing; lint never installs or fetches. The hook shares the existing TDD
Python environment, including PyYAML from `workflow-tools.txt`; it verifies this
version against the staged authority. `dev.py doctor --target workflows` checks
only those offline prerequisites, without requiring a compiler or CMake.
An explicit `--tool-prefix` supports a separately prepared owned host prefix.

The linter snapshots every indexed workflow, local action metadata, tool lock
and optional actionlint config, even when unchanged in the pending commit. It
honors the caller's actual alternate index. Parent object reads disable
replacement refs and promisor lazy fetching. Roots with any partialClone,
promisor or partialCloneFilter Git configuration are rejected before object
reads. This fail-closed complete-checkout contract also covers older Git that
may ignore the no-lazy-fetch environment setting. The isolated tool environment
removes Git selection and Git configuration redirects. Without a staged
config, the snapshot contains an explicit empty config at the normal default
path; unstaged and ambient config cannot change validation. Shellcheck and
pyflakes integration are explicitly disabled rather than discovered from PATH.
Literal local references must remain inside the snapshot and name regular
indexed reusable workflows or exactly one action metadata file. Upstream
handles the Actions DSL and input contracts; this presence check only closes
its silent missing-action behavior. The complete index is checked before and
after execution. Once tool execution is prepared, raw stdout/stderr, actual exit
receipts and compressed original input snapshots remain under
`build/quality/workflow-lint/` on success or failure. Early admission failures
are
diagnosed before those tool artifacts exist. Snapshots preserve blob content;
they do not reconstruct the complete Git index or its executable modes.
This is workflow DSL validation; it does not establish remote dispatch, SDK,
firmware or hardware qualification. The new lock joins normal provenance input
observation without changing the SDK's compiler or OCI tool requirements.
