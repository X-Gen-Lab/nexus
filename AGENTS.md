# Nexus maintenance instructions

Nexus is being maintained toward an industrial control and connected device platform. Read `docs/strategy/README.md`, the relevant architecture decisions, and the matching backlog item before architecture changes. These strategy documents describe a target; do not claim planned capabilities already work.

## Engineering boundaries

- The user authorizes architectural refactoring and removal of poor legacy designs. Prefer explicit, testable contracts over compatibility shims. Update callers, tests and documentation atomically when changing public interfaces; persisted product data still needs an explicit format and recovery policy.
- Keep common HAL and OSAL independent of vendor SDK headers, board wiring, product policy, and network services. Keep architecture primitives, SoC drivers, boards, and product profiles distinct.
- Document task versus ISR context, blocking and timeout rules, memory ownership, cancellation, lifecycle, and error state. Never return success for a required but unimplemented operation.
- Use per-build effective configuration. Reject contradictions and generation failures. Do not silently consume a developer's root `.config` or a stale generated header for a different target.
- DMA timeout and cancellation must settle hardware and callback ownership before the caller can release buffers. Do not implement cryptographic primitives or label RAM storage as persistent flash.
- Target deterministic control behavior and bounded resources. Measure hardware timing, memory, fault recovery, and persistence on the supported board profile.

## Validation and review

Run checks appropriate to the changed behavior. The CI helper tests are available without third-party test frameworks:

```sh
python -m unittest discover -s scripts/ci -p 'test_*.py'
```

Use a complete checkout, its pinned submodules and required toolchain for CMake/CTest. Host tests must reject zero tests. Retain logs, executed test counts, effective configuration, board identity and artifact hashes. State when full build, HIL or a backend was not executed; a workflow definition or coverage claim is not execution evidence.

For critical contracts, test user-visible outcomes, failures and cross-backend invariants. Keep native simulation and real ISR/DMA/electrical validation distinct. Introduce a regression test when fixing a meaningful defect; avoid tests that merely compare implementation text.

Use requirement and backlog IDs in PR context. ADRs are for dependency direction, public contracts, persistence, security and release architecture decisions. Update support claims and examples with their implementation and execution evidence. Do not add new MCU platforms before the current supported combination has a repeatable validation baseline.

## Delivery

Publishable artifacts must be built and verified before release, with a matching source, configuration and dependency identity. Candidate releases are drafts until the required product evidence exists. Signing and manufacturing credentials must never appear in source, test fixtures, logs or artifacts. Planned SBOM, signing, HIL and LTS capabilities must remain labeled as planned until completed.

Do not contact third parties or change remote repository settings simply because a document names a role or workflow. Continue reversible local analysis, implementation and validation within the user's established task scope. Make remaining remote changes concrete and reviewable.

The `.nexus-source-snapshot.json` file, if present, identifies an analysis snapshot rather than a full Git checkout. Check missing files, submodules and tool availability before attempting full builds or reporting Git history.
