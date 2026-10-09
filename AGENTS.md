# Nexus platform maintenance

Nexus owns reusable embedded mechanisms. Applications, product roles, protocol
register policy, workers, private PCB data, Flash reservations and recovery policy
belong in external repositories. Public examples live in X-Gen-Lab/nexus-examples.

Read [the design contracts](docs/design/README.md),
[delivery evidence](docs/delivery/README.md) and the relevant layer README before
changing behavior. `docs/archive/` contains historical contracts and results.
They do not establish current support or qualification.

## Architecture and configuration

- Keep the default C11 path statically assembled and allocation-free: Core,
  Arch, SoC, typed I/O, optional OS/owner adapters, optional common components.
- Public device APIs use Nexus types. Vendor SDKs and provider storage remain
  implementation inputs. A product uses generated fixed typed bindings.
- Do not reintroduce registries, name lookup, factory discovery, maximum object
  pools, hidden workers, mandatory OS locks or forwarding-only runtime layers.
- SoC facts, Board `board.json` and one assembly JSON resolve to one atomic
  bundle. JSON is the sole configuration authority. CMake targets own software
  dependencies. Do not introduce Kconfig/cache fallback or a second build graph.
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

Install commit tools with `python scripts/setup/install_dev_tools.py`, then
`.venv/bin/python -m pip install -r dependencies/environment-tools.txt` (Windows:
use `.venv/Scripts/python.exe`). This installs pre-commit and commit-msg hooks;
no hook bypass or automatic staging is allowed. CI uses the same checks.

Use `python tools/dev/dev.py configure|build|test --preset <name>`; native CMake
arguments pass through. `doctor` checks tools and `check` runs repository gates.
Fresh JUnit, nonzero tests and actual return codes are required. Missing tools,
empty reports and skipped-only executions fail qualification.

Keep the existing `.clang-format`, `.editorconfig` and backslash Doxygen style:
80 columns, four spaces, attached braces and type-attached pointer stars. Sources
have file/brief/author/version/date/copyright; headers have file/brief/author and
public context, deadline, ownership and failure contracts. Implementation comments
explain invariants and hardware ordering. Do not duplicate declaration parameter
lists in source definitions. New or changed files pass whole-file checks. Frozen
historical debt may only shrink; never rebaseline new defects.

Run the maintained host/model and tool boundary suites. Changes to interrupt,
lifecycle, persistence or concurrency need meaningful failure-path regressions.
Compile common components for both MCU families. Native models establish software
behavior, ARM ELF checks establish linking/resources, and physical station results
establish hardware behavior. Never inherit old test counts or hardware status.

A software candidate binds clean Git source, exact configuration, dependency and
OCI/toolchain identities, executed raw logs and the same ELF/BIN/map bytes. Two
fresh network-disabled builds establish reproducibility. A digest alone does not.
No unsigned software candidate is a product release, safety certification or LTS
promise. Record physical tests as not executed until real equipment runs them.
