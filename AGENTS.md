# Nexus maintenance instructions

Nexus owns the common industrial embedded platform. Read `docs/strategy/README.md`, the relevant architecture decisions and the matching RF item in `docs/implementation/refactor-execution.csv` before architecture changes. The external `nexus-examples/docs/platform-refactor-plan.md` defines the original 38 tasks/19 batches. Historical `docs/strategy/backlog.csv` uses earlier IDs and evidence. Plans and role metadata do not prove implemented capabilities.

## Engineering boundaries

- The user authorizes breaking architectural refactors and removal of poor designs. Update public interfaces, production callers, tests and documents atomically. Persisted product data needs a separate explicit format, migration and recovery policy.
- Preserve the repository's code formatting and comment style during refactors: root `.clang-format`/`.editorconfig`, contribution guides and `.kiro/steering/comment-standards.md` remain authoritative. Keep 80 columns, 4 spaces, attached braces, type-side pointer alignment, established names/file headers and backslash Doxygen tags. Public headers own full API contracts; source comments avoid duplicating parameter/return documentation. Existing compact code is a style gap, not a new convention.
- Keep HAL/OSAL independent of vendor headers, Board wiring and product policy. Arch, SoC/controllers and Board resources own separate contracts. Maintained controllers/clock/IRQ/system/private SDK live in `soc/{stm32f407,gd32f470,native}`; `platforms/` owns only startup/lifecycle/object assembly. `soc/native` is a host virtual model, not physical silicon. Family paths do not replace exact variant/density identities. Product main/workers, domain control, private protocols and partition/update/health/manufacturing policies belong to external repositories such as nexus-examples.
- Runtime owns only serial HAL/OSAL infrastructure. Firmware explicitly assembles platform objects/startup. Applications own scheduler, components and workers. Do not restore Product identity, automatic main wrappers or mandatory application choice.
- Document task/ISR context, timeout origin, blocking bound, ownership, cancellation, callback, lifecycle and failure state. Never return success for required unimplemented operations. An unsettled DMA/transfer buffer remains borrowed; timeout is not settlement.
- Use one per-build effective configuration and reject contradictions/generation failures. No source-root `.config` or stale generated-header fallback. Different Board/backend combinations use independent build roots.
- Board packages use one `NEXUS_BOARD_DIR`, declared hashed inputs and reviewed narrow resources. External `NEXUS_FLASH_LAYOUT_FILE` drives linker/regions from one parse. SoC exposes full physical Flash; no default product storage reservation. Nonzero image offsets, MPU/cache, MCU typed I2C and unsupported routes stay explicit until implemented and verified.
- Prefer bounded resources and honest capabilities. Do not write cryptographic primitives or call RAM persistent Flash. External product teams own physical timing, memory, power-loss, maintenance-window and worst-load budgets.

## Validation and review

Install the pinned local commit tools and hooks for every fresh clone:

```sh
python scripts/setup/install_dev_tools.py
```

Git commits run staged style/text checks and Conventional Commit validation.
Hooks report failures without formatting or staging files. The frozen historical
baseline only permits unchanged reviewed bytes; modified source is strict. Do
not add/rebase debt or move owned source into exclusions. Manual pre-commit
`--files`/`--all-files` checks read working-tree contents, not a different staged
snapshot. CI runs the same full source check for every invocation and retains
the actual report. See `docs/implementation/quality-gates.md` for scope and limits.

The CI helper tests require no third-party test framework:

```sh
python3 -m unittest discover -s scripts/ci -p 'test_*.py'
```

Use a complete checkout, fixed submodules/imports and required toolchain for CMake/CTest. Top-level ARM `NEXUS_BUILD_CONTRACTS` builds an independent platform link fixture; source consumers default development tests/contracts off. Host suites reject zero tests; required evidence rejects empty/stale/all-skipped reports and propagates nonzero command exits.

Keep actual command, exit, test enumeration, source/dep/config/toolchain, Board/layout and artifact hashes. Separate host models, real FreeRTOS POSIX execution, ARM compilation, real firmware linkage and physical qualification. A target, workflow definition, file or coverage claim is not execution evidence. Historical source counts cannot qualify later refactors.

Critical regression tests verify user-observable success/failure and cross-backend invariants. Relevant faults include stale owners, finite pool exhaustion, busy/error retry, timeout/cancel races, zero ticket recovery and final buffer return. Reversible prose/format changes need suitable checks, not new tests that mirror text.

The user currently defers physical boards: finish authorized software and HIL tooling without executing equipment. Record physical IRQ/DMA/electrical/power-cut/long-load qualification as unexecuted. Do not invent station IDs, measured budgets, passing HIL or hardware support from models.

Use RF/backlog IDs in review context. ADRs cover dependency direction, public contracts, persistence, security and release identity. Update exact support claims and external caller contracts with implementation/evidence. Do not add platforms before maintained combinations have repeatable software baselines.

## Delivery

Publishable artifacts need verified source, dependencies, effective configuration, Board/layout and ELF/BIN identity. Candidate and product promotion remain separate gates; same-artifact promotion does not rebuild a different image. Signing/manufacturing credentials never enter source, tests, logs or artifacts.

Relocatable source SDK preparation needs a clean complete checkout and actual consumer verification. Development fixtures remain `publishable=false` and require explicit opt-in; they are ineligible for promotion. Source packages do not establish installed binary SDK, ABI compatibility, signing or physical qualification.

Real HIL, trust/signing, manufacturing, named reviewers, branch rules, support window and LTS require actual external evidence and responsibility. Role files do not authorize contacting third parties or altering repository settings. Continue reversible authorized work and make remote changes concrete/reviewable.

`.nexus-source-snapshot.json`, if present, identifies an analysis snapshot, not a complete Git checkout. Verify missing files/submodules/tooling before reporting builds or history.
