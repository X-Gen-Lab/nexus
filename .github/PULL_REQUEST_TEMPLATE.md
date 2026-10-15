## Problem and resulting behavior

Describe the concrete trigger, failure and resulting behavior. Link the requirement
or NG task and the accountable role from `.github/maintainer-roles.json`.

## Contract and scope

Identify changed Core/Arch/I/O/OS/provider/component contracts and exact maintained
modes. Explain retained storage, deadlines, cancellation/drain and failure state
when applicable. Product policy and private PCB inputs belong in external consumers.

## Executed validation

| Evidence | Exact identity and executed result |
|---|---|
| Source/dependencies and resolved assembly | |
| SoC/Board/PCB/backend/compiler | |
| Host/model tests and fresh JUnit | |
| ARM ELF/BIN/map and resource report | |
| Applicable fault/concurrency regressions | |
| Physical station result or not_executed | |

Include the commands actually run and report paths. Distinguish development builds,
clean software qualification and physical HIL. Workflow definitions and hashes
alone are not execution evidence.

## TDD record

New or changed behavior and defect fixes define the contract with a failing test
before implementation. Record actual commands and raw logs:

| Step | Test identity, command, expected failure reason and raw result |
|---|---|
| RED before implementation | |
| GREEN after the smallest correct implementation | |
| Regression after refactoring | |

Explain why RED failed for the required missing behavior. Migrating historical
tests or preserving existing behavior does not justify manufacturing a failure
or claiming a historical RED run. The gate verifies current execution; it cannot
automatically prove the order of earlier development work.

## Review and delivery

- [ ] Changed files pass existing format and backslash Doxygen rules.
- [ ] Relevant failure paths retain ownership and have behavioral coverage.
- [ ] Unsupported/unknown scope and external consumer migration are explicit.
- [ ] Support claims bind the same source/config/artifact as their evidence.

Record any remaining hardware inputs, migration risk and software qualification.
