---
id: docs-development-workflow
type: how-to
audience: maintainer
source_language: en
---

(docs-development-workflow)=
# Develop with TDD and normal commit gates

This workflow preserves current C11 architecture, formatting and explicit evidence.
Codex owns platform maintenance; independent agents review high-risk changes.
No fictitious personnel or local file can establish GitHub branch protection.

## Install and check tools

First complete the [fixed source and recursive dependency guide](../guides/source-checkout.md).
From that repository root, install the maintenance tools:

```sh
python scripts/setup/install_dev_tools.py
.venv/bin/python -m pip install -r dependencies/environment-tools.txt
python tools/dev/dev.py doctor
```

Windows uses `.venv/Scripts/python.exe`. Installation establishes pre-commit and
commit-msg hooks. Hooks inspect staged inputs without staging or silently fixing
files. Preserve unstaged work and actual failure diagnostics; never bypass hooks.

## Prepare the pinned host test framework

GoogleTest and GoogleMock are the Git submodule `ext/googletest`, pinned to
version 1.16.0 and commit `6910c9d9165801d8827d628cb72eb7ea9dd538c5`.
The recursive checkout above includes it. For an existing checkout, prepare the
Native dependency scope explicitly before running tests:

```sh
python tools/dev/dev.py prepare --scope native --allow-network
```

To initialize only this submodule, use
`git submodule update --init --recursive ext/googletest` instead. Preparation may
use the network; CMake configuration uses only the prepared local source and
fails if the required source is missing, differs from the pin or has local
modifications. It neither fetches a dependency nor substitutes a system
installation. The framework stays in the host test graph; ARM builds and firmware
SDK consumers do not require it.

## Execute RED, GREEN and review

For new C behavior or defect fixes, first add a meaningful GoogleTest/GoogleMock
regression. Execute it and retain the expected failure reason and original command.
Implement the smallest correct production change, execute GREEN, then refactor.
Mock external ports/MMIO, not the implementation being tested. Cover relevant
rejection, cancellation, drain, overflow, quarantine and instance isolation.
Python production tools use stdlib unittest with the same actual RED/GREEN rule.

```sh
python scripts/ci/tdd_gate.py --all --preset native-debug
```

The full gate builds the pinned offline framework and executes discovered cases
with fresh reports and no filters/shards. Empty, skipped, partial, stale or changed
inputs fail. The hook checks the staged tree remains unchanged; CI independently
executes the gate. The framework is host-only C++17; firmware remains C11.
Automation proves execution, while retained development logs document historical
RED/GREEN order. Pure prose or diagram edits need corresponding document checks
and semantic review rather than mirrored implementation tests.

## Associate a complete local software plan with a pull request

When GitHub Actions is unavailable, `tools/maintenance/local_ci.py` checks an
existing execution of the maintained `tools/dev/plans/software.json`. The default
mode is read-only: it verifies actual complete stages, command receipts, clean
source, inputs and the same plan before reporting the exact source and scope.
It does not run the plan or convert interrupted or partial reports into success.

```sh
python tools/maintenance/local_ci.py --report /path/to/software/report.json
```

Explicit `--publish` uses an already configured authorized GitHub credential and
an existing open PR. It checks the PR head/base twice and publishes only to the
verified head SHA under `nexus/local-software-plan`; a later head cannot inherit
that status. Its retained receipt distinguishes pending, confirmed and unconfirmed
publication. Observations do not form an atomic GitHub merge transaction.

This status establishes only the maintained local plan scope. It is not a GitHub
Actions job, a sealed software candidate, a configured branch rule or a product
release. Server rules need their own real readback; independent agent review does
not invent a CODEOWNERS account or a human approval. The existing release workflow
still requires its declared final-main CI jobs and trusted publication checks;
a local status cannot silently replace those requirements.

## Keep feedback fast and evidence complete

Use a focused development run for the behavior being changed, then execute the
normal complete acceptance gates for the final source. Discover the maintained
case names instead of adding duplicate test programs. Retain the original RED
failure, GREEN result and actual command. A filter or a cached result cannot
qualify a complete candidate.

`python tools/dev/dev.py timings /path/to/receipts` summarizes existing process
receipts without rerunning work. Separate preparation, configuration, compilation,
tests, documentation and SDK consumption. Preserve failed/cancelled attempts and
unknown CPU or RSS observations; overlapping wall times are not additive elapsed
time. Compare equivalent source/tool/profile scopes before attributing a speedup.

On Linux, a normally reaped process, including a nonzero exit, can have
`resource_usage` observed by `wait4`: user/system CPU seconds and
`maximum_process_rss_bytes`.
The scope is the waited process and descendants it has already reaped. The RSS
value is the largest observed single-process peak, not simultaneous process-tree
memory. Timeout, cancellation, launch failure, older receipts and other platforms
retain unknown values rather than estimating them from wall time.

The timings summary exposes `measured_resource_attempts` and
`maximum_observed_process_rss_bytes` while preserving each original receipt.
Aggregate `cpu_seconds`, `peak_rss_bytes` and `cache_hits` remain unknown because
nested receipts can double-count work and these observations do not establish a
global memory peak or cache behavior. Compare cold and warm attempts separately
under the same toolchain, source and profile; measurements from a small tool
regression do not quantify whole-project build improvement.

Choose an explicit finite compiler budget for available RAM and disk, not just
CPU count. Reuse verified source/tool preparation and ordinary incremental builds;
invalidate changed configuration, dependencies and generated inputs normally.
Do not cache acceptance reports, restore incompatible objects or parallelize two
mutations of the same build tree. Full gate discovery, actual exits, output
identities and failure/cancellation retention remain mandatory.

Before running a long qualification plan, select an explicit finite outer
command budget and a fresh canonical absolute output path outside the platform
input checkout, without symlink or parent-path aliases. `NEXUS_COMMAND_TIMEOUT`
supplies the default where a process call has no explicit timeout; it does not
replace the plan's stage deadlines. The maintained software plan packages an SDK
under its output, so the existing SDK boundary rejects an output inside the
platform checkout or with path aliases.

Check the invocation environment first: maintained provider and TrustZone
qualification reject every `GIT_` variable, including the display-only
`GIT_PAGER`. Omit an injected `GIT_PAGER` only from this launched process, and
stop if other `GIT_` names remain. Follow the
[development command reference](../../tools/dev/README.md) for the maintained
invocation. Preserve failed and interrupted attempts; a larger retry budget and
successful early stages do not establish a completed plan or a sealed candidate.

Before deleting a test, identify its behavioral obligation and independent
remaining coverage. Remove only an obsolete behavior or a demonstrated duplicate
of the same contract. Keep tests that expose rejected ownership, retained borrows,
instance isolation, cancellation, IRQ ordering, real vendor behavior or external
SDK consumption. A passing implementation-shaped assertion alone is not an
independent substitute. If the audit finds no safe deletion, retain the test;
shrinking a count is not an optimization objective.

## Preserve style and scope

Use the unchanged `.clang-format` and `.editorconfig`: 80 columns, four spaces,
attached braces, type-attached pointer stars and backslash Doxygen comments.
Public headers document context, deadline, ownership and failures. Source comments
explain invariants and hardware ordering instead of repeating declaration parameters.
New/changed files pass whole-file checks; existing frozen debt may only shrink.

Commit one reviewable change with Conventional Commits. Review interface/configuration
changes together with diagrams, examples, translation and execution scope. Keep
products, workers, private PCB and recovery policy outside Nexus. Record actual
source/configuration/tool/ELF identities and physical pending items; a successful
commit or CI does not promote a product release.
