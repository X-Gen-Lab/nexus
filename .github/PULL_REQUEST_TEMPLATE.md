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

## Review and delivery

- [ ] Changed files pass existing format and backslash Doxygen rules.
- [ ] Relevant failure paths retain ownership and have behavioral coverage.
- [ ] Unsupported/unknown scope and external consumer migration are explicit.
- [ ] Support claims bind the same source/config/artifact as their evidence.

Record any remaining hardware inputs, migration risk and software qualification.
