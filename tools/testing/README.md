# Host TDD execution

New public C API and provider behavior uses GoogleTest and GoogleMock. Firmware
remains C11. The framework is pinned by `dependencies/googletest.lock.json` and
extracted from the local archive only for Native tests; configuration never
downloads dependencies.

Run the same check locally and in CI:

```sh
python scripts/ci/tdd_gate.py --all --preset native-debug
```

The gate executes the configuration and reporting tool tests, builds the explicit
`nexus_google_contracts` target, discovers all cases in each binary, then runs them
without filters or shards. Fresh XML must contain completed, passing cases and
the same number of cases as discovery. Empty, stale, skipped, failed and partial
runs cannot establish acceptance. Raw commands and results are in
`build/<preset>/tdd/`; `execution.json` is a development execution record.

The pre-commit hook calls the same gate for behavioral changes, including every
`tools/` and `scripts/` path. The local baseline executes configuration/reporting
tool tests and all GoogleTest contracts; other tool suites run in their explicit
CTest/CI entries. It checks relevant staged source against unstaged or untracked
behavior before and after execution and requires the exact Git index tree to
remain unchanged. A late source or staging change rejects the new summary. Git's
pre-commit runner temporarily stashes tracked unstaged edits before hooks execute.
It loads the current `.pre-commit-config.yaml`, so an already installed hook reads
new gate definitions without reinstalling. CI executes the gate independently.
Child configure/build/test processes clear Git's repository-local environment so
SDK submodules use their own repository and index. The gate itself retains the
active commit index for its staged snapshot checks.

The `evidence/` directory records selected actual RED/GREEN development steps.
Some initial RED records are compiler/linker failures for new interfaces; UART
owner, IRQ wake, stream storage and staged execution records include actual
failing behavior. Existing owner
regressions were added after the earlier implementation and have GREEN records
only. These records do not prove that every historical edit followed TDD, bind a
clean release candidate, or establish physical MCU qualification.

GoogleMock tests replace dependency ports while calling the real Nexus public
API. Provider storage, admission, cancellation, quarantine, instance isolation,
notification and DMA drain behavior are checked at their observable boundaries.
Python tool tests use stdlib unittest and must also execute nonempty suites with
no skipped or expected-failure cases.
